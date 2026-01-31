#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
A tool to identify and remove unnecessary UNSAFE_TODO() wrappers in Chromium.

This script automates the verification process by locating all instances of
UNSAFE_TODO(), determining if the wrapper is redundant, and confirming that the
code is compiled in the current build configuration.

Because this process requires approximately 10k-40k compiler invocations, we
can't afford the latency of siso/remoteexec/autoninja for each. Thus, this tool
compiles locally a single file from the compile_commands.json database generated
by GN. This works because UNSAFE_TODO() is a compile-time check. The only
caveat are UNSAFE_TODO in templates instantiations, which are not verified by
this method.

The script supports state persistence via a JSON file, enabling the analysis to
resume without losing progress. It also supports multiple builders, allowing
verification of the same UNSAFE_TODO() across different configurations.

Usage:
  ./unsafe_todo_remover.py            # Run analysis
  ./unsafe_todo_remover.py --apply    # Apply fixes from out.json
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
from collections import defaultdict
from collections.abc import Sequence

# --- Configuration ---

# State is persisted in this file. This allows resuming the analysis
# without losing progress.
STATE_FILE = "out.json"

# A set of builders covering most of the chromium C++ code.
#
# See infra/config/generated/builders/gn_args_locations.json for reference.
BUILDERS = [
    # Linux builders
    ('tryserver.chromium.linux', 'linux-rel'),
    ('tryserver.chromium.linux', 'linux_chromium_tsan_rel_ng'),
    ('chromium.linux', 'Linux Builder (dbg)'),
    ('chromium.memory', 'Linux ASan LSan Builder'),

    # Chromeos builders
    ('chromium.chromiumos', 'linux-cfm-rel'),
    ('chromium.chromiumos', 'linux-chromeos-rel'),
    ('tryserver.chromium.chromiumos', 'chromeos-libfuzzer-asan-rel'),

    # Android builders
    ('chromium.android', 'android-x86-rel'),
    ('chromium', 'android-arm64-archive-rel'),
    ('chromium.android', 'android-14-x64-rel'),
    ('chromium.android', 'android-x86-rel'),
    ('chromium.fuzz', 'android-arm64-libfuzzer-hwasan'),
    ('tryserver.chromium.android', 'android-cronet-arm-dbg'),
    ('tryserver.chromium.android', 'android-webview-13-x64-dbg'),

    # Fuchsia
    ('chromium.fuchsia', 'Deterministic Fuchsia (dbg)'),

    # Mac
    ('chromium.mac', 'mac-arm64-dbg'),
    ('chromium.mac', 'mac-arm64-rel'),
    ('tryserver.chromium.mac', 'mac-rel'),
    ('tryserver.chromium.mac', 'mac-rel'),

    # Dawn
    ('tryserver.chromium.dawn', 'dawn-win11-arm64-deps-rel'),

    # Windows builders
    ('chromium.win', 'linux-win-cross-rel'),
    ('chromium.memory', 'win-asan'),
    ('chromium.win', 'Win Builder'),
    ('chromium.win', 'Win x64 Builder (dbg)'),
    ('chromium.win', 'linux-win-cross-rel'),
    ('chromium.win', 'win-arm64-rel'),
    ('tryserver.chromium.win', 'win-arm64-compile-dbg'),
    ('tryserver.chromium.win', 'win-libfuzzer-asan-rel'),
    ('tryserver.chromium.win', 'win-rel'),
    ('tryserver.chromium.win', 'win_optional_gpu_tests_rel'),

    # Fuchsia cast
    ('chromium.fuchsia', 'fuchsia-x64-cast-receiver-rel'),

    # Android cast
    ('chromium.android', "android-cast-arm-dbg"),
]

