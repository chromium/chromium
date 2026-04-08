#!/usr/bin/env python3

import argparse
import json
import re
import sys
import posixpath
import os

# Exit codes
SUCCESS = 0
NOT_APPROVED = 1
TECHNICAL_ERROR = 255

def main():
    parser = argparse.ArgumentParser(
        description="Validate PR changes against auto-approver rules."
    )
    parser.add_argument(
        "--config",
        default=".github/auto-approvers.json",
        help="Path to the rules JSON.",
    )
    parser.add_argument(
        "--changed-files", help="Path to the fetched changed files JSON."
    )
    parser.add_argument(
        "--expected-count", type=int, help="Total number of files expected in the PR."
    )
    parser.add_argument(
        "--contributors", nargs="+", help="List of GitHub usernames to validate."
    )
    parser.add_argument(
        "--check-config",
        action="store_true",
        help="Only validate the configuration file and exit.",
    )
    args = parser.parse_args()

    # REGEX: Strict path structure, prevents absolute paths and weird characters
    VALID_PATH = re.compile(r"^([a-zA-Z0-9_.-]+/)*[a-zA-Z0-9_.-]+/?$")

    # Load and validate config
    try:
        with open(args.config) as f:
            rules = json.load(f)
    except FileNotFoundError:
        print(f"::error::❌ Config file not found at {args.config}")
        sys.exit(TECHNICAL_ERROR)
    except json.JSONDecodeError as e:
        print(f"::error::❌ Failed to parse config JSON: {e}")
        sys.exit(TECHNICAL_ERROR)

    safe_rules = {}
    for rule_path, users in rules.items():
        if not isinstance(users, list):
            print(
                f"::error::❌ Users for '{rule_path}' must be a JSON array (list), not a string."
            )
            sys.exit(TECHNICAL_ERROR)

        if not VALID_PATH.match(rule_path) or ".." in rule_path.split("/"):
            print(f"::error::❌ Invalid config path: {rule_path}")
            sys.exit(TECHNICAL_ERROR)

        safe_rules[rule_path] = [str(u).lower() for u in users]

    if not args.check_config:
        # Validate that required arguments are present if not in --check-config mode
        if not (
            args.changed_files and args.expected_count is not None and args.contributors
        ):
            print(
                "::error::❌ Missing required arguments: --changed-files, --expected-count, and --contributors are required unless --check-config is used."
            )
            sys.exit(TECHNICAL_ERROR)

        # Load and flatten changed files
        try:
            with open(args.changed_files) as f:
                file_objects = json.load(f)
        except FileNotFoundError:
            print(f"::error::❌ Changed files JSON not found at {args.changed_files}")
            sys.exit(TECHNICAL_ERROR)
        except json.JSONDecodeError as e:
            print(f"::error::❌ Failed to parse changed files JSON: {e}")
            sys.exit(TECHNICAL_ERROR)

        if not file_objects or len(file_objects) != args.expected_count:
            print(
                f"::error::❌ File truncation mismatch or empty PR. Expected {args.expected_count}, got {len(file_objects) if file_objects else 0}."
            )
            sys.exit(TECHNICAL_ERROR)

        if not all(isinstance(obj, list) for obj in file_objects):
            print("::error::❌ Invalid payload format. Expected a list of lists.")
            sys.exit(TECHNICAL_ERROR)

        changed_files = [path for obj in file_objects for path in obj]

        # Validate every file against every contributor
        contributors = set(str(c).lower() for c in args.contributors)
        print(f"👥 Validating contributors: {', '.join(contributors)}")

        for raw_file_path in changed_files:
            file_path = posixpath.normpath(raw_file_path)

            # Find the most specific (longest) matching rule.
            longest_match_rule = None
            for rule_path in safe_rules.keys():
                if rule_path.endswith('/'):
                    if file_path.startswith(rule_path):
                        if longest_match_rule is None or len(rule_path) > len(longest_match_rule):
                            longest_match_rule = rule_path
                else:
                    if file_path == rule_path:
                        if longest_match_rule is None or len(rule_path) > len(longest_match_rule):
                            longest_match_rule = rule_path

            # First, explicitly fail if the file isn't covered by ANY rule.
            if not longest_match_rule:
                print(
                    f"::error::❌ File '{file_path}' does not fall under any configured auto-approve rule."
                )
                sys.exit(NOT_APPROVED)

            # Then, verify every contributor has access to that specific rule.
            for user in contributors:
                if user not in safe_rules[longest_match_rule]:
                    print(
                        f"::error::❌ Contributor @{user} not authorized for '{file_path}'."
                    )
                    sys.exit(NOT_APPROVED)

    if args.check_config:
        print("✅ Configuration is structurally valid")
    else:
        print("✅ Validation passed")

    sys.exit(SUCCESS)


if __name__ == "__main__":
    main()
