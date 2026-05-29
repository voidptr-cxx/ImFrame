# Getting Started with ImFrame

> Placeholder — documentation is written from Phase 7 onwards once the
> Application and Window API is stable.

## Building

```bash
git clone https://github.com/voidptr-cxx/ImFrame.git
cd ImFrame

# Configure with vcpkg (set VCPKG_ROOT to your vcpkg installation)
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release
```

## Phase Status

See `.claude/PHASE_STATUS.md` for the current implementation progress.
