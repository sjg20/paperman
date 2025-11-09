#!/usr/bin/env python3
"""
Search paperman-server and display results in tree structure
"""

import sys
import json
import requests
from collections import defaultdict

def search_files(pattern, server="https://your-server.example.com", path="", repo="", api_key="", verify_ssl=True):
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
        response = requests.get(f"{server}/search", params=params, headers=headers, verify=verify_ssl)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

def get_file_as_pdf(file_path, repo="", server="https://tunbridge.chapterst.org", api_key="", verify_ssl=True):
    """Download a file as PDF"""
    params = {
        "path": file_path,
        "type": "pdf"
    }

    if repo:
        params["repo"] = repo

    headers = {}
    if api_key:
        headers["X-API-Key"] = api_key

    try:
        response = requests.get(f"{server}/file", params=params, headers=headers, verify=verify_ssl)
        response.raise_for_status()
        return response.content
    except requests.exceptions.RequestException as e:
        print(f"Error downloading file: {e}", file=sys.stderr)
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
    """Print files in tree structure with numbers"""
    # Sort directories
    sorted_dirs = sorted(tree.keys())

    # Build flat numbered list for easy reference
    numbered_files = []
    file_num = 1

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

            # Build file info with number
            info_parts = [f"[{file_num}]", file["name"]]

            if show_size:
                info_parts.append(f"({format_size(file['size'])})")

            if show_date:
                # Extract just the date part
                date = file["modified"].split("T")[0]
                info_parts.append(f"[{date}]")

            print(f"  {prefix}{' '.join(info_parts)}")

            # Store for retrieval
            numbered_files.append(file)
            file_num += 1

    return numbered_files

def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="Search paperman-server and display results in tree structure"
    )
    parser.add_argument("pattern", help="Search pattern (case-insensitive)")
    parser.add_argument("--server", default="https://your-server.example.com",
                       help="Server URL (default: https://your-server.example.com)")
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
    parser.add_argument("--insecure", action="store_true",
                       help="Disable SSL certificate verification (for self-signed certs)")
    parser.add_argument("--get", type=int, metavar="N",
                       help="Download file number N as PDF")

    args = parser.parse_args()

    # Suppress SSL warnings if --insecure is used
    if args.insecure:
        import urllib3
        urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

    # Perform search
    print(f"Searching for: {args.pattern}")
    if args.path:
        print(f"In path: {args.path}")
    print()

    result = search_files(args.pattern, args.server, args.path, args.repo, args.api_key,
                         verify_ssl=not args.insecure)

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
    numbered_files = print_tree(tree, show_size=not args.no_size, show_date=args.show_date)

    print("\n" + "=" * 70)
    print(f"Total: {count} file(s)")

    # Handle --get option
    if args.get:
        if args.get < 1 or args.get > len(numbered_files):
            print(f"\nError: File number {args.get} out of range (1-{len(numbered_files)})", file=sys.stderr)
            sys.exit(1)

        # Get the selected file (1-indexed)
        selected_file = numbered_files[args.get - 1]
        file_path = selected_file["path"]
        file_name = selected_file["name"]

        # Generate output filename (replace extension with .pdf)
        import os
        base_name = os.path.splitext(file_name)[0]
        output_name = base_name + ".pdf"

        print(f"\nDownloading [{args.get}] {file_name} as PDF...")
        pdf_content = get_file_as_pdf(file_path, args.repo, args.server, args.api_key,
                                      verify_ssl=not args.insecure)

        # Save to file
        with open(output_name, 'wb') as f:
            f.write(pdf_content)

        print(f"Saved to: {output_name} ({len(pdf_content)} bytes)")
    else:
        print("\nUse --get N to download file number N as PDF")

if __name__ == "__main__":
    main()
