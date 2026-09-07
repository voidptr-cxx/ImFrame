/**
 * @file     Delegate.hpp
 * @brief    Zero-heap-allocation single-target callable wrapper
 *
 * Delegate<R(Args...)> stores a single callable in a fixed-size internal
 * buffer — no dynamic allocation occurs for any supported target. Supported
 * targets are free functions and member functions (via Bind<>). Small lambdas
 * and functors are also supported if sizeof(F) <= BUFFER_SIZE; larger ones
 * are rejected at compile time via a requires clause.
 *
 * Prefer Delegate over std::function for hot paths: widget callbacks (Phase 10),
 * Logger sinks and Config change handlers (Phase 6), and Timer fire callbacks
 * (Phase 6). Use std::function when you need heap-allocated callables or
 * type-erasing a callable you cannot size-check at compile time.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Core/Error.hpp"

#include <concepts>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace ImFrame::Utility {

/**
 * @class    Delegate
 * @brief    Primary template — undefined; specialise on a function signature
 *
 * @tparam   Sig  Function signature, e.g. `void()` or `bool(float)`
 *
 * @since    0.5.0
 */
template <typename Sig>
class Delegate;

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class    Delegate<R(Args...)>
 * @brief    Zero-heap-allocation single-target callable with a 16-byte buffer
 *
 * Stores one callable target without dynamic allocation. The internal buffer
 * is `2 * sizeof(void*)` bytes (16 bytes on 64-bit platforms). Any callable
 * that does not fit in this buffer is rejected at compile time.
 *
 * Three storage strategies are supported:
 *  - **Free function** — function pointer stored directly in the buffer.
 *  - **Member function** — only the object pointer is stored; the method is
 *    encoded as a template parameter of `Bind<Method>(obj)`, giving the same
 *    cost as a virtual-dispatch trampoline without the vtable lookup.
 *  - **Functor / lambda** — placement-new'd into the buffer; requires
 *    `sizeof(F) <= BUFFER_SIZE` and `std::is_copy_constructible_v<F>`.
 *
 * Delegate is copyable and moveable. Copy is always `noexcept` for the
 * supported target types (all fit within a 16-byte trivially or safely
 * copyable range). Move leaves the source in the empty/unassigned state.
 *
 * @tparam R       Return type of the stored callable
 * @tparam Args    Parameter types of the stored callable
 *
 * @since    0.5.0
 *
 * @example
 * @code
 * using namespace ImFrame::Utility;
 *
 * // Free function
 * Delegate<int(int)> d1{[](int x) { return x * 2; }};
 *
 * // Member function
 * struct Processor { int Process(int x) { return x + 1; } };
 * Processor p;
 * auto d2 = Delegate<int(int)>::Bind<&Processor::Process>(&p);
 *
 * if (d2) std::println("{}", d2(41)); // 42
 * @endcode
 */
template <typename R, typename... Args>
class Delegate<R(Args...)> {
public:
    /// Size of the internal storage buffer (bytes).
    static constexpr std::size_t BUFFER_SIZE = 2 * sizeof(void*);

private:
    using InvokeFn  = R(*)(const std::byte*, Args...);
    using DestroyFn = void(*)(std::byte*);
    using CopyFn    = void(*)(std::byte*, const std::byte*);

    alignas(std::max_align_t) std::byte _storage[BUFFER_SIZE]{};
    InvokeFn  _invoke{nullptr};
    DestroyFn _destroy{nullptr};
    CopyFn    _copy{nullptr};

    // ── Free-function trampoline ───────────────────────────────────────────
    static R InvokeFreeFn(const std::byte* s, Args... args) {
        using FnPtr = R(*)(Args...);
        FnPtr fn;
        std::memcpy(&fn, s, sizeof(FnPtr));
        return fn(std::forward<Args>(args)...);
    }

