#!/usr/bin/env bash
# check_api_leaks.sh — Phase 18 API leak detector
#
# Verifies three invariants for include/ImFrame/ public headers:
#   1. No ImGui headers are directly included (#include <imgui*.h> / "imgui*.h")
#   2. No ImFrame::Internal:: qualified names in non-comment code
#   3. No raw ImGui type names in non-comment code (ImVec*, ImGui*, ImFont*, etc.)
#
# Allowlisted exceptions (intentional design from their respective phases):
#   include/ImFrame/Icons/IconFont.hpp — Phase 9 font loader; must expose ImFont*
#     and ImFontAtlas* because it IS the bridge between ImFrame and ImGui's font API.
#
# Exit 0 on pass, exit 1 on any violation (with file:line reports).

set -euo pipefail

PUBLIC_HEADERS="include/ImFrame"
ERRORS=0

if [ -t 1 ] && command -v tput &>/dev/null && tput colors &>/dev/null && [ "$(tput colors)" -ge 8 ]; then
    RED=$(tput setaf 1)
    GRN=$(tput setaf 2)
    RST=$(tput sgr0)
else
    RED=""
    GRN=""
    RST=""
fi

fail() {
    echo "${RED}FAIL${RST} $*"
    ERRORS=$((ERRORS + 1))
}

pass() {
    echo "${GRN}PASS${RST} $*"
}

# strip_comments: remove grep output lines where the code content (the part
# after "filepath:linenum:") starts with a C++ comment marker (// or block *).
# grep output format: "path/file.hpp:42: <content>"
strip_comments() {
    grep -vE ':[0-9]+:[[:space:]]*(//|/\*|\*)'
}

# ── Check 1: No imgui headers included in public headers ─────────────────────

echo "--- Check 1: No imgui #include in ${PUBLIC_HEADERS}/ ---"
HITS=$(grep -rn --include="*.hpp" -E '#[[:space:]]*include[[:space:]]*[<"][^>"]*imgui' \
    "${PUBLIC_HEADERS}" 2>/dev/null || true)
if [ -n "$HITS" ]; then
    while IFS= read -r line; do
        fail "imgui header included: ${line}"
    done <<< "$HITS"
else
    pass "No imgui #include found in public headers."
fi

# ── Check 2: No ImFrame::Internal:: in non-comment code ──────────────────────

echo "--- Check 2: No ImFrame::Internal:: in ${PUBLIC_HEADERS}/ ---"
HITS=$(grep -rn --include="*.hpp" 'ImFrame::Internal::' "${PUBLIC_HEADERS}" 2>/dev/null \
    | strip_comments \
    || true)
if [ -n "$HITS" ]; then
    while IFS= read -r line; do
        fail "Internal namespace exposed: ${line}"
    done <<< "$HITS"
else
    pass "No ImFrame::Internal:: in non-comment code."
fi

# ── Check 3: No raw ImGui type names in non-comment code ─────────────────────
# Matches: ImVec2/4, ImGuiXxx, ImDrawList, ImFont, ImFontAtlas, ImTextureID
# Allowlist: IconFont.hpp — intentional bridge to ImGui's font API (Phase 9).

echo "--- Check 3: No ImGui/ImVec/ImFont type names in ${PUBLIC_HEADERS}/ ---"
HITS=$(grep -rn --include="*.hpp" \
    -E '(ImVec[0-9]|ImGui[A-Za-z]+|ImDrawList|ImFont[A-Za-z]*|ImTextureID)' \
    "${PUBLIC_HEADERS}" 2>/dev/null \
    | strip_comments \
    | grep -v 'include/ImFrame/Icons/IconFont\.hpp' \
    || true)
if [ -n "$HITS" ]; then
    while IFS= read -r line; do
        fail "ImGui type leaked: ${line}"
    done <<< "$HITS"
else
    pass "No ImGui type names in non-comment code (IconFont.hpp exempted)."
fi

# ── Summary ───────────────────────────────────────────────────────────────────

echo ""
if [ "${ERRORS}" -eq 0 ]; then
    echo "${GRN}All API leak checks passed.${RST}"
    exit 0
else
    echo "${RED}${ERRORS} API leak violation(s) found. See above.${RST}"
    exit 1
fi
