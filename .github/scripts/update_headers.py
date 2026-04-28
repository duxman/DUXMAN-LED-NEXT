"""
update_headers.py
-----------------
Adds or updates the license/version header in all source files
(C/C++ .cpp/.h/.ino and Python .py) found under the directories
listed in SEARCH_DIRS.

Header format (C/C++):
    /*
     * duxman-led next - v<version>
     * Licensed under the Apache License 2.0
     * File: firmware/src/api/ApiService.cpp
     * Last commit: a1b2c3d - 2026-04-28
     */

Header format (Python):
    # duxman-led next - v<version>
    # Licensed under the Apache License 2.0
    # File: tools/flash.py
    # Last commit: a1b2c3d - 2026-04-28

Detection: the script recognises an existing header by checking whether the
first non-empty line of the file contains the string HEADER_MARKER.
If found it replaces the whole block; otherwise it prepends a fresh one.
"""

import json
import os
import re
import subprocess
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
RELEASE_INFO_PATH = "firmware/config/release-info.json"

# Directories (relative to repo root) to scan for source files
SEARCH_DIRS = [
    "firmware/src",
    "tools",
]

# File extensions to process
TARGET_EXTENSIONS = {".cpp", ".h", ".ino", ".py"}

# String that uniquely identifies our header inside the first ~300 chars
HEADER_MARKER = "duxman-led next"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def load_version() -> str:
    """Read the project version from release-info.json."""
    try:
        with open(RELEASE_INFO_PATH, encoding="utf-8") as f:
            data = json.load(f)
        return data.get("version", "unknown")
    except Exception as exc:
        print(f"[warn] Could not read {RELEASE_INFO_PATH}: {exc}")
        return "unknown"


def get_file_git_info(filepath: str) -> tuple[str, str]:
    """
    Return (short_hash, date) of the last commit that touched *filepath*.
    Falls back to ('new', '<today>') when the file has no git history yet.
    """
    try:
        result = subprocess.run(
            ["git", "log", "-1", "--format=%h|%ad", "--date=short", "--", filepath],
            capture_output=True,
            text=True,
            check=False,
        )
        output = result.stdout.strip()
        if output:
            parts = output.split("|", 1)
            if len(parts) == 2:
                return parts[0].strip(), parts[1].strip()
    except FileNotFoundError:
        pass  # git not available

    # Fallback: file is brand-new and not yet committed
    import datetime
    return "new", datetime.date.today().isoformat()


def build_c_header(rel_path: str, version: str, commit_hash: str, commit_date: str) -> str:
    return (
        f"/*\n"
        f" * {HEADER_MARKER} - v{version}\n"
        f" * Licensed under the Apache License 2.0\n"
        f" * File: {rel_path}\n"
        f" * Last commit: {commit_hash} - {commit_date}\n"
        f" */\n"
    )


def build_py_header(rel_path: str, version: str, commit_hash: str, commit_date: str) -> str:
    return (
        f"# {HEADER_MARKER} - v{version}\n"
        f"# Licensed under the Apache License 2.0\n"
        f"# File: {rel_path}\n"
        f"# Last commit: {commit_hash} - {commit_date}\n"
    )


# ---------------------------------------------------------------------------
# Core logic
# ---------------------------------------------------------------------------

def _strip_existing_c_header(content: str) -> str:
    """
    Remove the first C-style block comment if it contains HEADER_MARKER.
    Returns the remaining content stripped of leading blank lines.
    """
    stripped = content.lstrip()
    if not stripped.startswith("/*"):
        return content  # nothing to remove

    # Check marker is within the first comment
    end_idx = stripped.find("*/")
    if end_idx == -1:
        return content  # malformed, leave untouched

    candidate = stripped[: end_idx + 2]
    if HEADER_MARKER not in candidate:
        return content  # different comment, leave untouched

    rest = stripped[end_idx + 2 :].lstrip("\n")
    return rest


def _strip_existing_py_header(content: str) -> str:
    """
    Remove consecutive leading # lines that contain HEADER_MARKER in the first one.
    Returns the remaining content.
    """
    lines = content.split("\n")
    if not lines or HEADER_MARKER not in lines[0]:
        return content

    # Consume all leading comment lines that belong to our header block
    end = 0
    for i, line in enumerate(lines):
        if line.startswith("#"):
            end = i + 1
        else:
            break

    rest = "\n".join(lines[end:]).lstrip("\n")
    return rest


def update_file_header(filepath: Path, version: str, repo_root: Path) -> bool:
    """
    Inspect *filepath*, add or replace the license header, write back if changed.
    Returns True if the file was modified.
    """
    # Always use a forward-slash repo-relative path in the header
    rel_path = filepath.relative_to(repo_root).as_posix()
    is_python = filepath.suffix == ".py"

    try:
        original = filepath.read_text(encoding="utf-8", errors="replace")
    except Exception as exc:
        print(f"  [skip] Cannot read {rel_path}: {exc}")
        return False

    commit_hash, commit_date = get_file_git_info(rel_path)

    if is_python:
        new_header = build_py_header(rel_path, version, commit_hash, commit_date)
        body = _strip_existing_py_header(original)
        new_content = new_header + "\n" + body if body else new_header
    else:
        new_header = build_c_header(rel_path, version, commit_hash, commit_date)
        body = _strip_existing_c_header(original)
        new_content = new_header + "\n" + body if body else new_header

    if new_content == original:
        return False  # already up-to-date

    try:
        filepath.write_text(new_content, encoding="utf-8")
    except Exception as exc:
        print(f"  [error] Cannot write {rel_path}: {exc}")
        return False

    print(f"  updated : {rel_path}  [{commit_hash} | {commit_date}]")
    return True


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    version = load_version()
    print(f"Project version : v{version}")
    print(f"Scanning dirs   : {SEARCH_DIRS}\n")

    repo_root = Path(__file__).resolve().parent.parent.parent
    os.chdir(repo_root)  # git log paths are relative to repo root

    updated = 0
    scanned = 0

    for dir_rel in SEARCH_DIRS:
        search_root = repo_root / dir_rel
        if not search_root.exists():
            print(f"[warn] Directory not found, skipping: {dir_rel}")
            continue

        for ext in sorted(TARGET_EXTENSIONS):
            for fpath in sorted(search_root.rglob(f"*{ext}")):
                scanned += 1
                if update_file_header(fpath, version, repo_root):
                    updated += 1

    print(f"\nDone. Scanned {scanned} file(s), updated {updated} file(s).")


if __name__ == "__main__":
    main()