EXCLUDED_DIRECTORIES = [
    # Non production code:
    "agents/",
    "tools/",
    "build/",

    # UNSAFE_TODO in template.
    #
    # C++ templates can explode into many instantiations, making it impractical
    # to verify all of them. This script only build the file that contains the
    # UNSAFE_TODO, so template instantiations in other files are not verified.
    # Thus, we exclude files known to have UNSAFE_TODO in templates.
    "base/strings/string_tokenizer.h",
    "chrome/browser/media/router/discovery/discovery_network_list_posix.cc",
    "components/zucchini/io_utils.h",
    "components/zucchini/patch_utils.h",
    "components/zucchini/rel32_utils.h",
    "device/fido/cbor_extract.h",
    "gpu/command_buffer/common/cmd_buffer_common.h",
    "gpu/command_buffer/common/gles2_cmd_utils.h",
    "mojo/core/broker_messages.h",
    "mojo/public/cpp/bindings/lib/array_internal.h",
    "mojo/public/cpp/bindings/lib/array_serialization.h",
    "third_party/blink/renderer/core/css/parser/css_tokenizer_input_stream.h",
    "third_party/blink/renderer/platform/fonts/opentype/open_type_types.h",
    "ui/gfx/x/property_cache.h",

    # UNSAFE_TODO wrapping specific macros.
    #
    # Those macros expands sometimes into code that triggers
    # -WUnsafe-buffer-usage or not, depending on the platform. These variations
    # are not supported by the script, so we exclude them.
    "chrome/browser/media/router/discovery/discovery_network_list_posix.cc",
    "net/base/network_interfaces_posix.cc",
    "net/dns/address_info.cc",
    "net/dns/loopback_only.cc",
    "media/gpu/test/video_test_helpers.cc",
    "net/cert/ct_objects_extractor.cc",
    "sandbox/linux/syscall_broker/broker_simple_message.cc",
]

# List of functions that are considered unsafe and should not be unwrapped.
# There is not need to check the content of UNSAFE_TODO for these functions,
# as they are always unsafe. This is used to mark them as LIBC in the state
# file, which means they cannot be removed.
# This list is based on the Clang source code.
UNSAFE_LIBC_FUNCTIONS = {
    "atof",
    "atoi",
    "atol",
    "atoll",
    "bcopy",
    "bsearch",
    "bzero",
    "fgets",
    "fgetws",
    "fputs",
    "fputws",
    "fread",
    "fwrite",
    "gets",
    "memccpy",
    "memchr",
    "memcmp",
    "memcpy",
    "memmove",
    "mempcpy",
    "memset",
    "puts",
    "qsort",
    "strcasecmp",
    "strcat",
    "strchr",
    "strcmp",
    "strcoll",
    "strcpy",
    "strcspn",
    "strdup",
    "strerror_r",
    "strerror_s",
    "stricmp",
    "strlcat",
    "strlcpy",
    "strlen",
    "strncat",
    "strncmp",
    "strncpy",
    "strndup",
    "strnlen",
    "strpbrk",
    "strrchr",
    "strspn",
    "strstr",
    "strtod",
    "strtof",
    "strtoimax",
    "strtok",
    "strtol",
    "strtold",
    "strtoll",
    "strtoul",
    "strtoull",
    "strtoumax",
    "strxfrm",
    "wmemchr",
    "wmemcmp",
    "wmemcpy",
    "wmemmove",
    "wmemset",
}

# Regex to match: optional scope (:: or std::), one of the functions, followed
# by word boundary. Used to check the content inside UNSAFE_TODO(...).
LIBC_REGEX = re.compile(r'^\s*(?:(?:std)?::)?(?:' +
                        '|'.join(re.escape(f)
                                 for f in UNSAFE_LIBC_FUNCTIONS) + r')\b')

# --- Helpers ---


class Colors:
  CYAN = '\033[96m'
  GREEN = '\033[92m'
  YELLOW = '\033[93m'
  RED = '\033[91m'
  MAGENTA = '\033[95m'
  BLUE = '\033[94m'
  RESET = '\033[0m'


def log(category: str, message: str):
  """Prints a categorized, colored message to stderr."""
  cat_upper = category.upper()
  color = Colors.RESET

  if cat_upper in ("ERR", "FAIL", "ERROR"):
    color = Colors.RED
  elif cat_upper in ("WARN", "WARNING"):
    color = Colors.YELLOW
  elif cat_upper in ("OK", "SUCCESS", "PASS"):
    color = Colors.GREEN
  elif cat_upper in ("INFO", "SCAN", "EDIT", "FMT") or cat_upper.isdigit():
    color = Colors.CYAN
  elif cat_upper in ("BUILD", "GN", "BUILDER"):
    color = Colors.MAGENTA

  print(f"{color}[{cat_upper}]{Colors.RESET} {message}", file=sys.stderr)


def run_command(cmd: Sequence[str],
                cwd: str | None = None,
                capture_output: bool = True) -> subprocess.CompletedProcess:
  """Executes a subprocess command."""
  try:
    return subprocess.run(cmd,
                          cwd=cwd,
                          text=True,
                          capture_output=capture_output,
                          check=False)
  except Exception as e:
    log("ERR", f"Command execution failed: {cmd} -> {e}")
    return subprocess.CompletedProcess(cmd, 1, "", str(e))


