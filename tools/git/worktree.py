#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Chromium Worktree Automation Tool
Automates the creation of git worktrees for Chromium checkouts.
"""

import argparse
import json
import os
import random
import shutil
import subprocess
import sys
from pathlib import Path

# Common VSCode Title Bar Tint colors from using-gitworktree.md
VSCODE_COLOR_PRESETS = {
    "red": {
        "titleBar.activeBackground": "#3a1414",
        "titleBar.activeForeground": "#d3adad",
        "titleBar.inactiveBackground": "#401a1a",
        "titleBar.inactiveForeground": "#ab8585"
    },
    "green": {
        "titleBar.activeBackground": "#143a14",
        "titleBar.activeForeground": "#add3ad",
        "titleBar.inactiveBackground": "#1a401a",
        "titleBar.inactiveForeground": "#85ab85"
    },
    "blue": {
        "titleBar.activeBackground": "#14143a",
        "titleBar.activeForeground": "#adc3d3",
        "titleBar.inactiveBackground": "#1a1a40",
        "titleBar.inactiveForeground": "#8585ab"
    }
}


def run_cmd(cmd, cwd=None, capture=False, check=True):
  """Utility to run shell commands."""
  print(f"Executing: {' '.join(cmd)} in {cwd or os.getcwd()}")
  try:
    result = subprocess.run(cmd,
                            cwd=cwd,
                            check=True,
                            text=True,
                            capture_output=capture)
    return result.stdout.strip() if capture else None
  except subprocess.CalledProcessError as e:
    if check:
      print(f"Error executing command: {e}")
      if e.stderr:
        print(f"Stderr: {e.stderr}")
      sys.exit(1)
    return None


def validate_parent_repo(parent_path):
  """Ensures the parent path is a valid gclient checkout with a src repo."""
  target = Path(parent_path).resolve()

  if not target.is_dir():
    print(f"Error: Parent path {target} is not a directory.")
    sys.exit(1)

  # If the user pointed to 'src', the gclient root is one level up
  if target.name == "src" and (target.parent / ".gclient").exists():
    parent_dir = target.parent
    src_dir = target
  else:
    parent_dir = target
    src_dir = target / "src"

  gclient_config = parent_dir / ".gclient"

  if not gclient_config.exists():
    print(f"Error: .gclient file not found in {parent_dir}")
    sys.exit(1)

  if not (src_dir / ".git").exists():
    print(f"Error: Git repository not found in {src_dir}")
    sys.exit(1)

  return parent_dir, gclient_config, src_dir


def check_cache_dir(gclient_config):
  """Check if cache_dir is defined in .gclient."""
  content = gclient_config.read_text()
  if "cache_dir" not in content:
    print("\nWARNING: 'cache_dir' not found in your .gclient file.")
    print("Without cache_dir, gclient sync will download ~20GB of submodules.")
    print("It is highly recommended to add it to your main .gclient first.\n")
    return False
  return True


def create_worktree(parent_src, variant_path, branch_name, base_branch=None):
  """Runs git worktree add."""
  print(f"Creating git worktree at {variant_path}...")
  cmd = ["git", "worktree", "add", str(variant_path), "-b", branch_name]
  if base_branch:
    cmd.append(base_branch)
  run_cmd(cmd, cwd=parent_src)

  if base_branch:
    print(f"Setting upstream for '{branch_name}' to '{base_branch}'...")
    run_cmd(["git", "branch", "--set-upstream-to", base_branch, branch_name],
            cwd=variant_path)


def setup_vscode_color(variant_src, color_name):
  """Configures VSCode title bar colors in settings.json."""
  color_config = VSCODE_COLOR_PRESETS.get(color_name.lower())
  if not color_config:
    print(
        f"Warning: Color preset '{color_name}' not found. Skipping VSCode coloring."
    )
    return

  vscode_dir = variant_src / ".vscode"
  vscode_dir.mkdir(exist_ok=True)
  settings_file = vscode_dir / "settings.json"

  settings = {}
  if settings_file.exists():
    try:
      settings = json.loads(settings_file.read_text())
    except Exception:
      pass

  settings["workbench.colorCustomizations"] = color_config
  settings_file.write_text(json.dumps(settings, indent=2))
  print(f"Configured VSCode coloring to '{color_name}' in {settings_file}")


def get_next_variant_name(parent_dir):
  """Suggests an incremental name (e.g. bling_1, bling_2)."""
  for i in range(1, 31):
    name = f"{parent_dir.name}_{i}"
    if not (parent_dir.parent / name).exists():
      return name

  # Fallback to a 5-digit random number to avoid collision
  random_id = random.randint(10000, 99999)
  return f"{parent_dir.name}_{random_id}"


def main():
  parser = argparse.ArgumentParser(
      description="Create a Chromium git worktree.")
  parser.add_argument(
      "parent_repo",
      help="Path to the parent repository (containing .gclient and src/)")
  parser.add_argument("--name", help="Name for the new worktree (folder name)")
  parser.add_argument("--branch",
                      help="Specific branch name to create (defaults to name)")
  parser.add_argument("--base",
                      help="Base branch to fork from (defaults to HEAD)")
  parser.add_argument(
      "--internal-base",
      help="Base branch to fork from in internal (defaults to pinned commit)")
  parser.add_argument(
      "--light",
      action="store_true",
      help="Light worktree: skip gclient sync (saves ~20GB, good for agents)")
  parser.add_argument("--color",
                      help="VSCode title bar color (red, green, blue)")

  args = parser.parse_args()

  parent_dir, gclient_config, parent_src = validate_parent_repo(
      args.parent_repo)
  has_cache_dir = check_cache_dir(gclient_config)

  # Determine new path
  if args.name:
    variant_name = args.name
  else:
    variant_name = get_next_variant_name(parent_dir)
    print(f"No name provided, using suggested name: {variant_name}")

  variant_root = parent_dir.parent / variant_name
  variant_src = variant_root / "src"

  if variant_root.exists():
    print(
        f"Error: Target path {variant_root} already exists. Choose a different name."
    )
    sys.exit(1)

  branch_name = args.branch if args.branch else variant_name

  # 1. Create the root directory
  variant_root.mkdir(parents=True)

  # 2. Add worktree
  create_worktree(parent_src, variant_src, branch_name, args.base)

  # 3. Copy .gclient
  print(f"Copying .gclient to {variant_root}")
  shutil.copy2(gclient_config, variant_root / ".gclient")

  # 4. Configure VSCode coloring
  if args.color:
    setup_vscode_color(variant_src, args.color)

  # 5. gclient sync (skip if light)
  if not args.light:
    print(
        "\nStarting gclient sync... this may take a few minutes if cache_dir is enabled."
    )
    run_cmd(["gclient", "sync"], cwd=variant_src)
  else:
    print(
        "\nSkipping gclient sync (--light mode enabled). Tree is approx. 4GB.")

  # 6. Fork internal repo if requested
  if args.internal_base:
    internal_src = variant_src / "internal"
    parent_internal_src = parent_src / "internal"
    if internal_src.exists():
      # Check if the branch exists locally
      has_branch = run_cmd(
          ["git", "show-ref", "--verify", f"refs/heads/{args.internal_base}"],
          cwd=internal_src,
          capture=True,
          check=False) is not None

      if not has_branch:
        print(
            f"Base branch '{args.internal_base}' not found in {internal_src}. Attempting to fetch from parent..."
        )
        run_cmd(["git", "remote", "add", "parent",
                 str(parent_internal_src)],
                cwd=internal_src)
        run_cmd(["git", "fetch", "parent", args.internal_base],
                cwd=internal_src)
        base_ref = "FETCH_HEAD"
        upstream_ref = f"parent/{args.internal_base}"
      else:
        base_ref = args.internal_base
        upstream_ref = args.internal_base

      print(f"Forking branch '{branch_name}' in internal from '{base_ref}'")
      run_cmd(["git", "checkout", "-b", branch_name, base_ref],
              cwd=internal_src)
      run_cmd(["git", "branch", "--set-upstream-to", upstream_ref, branch_name],
              cwd=internal_src)
    else:
      print(
          f"Warning: internal not found at {internal_src}. Skipping internal fork."
      )

  print(f"\nSUCCESS! Your Chromium worktree is ready at:")
  print(f"{variant_src}")

  print(f"\nShell Integration Tips:")
  alias_name = variant_name.replace(f"{parent_dir.name}_", "")
  print(f"Add this to your ~/.zshrc for quick jumping:")
  print(f"  function {alias_name}() {{ cd {variant_src} }}")
  print(f"\nYou can now cd into it and start working.")

  if not has_cache_dir:
    print("\n💡 Pro Tip: Speed up `gclient sync` for worktrees!")
    print(
        "Configure a single universal Git object cache directory to perform \"shared clones\"."
    )
    print("Add `cache_dir` to your `.gclient` file:")
    print("```py")
    print("solutions = [")
    print("  {")
    print("    \"name\": \"src\",")
    print(
        "    \"url\": \"https://chromium.googlesource.com/chromium/src.git\",")
    print("    \"managed\": False,")
    print("    \"custom_deps\": {},")
    print("    \"custom_vars\": {},")
    print("  },")
    print("]")
    print("cache_dir = \"~/dev/git_cache\" # Add this line")
    print("```\n")


if __name__ == "__main__":
  main()
