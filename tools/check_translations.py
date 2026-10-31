#!/usr/bin/env python3
"""
Translation Checker for Bus Pirate 5 Firmware

This script analyzes the en-us.h translation file to:
1. Find dead/unused translation entries (not referenced in source code)
2. Find duplicate translation text values

Usage: 
    python3 check_translations.py              # Print to console
    python3 check_translations.py -o report.txt # Save to file
    python3 check_translations.py --help        # Show help
"""

import os
import re
import sys
import argparse
from pathlib import Path
from collections import defaultdict

# Configuration
TRANSLATION_FILE = "src/translation/en-us.h"
SOURCE_DIR = "src"
EXCLUDE_DIRS = ["src/translation"]

def parse_translation_file(file_path):
    """
    Parse the en-us.h file and extract translation entries.
    
    Returns:
        dict: {key: text} mapping of translation entries
    """
    translations = {}
    
    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    
    # Pattern to match translation entries like: [T_KEY]="text",
    # This handles multi-line strings and escaped quotes
    pattern = r'\[([T_][A-Z0-9_]+)\]\s*=\s*"([^"]*(?:\\"[^"]*)*)"'
    
    matches = re.finditer(pattern, content)
    for match in matches:
        key = match.group(1)
        text = match.group(2)
        translations[key] = text
    
    return translations

def find_source_files(source_dir, exclude_dirs):
    """
    Find all C/C++ source files in the source directory, excluding specified directories.
    
    Returns:
        list: List of Path objects for source files
    """
    source_files = []
    exclude_paths = [Path(d) for d in exclude_dirs]
    
    for root, dirs, files in os.walk(source_dir):
        root_path = Path(root)
        
        # Skip excluded directories
        skip = False
        for exclude in exclude_paths:
            try:
                root_path.relative_to(exclude)
                skip = True
                break
            except ValueError:
                pass
        
        if skip:
            continue
        
        for file in files:
            if file.endswith(('.c', '.h', '.cpp', '.hpp')):
                source_files.append(root_path / file)
    
    return source_files

