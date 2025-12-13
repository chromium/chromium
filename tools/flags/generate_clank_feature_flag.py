#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Add feature flags for Clank.

This script can be used to quickly add Chrome feature flags and export
them to the Java-side code for Clank. It also creates the necessary
code to add the flag to chrome://flags. It does NOT create a fieldtrial.

The script presents the user with a series of prompts to specify the
basic parts of a flag that are unique to each flag. It then uses the
standard naming conventions and file conventions (e.g. alphabetical
ordering) to add the new flag to the code.

Example (Basic):

# Run the script from the base directory with:
tools/flags/generate_clank_feature_flag.py

# You will be presented with a series of prompts. In order, the answers
# to the prompts may look something like:
1. MyNewFeatureFlag
2. My new feature
3. When enabled, this feature does ...
4. foo@google.com, bar@google.com
5. 157

The script will assume that "MyNewFeatureFlag" should be "kMyNewFeatureFlag"
in the code, with a string identifier as "MyNewFeatureFlag". For the
Java-side it will use "MY_NEW_FEATURE_FLAG" as the variable name. The
answers to the 2nd and 3rd prompts are what appear in chrome://flags. The
answers to the 4th and 5th prompts are what is used in flag-metadata.json.

Example (Advanced):

# Run the script from the base directory with arguments:
tools/flags/generate_clank_feature_flag.py --name "MyNewFeatureFlag" \
  --display-name "Name for chrome://flags" \
  --description "Description for chrome://flags" \
  --owners "foo@google.com,bar@chromium.org" \
  --milestone 145

This will set each of the necessary values as above, using the same assumptions
above variable naming etc. You do not need to include every argument. All of
these pieces of information are required for the script to add a complete flag,
so the user will be presented with a series of prompts to fill in any of the
remaining information not included in the arguments.

The script will also run: tools/metrics/histograms/generate_flag_enums.py
to generate the enums.xml values for the flag.

For a simple/standard flag (e.g. no caching, no FeatureParams, etc), you
should be able to raise a CL to add your flag once the script finishes.

If there are any bugs/issues, please file them in the Flags component:
Chromium > Internals > Flags (id: 1456387)
"""

import re
import json
import subprocess
import os
import sys
import argparse

# --- Helper Functions for Name Formatting ---


def to_java_upper_snake_case(name):
  """Converts 'MyNewFeatureFlagName' to 'MY_NEW_FEATURE_FLAG_NAME'."""
  s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
  return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).upper()


def to_cpp_lower_camel_case_k(name):
  """Converts 'MyNewFeatureFlagName' to 'kMyNewFeatureFlagName'."""
  return 'k' + name[0].upper() + name[1:] if name else 'k'


def to_kebab_case(name):
  """Converts 'MyNewFeatureFlagName' to 'my-new-feature-flag-name'."""
  s1 = re.sub('(.)([A-Z][a-z]+)', r'\1-\2', name)
  return re.sub('([a-z0-9])([A-Z])', r'\1-\2', s1).lower()


def format_flag_names(user_flag_name):
  """Generates all required name formats from the user's flag name."""
  return {
      "user_flag_name":
      user_flag_name,
      "java_constant_name":
      to_java_upper_snake_case(user_flag_name),
      "cpp_variable_name":
      to_cpp_lower_camel_case_k(user_flag_name),
      "cpp_lower_camel_case_name":
      to_kebab_case(user_flag_name),
      "flag_description_name":
      to_cpp_lower_camel_case_k(user_flag_name) + "Name",
      "flag_description_description":
      to_cpp_lower_camel_case_k(user_flag_name) + "Description",
  }


# --- File Paths ---
FILE_PATHS = {
    "java_feature_list": ("chrome/browser/flags/android/java/src/org/chromium/"
                          "chrome/browser/flags/ChromeFeatureList.java"),
    "cpp_feature_list_h":
    "chrome/browser/flags/android/chrome_feature_list.h",
    "cpp_feature_list_cc":
    "chrome/browser/flags/android/chrome_feature_list.cc",
    "about_flags_cc":
    "chrome/browser/about_flags.cc",
    "flag_descriptions_h":
    "chrome/browser/flag_descriptions.h",
    "flag_metadata_json":
    "chrome/browser/flag-metadata.json",
}