    // ── Functor trampolines ────────────────────────────────────────────────
    template <typename F>
    static R InvokeFunctor(const std::byte* s, Args... args) {
        return (*std::launder(reinterpret_cast<const F*>(s)))(std::forward<Args>(args)...);
    }

    template <typename F>
    static void DestroyFunctor(std::byte* s) {
        std::launder(reinterpret_cast<F*>(s))->~F();
    }

    template <typename F>
    static void CopyFunctor(std::byte* dst, const std::byte* src) {
        ::new (dst) F(*std::launder(reinterpret_cast<const F*>(src)));
    }

    // ── Member-function trampoline (NTTP) ──────────────────────────────────
    template <auto Method, typename T>
    static R InvokeMemberFn(const std::byte* s, Args... args) {
        T* obj;
        std::memcpy(&obj, s, sizeof(T*));
        return (obj->*Method)(std::forward<Args>(args)...);
    }

    // ── Trivial bitwise copy (fn-ptrs, obj-ptrs) ──────────────────────────
    static void TrivialCopy(std::byte* dst, const std::byte* src) {
        std::memcpy(dst, src, BUFFER_SIZE);
    }

public:
    // ── Lifecycle ─────────────────────────────────────────────────────────

    /// Constructs an empty (unassigned) delegate. `operator bool()` returns false.
    Delegate() noexcept = default;

    /**
     * @brief  Destructor — calls the stored callable's destructor if needed
     */
    ~Delegate() noexcept { Reset(); }

    /**
     * @brief    Constructs a delegate targeting a free function
     *
     * @param[in] fn  Free function pointer. Must not be null.
     */
    /* implicit */ Delegate(R(*fn)(Args...)) noexcept { // NOLINT(google-explicit-constructor)
        static_assert(sizeof(fn) <= BUFFER_SIZE, "function pointer does not fit in Delegate buffer");
        std::memcpy(_storage, &fn, sizeof(fn));
        _invoke  = &InvokeFreeFn;
        _copy    = &TrivialCopy;
    }

    /**
     * @brief    Constructs a delegate from a functor or lambda
     *
     * The callable is placement-new'd into the internal buffer. Compile error
     * if `sizeof(F) > BUFFER_SIZE` or if `F` is not copy-constructible.
     *
     * @tparam   F  Deduced callable type (must not be a raw function pointer)
     * @param[in] f  The callable to store. Captured by value.
     */
    template <typename F>
        requires (!std::is_pointer_v<std::decay_t<F>> &&
                  !std::is_member_pointer_v<std::decay_t<F>> &&
                  std::invocable<std::decay_t<F>, Args...> &&
                  sizeof(std::decay_t<F>) <= BUFFER_SIZE &&
                  std::is_copy_constructible_v<std::decay_t<F>>)
    /* implicit */ Delegate(F&& f) noexcept(std::is_nothrow_constructible_v<std::decay_t<F>, F>) { // NOLINT(google-explicit-constructor)
        using FD = std::decay_t<F>;
        ::new (_storage) FD(std::forward<F>(f));
        _invoke  = &InvokeFunctor<FD>;
        _destroy = std::is_trivially_destructible_v<FD> ? nullptr : &DestroyFunctor<FD>;
        _copy    = std::is_trivially_copyable_v<FD>     ? &TrivialCopy : &CopyFunctor<FD>;
    }

    /**
     * @brief  Copy constructor
     */
    Delegate(const Delegate& other) noexcept(false) {
        if (other._invoke) {
            if (other._copy) other._copy(_storage, other._storage);
            else             std::memcpy(_storage, other._storage, BUFFER_SIZE);
        }
        _invoke  = other._invoke;
        _destroy = other._destroy;
        _copy    = other._copy;
    }

    /**
     * @brief  Copy-assignment operator
     */
    Delegate& operator=(const Delegate& other) noexcept(false) {
        if (this != &other) {
            Reset();
            if (other._invoke) {
                if (other._copy) other._copy(_storage, other._storage);
                else             std::memcpy(_storage, other._storage, BUFFER_SIZE);
            }
            _invoke  = other._invoke;
            _destroy = other._destroy;
            _copy    = other._copy;
        }
        return *this;
    }