def is_excluded(path: str) -> bool:
  """Checks if a path should be ignored based on project rules."""

  # 1. Only modify C++ files
  valid_extensions = ('.cc', '.h', '.cpp', '.mm', '.hh')
  if not path.endswith(valid_extensions):
    return True

  # 2. Specific directory exclusions
  if path.startswith(tuple(EXCLUDED_DIRECTORIES)):
    return True

  # 3. Third party logic: Exclude all third_party EXCEPT third_party/blink
  if path.startswith("third_party/"):
    if not path.startswith("third_party/blink/"):
      return True

  return False


# --- Core Logic ---


class StateManager:
  """Manages the persistence of analysis results."""

  def __init__(self, filepath: str):
    self.filepath = filepath
    # Structure: { file_key: { builder_name: status } }
    self.data: dict[str, dict[str, str]] = {}
    self.load()

  def load(self):
    if os.path.exists(self.filepath):
      try:
        with open(self.filepath, 'r') as f:
          self.data = json.load(f)
      except json.JSONDecodeError:
        log("WARN", f"Corrupt {self.filepath}, starting fresh.")

  def save(self):
    with open(self.filepath, 'w') as f:
      json.dump(self.data, f, indent=2, sort_keys=True)

  def get_builder_status(self, key: str, builder: str) -> str | None:
    """Returns the status of a specific builder for the given key."""
    entry = self.data.get(key, {})
    # Handle legacy format where value was a string
    if isinstance(entry, str):
      return None
    return entry.get(builder)

  def set_status(self, key: str, builder: str, status: str):
    """Sets the status for a specific builder."""
    if key not in self.data or not isinstance(self.data[key], dict):
      self.data[key] = {}

    self.data[key][builder] = status
    self.save()

  def is_resolved(self, key: str) -> bool:
    """
    Returns True if the UNSAFE_TODO has a definitive result
    (SUCCESS or FAIL) from *any* builder, or is marked as LIBC.
    """
    if key not in self.data:
      return False

    entry = self.data[key]
    if isinstance(entry, str):
      # Legacy format support
      return entry in ["SUCCESS", "FAIL", "LIBC"]

    vals = entry.values()
    return "SUCCESS" in vals or "FAIL" in vals or "LIBC" in vals


class SourceLocation:
  """Represents a specific UNSAFE_TODO instance."""

  def __init__(self, filepath: str, line: int, col: int, content: str):
    self.filepath = filepath
    self.line = line
    self.col = col
    self.original_content = content

    # Unique key for state tracking: "path/to/file.cc:10:5"
    self.key = f"{filepath}:{line}:{col}"

  def __repr__(self):
    return f"{self.filepath}:{self.line}"


