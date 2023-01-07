# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys

_CWD = os.getcwd()

# TODO(crbug.com/1320176): Consider either integrating this functionality into
# ts_library.py or replacing the regex if only "tslib" is ever rewritten.
def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--import_mappings', nargs='*')
    parser.add_argument('--out_dir', required=True)
    parser.add_argument('--in_files', nargs='*')
    args = parser.parse_args(argv)

    import_mappings = dict()
    for mapping in args.import_mappings:
        (src, dst) = mapping.split('|')
        import_mappings[src] = dst

    for f in args.in_files:
        filename = os.path.basename(f)
        output = []
        for line in open(f, 'r').readlines():
            # Investigate JS parsing if this is insufficient.
            match = re.match(r'^(import .*["\'])(.*)(["\'];)$', line)
            if match:
                import_src = match.group(2)
                if import_src in import_mappings:
                    line = (match.group(1) + import_mappings[import_src] +
                            match.group(3) + '\n')

            output.append(line)

        with open(os.path.join(args.out_dir, filename), 'w') as out_file:
            out_file.write(''.join(output))


if __name__ == '__main__':
    main(sys.argv[1:])
