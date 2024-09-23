#!/usr/bin/env python3

# This script expects your out/Debug directory to be compiling for linux on
# a linux machine. If this is not your case just compile protoc and run the
# command on the last line of the script (from within
# //third_party/protobuf/src).

import argparse
import os

PROTO_DIR = os.path.dirname(__file__)
DIR_SOURCE_ROOT = os.path.join(PROTO_DIR, '..','..')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-directory',
                        '-C',
                        help='Path to build directory',
                        required=True)

    args = parser.parse_args()

    out_build_dir = args.output_directory

    def r(c):
        print('Running:', c)
        os.system(c)

    r('autoninja -C {} protoc'.format(out_build_dir))

    protoc = os.path.join(out_build_dir, 'protoc')
    print('Creating //third_party/protobuf/python/google/protobuf/'
          'descriptor_pb2.py')
    r('{0} --proto_path={1}/src --python_out={1}/python '
      '{1}/src/google/protobuf/descriptor.proto'.format(protoc, PROTO_DIR))

    print('Creating //third_party/protobuf/python/google/protobuf/compiler/'
          'plugin_pb2.py')
    r('{0} --proto_path={1}/src --python_out={1}/python '
      '{1}/src/google/protobuf/compiler/plugin.proto'.format(protoc, PROTO_DIR))

if __name__ == '__main__':
    main()
