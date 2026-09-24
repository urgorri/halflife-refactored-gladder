#!/usr/bin/env python3
"""Verify TYPEDESCRIPTION save/restore tables against a golden baseline.

This script parses all TYPEDESCRIPTION arrays from Half-Life SDK source files
and validates them against a checked-in golden baseline JSON. It checks field
counts, names, types, array sizes, and positions to prevent savegame and
level-transition serialization regressions.

Usage:
    python verify_saverestore.py [--golden PATH] [--repo-root PATH]

To update the baseline after intentional entity serialization changes:
    1. Make the change in the source code.
    2. Run: python verify_saverestore.py --update
    3. Review the diff in tests/golden/saverestore_baseline.json
    4. Commit both the source change and updated golden file.
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path


KNOWN_CONSTANTS = {
    "MAX_ITEMS": 5,
    "MAX_ITEM_TYPES": 6,
    "MAX_AMMO_SLOTS": 32,
    "MAX_AMMO_TYPES": 32,
    "CSUITPLAYLIST": 4,
    "CSUITNOREPEAT": 32,
    "MAX_WEAPONS": 32,
}


def find_repo_root(start: Path) -> Path:
    """Walk up from start until AGENTS.md is found."""
    current = start.resolve()
    while current != current.parent:
        if (current / "AGENTS.md").exists():
            return current
        current = current.parent
    return start.parent


def resolve_size(size_str: str | int) -> int:
    """Resolve a field size expression (literal or macro constant)."""
    if isinstance(size_str, int):
        return size_str
    size_str = str(size_str).strip()
    if re.fullmatch(r"\d+", size_str):
        return int(size_str)
    if size_str in KNOWN_CONSTANTS:
        return KNOWN_CONSTANTS[size_str]
    return 1


def parse_saverestore_tables(repo_root: Path) -> dict:
    """Parse all TYPEDESCRIPTION tables from dlls/ directory."""
    dlls_dir = repo_root / "dlls"
    if not dlls_dir.exists():
        return {}

    save_pattern = re.compile(
        r"TYPEDESCRIPTION\s+(?:(\w+)::)?(\w+)\[\]\s*=\s*\{([^}]+)\};", re.DOTALL
    )
    field_pattern = re.compile(
        r"(DEFINE_FIELD|DEFINE_ARRAY|DEFINE_ENTITY_FIELD|DEFINE_GLOBAL_FIELD|DEFINE_ENTITY_GLOBAL_FIELD)\s*\(([^)]+)\)"
    )

    tables: dict[str, dict] = {}

    for filepath in sorted(dlls_dir.rglob("*.cpp")):
        content = filepath.read_text(encoding="utf-8", errors="replace")

        # Handle CLIENT_WEAPONS conditional blocks: keep CLIENT_WEAPONS branch
        content = re.sub(
            r"#if\s+defined\(\s*CLIENT_WEAPONS\s*\)(.*?)#else.*?#endif",
            r"\1",
            content,
            flags=re.DOTALL,
        )

        for match in save_pattern.finditer(content):
            cls_name = match.group(1) or match.group(2)
            arr_name = match.group(2)
            body = match.group(3)

            fields = []
            for line in body.split("\n"):
                line = re.sub(r"//.*$", "", line)
                line = re.sub(r"/\*.*?\*/", "", line).strip()
                if not line:
                    continue

                fm = field_pattern.search(line)
                if not fm:
                    continue

                macro = fm.group(1)
                args = [a.strip() for a in fm.group(2).split(",")]

                if macro in ("DEFINE_FIELD", "DEFINE_GLOBAL_FIELD"):
                    # DEFINE_FIELD( Class, Member, Type )
                    fields.append({
                        "name": args[1],
                        "type": args[2],
                        "size": 1,
                        "global": (macro == "DEFINE_GLOBAL_FIELD"),
                    })
                elif macro in ("DEFINE_ENTITY_FIELD", "DEFINE_ENTITY_GLOBAL_FIELD"):
                    # DEFINE_ENTITY_FIELD( Member, Type )
                    fields.append({
                        "name": args[0],
                        "type": args[1],
                        "size": 1,
                        "global": (macro == "DEFINE_ENTITY_GLOBAL_FIELD"),
                    })
                elif macro == "DEFINE_ARRAY":
                    # DEFINE_ARRAY( Class, Member, Type, Count )
                    raw_count = args[3]
                    resolved_count = resolve_size(raw_count)
                    fields.append({
                        "name": args[1],
                        "type": args[2],
                        "size": resolved_count,
                        "size_expr": raw_count,
                        "global": False,
                    })

            rel_file = str(filepath.relative_to(repo_root)).replace("\\", "/")
            tables[cls_name] = {
                "array": arr_name,
                "file": rel_file,
                "field_count": len(fields),
                "fields": fields,
            }

    return tables


def verify_table(
    table_name: str, golden_table: dict, actual_table: dict
) -> list[str]:
    """Verify a single table against golden baseline."""
    errors: list[str] = []

    expected_count = golden_table["field_count"]
    actual_count = actual_table["field_count"]

    if expected_count != actual_count:
        errors.append(
            f"  FIELD COUNT MISMATCH in {table_name}: expected {expected_count}, got {actual_count}"
        )

    expected_fields = golden_table["fields"]
    actual_fields = actual_table["fields"]

    for i, exp in enumerate(expected_fields):
        if i >= len(actual_fields):
            errors.append(
                f"  MISSING FIELD at index {i}: '{exp['name']}' ({exp['type']})"
            )
            continue

        act = actual_fields[i]
        if exp["name"] != act["name"]:
            errors.append(
                f"  FIELD NAME MISMATCH at index {i}: expected '{exp['name']}', got '{act['name']}'"
            )
        if exp["type"] != act["type"]:
            errors.append(
                f"  FIELD TYPE MISMATCH for '{exp['name']}' at index {i}: expected {exp['type']}, got {act['type']}"
            )
        if exp.get("size", 1) != act.get("size", 1):
            errors.append(
                f"  FIELD SIZE MISMATCH for '{exp['name']}' at index {i}: expected {exp.get('size')}, got {act.get('size')}"
            )

    if len(actual_fields) > len(expected_fields):
        for i in range(len(expected_fields), len(actual_fields)):
            errors.append(
                f"  EXTRA FIELD at index {i}: '{actual_fields[i]['name']}' ({actual_fields[i]['type']})"
            )

    return errors


def build_golden_baseline(repo_root: Path) -> dict:
    """Build complete golden baseline dict."""
    tables = parse_saverestore_tables(repo_root)

    # Scoped critical tables to verify for Phase 1 + core entity persistence
    priority_tables = [
        "CBasePlayer",
        "CBasePlayerItem",
        "CBasePlayerWeapon",
        "CGauss",
        "CEgon",
        "CRpg",
        "CSatchel",
        "CShotgun",
        "CWeaponBox",
        "CHornet",
        "CSqueakGrenade",
        "CRpgRocket",
        "CTripmineGrenade",
        "CBaseEntity",
        "CBaseDelay",
        "CBaseAnimating",
        "CBaseToggle",
        "CWallHealth",
        "CWallRecharge",
        "gEntvarsDescription",
    ]

    scoped_tables = {}
    for name in priority_tables:
        if name in tables:
            scoped_tables[name] = tables[name]

    return {
        "_description": "Golden baseline for TYPEDESCRIPTION save/restore tables (Phase 1: Player, Weapons, Projectiles, and Base Entities).",
        "_version": 1,
        "tables": scoped_tables,
        "all_discovered_tables_count": len(tables),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify TYPEDESCRIPTION save/restore tables against golden baseline"
    )
    parser.add_argument(
        "--golden",
        default=None,
        help="Path to golden saverestore JSON file",
    )
    parser.add_argument(
        "--repo-root",
        default=None,
        help="Path to repository root",
    )
    parser.add_argument(
        "--update",
        action="store_true",
        help="Update the golden file with current values",
    )
    args = parser.parse_args()

    script_dir = Path(__file__).parent
    repo_root = Path(args.repo_root) if args.repo_root else find_repo_root(script_dir)
    golden_path = (
        Path(args.golden)
        if args.golden
        else (repo_root / "tests" / "golden" / "saverestore_baseline.json")
    )

    if args.update:
        print(f"Updating golden saverestore baseline at: {golden_path}")
        golden_data = build_golden_baseline(repo_root)
        golden_path.parent.mkdir(parents=True, exist_ok=True)
        with open(golden_path, "w", encoding="utf-8") as f:
            json.dump(golden_data, f, indent=2)
            f.write("\n")
        print(f"Successfully wrote golden baseline to {golden_path}")
        return 0

    if not golden_path.exists():
        print(f"ERROR: Golden file not found: {golden_path}", file=sys.stderr)
        return 1

    with open(golden_path, "r", encoding="utf-8") as f:
        golden = json.load(f)

    actual_tables = parse_saverestore_tables(repo_root)
    golden_tables = golden.get("tables", {})

    all_errors: list[str] = []
    tables_checked = 0

    for table_name, expected_table in golden_tables.items():
        if table_name not in actual_tables:
            all_errors.append(f"MISSING TABLE: {table_name} not found in parsed codebase")
            continue

        print(f"Checking SaveData table: {table_name} ...")
        table_errors = verify_table(table_name, expected_table, actual_tables[table_name])
        if table_errors:
            all_errors.append(f"Table {table_name}:")
            all_errors.extend(table_errors)
        tables_checked += 1

    print(f"\nChecked {tables_checked} SaveData tables.")

    if all_errors:
        print(f"\nFAILED: {len(all_errors)} error(s) found:\n")
        for error in all_errors:
            print(error)
        print(
            "\nIf these changes are intentional, update the golden file:\n"
            "  python tests/verify_saverestore.py --update"
        )
        return 1
    else:
        print("PASSED: All SaveData tables match golden baseline.")
        return 0


if __name__ == "__main__":
    sys.exit(main())
