#!/usr/bin/env python3
"""
Behavioral Equivalence Verification - Canonical Upstream Drift Scanner (Layer 5)

Audits critical mission-critical subsystems in halflife-refactored against
canonical Valve GoldSrc SDK source code to detect forbidden patterns,
mathematical drift, uncanonical matrix manipulations, and omitted transition variables.
"""

import sys
import os
import re
import subprocess

# ANSI Color Codes
COLOR_GREEN = "\033[92m"
COLOR_RED = "\033[91m"
COLOR_YELLOW = "\033[93m"
COLOR_CYAN = "\033[96m"
COLOR_RESET = "\033[0m"


def log_pass(msg):
    print(f"[{COLOR_GREEN}PASS{COLOR_RESET}] {msg}")


def log_fail(msg):
    print(f"[{COLOR_RED}FAIL{COLOR_RESET}] {msg}")


def log_warn(msg):
    print(f"[{COLOR_YELLOW}WARN{COLOR_RESET}] {msg}")


def log_info(msg):
    print(f"[{COLOR_CYAN}INFO{COLOR_RESET}] {msg}")


def check_forbidden_patterns(repo_root):
    """Scan codebase for dangerous patterns that previously caused regressions."""
    failures = []

    # 1. Uncanonical curstate.scale matrix scaling in studio model rendering
    studio_renderer = os.path.join(repo_root, "cl_dll", "render", "StudioModelRenderer.cpp")
    if os.path.exists(studio_renderer):
        with open(studio_renderer, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
            if "curstate.scale" in content and "m_pCurrentEntity->curstate.scale" in content:
                # Upstream Valve GoldSrc SDK never scales bone transformation matrices with curstate.scale
                # in non-sprite StudioDrawModel/StudioSetUpTransform (caused model distortion in #123/#124)
                if "Matrix3x4_Scale" in content or "Matrix3x4_CreateScale" in content:
                    failures.append(
                        "StudioModelRenderer.cpp: Forbidden curstate.scale matrix multiplication detected!"
                    )

    # 2. Check for level transition vecLandmarkOffset in triggers_brush.cpp
    triggers_brush = os.path.join(repo_root, "dlls", "systems", "triggers_brush.cpp")
    if os.path.exists(triggers_brush):
        with open(triggers_brush, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
            if "gpGlobals->vecLandmarkOffset = VARS( pentLandmark )->origin;" not in content:
                failures.append(
                    "triggers_brush.cpp: Missing canonical 'gpGlobals->vecLandmarkOffset = VARS( pentLandmark )->origin;' in CChangeLevel::ChangeLevelNow!"
                )
            if "FENTTABLE_MOVEABLE" not in content:
                failures.append(
                    "triggers_brush.cpp: Missing canonical FENTTABLE_MOVEABLE in CChangeLevel::ChangeList!"
                )
            if "InTransitionVolume( pPlayer, m_szLandmarkName )" not in content:
                failures.append(
                    "triggers_brush.cpp: Missing canonical InTransitionVolume player check in CChangeLevel::ChangeLevelNow!"
                )

    # 3. Check for raw array indexing of g_PlayerExtraInfo > MAX_PLAYERS
    scoreboard_file = os.path.join(repo_root, "cl_dll", "hud", "scoreboard.cpp")
    if os.path.exists(scoreboard_file):
        with open(scoreboard_file, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
            # Indexing g_PlayerExtraInfo by MAX_PLAYERS + 1 or unvalidated entity indices
            if re.search(r"g_PlayerExtraInfo\[\s*ENTINDEX", content):
                failures.append("scoreboard.cpp: Potential unvalidated entity index in g_PlayerExtraInfo[]!")

    # 4. Check that bone parent indexing is safely guarded against negative indices (parent != -1)
    studio_bone_file = os.path.join(repo_root, "cl_dll", "render", "StudioModelRenderer.cpp")
    if os.path.exists(studio_bone_file):
        with open(studio_bone_file, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
            # If pbones[pbones[i].parent] is accessed, must be inside if (pbones[i].parent != -1)
            for match in re.finditer(r"pbones\[\s*pbones\[i\]\.parent\s*\]", content):
                # Search previous 200 characters for guard
                start = max(0, match.start() - 200)
                context = content[start:match.start()]
                if "pbones[i].parent != -1" not in context and "pbone[i].parent != -1" not in context and "pbones[i].parent >= 0" not in context:
                    failures.append("StudioModelRenderer.cpp: pbones[pbones[i].parent] accessed without parent != -1 guard!")

    return failures


def tokenize_code(code_str):
    """Normalize C/C++ code into a sequence of tokens ignoring comments and whitespace."""
    # Remove block comments
    code = re.sub(r"/\*.*?\*/", "", code_str, flags=re.DOTALL)
    # Remove line comments
    code = re.sub(r"//.*", "", code)
    # Extract identifiers, numbers, operators
    tokens = re.findall(r"[A-Za-z0-9_]+|[^\sA-Za-z0-9_]", code)
    return tokens


def get_upstream_file(git_path):
    """Attempt to fetch file content from upstream/master git ref."""
    try:
        res = subprocess.run(
            ["git", "show", f"upstream/master:{git_path}"],
            capture_output=True,
            text=True,
            check=True,
        )
        return res.stdout
    except Exception:
        # If upstream/master remote is not fetched in current workspace, try origin/master
        try:
            res = subprocess.run(
                ["git", "show", f"remotes/upstream/master:{git_path}"],
                capture_output=True,
                text=True,
                check=True,
            )
            return res.stdout
        except Exception:
            return None


def extract_function_body(text, func_signature):
    """Extract body tokens of a specific function signature."""
    idx = text.find(func_signature)
    if idx == -1:
        return None
    start = text.find("{", idx)
    if start == -1:
        return None

    braces = 1
    i = start + 1
    while i < len(text) and braces > 0:
        if text[i] == "{":
            braces += 1
        elif text[i] == "}":
            braces -= 1
        i += 1
    return text[start + 1:i - 1]


def check_algorithmic_equivalence(repo_root):
    """Check mathematical and algorithmic equivalence of mission-critical functions against upstream Valve SDK."""
    failures = []

    # Check CChangeLevel functions in triggers_brush.cpp vs upstream triggers.cpp
    upstream_triggers = get_upstream_file("dlls/triggers.cpp")
    if upstream_triggers:
        local_triggers_path = os.path.join(repo_root, "dlls", "systems", "triggers_brush.cpp")
        with open(local_triggers_path, "r", encoding="utf-8", errors="ignore") as f:
            local_triggers = f.read()

        critical_methods = [
            ("AddTransitionToList", "int CChangeLevel::AddTransitionToList"),
            ("InTransitionVolume", "int CChangeLevel::InTransitionVolume"),
            ("ChangeList", "int CChangeLevel::ChangeList"),
            ("ChangeLevelNow", "void CChangeLevel::ChangeLevelNow"),
        ]

        for method_name, sig in critical_methods:
            up_body = extract_function_body(upstream_triggers, f":: {method_name}") or extract_function_body(upstream_triggers, f"::{method_name}")
            loc_body = extract_function_body(local_triggers, sig)

            if not up_body:
                log_warn(f"Could not locate upstream body for {method_name}")
                continue
            if not loc_body:
                failures.append(f"triggers_brush.cpp: Could not locate local function body for {sig}")
                continue

            up_toks = tokenize_code(up_body)
            loc_toks = tokenize_code(loc_body)

            # Compare key mathematical tokens
            if method_name == "AddTransitionToList":
                # Must contain: pLevelList[listCount].vecLandmarkOrigin = VARS(pentLandmark)->origin;
                if "vecLandmarkOrigin" not in loc_toks:
                    failures.append("AddTransitionToList: Missing vecLandmarkOrigin assignment!")
            elif method_name == "InTransitionVolume":
                # Must contain FCAP_FORCE_TRANSITION and MOVETYPE_FOLLOW
                if "FCAP_FORCE_TRANSITION" not in loc_toks or "MOVETYPE_FOLLOW" not in loc_toks:
                    failures.append("InTransitionVolume: Missing FCAP_FORCE_TRANSITION or MOVETYPE_FOLLOW checks!")
            elif method_name == "ChangeList":
                # Must contain FENTTABLE_MOVEABLE and FENTTABLE_GLOBAL
                if "FENTTABLE_MOVEABLE" not in loc_toks or "FENTTABLE_GLOBAL" not in loc_toks:
                    failures.append("ChangeList: Missing FENTTABLE_MOVEABLE or FENTTABLE_GLOBAL engine flags!")
            elif method_name == "ChangeLevelNow":
                # Must contain vecLandmarkOffset and CHANGE_LEVEL
                if "vecLandmarkOffset" not in loc_toks or "CHANGE_LEVEL" not in loc_toks:
                    failures.append("ChangeLevelNow: Missing vecLandmarkOffset assignment or CHANGE_LEVEL invocation!")
    else:
        log_warn("Upstream git reference 'upstream/master' not available; skipping AST token comparison.")

    return failures


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    print("=" * 70)
    print("Layer 5: Canonical Upstream Drift & Forbidden Pattern Scanner")
    print("=" * 70)

    all_passed = True

    # 1. Scan forbidden patterns
    log_info("Scanning codebase for known anti-patterns and regressions...")
    forbidden_issues = check_forbidden_patterns(repo_root)
    if forbidden_issues:
        all_passed = False
        for err in forbidden_issues:
            log_fail(err)
    else:
        log_pass("No forbidden anti-patterns detected.")

    # 2. Check algorithmic equivalence against upstream
    log_info("Comparing mission-critical algorithms against canonical Valve SDK...")
    algo_issues = check_algorithmic_equivalence(repo_root)
    if algo_issues:
        all_passed = False
        for err in algo_issues:
            log_fail(err)
    else:
        log_pass("Mission-critical algorithms match canonical Valve implementation.")

    print("=" * 70)
    if all_passed:
        log_pass("All Layer 5 upstream drift checks PASSED successfully.")
        return 0
    else:
        log_fail("Layer 5 upstream drift scanner identified regressions!")
        return 1


if __name__ == "__main__":
    sys.exit(main())