class BuildEnvironment:
  """Manages the GN/Ninja build environment."""

  def __init__(self, builder_group: str, builder_config: str):
    self.group = builder_group
    self.config = builder_config
    # Sanitize directory name to avoid spaces/parens.
    # Allow alphanumeric, underscore, dash. Replace others with underscore.
    safe_dirname = re.sub(r'[^a-zA-Z0-9_-]', '_', builder_config)
    self.build_dir = os.path.join("out", safe_dirname)
    self.is_ready = False
    self.compile_db: dict[str, str] = {}

  def ensure_ready(self):
    """Sets up the build directory if it doesn't exist."""
    if self.is_ready:
      return

    if not os.path.exists(self.build_dir):
      log("BUILD", f"Setting up builder: {self.config}...")
      cmd = [
          "./tools/mb/mb.py", "gen", "-m", self.group, "-b", self.config,
          self.build_dir
      ]
      res = run_command(cmd)
      if res.returncode != 0:
        log("ERR",
            f"Failed to generate build files for {self.config}: {res.stderr}")
        raise RuntimeError("MB gen failed")

    # Always regenerate compile_commands.json to ensure it's fresh
    log("GN", f"Generating compile_commands.json for {self.config}...")
    res = run_command(
        ["gn", "gen", self.build_dir, "--export-compile-commands"])
    if res.returncode != 0:
      log("ERR", f"Failed to export compile commands: {res.stderr}")
      raise RuntimeError("GN gen failed")

    # Build everything to ensure generated files (headers, etc.) are present.
    log("BUILD",
        f"Building all targets in {self.config} (this may take a while)...")
    run_command(["autoninja", "-C", self.build_dir], capture_output=False)

    self._load_compile_db()
    self.is_ready = True

  def _load_compile_db(self):
    """Loads the compile_commands.json database."""

    db_path = os.path.join(self.build_dir, "compile_commands.json")
    try:
      with open(db_path, 'r') as f:
        data = json.load(f)

      self.compile_db = {}
      for entry in data:
        directory = entry.get('directory', '.')
        filepath = entry.get('file', '')

        if not filepath:
          continue

        abs_path = os.path.abspath(os.path.join(directory, filepath))
        self.compile_db[abs_path] = entry.get('command', '')

    except (IOError, json.JSONDecodeError) as e:
      log("WARN", f"Failed to load compile_commands.json: {e}")

  def has_file(self, filepath: str) -> bool:
    """Checks if the file exists in the compile database."""
    abs_path = os.path.abspath(filepath)
    return abs_path in self.compile_db

  def find_compilable_source(self, filepath: str) -> str | None:
    """Returns a file that can be compiled for the given filepath."""
    abs_path = os.path.abspath(filepath)
    if abs_path in self.compile_db:
      return filepath

    # Header fallback (Source/Header proxy)
    if filepath.endswith(('.h', '.hh', '.hpp')):
      base = os.path.splitext(filepath)[0]
      for ext in ['.cc', '.cpp', '.mm']:
        surrogate = base + ext
        abs_surrogate = os.path.abspath(surrogate)
        if abs_surrogate in self.compile_db:
          return surrogate

    return None

  def compile_file(self, filepath: str) -> bool:
    """
    Compiles a single file using the `compile_commands.json` database.
    """
    compilable_source = self.find_compilable_source(filepath)
    if not compilable_source:
      return False

    cmd_list = shlex.split(self.compile_db[os.path.abspath(compilable_source)])
    res = run_command(cmd_list, cwd=self.build_dir, capture_output=False)
    return res.returncode == 0


