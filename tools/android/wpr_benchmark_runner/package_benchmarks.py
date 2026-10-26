# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Validates and packages a user-supplied on-device benchmark definition."""

import argparse
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import zipfile

_DEVTOOLS_SOCKET = "chrome_devtools_remote"
_REPLAY_HOST = "127.0.0.1"
_VPYTHON_SPEC = Path(__file__).resolve().parents[3] / "vpython.toml"
_PLACEHOLDER = re.compile(r"\{([a-zA-Z_][a-zA-Z0-9_]*)\}")
_ENDPOINT_KEYS = {"http_port", "https_port"}


def _source(root, parent, value):
  if not isinstance(value, str) or not value.strip():
    raise ValueError("File paths must be nonempty strings")
  if value.startswith("//"):
    path = root / value[2:]
  else:
    path = parent / value
  path = path.resolve()
  if not path.is_file():
    raise ValueError(f"Not a regular file: {path}")
  with path.open("rb"):
    pass
  return path


def _destination(value):
  path = PurePosixPath(value)
  if not value or path.is_absolute() or ".." in path.parts or "\\" in value:
    raise ValueError(f"Unsafe packaged path: {value!r}")
  if path == PurePosixPath(".") or path.parts[0] in ("config.json", "scratch"):
    raise ValueError(f"Reserved packaged path: {value!r}")
  return str(path)


def _flag(command, name):
  prefix = name + "="
  for index, argument in enumerate(command):
    if argument.startswith(prefix):
      return argument[len(prefix) :]
    if argument == name:
      if index + 1 == len(command):
        raise ValueError(f"Missing value for {name}")
      return command[index + 1]
  return None


def _alias(value, files):
  match = _PLACEHOLDER.fullmatch(value)
  if not match or match.group(1) not in files:
    raise ValueError(f"Expected a declared file placeholder, got {value!r}")
  return match.group(1)


def _injections(command, files):
  bindings = {}
  rules_value = _flag(command, "--rules_file")
  if rules_value is not None:
    rules_alias = _alias(rules_value, files)
    if files[rules_alias]["executable"]:
      raise ValueError(f"WPR rules are an executable: {rules_alias}")
    rules_source = files[rules_alias]["source"]
    rules = json.loads(rules_source.read_text(encoding="utf-8"))
    if not isinstance(rules, list):
      raise ValueError(f"WPR rules must be an array: {rules_source}")
    scripts = {}
    for rule in rules:
      reference = rule.get("InjectedScript")
      if reference is None:
        continue
      if not isinstance(reference, str) or not reference.strip():
        raise ValueError(f"Empty InjectedScript in {rules_source}")
      if Path(reference).is_absolute():
        raise ValueError(f"InjectedScript must be relative: {reference}")
      script = (rules_source.parent / reference).resolve()
      matches = []
      for file_alias, entry in files.items():
        if entry["source"] == script and not entry["executable"]:
          matches.append(file_alias)
      if not matches:
        raise ValueError(f"Undeclared injection dependency: {script}")
      scripts[reference] = matches[0]
    bindings[rules_alias] = scripts
  scripts_value = _flag(command, "--inject_scripts")
  if scripts_value:
    for script in scripts_value.split(","):
      alias = _alias(script, files)
      if files[alias]["executable"]:
        raise ValueError(f"Injection script is an executable: {alias}")
  return bindings


def _load_pages(source):
  command = [
    "vpython3",
    "-vpython-spec",
    str(_VPYTHON_SPEC),
    "-m",
    "hjson.tool",
    "-c",
    str(source),
  ]
  definitions = json.loads(subprocess.check_output(command, text=True))["pages"]
  if not isinstance(definitions, dict) or not definitions:
    raise ValueError("pages must be a nonempty object")
  pages = []
  for name, page in definitions.items():
    block_definitions = page["blocks"]
    if not isinstance(block_definitions, dict) or not block_definitions:
      raise ValueError(f"blocks must be a nonempty object: {name}")
    blocks = []
    for label, actions in block_definitions.items():
      if not isinstance(actions, list) or not actions:
        raise ValueError(
          f"Block actions must be a nonempty array: {name}.{label}"
        )
      blocks.append({"name": label, "actions": actions})
    pages.append({"name": name, "blocks": blocks})
  return pages


