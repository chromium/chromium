#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Script to generate boilerplate code for a new IPH feature.

This script will prompt the user for a feature name (which should be in
UpperCamelCase) and a simple one-line description. It will then update the
following files with the appropriate boilerplate code for the new IPH feature:

- components/feature_engagement/public/android/java/src/org/chromium/components/feature_engagement/FeatureConstants.java
- components/feature_engagement/public/android/java/src/org/chromium/components/feature_engagement/EventConstants.java
- components/feature_engagement/public/feature_configurations.cc
- components/feature_engagement/public/feature_constants.h
- components/feature_engagement/public/feature_constants.cc
- components/feature_engagement/public/feature_list.h
- components/feature_engagement/public/feature_list.cc
- tools/metrics/actions/actions.xml
- tools/metrics/histograms/metadata/feature_engagement/histograms.xml

The script will ensure that the new entries are added in the correct
alphabetical order and will handle spacing and formatting to match the existing
style of the codebase. It also includes comments with TODOs for developers to
fill in specific details about the feature's configuration and events.

Run instructions: python3 tools/feature_engagement/generate_iph_entry.py
"""

import sys
import os
import re


def check_file_exists(file_path):
  """Checks if the specified file exists."""
  if not os.path.exists(file_path):
    print(f"Error: File not found at {file_path}. Cannot add IPH code.")
    sys.exit(1)


def to_snake_case(name):
  """Converts UpperCamelCase to SNAKE_CASE."""
  s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
  return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).upper()


def get_user_input():
  """Prompts the user for feature name, description, and Java export preference."""
  print("Welcome to the IPH Entry Generator.")
  feature_name = input("Enter the feature name (UpperCamelCase): ").strip()

  if feature_name.startswith("IPH_"):
    print("Warning: Removed 'IPH_' prefix from the feature name.")
    feature_name = feature_name[4:]

  description = input("Enter a simple one-line description: ").strip()

  if not feature_name or not description:
    print("Error: Both feature name and description are required.")
    sys.exit(1)

  export_java_str = input(
      "Do you want to export to Java? (y/N): ").strip().lower()
  export_java = export_java_str in ('y', 'yes')

  return feature_name, description, export_java


def update_feature_constants_java(feature_name, description):
  """Updates FeatureConstants.java with the new IPH feature."""
  file_path = (
      "components/feature_engagement/public/android/java/src/"
      "org/chromium/components/feature_engagement/FeatureConstants.java")
  check_file_exists(file_path)

  print(f"Updating {file_path} for {feature_name}...")

  snake_name = to_snake_case(feature_name)
  constant_name = f"FeatureConstants.{snake_name}"

  with open(file_path, 'r') as f:
    lines = f.readlines()

  out_lines = []

  # We have two blocks to update:
  # 1. The @StringDef block where we register the constant as a feature.
  # 2. The @interface block where we define the actual constant value.

  in_string_def = False
  inserted_in_string_def = False
  in_interface = False
  interface_insert_idx = -1

  for line in lines:
    # 1. Handle @StringDef block.
    if "// FEATURE_CONSTANTS_JAVA_STRING_DEF_START" in line:
      in_string_def = True
      out_lines.append(line)
      continue

    if in_string_def:
      if "// FEATURE_CONSTANTS_JAVA_STRING_DEF_END" in line:
        in_string_def = False
        if not inserted_in_string_def:
          out_lines.append(f"    {constant_name},\n")
          inserted_in_string_def = True
        out_lines.append(line)
        continue

      if not inserted_in_string_def:
        stripped = line.strip()
        if stripped.startswith("FeatureConstants."):
          current_constant = stripped.rstrip(",").strip()
          if constant_name.lower() < current_constant.lower():
            out_lines.append(f"    {constant_name},\n")
            inserted_in_string_def = True

      out_lines.append(line)
      continue

    # 2. Handle @interface block.
    if "// FEATURE_CONSTANTS_JAVA_INTERFACE_START" in line:
      in_interface = True
      out_lines.append(line)
      continue

    if in_interface:
      if "// FEATURE_CONSTANTS_JAVA_INTERFACE_END" in line:
        in_interface = False
        if interface_insert_idx == -1:
          interface_insert_idx = len(out_lines)
        out_lines.append(line)
        continue

      if interface_insert_idx == -1:
        if line.strip().startswith("String "):
          match = re.search(r'String\s+([A-Z0-9_]+)\s*=', line)
          if match:
            existing_name = match.group(1)
            if snake_name.lower() < existing_name.lower():
              # Backtrack to find the start of the comment block or empty lines.
              backtrack_idx = len(out_lines)
              while backtrack_idx > 0:
                prev_line = out_lines[backtrack_idx - 1].strip()
                if prev_line.startswith("/**") or prev_line.startswith(
                    "*") or prev_line == "":
                  backtrack_idx -= 1
                else:
                  break
              interface_insert_idx = backtrack_idx
      out_lines.append(line)
      continue

    out_lines.append(line)

  if interface_insert_idx != -1:
    new_item = [
        f"    /** {description} */\n",
        f"    String {snake_name} = \"IPH_{feature_name}\";\n"
    ]

    # Manage spacing above.
    if interface_insert_idx > 0 and out_lines[interface_insert_idx - 1].strip(
    ) != "" and "// FEATURE_CONSTANTS_JAVA_INTERFACE_START" not in out_lines[
        interface_insert_idx - 1]:
      new_item.insert(0, "\n")

    # Manage spacing below.
    if interface_insert_idx < len(
        out_lines) and out_lines[interface_insert_idx].strip(
        ) != "" and "// FEATURE_CONSTANTS_JAVA_INTERFACE_END" not in out_lines[
            interface_insert_idx]:
      new_item.append("\n")

    out_lines = (out_lines[:interface_insert_idx] + new_item +
                 out_lines[interface_insert_idx:])

  with open(file_path, 'w') as f:
    f.writelines(out_lines)


def update_feature_configurations_cc(feature_name, description):
  """Updates feature_configurations.cc with the new IPH feature."""
  file_path = "components/feature_engagement/public/feature_configurations.cc"
  check_file_exists(file_path)

  print(f"Updating {file_path} for {feature_name}...")

  snake_name = to_snake_case(feature_name).lower()
  trigger_event = f"{snake_name}_trigger"
  used_event = f"{snake_name}_used"

  config_block = [
      f"  if (kIPH{feature_name}.name == feature->name) {{\n",
      f"    // TODO: Verify the validity of these restrictions.\n",
      "    FeatureConfig config;\n", "    config.valid = true;\n\n",
      "    // IPH is always available at start-up.\n",
      "    config.availability = Comparator(ANY, 0);\n\n",
      "    // IPH only shows if no other IPH has shown this session.\n",
      "    config.session_rate = Comparator(EQUAL, 0);\n\n",
      "    // IPH only shows once per 360 days.\n",
      f"    config.trigger = EventConfig(\"{trigger_event}\",\n",
      "                                 Comparator(EQUAL, 0), 360, 360);\n\n",
      "    // IPH will not show for 360 days after ABC event.\n",
      f"    // TODO: Document what will count as \"used\", you "
      "may also\n",
      "    // want to rename this event to be specific to what \"ABC\" is.\n",
      f"    config.used = EventConfig(\"{used_event}\",\n",
      "                              Comparator(EQUAL, 0), 360, 360);\n",
      "    return config;\n", "  }\n\n"
  ]

  with open(file_path, 'r') as f:
    lines = f.readlines()

  out_lines = []
  in_android_block = False
  inserted = False

  for i, line in enumerate(lines):
    if "// CONFIGURATION_ANDROID_START" in line:
      in_android_block = True
      out_lines.append(line)
      continue

    if in_android_block:
      if "// CONFIGURATION_ANDROID_END" in line:
        in_android_block = False
        if not inserted:
          out_lines.extend(config_block)
          inserted = True
        out_lines.append(line)
      else:
        if (not inserted and "if (" in line
            and ".name == feature->name)" in line):
          match = re.search(r'if \((kIPH[a-zA-Z0-9_]+)\.name', line)
          if match:
            existing_name = match.group(1)
            if f"kIPH{feature_name}".lower() < existing_name.lower():
              out_lines.extend(config_block)
              inserted = True
        out_lines.append(line)
    else:
      out_lines.append(line)

  with open(file_path, 'w') as f:
    f.writelines(out_lines)


def update_feature_constants_h(feature_name, description):
  """Updates feature_constants.h with the new IPH feature."""
  file_path = "components/feature_engagement/public/feature_constants.h"
  check_file_exists(file_path)

  print(f"Updating {file_path} for {feature_name}...")

  declaration = f"FEATURE_CONSTANTS_DECLARE_FEATURE(kIPH{feature_name});"

  with open(file_path, 'r') as f:
    lines = f.readlines()

  out_lines = []
  in_android_block = False
  inserted = False

  for i, line in enumerate(lines):
    if "// FEATURE_CONSTANTS_DECLARE_FEATURE_ANDROID_START" in line:
      in_android_block = True
      out_lines.append(line)
      continue

    if in_android_block:
      if "// FEATURE_CONSTANTS_DECLARE_FEATURE_ANDROID_END" in line:
        in_android_block = False
        if not inserted:
          out_lines.append(f"{declaration}\n")
          inserted = True
        out_lines.append(line)
      else:
        # Look for lines like FEATURE_CONSTANTS_DECLARE_FEATURE(kIPH...);.
        if not inserted and "FEATURE_CONSTANTS_DECLARE_FEATURE" in line:
          match = re.search(
              r'FEATURE_CONSTANTS_DECLARE_FEATURE\((kIPH[a-zA-Z0-9_]+)\)', line)
          if match:
            existing_name = match.group(1)
            if f"kIPH{feature_name}".lower() < existing_name.lower():
              out_lines.append(f"{declaration}\n")
              inserted = True
        out_lines.append(line)
    else:
      out_lines.append(line)

  with open(file_path, 'w') as f:
    f.writelines(out_lines)


def update_feature_constants_cc(feature_name, description):
  """Updates feature_constants.cc with the new IPH feature."""
  file_path = "components/feature_engagement/public/feature_constants.cc"
  check_file_exists(file_path)

  print(f"Updating {file_path} for {feature_name}...")

  declaration = (f"BASE_FEATURE(kIPH{feature_name},\n"
                 f"             \"IPH_{feature_name}\",\n"
                 f"             base::FEATURE_DISABLED_BY_DEFAULT);")

  with open(file_path, 'r') as f:
    lines = f.readlines()

  out_lines = []
  in_android_block = False
  inserted = False

  for i, line in enumerate(lines):
    if "// BASE_FEATURE_ANDROID_START" in line:
      in_android_block = True
      out_lines.append(line)
      continue

    if in_android_block:
      if "// BASE_FEATURE_ANDROID_END" in line:
        in_android_block = False
        if not inserted:
          out_lines.append(f"{declaration}\n")
          inserted = True
        out_lines.append(line)
      else:
        # Look for BASE_FEATURE(kIPH....
        if not inserted and line.startswith("BASE_FEATURE("):
          match = re.search(r'BASE_FEATURE\((kIPH[a-zA-Z0-9_]+),', line)
          if match:
            existing_name = match.group(1)
            if f"kIPH{feature_name}".lower() < existing_name.lower():
              out_lines.append(f"{declaration}\n")
              inserted = True
        out_lines.append(line)
    else:
      out_lines.append(line)

  with open(file_path, 'w') as f:
    f.writelines(out_lines)


def update_feature_list_h(feature_name, description):
  """Updates feature_list.h with the new IPH feature."""
  file_path = "components/feature_engagement/public/feature_list.h"
  check_file_exists(file_path)

  print(f"Updating {file_path} for {feature_name}...")

  variation_param = (f"DEFINE_VARIATION_PARAM(kIPH{feature_name}, "
                     f"\"IPH_{feature_name}\");")
  variation_entry = f"        VARIATION_ENTRY(kIPH{feature_name}),"

  with open(file_path, 'r') as f:
    lines = f.readlines()

  out_lines = []

  # We have two blocks to update:
  # 1. The first anchor block for DEFINE_VARIATION_PARAM.
  # 2. The second anchor block for VARIATION_ENTRY.

  in_define_variation_param_android_block = False
  in_variation_entry_android_block = False
  inserted_param = False
  inserted_entry = False

  for i, line in enumerate(lines):
    if "// DEFINE_VARIATION_PARAM_ANDROID_START" in line:
      in_define_variation_param_android_block = True
      out_lines.append(line)
      continue

    if "// VARIATION_ENTRY_ANDROID_START" in line:
      in_variation_entry_android_block = True
      out_lines.append(line)
      continue

    if in_define_variation_param_android_block:
      if "// DEFINE_VARIATION_PARAM_ANDROID_END" in line:
        in_define_variation_param_android_block = False
        if not inserted_param:
          out_lines.append(f"{variation_param}\n")
          inserted_param = True
        out_lines.append(line)
      else:
        if not inserted_param and line.startswith("DEFINE_VARIATION_PARAM("):
          match = re.search(r'DEFINE_VARIATION_PARAM\((kIPH[a-zA-Z0-9_]+),',
                            line)
          if match:
            existing_name = match.group(1)
            if f"kIPH{feature_name}".lower() < existing_name.lower():
              out_lines.append(f"{variation_param}\n")
              inserted_param = True
        out_lines.append(line)
    elif in_variation_entry_android_block:
      if "// VARIATION_ENTRY_ANDROID_END" in line:
        in_variation_entry_android_block = False
        if not inserted_entry:
          out_lines.append(f"{variation_entry}\n")
          inserted_entry = True
        out_lines.append(line)
      else:
        if not inserted_entry and "VARIATION_ENTRY(" in line:
          match = re.search(r'VARIATION_ENTRY\((kIPH[a-zA-Z0-9_]+)\)', line)
          if match:
            existing_name = match.group(1)
            if f"kIPH{feature_name}".lower() < existing_name.lower():
              out_lines.append(f"{variation_entry}\n")
              inserted_entry = True
        out_lines.append(line)
    else:
      out_lines.append(line)

  with open(file_path, 'w') as f:
    f.writelines(out_lines)


def update_feature_list_cc(feature_name, description):
  """Updates feature_list.cc with the new IPH feature."""
  file_path = "components/feature_engagement/public/feature_list.cc"
  check_file_exists(file_path)

  print(f"Updating {file_path} for {feature_name}...")

  entry = f"    &kIPH{feature_name},"

  with open(file_path, 'r') as f:
    lines = f.readlines()

  out_lines = []
  in_android_block = False
  inserted = False

  for i, line in enumerate(lines):
    if "// ALL_FEATURES_ANDROID_START" in line:
      in_android_block = True
      out_lines.append(line)
      continue

    if in_android_block:
      if "// ALL_FEATURES_ANDROID_END" in line:
        in_android_block = False
        if not inserted:
          out_lines.append(f"{entry}\n")
          inserted = True
        out_lines.append(line)
      else:
        if not inserted and line.strip().startswith("&kIPH"):
          match = re.search(r'&(kIPH[a-zA-Z0-9_]+)', line)
          if match:
            existing_name = match.group(1)
            if f"kIPH{feature_name}".lower() < existing_name.lower():
              out_lines.append(f"{entry}\n")
              inserted = True
        out_lines.append(line)
    else:
      out_lines.append(line)

  with open(file_path, 'w') as f:
    f.writelines(out_lines)


def update_event_constants_java(feature_name, description):
  """Updates EventConstants.java with the new IPH used event."""
  file_path = ("components/feature_engagement/public/android/java/src/"
               "org/chromium/components/feature_engagement/EventConstants.java")
  check_file_exists(file_path)

  print(f"Updating {file_path} for {feature_name}...")

  snake_name = to_snake_case(feature_name).lower()
  used_event_val = f"{snake_name}_used"
  used_event_var = used_event_val.upper()

  event_block = [
      f"    /** TODO: Document what this event is. */\n",
      f"    public static final String {used_event_var} = "
      f"\"{used_event_val}\";\n"
  ]

  with open(file_path, 'r') as f:
    lines = f.readlines()

  out_lines = []
  in_class = False
  inserted = False
  insert_idx = -1

  # Find the correct line index to insert to maintain alphabetical order
  # of the variable names.
  for i, line in enumerate(lines):
    if "// EVENT_CONSTANTS_JAVA_CLASS_START" in line:
      in_class = True
      out_lines.append(line)
      continue

    if in_class:
      if "// EVENT_CONSTANTS_JAVA_CLASS_END" in line:
        # We reached the end of the variables. If not inserted yet, insert here.
        if not inserted:
          insert_idx = len(out_lines)
          inserted = True
        out_lines.append(line)
      else:
        if not inserted and line.strip().startswith(
            "public static final String "):
          match = re.search(r'public static final String\s+([A-Z0-9_]+)\s*=',
                            line)
          if match:
            existing_var = match.group(1)
            # Both are uppercase, so normal comparison is case-insensitive.
            if used_event_var < existing_var:
              # Insert *before* the comment block of the current variable.
              # Backtrack `out_lines` to find the start of the comment or
              # empty lines.
              backtrack_idx = len(out_lines)
              while backtrack_idx > 0:
                prev_line = out_lines[backtrack_idx - 1].strip()
                if prev_line.startswith("/**") or prev_line.startswith(
                    "*") or prev_line == "":
                  backtrack_idx -= 1
                else:
                  break

              # If we backtracked over an empty line, we want to leave one empty
              # line before our new block. We will just insert exactly at
              # `backtrack_idx` and handle spacing.
              insert_idx = backtrack_idx
              inserted = True
        out_lines.append(line)
    else:
      out_lines.append(line)

  if insert_idx != -1:
    # Manage spacing.
    if insert_idx > 0 and out_lines[insert_idx - 1].strip(
    ) != "" and "// EVENT_CONSTANTS_JAVA_CLASS_START" not in out_lines[
        insert_idx - 1]:
      event_block.insert(0, "\n")

    # Add a newline after our block if the next line isn't already empty.
    if insert_idx < len(out_lines) and out_lines[insert_idx].strip(
    ) != "" and "// EVENT_CONSTANTS_JAVA_CLASS_END" not in out_lines[insert_idx]:
      event_block.append("\n")

    out_lines = out_lines[:insert_idx] + event_block + out_lines[insert_idx:]

  with open(file_path, 'w') as f:
    f.writelines(out_lines)


def update_actions_xml(feature_name, description):
  """Updates actions.xml with the new IPH feature."""
  file_path = "tools/metrics/actions/actions.xml"
  check_file_exists(file_path)

  print(f"Updating {file_path} for {feature_name}...")

  variant_entry = (f"  <variant name=\"_{feature_name}\"\n"
                   f"      summary=\"{description}\"/>\n")

  with open(file_path, 'r') as f:
    lines = f.readlines()

  out_lines = []
  in_iph_type_block = False
  inserted = False

  for line in lines:
    if "<variants name=\"InProductHelp_Type\">" in line:
      in_iph_type_block = True
      out_lines.append(line)
      continue

    if in_iph_type_block:
      if "</variants>" in line:
        in_iph_type_block = False
        if not inserted:
          out_lines.append(variant_entry)
          inserted = True
        out_lines.append(line)
      else:
        if not inserted and "<variant name=\"_" in line:
          match = re.search(r'<variant name="(_[a-zA-Z0-9_]+)"', line)
          if match:
            existing_name = match.group(1)
            if f"_{feature_name}".lower() < existing_name.lower():
              out_lines.append(variant_entry)
              inserted = True
        out_lines.append(line)
    else:
      out_lines.append(line)

  with open(file_path, 'w') as f:
    f.writelines(out_lines)


def update_histograms_xml(feature_name, description):
  """Updates histograms.xml with the new IPH feature."""
  file_path = ("tools/metrics/histograms/metadata/"
               "feature_engagement/histograms.xml")
  check_file_exists(file_path)

  print(f"Updating {file_path} for {feature_name}...")

  # Format it to match the existing style (indentation and multi-line if
  # needed, but we will stick to the two-line format for consistency).
  variant_entry = (f"  <variant name=\"IPH_{feature_name}\"\n"
                   f"      summary=\"{description}\"/>\n")

  with open(file_path, 'r') as f:
    lines = f.readlines()

  out_lines = []
  in_iph_feature_block = False
  inserted = False

  for i, line in enumerate(lines):
    if "<variants name=\"IPHFeature\">" in line:
      in_iph_feature_block = True
      out_lines.append(line)
      continue

    if in_iph_feature_block:
      if "</variants>" in line:
        in_iph_feature_block = False
        if not inserted:
          out_lines.append(variant_entry)
          inserted = True
        out_lines.append(line)
      else:
        if not inserted and "<variant name=\"IPH_" in line:
          match = re.search(r'<variant name="(IPH_[a-zA-Z0-9_]+)"', line)
          if match:
            existing_name = match.group(1)
            if f"IPH_{feature_name}".lower() < existing_name.lower():
              out_lines.append(variant_entry)
              inserted = True
        out_lines.append(line)
    else:
      out_lines.append(line)

  with open(file_path, 'w') as f:
    f.writelines(out_lines)


def main():
  feature_name, description, export_java = get_user_input()

  if export_java:
    update_feature_constants_java(feature_name, description)
    update_event_constants_java(feature_name, description)
  else:
    print("Skipping Java exports.")

  update_feature_configurations_cc(feature_name, description)
  update_feature_constants_h(feature_name, description)
  update_feature_constants_cc(feature_name, description)
  update_feature_list_h(feature_name, description)
  update_feature_list_cc(feature_name, description)
  update_histograms_xml(feature_name, description)
  update_actions_xml(feature_name, description)

  print("""
Boilerplate generation complete!
Please review the changes and fill in the TODOs:
Two TODOs are in the file: components/feature_engagement/public/feature_configurations.cc
""")
  if export_java:
    print(
        "One TODO is in: components/feature_engagement/public/android/java/src/org/chromium/components/feature_engagement/EventConstants.java"
    )
    print("")


if __name__ == "__main__":
  main()