# --- File Operations Helper Functions ---


def read_file(filepath):
  """Reads content from a file."""
  if not os.path.exists(filepath):
    print(f"Error: File not found at {filepath}")
    return None
  with open(filepath, 'r') as f:
    return f.readlines()


def write_file(filepath, content_lines):
  """Writes a list of lines back to a file."""
  with open(filepath, 'w') as f:
    f.writelines(content_lines)


def insert_block_before_line(lines, new_block_lines, target_line_pattern):
  """Inserts a block of lines before a specific target line."""
  for i, line in enumerate(lines):
    if target_line_pattern in line:
      return lines[:i] + new_block_lines + lines[i:]
  print(f"Error: Target line pattern '{target_line_pattern}'"
        " not found for insertion.")
  return lines


def get_current_username():
  """Gets the current user's username and formats as an email."""
  try:
    username = subprocess.check_output("git config user.email",
                                       shell=True).decode('utf-8').strip()
    if not username:
      print("Error: 'git config user.email' returned an empty value.",
            file=sys.stderr)
      sys.exit(1)
    return username
  except subprocess.CalledProcessError:
    print(
        "Error: Could not determine username. Please ensure"
        "'git config user.email' is set.",
        file=sys.stderr)
    sys.exit(1)


def add_metadata_entry_textually(file_path, new_entry_dict):
  """
    Updates flag-metadata.json by treating it as a text file to preserve
    comments and formatting. It finds the correct alphabetical insertion
    point and adds the new flag metadata block.
    """
  if not os.path.exists(file_path):
    print(f"Error: File not found at {file_path}. Cannot add metadata.")
    return

  file_lines = read_file(file_path)
  if not file_lines:
    print(f"Warning: {file_path} is empty. Cannot add metadata.")
    return

  new_flag_name = new_entry_dict["name"]

  # --- Step 1: Find the insertion point ---
  insertion_line_index = -1
  last_name_line_index = -1

  # Find the line index of the first "name" entry that should come after this.
  for i, line in enumerate(file_lines):
    # Regex to find a "name" entry and capture its value
    match = re.search(r'^\s*"name":\s*"(?P<name>.*?)",?', line)
    if match:
      last_name_line_index = i
      existing_flag_name = match.group("name")
      if new_flag_name < existing_flag_name:
        # We found the spot. Now, find the opening brace `{` of this block by
        # searching backwards.
        for j in range(i - 1, -1, -1):
          if "{" in file_lines[j]:
            insertion_line_index = j
            break
        if insertion_line_index != -1:
          break  # Exit the main loop once we've found our insertion point

  # --- Step 2: Prepare the text block for the new entry ---
  owners_json_string = json.dumps(new_entry_dict["owners"])
  new_block_lines = [
      '  {\n',
      f'    "name": "{new_flag_name}",\n',
      f'    "owners": {owners_json_string},\n',
      f'    "expiry_milestone": {new_entry_dict["expiry_milestone"]}\n',
  ]

  # --- Step 3: Insert the block and handle trailing commas ---
  if insertion_line_index != -1:
    # Case 1: Inserting in the middle of the list.
    # The new block needs a trailing comma.
    new_block_lines.append('  },\n')
    # Insert the new block before the block of the next alphabetical feature.
    file_lines = (file_lines[:insertion_line_index] + new_block_lines +
                  file_lines[insertion_line_index:])
  else:
    # Case 2: Appending to the end of the list.
    # The new block does NOT get a trailing comma.
    new_block_lines.append('  }\n')

    # Find the closing brace of the *previous* last entry and add a comma to it.
    if last_name_line_index != -1:
      # Search forward from the last known "name" to find its closing brace `}`.
      for i in range(last_name_line_index, len(file_lines)):
        if "}" in file_lines[i]:
          # Add a comma to that line.
          file_lines[i] = file_lines[i].rstrip() + ',\n'
          break

    # Find the final closing bracket `]` of the whole list to insert before it.
    closing_bracket_index = -1
    for i in range(len(file_lines) - 1, -1, -1):
      if "]" in file_lines[i]:
        closing_bracket_index = i
        break

    if closing_bracket_index != -1:
      file_lines = (file_lines[:closing_bracket_index] + new_block_lines +
                    file_lines[closing_bracket_index:])
    else:
      print(f"Error: Could not find closing bracket ']' in {file_path}."
            "Appending to end of file as a fallback.")
      file_lines.extend(new_block_lines)

  write_file(file_path, file_lines)


