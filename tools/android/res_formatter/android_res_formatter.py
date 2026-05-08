#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to format Android resource XML files.

It enforces:
- Indentation of 2 spaces for children.
- Continuation indent of 4 spaces for attributes on new lines.
- Attribute ordering from tools/android/android_studio/ChromiumStyle.xml.
- Self-closing tags for empty elements.
- Collapsing of multiple empty lines between elements to a single one.
"""

import argparse
import difflib
import os
import sys
from xml.dom import minidom

INDENT_SIZE = 2
CONTINUATION_INDENT_SIZE = 4
MAX_LINE_LENGTH = 100


def sort_key(item):
  name, value = item

  if name == 'xmlns:android':
    return (0, name)
  if name.startswith('xmlns:'):
    return (1, name)
  if name == 'android:id':
    return (2, name)
  if name == 'android:name':
    return (3, name)
  if name == 'name':
    return (4, name)
  if name == 'style':
    return (5, name)
  if ':' not in name:  # No namespace
    return (6, name)
  if name.startswith('android:') and name.endswith('Style'):
    return (7, name)
  if name == 'android:width':
    return (8, name)
  if name == 'android:height':
    return (9, name)
  if name == 'android:layout_width':
    return (10, name)
  if name == 'android:layout_height':
    return (11, name)
  if name == 'android:layout_weight':
    return (12, name)
  if name == 'android:layout_margin':
    return (13, name)
  if name == 'android:layout_marginTop':
    return (14, name)
  if name == 'android:layout_marginBottom':
    return (15, name)
  if name == 'android:layout_marginStart':
    return (16, name)
  if name == 'android:layout_marginEnd':
    return (17, name)
  if name == 'android:layout_marginLeft':
    return (18, name)
  if name == 'android:layout_marginRight':
    return (19, name)
  if name.startswith('android:layout_'):
    return (20, name)
  if name == 'android:padding':
    return (21, name)
  if name == 'android:paddingTop':
    return (22, name)
  if name == 'android:paddingBottom':
    return (23, name)
  if name == 'android:paddingStart':
    return (24, name)
  if name == 'android:paddingEnd':
    return (25, name)
  if name == 'android:paddingLeft':
    return (26, name)
  if name == 'android:paddingRight':
    return (27, name)
  if name.startswith('android:'):
    return (28, name)
  if name.startswith('app:'):
    return (29, name)
  if name.startswith('tools:'):
    return (30, name)

  return (31, name)


def sort_attributes(attrs):
  """Sorts attributes according to ChromiumStyle.xml rules."""
  if not attrs:
    return []

  items = list(attrs.items())
  items.sort(key=sort_key)
  return items


def format_node(node, indent=0, indent_str=' ' * INDENT_SIZE):
  """Recursively formats an XML node.

  Args:
    node: The minidom node to format.
    indent: The current indentation level.
    indent_str: The string used for one level of indentation.
  """
  if node.nodeType == minidom.Node.DOCUMENT_NODE:
    parts = [
        format_node(child, indent, indent_str) for child in node.childNodes
    ]
    return ''.join(parts)

  ret = ''

  if node.nodeType == minidom.Node.ELEMENT_NODE:
    tag = node.tagName
    attrs = sort_attributes(node.attributes)

    real_children = [
        c for c in node.childNodes
        if c.nodeType != minidom.Node.TEXT_NODE or c.nodeValue.strip()
    ]

    # Try single-line formatting first
    attr_strs = [f'{name}="{value}"' for name, value in attrs]
    attr_str = ' ' + ' '.join(attr_strs) if attr_strs else ''

    if not real_children:
      single_line = f"{indent_str * indent}<{tag}{attr_str} />\n"
      if len(single_line) <= MAX_LINE_LENGTH:
        return single_line
    elif len(real_children
             ) == 1 and real_children[0].nodeType == minidom.Node.TEXT_NODE:
      text = real_children[0].nodeValue.strip()
      single_line = f"{indent_str * indent}<{tag}{attr_str}>{text}</{tag}>\n"
      if len(single_line) <= MAX_LINE_LENGTH:
        return single_line

    # Fallback to multi-line if it doesn't fit or has complex children
    ret = ''
    attr_strs = [f'{name}="{value}"' for name, value in attrs]
    attr_str = ' ' + ' '.join(attr_strs) if attr_strs else ''
    single_line_open = f"{indent_str * indent}<{tag}{attr_str}"

    if len(single_line_open) <= MAX_LINE_LENGTH or len(attrs) <= 1:
      ret += single_line_open
    else:
      continuation_indent = ' ' * (indent * len(indent_str) +
                                   CONTINUATION_INDENT_SIZE)
      attr_strs_wrapped = [
          f'\n{continuation_indent}{name}="{value}"' for name, value in attrs
      ]
      ret += f"{indent_str * indent}<{tag}{''.join(attr_strs_wrapped)}"

    if real_children:
      parts = ['>\n']
      for child in node.childNodes:
        is_text_node = child.nodeType == minidom.Node.TEXT_NODE
        if is_text_node and not child.nodeValue.strip():
          if child.nodeValue.count('\n') >= 2:
            parts.append('\n')
          continue
        parts.append(format_node(child, indent + 1, indent_str))
      parts.append(f"{indent_str * indent}</{tag}>\n")
      return ret + ''.join(parts)
    else:
      ret += ' />\n'

    return ret

  if node.nodeType == minidom.Node.TEXT_NODE:
    text = node.nodeValue.strip()
    if text:
      return f"{indent_str * indent}{text}\n"
    return ''

  if node.nodeType == minidom.Node.COMMENT_NODE:
    return f"{indent_str * indent}<!--{node.nodeValue}-->\n"

  return ''


def format_xml(content):
  """Parses and formats XML content."""
  dom = minidom.parseString(content)
  formatted = format_node(dom)

  if content.startswith('<?xml'):
    end_idx = content.find('?>')
    if end_idx != -1:
      xml_declaration = content[:end_idx + 2] + '\n'
      return xml_declaration + formatted

  return formatted


def print_diff(original, formatted, filename):
  """Prints a unified diff between original and formatted text.

  Returns:
      2 if differences were found, 0 otherwise.
  """
  diff = list(
      difflib.unified_diff(original.splitlines(keepends=True),
                           formatted.splitlines(keepends=True),
                           fromfile=f'a/{filename}',
                           tofile=f'b/{filename}'))

  if not diff:
    return 0

  sys.stdout.writelines(diff)
  return 2


def process_file(file_path, check=False, stdout=False, diff=False):
  """Processes a single XML file."""
  with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

  formatted = format_xml(content)

  if check:
    return content == formatted

  if diff:
    return print_diff(content, formatted, file_path) == 0

  if stdout:
    sys.stdout.write(formatted)
    return True

  with open(file_path, 'w', encoding='utf-8') as f:
    f.write(formatted)
  print(f"Formatted {file_path}")
  return True


def main():
  """Main function to handle CLI arguments and run the formatter."""
  parser = argparse.ArgumentParser(
      description='Format Android resource XML files.')
  parser.add_argument('paths',
                      nargs='+',
                      help='Paths to XML files or directories to format.')
  parser.add_argument(
      '--check',
      action='store_true',
      help='Check if files are formatted without modifying them.')
  parser.add_argument('--diff',
                      action='store_true',
                      help='Print diff to stdout rather than modifying files.')
  parser.add_argument('--stdout',
                      action='store_true',
                      help='Print formatted content to stdout instead of file.')

  args = parser.parse_args()

  success = True
  invalid_path_found = False

  for path in args.paths:
    if os.path.isdir(path):
      for root, _, files in os.walk(path):
        for file in files:
          if file.endswith('.xml'):
            file_path = os.path.join(root, file)
            if not process_file(file_path, args.check, args.stdout, args.diff):
              success = False
              if args.check or args.diff:
                print(f"Requires formatting: {file_path}")
    elif os.path.isfile(path):
      if not process_file(path, args.check, args.stdout, args.diff):
        success = False
        if args.check or args.diff:
          print(f"Requires formatting: {path}")
    else:
      print(f"Error: {path} is not a valid file or directory", file=sys.stderr)
      invalid_path_found = True

  if invalid_path_found:
    sys.exit(1)

  if (args.check or args.diff) and not success:
    sys.exit(2)


if __name__ == '__main__':
  main()
