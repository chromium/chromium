#!/usr/bin/env python

import json
import os
import re


def Clean(start_dir, attr_pattern, file_pattern):
  cleaned = False

  def _remove_attrs(json_dict, attr_pattern):
    assert isinstance(json_dict, dict)

    removed = False

    for key, val in json_dict.items():
      if isinstance(val, dict):
        if _remove_attrs(val, attr_pattern):
          removed = True
      elif re.search(attr_pattern, key):
        del json_dict[key]
        removed = True

    return removed

  for root, dirs, files in os.walk(start_dir):
    for f in files:
      if not re.search(file_pattern, f):
        continue

      path = os.path.join(root, f)
      json_dict = json.loads(open(path).read())
      if not _remove_attrs(json_dict, attr_pattern):
        continue

      with open(path, 'w') as new_contents:
        new_contents.write(json.dumps(json_dict))
      cleaned = True

  return cleaned


if __name__ == '__main__':
  import argparse
  import sys
  parser = argparse.ArgumentParser(
      description='Recursively removes attributes from JSON files')
  parser.add_argument('--attr_pattern', type=str, required=True,
      help='A regex of attributes to remove')
  parser.add_argument('--file_pattern', type=str, required=True,
      help='A regex of files to clean')
  parser.add_argument('start_dir', type=str,
      help='A directory to start scanning')
  args = parser.parse_args(sys.argv[1:])
  Clean(start_dir=args.start_dir, attr_pattern=args.attr_pattern,
        file_pattern=args.file_pattern)
