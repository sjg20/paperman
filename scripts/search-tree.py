#!/usr/bin/env python3
"""
Search paperman-server and display results in tree structure
"""

import sys
import json
import requests
from collections import defaultdict

def search_files(pattern, server="https://tunbridge.chapterst.org", path="", repo="", api_key=""):
    """Search for files matching pattern"""
    params = {
        "q": pattern,
        "recursive": "true"
    }

    if path:
        params["path"] = path
    if repo:
        params["repo"] = repo

    headers = {}
    if api_key:
        headers["X-API-Key"] = api_key

    try:
        response = requests.get(f"{server}/search", params=params, headers=headers)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

def build_tree(files):
    """Build a tree structure from file paths"""
    tree = defaultdict(list)

    for file in files:
        path = file["path"]
        name = file["name"]
        size = file["size"]
        modified = file["modified"]

        # Get directory path
        if "/" in path:
            dir_path = path.rsplit("/", 1)[0]
        else:
            dir_path = "."

        tree[dir_path].append({
            "name": name,
            "size": size,
            "modified": modified,
            "path": path
        })

    return tree

def format_size(size):
    """Format file size in human-readable format"""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size < 1024.0:
            return f"{size:6.1f} {unit}"
        size /= 1024.0
    return f"{size:6.1f} TB"

def print_tree(tree, show_size=True, show_date=False):
    """Print files in tree structure"""
    # Sort directories
    sorted_dirs = sorted(tree.keys())

    for dir_path in sorted_dirs:
        # Print directory header
        if dir_path == ".":
            print("\n[Root Directory]")
        else:
            print(f"\n[{dir_path}/]")

        # Sort files within directory
        files = sorted(tree[dir_path], key=lambda x: x["name"])

        for i, file in enumerate(files):
            # Determine tree characters
            is_last = (i == len(files) - 1)
            prefix = "└── " if is_last else "├── "

            # Build file info
            info_parts = [file["name"]]

            if show_size:
                info_parts.append(f"({format_size(file['size'])})")

            if show_date:
                # Extract just the date part
                date = file["modified"].split("T")[0]
                info_parts.append(f"[{date}]")

            print(f"  {prefix}{' '.join(info_parts)}")

def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="Search paperman-server and display results in tree structure"
    )
    parser.add_argument("pattern", help="Search pattern (case-insensitive)")
    parser.add_argument("--server", default="https://tunbridge.chapterst.org",
                       help="Server URL (default: https://tunbridge.chapterst.org)")
    parser.add_argument("--path", default="",
                       help="Search within subdirectory")
    parser.add_argument("--repo", default="",
                       help="Repository name (for multi-repo setups)")
    parser.add_argument("--api-key", default="",
                       help="API key for authentication")
    parser.add_argument("--no-size", action="store_true",
                       help="Don't show file sizes")
    parser.add_argument("--show-date", action="store_true",
                       help="Show modification dates")

    args = parser.parse_args()

    # Perform search
    print(f"Searching for: {args.pattern}")
    if args.path:
        print(f"In path: {args.path}")
    print()

    result = search_files(args.pattern, args.server, args.path, args.repo, args.api_key)

    if not result.get("success"):
        print(f"Error: {result.get('error', 'Unknown error')}", file=sys.stderr)
        sys.exit(1)

    files = result.get("results", [])
    count = result.get("count", 0)

    if count == 0:
        print("No files found.")
        return

    print(f"Found {count} file(s):\n")
    print("=" * 70)

    # Build and print tree
    tree = build_tree(files)
    print_tree(tree, show_size=not args.no_size, show_date=args.show_date)

    print("\n" + "=" * 70)
    print(f"Total: {count} file(s)")

if __name__ == "__main__":
    main()
