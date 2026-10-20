#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
# SPDX-License-Identifier: GPL-3.0-or-later

import sys
import re
import xml.etree.ElementTree as ET
from pathlib import Path

def main():
    repo_dir = Path(__file__).resolve().parent.parent
    kcfg_file = repo_dir / "activewindowoutlineconfig.kcfg"
    install_file = repo_dir / "install-active-window-outline.sh"
    uninstall_file = repo_dir / "uninstall-active-window-outline.sh"

    if not kcfg_file.exists():
        sys.exit(f"Error: {kcfg_file} not found")
    if not install_file.exists():
        sys.exit(f"Error: {install_file} not found")
    if not uninstall_file.exists():
        sys.exit(f"Error: {uninstall_file} not found")

    # 1. Parse all entry names from .kcfg
    tree = ET.parse(kcfg_file)
    root = tree.getroot()
    kcfg_keys = set()
    for elem in root.iter():
        if elem.tag.endswith("entry"):
            name = elem.get("name")
            if name:
                kcfg_keys.add(name)

    print(f"Found {len(kcfg_keys)} configuration keys in {kcfg_file.name}: {sorted(kcfg_keys)}")

    install_content = install_file.read_text(encoding="utf-8")
    uninstall_content = uninstall_file.read_text(encoding="utf-8")

    failures = 0

    # 2. Check each key in installer
    for key in sorted(kcfg_keys):
        pattern = rf"\bset_default\s+{re.escape(key)}\b"
        if not re.search(pattern, install_content):
            print(f"FAIL: Key '{key}' defined in {kcfg_file.name} is missing 'set_default' in {install_file.name}", file=sys.stderr)
            failures += 1

    # 3. Check each key in uninstaller
    for key in sorted(kcfg_keys):
        # Look for the key name as a distinct word in the key removal loop
        pattern = rf"\b{re.escape(key)}\b"
        if not re.search(pattern, uninstall_content):
            print(f"FAIL: Key '{key}' defined in {kcfg_file.name} is missing cleanup in {uninstall_file.name}", file=sys.stderr)
            failures += 1

    if failures:
        print(f"\nconfigsync_test: FAILED ({failures} mismatches found)", file=sys.stderr)
        return 1

    print("configsync_test: PASSED (all configuration keys are fully synchronized)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