def check_translation_usage(translations, source_files):
    """
    Check which translation keys are actually used in source files.
    
    Returns:
        tuple: (used_keys set, unused_keys set)
    """
    used_keys = set()
    
    for source_file in source_files:
        try:
            with open(source_file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                
            # Search for each translation key
            for key in translations.keys():
                if key in content:
                    used_keys.add(key)
        except Exception as e:
            print(f"Warning: Could not read {source_file}: {e}")
    
    all_keys = set(translations.keys())
    unused_keys = all_keys - used_keys
    
    return used_keys, unused_keys

def find_duplicates(translations):
    """
    Find translation entries with duplicate text values.
    
    Returns:
        dict: {text: [keys]} mapping of duplicate text to list of keys
    """
    text_to_keys = defaultdict(list)
    
    for key, text in translations.items():
        # Normalize text for comparison (ignore case and whitespace differences)
        normalized_text = text.strip()
        if normalized_text:  # Ignore empty strings
            text_to_keys[normalized_text].append(key)
    
    # Filter to only include entries with duplicates
    duplicates = {text: keys for text, keys in text_to_keys.items() if len(keys) > 1}
    
    return duplicates

def main():
    """Main function to run the translation checker."""
    
    # Parse command line arguments
    parser = argparse.ArgumentParser(
        description="Check Bus Pirate 5 translation file for unused entries and duplicates"
    )
    parser.add_argument(
        '-o', '--output',
        help='Save output to file instead of printing to console',
        metavar='FILE'
    )
    parser.add_argument(
        '--unused-only',
        action='store_true',
        help='Only check for unused translations'
    )
    parser.add_argument(
        '--duplicates-only',
        action='store_true',
        help='Only check for duplicate translations'
    )
    parser.add_argument(
        '--remove-unused',
        action='store_true',
        help='Remove unused translation entries from the translation file (in-place)'
    )
    parser.add_argument(
        '--yes',
        action='store_true',
        help='Do not prompt for confirmation when removing unused translations'
    )
    args = parser.parse_args()
    
    # Redirect output if needed
    if args.output:
        output_file = open(args.output, 'w', encoding='utf-8')
        original_stdout = sys.stdout
        sys.stdout = output_file

    def remove_unused_from_file(unused_keys, translation_file):
        """
        Remove unused translation entries from the translation file in-place.
        """
        with open(translation_file, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        # Build regex to match unused translation lines
        # e.g. [T_KEY]="..."
        unused_pattern = re.compile(r'\[(' + '|'.join(re.escape(k) for k in unused_keys) + r')\]\s*=\s*"[^\n]*"\s*,?')

        new_lines = []
        removed = set()
        for line in lines:
            m = unused_pattern.search(line)
            if m:
                removed.add(m.group(1))
                continue  # skip this line
            new_lines.append(line)

        with open(translation_file, 'w', encoding='utf-8') as f:
            f.writelines(new_lines)
        return removed

    try:
        print("=" * 80)
        print("Bus Pirate 5 Translation Checker")
        print("=" * 80)
        print()
        
        # Check if files exist
        if not os.path.exists(TRANSLATION_FILE):
            print(f"ERROR: Translation file not found: {TRANSLATION_FILE}")
            return 1
        
        if not os.path.exists(SOURCE_DIR):
            print(f"ERROR: Source directory not found: {SOURCE_DIR}")
            return 1
        
        # Parse translation file
        print(f"Parsing translation file: {TRANSLATION_FILE}")
        translations = parse_translation_file(TRANSLATION_FILE)
        print(f"Found {len(translations)} translation entries")
        print()
        
        # Find source files
        print(f"Scanning source directory: {SOURCE_DIR}")
        print(f"Excluding: {', '.join(EXCLUDE_DIRS)}")
        source_files = find_source_files(SOURCE_DIR, EXCLUDE_DIRS)
        print(f"Found {len(source_files)} source files to check")
        print()
        
        # Check for unused translations (unless duplicates-only mode)
        if not args.duplicates_only:
            print("-" * 80)
            print("CHECKING FOR UNUSED TRANSLATION ENTRIES")
            print("-" * 80)
            used_keys, unused_keys = check_translation_usage(translations, source_files)

            if unused_keys:
                print(f"\nFound {len(unused_keys)} unused translation entries:\n")
                for key in sorted(unused_keys):
                    text = translations[key]
                    display_text = text if len(text) <= 60 else text[:57] + "..."
                    print(f"  {key:40} = \"{display_text}\"")
                print(f"\nSummary: {len(used_keys)} used, {len(unused_keys)} unused")
                print()
                if args.remove_unused:
                    if not args.yes:
                        confirm = input(f"\nRemove these {len(unused_keys)} unused entries from {TRANSLATION_FILE}? [y/N]: ").strip().lower()
                        if confirm not in ("y", "yes"):
                            print("Aborted removal.")
                            return 0
                    removed = remove_unused_from_file(unused_keys, TRANSLATION_FILE)
                    print(f"Removed {len(removed)} unused translation entries from {TRANSLATION_FILE}.")
            else:
                print("\nNo unused translation entries found!")
                print(f"\nSummary: {len(used_keys)} used, {len(unused_keys)} unused")
                print()
        else:
            # Still need to compute used_keys for duplicate checking
            used_keys, unused_keys = check_translation_usage(translations, source_files)
        
        # Check for duplicates (unless unused-only mode)
        if not args.unused_only:
            print("-" * 80)
            print("CHECKING FOR DUPLICATE TRANSLATION TEXT")
            print("-" * 80)
            duplicates = find_duplicates(translations)
            
            if duplicates:
                print(f"\nFound {len(duplicates)} sets of duplicate text:\n")
                
                for i, (text, keys) in enumerate(sorted(duplicates.items(), key=lambda x: len(x[1]), reverse=True), 1):
                    # Truncate long text for display
                    display_text = text if len(text) <= 60 else text[:57] + "..."
                    print(f"{i}. Text: \"{display_text}\"")
                    print(f"   Used by {len(keys)} keys:")
                    for key in sorted(keys):
                        # Check if this key is used
                        status = "USED" if key in used_keys else "UNUSED"
                        print(f"     - {key:40} [{status}]")
                    print()
            else:
                print("\nNo duplicate translation text found!")
            
            print(f"Summary: {len(duplicates)} duplicate text groups")
            print()
        
        # Final summary (unless in specific mode)
        if not (args.unused_only or args.duplicates_only):
            print("=" * 80)
            print("FINAL SUMMARY")
            print("=" * 80)
            print(f"Total translations:     {len(translations)}")
            print(f"Used translations:      {len(used_keys)}")
            print(f"Unused translations:    {len(unused_keys)}")
            if not args.unused_only:
                duplicates = find_duplicates(translations)
                print(f"Duplicate text groups:  {len(duplicates)}")
            print()
        
        return 0
    
    finally:
        # Restore stdout if redirected
        if args.output:
            sys.stdout = original_stdout
            output_file.close()
            print(f"Results saved to: {args.output}")

if __name__ == "__main__":
    exit(main())
