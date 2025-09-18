#!/usr/bin/env python3
"""
Fix SARIF column indexing for GitHub Code Scanning compatibility.

CodeChecker outputs 0-based column indexing, but GitHub's SARIF validation
requires 1-based indexing (columns >= 1). This script converts all 0-based
column numbers to 1-based.
"""

import json
import sys
import argparse


def fix_sarif_columns(sarif_file):
    """Fix 0-based column numbers to 1-based in SARIF file."""
    with open(sarif_file, 'r') as f:
        data = json.load(f)

    fixed_count = 0

    for run in data.get('runs', []):
        for result in run.get('results', []):
            # Fix main locations
            for location in result.get('locations', []):
                region = location.get('physicalLocation', {}).get('region', {})
                if region.get('startColumn') == 0:
                    region['startColumn'] = 1
                    fixed_count += 1
                if region.get('endColumn') == 0:
                    region['endColumn'] = 1
                    fixed_count += 1

            # Fix codeFlow locations
            for codeFlow in result.get('codeFlows', []):
                for threadFlow in codeFlow.get('threadFlows', []):
                    for location in threadFlow.get('locations', []):
                        region = location.get('location', {}).get('physicalLocation', {}).get('region', {})
                        if region.get('startColumn') == 0:
                            region['startColumn'] = 1
                            fixed_count += 1
                        if region.get('endColumn') == 0:
                            region['endColumn'] = 1
                            fixed_count += 1

    with open(sarif_file, 'w') as f:
        json.dump(data, f)

    print(f"Fixed {fixed_count} column values from 0 to 1 in {sarif_file}")
    return fixed_count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('sarif_file', help='SARIF file to fix')
    args = parser.parse_args()

    try:
        fix_sarif_columns(args.sarif_file)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()