# --- Main Script Logic ---


def add_chrome_android_feature_flag(user_provided_flag_name, display_name,
                                    description, owners_list, expiry_milestone):
  """Performs all file modifications to add the new feature flag."""
  print("\nStarting to add new feature flag to the codebase...")

  names = format_flag_names(user_provided_flag_name)
  java_constant_name = names["java_constant_name"]
  cpp_variable_name = names["cpp_variable_name"]
  cpp_lower_camel_case_name = names["cpp_lower_camel_case_name"]
  flag_desc_name_var = names["flag_description_name"]
  flag_desc_description_var = names["flag_description_description"]

  # Step 3: Add Java-side functional component
  print("\nStep 3: Adding Java-side functional code...")
  java_file_lines = read_file(FILE_PATHS["java_feature_list"])
  if java_file_lines is None: return

  # The new flag definition will always be a single line.
  # A code formatter can split it later if necessary.
  new_java_line = (f'    public static final String {java_constant_name}'
                   f' = "{user_provided_flag_name}";\n')

  insert_idx = -1
  last_flag_block_end_idx = -1

  # Iterate through the file to find the correct alphabetical insertion point.
  for i, line in enumerate(java_file_lines):
    # We only care about the "anchor" lines that start a definition.
    if line.strip().startswith("public static final String"):
      # Extract the constant name from this anchor line.
      match = re.search(r'public static final String ([A-Z_]+)', line)
      if not match:
        continue

      existing_flag_constant_name = match.group(1)

      # Check if our new flag comes before the current one.
      if java_constant_name < existing_flag_constant_name:
        # If it does, this is our insertion point. We insert *before*
        # the anchor line of the entry that should come after ours.
        insert_idx = i
        break
      else:
        # If not, we keep track of the end of the current flag's
        # definition block. This is in case our new flag needs to
        # be added at the very end of the list.
        # We scan forward from the current anchor line to find the ';'.
        current_block_end = -1
        for j in range(i, len(java_file_lines)):
          if ";" in java_file_lines[j]:
            current_block_end = j
            break
        if current_block_end != -1:
          last_flag_block_end_idx = current_block_end

  # Perform the insertion.
  if insert_idx != -1:
    # Case 1: Insert in the middle of the list.
    java_file_lines.insert(insert_idx, new_java_line)
  elif last_flag_block_end_idx != -1:
    # Case 2: Append after the last known flag definition block.
    java_file_lines.insert(last_flag_block_end_idx + 1, new_java_line)
  else:
    # Fallback case: No feature flags were found. We can't safely insert.
    print(f"Warning: No existing feature flags found in",
          f"{FILE_PATHS['java_feature_list']}. Could not add new flag.")
    return  # Exit the step to prevent incorrect file modification.

  write_file(FILE_PATHS["java_feature_list"], java_file_lines)
  print(f"Added Java constant: {java_constant_name} to ",
        f"{FILE_PATHS['java_feature_list']}")

  # Step 4: Add C++-side functional component
  print("\nStep 4: Adding C++-side functional code...")
  # .h file
  h_file_lines = read_file(FILE_PATHS["cpp_feature_list_h"])
  if h_file_lines is None: return
  new_h_line = f'BASE_DECLARE_FEATURE({cpp_variable_name});\n'
  h_insert_idx, last_h_decl_idx = -1, -1
  for i, line in enumerate(h_file_lines):
    match = re.search(r'BASE_DECLARE_FEATURE\((k[A-Za-z0-9]+)\);', line)
    if match:
      last_h_decl_idx = i
      if cpp_variable_name < match.group(1):
        h_insert_idx = i
        break
  h_file_lines.insert(
      h_insert_idx if h_insert_idx != -1 else last_h_decl_idx + 1, new_h_line)
  write_file(FILE_PATHS["cpp_feature_list_h"], h_file_lines)
  print(f"Added C++ declaration to {FILE_PATHS['cpp_feature_list_h']}")

  # .cc file
  cc_file_lines = read_file(FILE_PATHS["cpp_feature_list_cc"])
  if cc_file_lines is None: return

  # Determine what to insert based on the context.
  lines_to_insert = [
      f'BASE_FEATURE({cpp_variable_name}, base::FEATURE_DISABLED_BY_DEFAULT);\n'
  ]

  insertion_anchor_idx = -1
  prev_feature_idx = -1
  last_feature_idx = -1

  # Find the insertion anchor (the next feature) and track the
  # previous/last feature.
  for i, line in enumerate(cc_file_lines):
    if line.strip().startswith("BASE_FEATURE"):
      match = re.search(r'BASE_FEATURE\((k[A-Za-z0-9]+),', line)
      if not match:
        continue

      last_feature_idx = i
      existing_cpp_var_name = match.group(1)

      if cpp_variable_name < existing_cpp_var_name:
        insertion_anchor_idx = i
        break
      else:
        prev_feature_idx = i

  # --- Case 1: Inserting in the middle of the list ---
  if insertion_anchor_idx != -1:
    # Find the true start of the next block to insert before.
    true_insertion_point = insertion_anchor_idx
    for j in range(insertion_anchor_idx - 1, -1, -1):
      stripped_line = cc_file_lines[j].strip()
      if stripped_line.startswith('//') or not stripped_line:
        true_insertion_point = j
      else:
        break

    cc_file_lines = (cc_file_lines[:true_insertion_point] + lines_to_insert +
                     cc_file_lines[true_insertion_point:])

  # --- Case 2: Appending to the end of the list ---
  else:
    if last_feature_idx > 0 and cc_file_lines[last_feature_idx -
                                              1].strip().startswith('//'):
      # If the last feature in the file was commented, add a preceding newline.
      lines_to_insert.insert(0, '\n')

    # Find the closing brace of the last feature block to insert after.
    last_block_end_idx = -1
    if last_feature_idx != -1:
      for i in range(last_feature_idx, len(cc_file_lines)):
        if ');' in cc_file_lines[i]:
          last_block_end_idx = i
          break

    if last_block_end_idx != -1:
      cc_file_lines = (cc_file_lines[:last_block_end_idx + 1] +
                       lines_to_insert + cc_file_lines[last_block_end_idx + 1:])
    else:
      print(f"Warning: No BASE_FEATUREs found in "
            f"{FILE_PATHS['cpp_feature_list_cc']}. Appending to end of file.")
      cc_file_lines.extend(lines_to_insert)

  write_file(FILE_PATHS["cpp_feature_list_cc"], cc_file_lines)
  print(f"Added C++ feature definition to {FILE_PATHS['cpp_feature_list_cc']}")

  # Add to kFeaturesExposedToJava
  start_pattern = "FEATURE_EXPORT_LIST_START"
  end_marker = "FEATURE_EXPORT_LIST_END"
  start_idx = next(
      (i for i, line in enumerate(cc_file_lines) if start_pattern in line), -1)
  if start_idx != -1:
    end_idx = next((i for i, line in enumerate(cc_file_lines)
                    if end_marker in line and i > start_idx), -1)
    if end_idx != -1:
      insertion_point, last_feature_line = -1, -1
      for i in range(start_idx + 1, end_idx):
        match = re.search(r'&\s*(k[A-Za-z0-9]+)', cc_file_lines[i])
        if match:
          last_feature_line = i
          if cpp_variable_name < match.group(1):
            insertion_point = i
            break
      new_exposed_line = f'    &{cpp_variable_name},\n'
      cc_file_lines.insert(
          insertion_point if insertion_point != -1 else last_feature_line + 1,
          new_exposed_line)
  write_file(FILE_PATHS["cpp_feature_list_cc"], cc_file_lines)
  print(
      f"Added C++ feature and exposed it in {FILE_PATHS['cpp_feature_list_cc']}"
  )

  # Step 5: Add about_flags portion
  print("\nStep 5: Adding about_flags entry...")
  about_flags_lines = read_file(FILE_PATHS["about_flags_cc"])
  if about_flags_lines is None: return
  new_about_flags_entry = [
      '#if BUILDFLAG(IS_ANDROID)\n', f'    {{"{cpp_lower_camel_case_name}",\n',
      f'     flag_descriptions::{flag_desc_name_var},\n',
      f'     flag_descriptions::{flag_desc_description_var}, kOsAndroid,\n',
      f'     FEATURE_VALUE_TYPE(chrome::android::{cpp_variable_name})}},\n',
      '#endif\n'
  ]
  about_flags_lines = insert_block_before_line(
      about_flags_lines, new_about_flags_entry,
      "// Add new entries above this line.")
  write_file(FILE_PATHS["about_flags_cc"], about_flags_lines)
  print(f"Added about_flags entry for {cpp_lower_camel_case_name} to "
        f"{FILE_PATHS['about_flags_cc']}")
  # Step 6: Add flag_descriptions portion
  print("\nStep 6: Adding flag_descriptions...")

  # --- Modify flag_descriptions.h ---
  flag_desc_cc_lines = read_file(FILE_PATHS["flag_descriptions_h"])
  if flag_desc_cc_lines is None: return

  cc_start_marker = "FLAG_DESCRIPTIONS_ANDROID_START"
  cc_end_marker = "FLAG_DESCRIPTIONS_ANDROID_END"

  cc_start_idx = next(
      (i
       for i, line in enumerate(flag_desc_cc_lines) if cc_start_marker in line),
      -1)
  cc_end_idx = next(
      (i for i, line in enumerate(flag_desc_cc_lines) if cc_end_marker in line),
      -1)

  if cc_start_idx != -1 and cc_end_idx != -1:
    cc_insertion_point = -1
    # Search within the block for the alphabetical insertion point.
    for i in range(cc_start_idx + 1, cc_end_idx):
      line = flag_desc_cc_lines[i]
      match = re.search(r'inline constexpr char (k[A-Za-z0-9]+Name)\[\]', line)
      if match:
        existing_name_var = match.group(1)
        if flag_desc_name_var < existing_name_var:
          # Found the insertion point. We need to find the start of this entire
          # block to ensure we insert before it.
          true_insertion_point = i
          # Scan backwards to find the start of the block (the "Name" line).
          for j in range(i - 1, cc_start_idx, -1):
            if 'inline constexpr char' in flag_desc_cc_lines[
                j] and 'Name[]' in flag_desc_cc_lines[j]:
              true_insertion_point = j
            elif flag_desc_cc_lines[j].strip(
            ) == '':  # Stop at the blank line of the previous entry
              break
          cc_insertion_point = true_insertion_point
          break

    new_cc_pair = [
        f'inline constexpr char {flag_desc_name_var}[] = "{display_name}";\n',
        f'inline constexpr char {flag_desc_description_var}[] =\n',
        f'    "{description}";\n', '\n'
    ]

    # If no later entry was found, insert before the end marker. Otherwise,
    # insert at the found point.
    final_cc_insertion_idx = (cc_end_idx if cc_insertion_point == -1 else
                              cc_insertion_point)
    flag_desc_cc_lines = (flag_desc_cc_lines[:final_cc_insertion_idx] +
                          new_cc_pair +
                          flag_desc_cc_lines[final_cc_insertion_idx:])

    write_file(FILE_PATHS["flag_descriptions_h"], flag_desc_cc_lines)
    print(f"Added definitions to {FILE_PATHS['flag_descriptions_h']}")
  else:
    print(f"Error: Could not find START/END markers in"
          f"{FILE_PATHS['flag_descriptions_h']}")

  # Step 7: Add flag-metadata portion
  print("\nStep 7: Adding flag-metadata entry...")
  new_metadata_entry = {
      "name": cpp_lower_camel_case_name,
      "owners": owners_list,
      "expiry_milestone": expiry_milestone
  }
  add_metadata_entry_textually(FILE_PATHS["flag_metadata_json"],
                               new_metadata_entry)
  print(f"Added metadata for {cpp_lower_camel_case_name} to "
        f"{FILE_PATHS['flag_metadata_json']}")

  # Step 12: Run generate_flag_enums.py
  print(
      "\nStep 12: Running tools/metrics/histograms/generate_flag_enums.py ...")
  try:
    subprocess.run([
        "python3", "tools/metrics/histograms/generate_flag_enums.py",
        "--feature", user_provided_flag_name
    ],
                   check=True,
                   capture_output=True,
                   text=True)
    print("Successfully ran generate_flag_enums.py.")
  except (subprocess.CalledProcessError, FileNotFoundError) as e:
    print(f"Error running generate_flag_enums.py: {e}")

  # Step 13: Give confirmation
  print(
      "\n\nFeature flag addition process complete. Remember to check the diff!")