    /**
     * @brief  Move constructor — source is left empty after move
     */
    Delegate(Delegate&& other) noexcept {
        if (other._invoke) {
            // Copy-construct into our storage (all supported types are small; move = copy + destroy)
            if (other._copy) other._copy(_storage, other._storage);
            else             std::memcpy(_storage, other._storage, BUFFER_SIZE);
            _invoke  = other._invoke;
            _destroy = other._destroy;
            _copy    = other._copy;
            // Destroy source storage, then clear its pointers
            if (other._destroy) other._destroy(other._storage);
            other._invoke  = nullptr;
            other._destroy = nullptr;
            other._copy    = nullptr;
        }
    }

    /**
     * @brief  Move-assignment operator — source is left empty after move
     */
    Delegate& operator=(Delegate&& other) noexcept {
        if (this != &other) {
            Reset();
            if (other._invoke) {
                if (other._copy) other._copy(_storage, other._storage);
                else             std::memcpy(_storage, other._storage, BUFFER_SIZE);
                _invoke  = other._invoke;
                _destroy = other._destroy;
                _copy    = other._copy;
                if (other._destroy) other._destroy(other._storage);
                other._invoke  = nullptr;
                other._destroy = nullptr;
                other._copy    = nullptr;
            }
        }
        return *this;
    }

    // ── Factory ───────────────────────────────────────────────────────────

    /**
     * @brief    Creates a delegate bound to a member function via NTTP
     *
     * The object pointer is stored in the buffer; the method is encoded as
     * a template parameter of the trampoline function — no PMF stored at
     * runtime. This avoids MSVC multiple-inheritance PMF size issues.
     *
     * @tparam   Method  Non-type template parameter: a pointer-to-member-function
     *                   with signature `R(T::*)(Args...)`.
     * @tparam   T       Object type, deduced from `obj`.
     *
     * @param[in] obj  Pointer to the object whose method to invoke. Must not be null.
     *                 The object must remain alive for the lifetime of the delegate.
     *
     * @return   A delegate that calls `(obj->*Method)(args...)` on invocation.
     */
    template <auto Method, typename T>
    [[nodiscard]] static Delegate Bind(T* obj) noexcept {
        static_assert(sizeof(T*) <= BUFFER_SIZE, "object pointer does not fit in Delegate buffer");
        Delegate d;
        std::memcpy(d._storage, &obj, sizeof(T*));
        d._invoke = &InvokeMemberFn<Method, T>;
        d._copy   = &TrivialCopy;
        return d;
    }

    // ── Observers ─────────────────────────────────────────────────────────

    /**
     * @brief   Returns true if a callable target is assigned
     * @return  `true` when the delegate holds a target; `false` when empty
     */
    [[nodiscard]] explicit operator bool() const noexcept { return _invoke != nullptr; }

    // ── Invocation ────────────────────────────────────────────────────────

    /**
     * @brief    Invokes the stored callable
     *
     * @param[in] args  Arguments forwarded to the target
     * @return   The return value of the target
     *
     * @warning  Undefined behaviour if the delegate is empty (operator bool() == false).
     *           IMF_ASSERT fires in debug builds.
     */
    R operator()(Args... args) const {
        IMF_ASSERT(_invoke != nullptr);
        return _invoke(_storage, std::forward<Args>(args)...);
    }

    // ── Mutators ──────────────────────────────────────────────────────────

    /**
     * @brief  Clears the stored target, leaving the delegate empty
     */
    void Reset() noexcept {
        if (_destroy) {
            _destroy(_storage);
        }
        _invoke  = nullptr;
        _destroy = nullptr;
        _copy    = nullptr;
    }
};

} // namespace ImFrame::Utility