class CodeModifier:
  """Handles parsing and modifying source files."""

  @staticmethod
  def find_unsafe_todos() -> Sequence[SourceLocation]:
    """Scans the codebase for UNSAFE_TODO macros."""
    log("SCAN", "Scanning for UNSAFE_TODO instances...")
    cmd = [
        "git", "grep", "-n", "--column", "-w", "UNSAFE_TODO", "--", "*.cc",
        "*.h", "*.cpp", "*.mm", "*.hh"
    ]
    res = run_command(cmd)
    lines = res.stdout.strip().splitlines()

    locations = []
    for line in lines:
      parts = line.split(":", 3)  # path:line:col:content
      if len(parts) < 4:
        continue

      fpath = parts[0]
      linenum = int(parts[1])
      colnum = int(parts[2])

      if is_excluded(fpath):
        continue

      locations.append(SourceLocation(fpath, linenum, colnum, ""))
    return locations

  @staticmethod
  def _read_file(filepath: str) -> Sequence[str]:
    with open(filepath, 'r') as f:
      return f.readlines()

  @staticmethod
  def _write_file(filepath: str, lines: Sequence[str]):
    with open(filepath, 'w') as f:
      f.writelines(lines)

  @staticmethod
  def replace_macro(loc: SourceLocation):
    """
    Simple string replacement on the specific line.
    We replace UNSAFE_TODO with UNSAFE_XXXX.
    """
    lines = CodeModifier._read_file(loc.filepath)
    line_idx = loc.line - 1
    col_idx = loc.col - 1
    original_line = lines[line_idx]

    lines[line_idx] = (original_line[:col_idx] + "UNSAFE_XXXX" +
                       original_line[col_idx + len("UNSAFE_TODO"):])
    CodeModifier._write_file(loc.filepath, lines)

  @staticmethod
  def _remove_wrapper_from_content(content: str, line: int, col: int) -> str:
    """
    Internal logic to remove one wrapper from string content.

    Note: Relying on parenthesis matching is "good" enough here, most of the
    time. Failures will simply skip the removal.
    """
    # Find the line start offset
    lines = content.splitlines(keepends=True)
    offset = 0
    for i in range(line - 1):
      offset += len(lines[i])
    offset += (col - 1)

    # Verify we are at UNSAFE_TODO
    if not content[offset:].startswith("UNSAFE_TODO"):
      log("WARN", f"Offset mismatch at {line}:{col}. Skipping...")
      return content

    # Find opening paren
    open_paren = content.find("(", offset)
    if open_paren == -1:
      return content

    # Find matching closing paren
    balance = 1
    close_paren = -1
    for i in range(open_paren + 1, len(content)):
      if content[i] == "(":
        balance += 1
      elif content[i] == ")":
        balance -= 1

      if balance == 0:
        close_paren = i
        break

    if close_paren == -1:
      return content

    # Extract inner content
    inner = content[open_paren + 1:close_paren]

    # Reconstruct
    new_content = content[:offset] + inner + content[close_paren + 1:]
    return new_content

  @staticmethod
  def remove_wrapper(loc: SourceLocation):
    """Removes UNSAFE_TODO(...) wrapper, keeping inner content."""
    try:
      with open(loc.filepath, 'r') as f:
        content = f.read()

      new_content = CodeModifier._remove_wrapper_from_content(
          content, loc.line, loc.col)

      if content != new_content:
        with open(loc.filepath, 'w') as f:
          f.write(new_content)
    except Exception as e:
      log("ERR", f"Failed to modify {loc}: {e}")

  @staticmethod
  def is_libc_call(loc: SourceLocation) -> bool:
    """Checks if the UNSAFE_TODO wraps a known unsafe libc function."""
    try:
      with open(loc.filepath, 'r') as f:
        content = f.read()

      # Find start offset based on line/col
      lines = content.splitlines(keepends=True)
      offset = 0
      for i in range(loc.line - 1):
        offset += len(lines[i])
      offset += (loc.col - 1)

      if not content[offset:].startswith("UNSAFE_TODO"):
        return False

      open_paren = content.find("(", offset)
      if open_paren == -1: return False

      balance = 1
      close_paren = -1
      for i in range(open_paren + 1, len(content)):
        if content[i] == "(": balance += 1
        elif content[i] == ")": balance -= 1
        if balance == 0:
          close_paren = i
          break

      if close_paren == -1: return False

      inner = content[open_paren + 1:close_paren].strip()
      return bool(LIBC_REGEX.match(inner))
    except Exception:
      return False

  @staticmethod
  def remove_wrappers(locs: Sequence[SourceLocation]):
    """Removes multiple wrappers from the same file efficiently."""
    if not locs:
      return

    filepath = locs[0].filepath
    try:
      with open(filepath, 'r') as f:
        content = f.read()

      # Sort in reverse order (bottom-up, right-to-left) to preserve offsets
      # of earlier modifications.
      sorted_locs = sorted(locs, key=lambda x: (x.line, x.col), reverse=True)

      original_content = content
      for loc in sorted_locs:
        content = CodeModifier._remove_wrapper_from_content(
            content, loc.line, loc.col)

      if content != original_content:
        with open(filepath, 'w') as f:
          f.write(content)
    except Exception as e:
      log("ERR", f"Failed to batch modify {filepath}: {e}")

  @staticmethod
  def revert_file(filepath: str):
    """Restores file using git checkout."""
    run_command(["git", "checkout", filepath])


# --- Main Analysis Loop ---