def parse_arguments():
  """Parses command line arguments."""
  parser = argparse.ArgumentParser(
      description="New Chrome Android Feature Flag Creator. "
      "If arguments are not provided, the script will prompt interactively.",
      formatter_class=argparse.RawTextHelpFormatter)

  parser.add_argument(
      '--name',
      type=str,
      help='Internal feature flag name (e.g., MyNewFeatureFlag).')
  parser.add_argument('--display-name',
                      type=str,
                      help='User-facing name for chrome://flags.')
  parser.add_argument('--description',
                      type=str,
                      help='Description for chrome://flags.')
  parser.add_argument(
      '--owners',
      type=str,
      help=('Additional owners as a comma-separated list of emails '
            '(e.g., "user1@a.com,user2@b.com").'))
  parser.add_argument(
      '--milestone',
      type=int,
      help='The expiry milestone as a positive whole number (e.g., 152).')

  return parser.parse_args()


def main():
  # Parse command-line arguments first
  args = parse_arguments()

  print("--- New Chrome Android Feature Flag Creator ---")
  print("Please answer the following questions to generate the new flag.")

  # 1. Get the internal flag name
  user_provided_flag_name = args.name
  while not user_provided_flag_name:
    user_provided_flag_name = input(
        "\nEnter the internal feature flag name (e.g., MyNewFeatureFlag): "
    ).strip()
    if not user_provided_flag_name:
      print("The internal name cannot be empty.")

  # 2. Get the user-facing name
  display_name = args.display_name
  default_display_name = re.sub(r'([a-z])([A-Z])', r'\1 \2',
                                user_provided_flag_name)

  if not display_name:
    display_name = input(
        f"Enter the user-facing name for chrome://flags "
        f"(press Enter for default: '{default_display_name}'): ").strip()
    if not display_name:
      display_name = default_display_name

  # 3. Get the description
  description = args.description
  default_description = f"Enables the {display_name} feature."

  if not description:
    description = input(
        f"Enter the description for chrome://flags "
        f"(press Enter for default: '{default_description}'): ").strip()
    if not description:
      description = default_description

  # 4. Get owners
  current_user = get_current_username()
  additional_owners_input = args.owners

  if not additional_owners_input:
    print(f"\nYou ({current_user}) will be added as an owner automatically.")
    additional_owners_input = input(
        "Enter any additional owners as a comma-separated list of emails "
        "(or press Enter to skip): ").strip()

  additional_owners = [
      owner.strip() for owner in additional_owners_input.split(',')
      if owner.strip()
  ]

  # Remove duplicates while preserving the order of the first occurrence.
  all_owners_with_potential_dupes = [current_user] + additional_owners
  seen = set()
  all_owners = []
  for owner in all_owners_with_potential_dupes:
    if owner not in seen:
      all_owners.append(owner)
      seen.add(owner)

  # 5. Get expiry milestone
  expiry_milestone = args.milestone

  # Ensure milestone is a positive integer
  while not isinstance(expiry_milestone, int) or expiry_milestone <= 0:
    milestone_input = input(
        "Enter the expiry milestone as a number (e.g., 152): ").strip()
    if milestone_input.isdigit():
      expiry_milestone = int(milestone_input)
    else:
      print("Invalid input. Please enter a positive whole number.")

  print("\nThank you. Using the following information:")
  print(f"  - Internal Name: {user_provided_flag_name}")
  print(f"  - Display Name:  {display_name}")
  print(f"  - Description:   {description}")
  print(f"  - Owners:        {all_owners}")
  print(f"  - Milestone:     {expiry_milestone}")

  add_chrome_android_feature_flag(user_provided_flag_name, display_name,
                                  description, all_owners, expiry_milestone)


if __name__ == "__main__":
  sys.exit(main())