def load_benchmark(root, config_path):
  source = _source(root, root, config_path)
  native_libraries = {}
  definition = json.loads(source.read_text(encoding="utf-8"))
  actions_file = _source(root, source.parent, definition.pop("actions_file"))
  definition["pages"] = _load_pages(actions_file)
  definition.setdefault("output_processing", [])
  trace_processor = definition.get("trace_processor", {"enabled": False})
  if not isinstance(trace_processor, dict) or not isinstance(
    trace_processor.get("enabled"), bool
  ):
    raise ValueError("trace_processor.enabled must be boolean")
  browser = definition["browser"]
  if not isinstance(browser, dict):
    raise ValueError("browser must be an object")
  package_name = browser.get("package")
  if not isinstance(package_name, str) or not package_name.strip():
    raise ValueError("browser.package must be a nonempty string")
  for key in ("http_port", "https_port"):
    port = browser.get(key)
    if type(port) is not int or not 1 <= port <= 65535:
      raise ValueError(f"browser.{key} must be an integer between 1 and 65535")
  if browser["http_port"] == browser["https_port"]:
    raise ValueError("Replay HTTP and HTTPS ports must differ")
  flags = browser.setdefault("flags", [])
  if not isinstance(flags, list):
    raise ValueError("browser.flags must be an array of switches")
  for flag in flags:
    if (
      not isinstance(flag, str)
      or not flag.startswith("--")
      or flag == "--"
      or any(character in flag for character in "\0\r\n")
    ):
      raise ValueError("browser.flags must contain nonempty switches")
    if flag.split("=", 1)[0] == "--host-resolver-rules":
      raise ValueError("The runner sets browser replay routing")
  browser["replay_host"] = _REPLAY_HOST
  browser["devtools_socket"] = _DEVTOOLS_SOCKET
  files = {}
  destinations = {}
  for entry in definition["files"]:
    alias = entry["name"]
    if not re.fullmatch(r"[a-zA-Z_][a-zA-Z0-9_]*", alias):
      raise ValueError(f"Invalid file alias: {alias}")
    if alias in files:
      raise ValueError(f"Duplicate file alias: {alias}")
    path = _source(root, source.parent, entry["path"])
    executable = entry.get("executable", False)
    if not isinstance(executable, bool):
      raise ValueError(f"executable must be boolean: {alias}")
    binding = {"source": path, "executable": executable}
    if executable:
      library = f"libwpr_{alias}.so"
      binding["native_library"] = library
      native_libraries[library] = str(path)
    else:
      destination = _destination(entry.get("destination", path.name))
      if destination in destinations and destinations[destination] != path:
        raise ValueError(f"Colliding packaged path: {destination}")
      destinations[destination] = path
      binding["relative_path"] = destination
    files[alias] = binding
  command = definition["wpr"]["command"]
  if not isinstance(command, list) or not command:
    raise ValueError(f"wpr.command must be a nonempty argv array: {source}")
  for argument in command:
    if not isinstance(argument, str) or not argument.strip():
      raise ValueError(f"Empty WPR argument in {source}")
    for placeholder in _PLACEHOLDER.findall(argument):
      if placeholder not in files and placeholder not in _ENDPOINT_KEYS:
        raise ValueError(f"Undeclared WPR placeholder: {placeholder}")
  for flag in ("--host", "-host"):
    if _flag(command, flag) is not None:
      raise ValueError(
        "The runner sets the WPR host; omit --host from wpr.command"
      )
  executable_alias = _alias(command[0], files)
  if not files[executable_alias]["executable"]:
    raise ValueError("The first WPR argument must name an executable")
  actions = list(definition["output_processing"])
  for page in definition["pages"]:
    for block in page["blocks"]:
      actions.extend(block["actions"])
  for action in actions:
    if (
      action.get("action") in ("trace_processor", "process_llp2_trace")
      and not trace_processor["enabled"]
    ):
      raise ValueError(
        f'{action["action"]} requires trace_processor.enabled=true'
      )
    if "query_file" not in action:
      continue
    alias = action["query_file"]
    if alias not in files:
      raise ValueError(f"Undeclared query_file: {alias}")
    if files[alias]["executable"]:
      raise ValueError(f"Query file is an executable: {alias}")
  command.insert(2, f"--host={_REPLAY_HOST}")
  definition["_injection_bindings"] = _injections(command, files)
  device_files = {}
  for alias, entry in files.items():
    device_file = entry.copy()
    del device_file["source"]
    device_files[alias] = device_file
  definition["files"] = device_files
  benchmark = {"definition": definition, "files": files}
  return benchmark, native_libraries


def package(root, config_path, output):
  benchmark, libraries = load_benchmark(root, config_path)
  output.mkdir(parents=True, exist_ok=True)
  definition = benchmark["definition"]
  with zipfile.ZipFile(output / "wpr_benchmark.zip", "w") as archive:
    archive.writestr(
      zipfile.ZipInfo("config.json"), json.dumps(definition, indent=2) + "\n"
    )
    copied_files = set()
    for entry in benchmark["files"].values():
      if entry["executable"]:
        continue
      destination = entry["relative_path"]
      if destination in copied_files:
        continue
      copied_files.add(destination)
      info = zipfile.ZipInfo(destination)
      with (
        entry["source"].open("rb") as source,
        archive.open(info, "w") as target,
      ):
        shutil.copyfileobj(source, target)
  for library, source in libraries.items():
    shutil.copyfile(source, output / library)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--root", type=Path, required=True)
  parser.add_argument("--config", default="")
  parser.add_argument("--output", type=Path, required=True)
  args = parser.parse_args()
  try:
    package(args.root, args.config, args.output)
  except (
    OSError,
    ValueError,
    KeyError,
    TypeError,
    subprocess.CalledProcessError,
  ) as error:
    parser.error(str(error))


if __name__ == "__main__":
  main()