def analyze_todos(args):
  state = StateManager(STATE_FILE)
  todos = CodeModifier.find_unsafe_todos()

  log("INFO", f"Found {len(todos)} UNSAFE_TODO candidates.")

  # Pre-scan for LIBC functions
  log("INFO", "Checking for known LIBC functions...")
  libc_count = 0
  for todo in todos:
    if state.is_resolved(todo.key):
      continue

    if CodeModifier.is_libc_call(todo):
      state.set_status(todo.key, "scanner", "LIBC")
      libc_count += 1

  if libc_count > 0:
    log("INFO", f"Marked {libc_count} UNSAFE_TODOs as LIBC (non-removable).")

  # Group TODOs by file
  todos_by_file = defaultdict(list)
  for todo in todos:
    if not state.is_resolved(todo.key):
      todos_by_file[todo.filepath].append(todo)

  log("INFO", f"{len(todos_by_file)} files remain unresolved.")

  # Initialize builders
  builders = [BuildEnvironment(g, c) for g, c in BUILDERS]

  for builder in builders:
    try:
      builder.ensure_ready()
    except Exception as e:
      log("ERR", f"Skipping builder {builder.config} due to setup failure: {e}")
      continue

    log("BUILDER", f"Processing files on builder: {builder.config}...")

    processed_files_count = 0

    filenames = list(todos_by_file.keys())

    for filepath in filenames:
      file_todos = todos_by_file[filepath]

      # 1. Check if the file is already resolved
      unresolved_todos = [t for t in file_todos if not state.is_resolved(t.key)]
      if not unresolved_todos:
        continue

      # 2. Check if this builder has already run for these TODOs
      todos_to_check = [
          t for t in unresolved_todos
          if not state.get_builder_status(t.key, builder.config)
      ]
      if not todos_to_check:
        continue

      processed_files_count += 1
      log(str(processed_files_count),
          f"Processing {filepath} ({len(todos_to_check)} items)...")

      try:
        # --- STEP 0: Skip files that can't be compiled ---
        if not builder.find_compilable_source(filepath):
          log("WARN",
              f"Cannot compile {filepath} on {builder.config}. Skipping.")
          for todo in todos_to_check:
            state.set_status(todo.key, builder.config, "SKIP")
          continue

        # --- STEP 1: Individual Checks ---
        for todo in todos_to_check:
          # Inverted Check Logic:
          # 1. First, check if removing the wrapper compile.
          # 2. Second, check if the line is active under this builder.

          # 1. Remove wrapper
          CodeModifier.remove_wrapper(todo)
          safety_pass = builder.compile_file(filepath)
          CodeModifier.revert_file(filepath)

          if not safety_pass:
            # Removing the wrapper broke the build. It is required.
            log("FAIL", f"Line {todo.line}: Unsafe (wrapper required).")
            state.set_status(todo.key, builder.config, "FAIL")
            continue

          # 2. Rename UNSAFE_TODO -> UNSAFE_XXXX to verify usage.
          CodeModifier.replace_macro(todo)
          usage_pass = builder.compile_file(filepath)
          CodeModifier.revert_file(filepath)

          if usage_pass:
            # UNSAFE_XXXX compiled successfully -> Macro content is dead code on
            # this builder (compiler didn't see error). We record SKIP so other
            # builders can try to verify it.
            state.set_status(todo.key, builder.config, "SKIP")
          else:
            # UNSAFE_XXXX failed to compile -> Code is live. Since safety_pass
            # was True (removal worked), this is a confirmed Success.
            log("OK", f"Line {todo.line}: Safe.")
            state.set_status(todo.key, builder.config, "SUCCESS")

      except KeyboardInterrupt:
        log("WARN", f"Interrupted. Reverting {filepath}...")
        CodeModifier.revert_file(filepath)
        sys.exit(1)
      except Exception as e:
        log("ERR", f"Unexpected error processing {filepath}: {e}")
        CodeModifier.revert_file(filepath)


def apply_fixes(args):
  """Reads out.json and applies changes for SUCCESS entries."""
  state = StateManager(STATE_FILE)
  if not state.data:
    log("WARN", "No state file found or file empty. Run analysis first.")
    return

  # Group by file to minimize IO and handle multiple edits per file
  files_to_edit = {}

  # Iterate over all keys in state
  for key, builder_status_map in state.data.items():
    # Check for legacy format
    if not isinstance(builder_status_map, dict):
      status = builder_status_map
      is_success = (status == "SUCCESS")
    else:
      # Check if ANY builder succeeded
      is_success = "SUCCESS" in builder_status_map.values()

    if not is_success:
      continue

    parts = key.rsplit(":", 2)
    fpath = parts[0]
    line = int(parts[1])
    col = int(parts[2])

    if fpath not in files_to_edit:
      files_to_edit[fpath] = []
    files_to_edit[fpath].append(SourceLocation(fpath, line, col, ""))

  log("INFO", f"Applying fixes to {len(files_to_edit)} files...")

  for fpath, locs in files_to_edit.items():
    log("EDIT", f"Updating {fpath} ({len(locs)} fixes)...")
    CodeModifier.remove_wrappers(locs)

  # Run git cl format on modified files
  if files_to_edit:
    log("FMT", "Running git cl format...")
    run_command(["git", "cl", "format"] + list(files_to_edit.keys()))


def main():
  parser = argparse.ArgumentParser(description="Cleanup UNSAFE_TODO macros")
  parser.add_argument("--apply",
                      action="store_true",
                      help="Apply fixes based on out.json")
  args = parser.parse_args()

  if args.apply:
    apply_fixes(args)
  else:
    analyze_todos(args)


if __name__ == "__main__":
  main()
