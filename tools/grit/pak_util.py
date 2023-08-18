#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool for interacting with .pak files.

For details on the pak file format, see:
https://dev.chromium.org/developers/design-documents/linuxresourcesandlocalizedstrings
"""


import argparse
import gzip
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

from grit import constants
from grit.format import data_pack

_GZIP_HEADER = b'\x1f\x8b'


def _RepackMain(args):
  output_info_filepath = args.output_pak_file + '.info'
  if args.compress:
    # If the file needs to be compressed, call RePack with a tempfile path,
    # then compress the tempfile to args.output_pak_file.
    temp_outfile = tempfile.NamedTemporaryFile()
    out_path = temp_outfile.name
    # Strip any non .pak extension from the .info output file path.
    splitext = os.path.splitext(args.output_pak_file)
    if splitext[1] != '.pak':
      output_info_filepath = splitext[0] + '.info'
  else:
    out_path = args.output_pak_file
  data_pack.RePack(out_path,
                   args.input_pak_files,
                   args.allowlist,
                   args.suppress_removed_key_output,
                   output_info_filepath=output_info_filepath)
  if args.compress:
    with open(args.output_pak_file, 'wb') as out:
      with gzip.GzipFile(filename='', mode='wb', fileobj=out, mtime=0) as outgz:
        shutil.copyfileobj(temp_outfile, outgz)


def _MaybeDecompress(payload, brotli_path=None):
  if payload.startswith(_GZIP_HEADER):
    return gzip.decompress(payload)
  if payload.startswith(constants.BROTLI_CONST):
    shell = brotli_path is None
    brotli_path = brotli_path or 'brotli'
    # Header is 2 bytes, size is 6 bytes.
    payload = payload[8:]
    try:
      result = subprocess.run([brotli_path, '--decompress', '--stdout'],
                              shell=shell,
                              input=payload,
                              stdout=subprocess.PIPE,
                              check=True)
      return result.stdout
    except subprocess.CalledProcessError as e:
      sys.stderr.write(str(e) + '\n')
      sys.exit(1)
  return payload


def _ExtractMain(args):
  pak = data_pack.ReadDataPack(args.pak_file)
  if args.textual_id:
    info_dict = data_pack.ReadGrdInfo(args.pak_file)
  for resource_id, payload in pak.resources.items():
    filename = (
        info_dict[resource_id].textual_id
        if args.textual_id else str(resource_id))
    path = os.path.join(args.output_dir, filename)
    with open(path, 'wb') as f:
      if not args.raw:
        payload = _MaybeDecompress(payload, args.brotli)
      f.write(payload)


def _CreateMain(args):
  pak = {}
  for name in os.listdir(args.input_dir):
    try:
      resource_id = int(name)
    except:
      continue
    filename = os.path.join(args.input_dir, name)
    if os.path.isfile(filename):
      with open(filename, 'rb') as f:
        pak[resource_id] = f.read()
  data_pack.WriteDataPack(pak, args.output_pak_file, data_pack.UTF8)


def _PrintMain(args):
  pak = data_pack.ReadDataPack(args.pak_file)
  if args.textual_id:
    info_dict = data_pack.ReadGrdInfo(args.pak_file)
  output = args.output
  encoding = 'binary'
  if pak.encoding == 1:
    encoding = 'utf-8'
  elif pak.encoding == 2:
    encoding = 'utf-16'
  else:
    encoding = '?' + str(pak.encoding)

  output.write('version: {}\n'.format(pak.version))
  output.write('encoding: {}\n'.format(encoding))
  output.write('num_resources: {}\n'.format(len(pak.resources)))
  output.write('num_aliases: {}\n'.format(len(pak.aliases)))
  breakdown = ', '.join('{}: {}'.format(*x) for x in pak.sizes)
  output.write('total_size: {} ({})\n'.format(pak.sizes.total, breakdown))

  try_decode = args.decode and encoding.startswith('utf')
  # Print IDs in ascending order, since that's the order in which they appear in
  # the file (order is lost by Python dict).
  for resource_id in sorted(pak.resources):
    data = pak.resources[resource_id]
    canonical_id = pak.aliases.get(resource_id, resource_id)
    desc = '<data>'
    if try_decode:
      try:
        desc = str(data, encoding)
        if len(desc) > 60:
          desc = desc[:60] + '...'
        desc = desc.replace('\n', '\\n')
      except UnicodeDecodeError:
        pass
    sha1 = hashlib.sha1(data).hexdigest()[:10]
    if args.textual_id:
      textual_id = info_dict[resource_id].textual_id
      canonical_textual_id = info_dict[canonical_id].textual_id
      output.write(
          'Entry(id={}, canonical_id={}, size={}, sha1={}): {}\n'.format(
              textual_id, canonical_textual_id, len(data), sha1,
              desc))
    else:
      output.write(
          'Entry(id={}, canonical_id={}, size={}, sha1={}): {}\n'.format(
              resource_id, canonical_id, len(data), sha1, desc))


def _ListMain(args):
  pak = data_pack.ReadDataPack(args.pak_file)
  if args.textual_id or args.path:
    info_dict = data_pack.ReadGrdInfo(args.pak_file)
    fmt = ''.join([
        '{id}', ' = {textual_id}' if args.textual_id else '',
        ' @ {path}' if args.path else '', '\n'
    ])
    for resource_id in sorted(pak.resources):
      item = info_dict[resource_id]
      args.output.write(
          fmt.format(textual_id=item.textual_id, id=item.id, path=item.path))
  else:
    for resource_id in sorted(pak.resources):
      args.output.write('%d\n' % resource_id)


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
  # Subparsers are required by default under Python 2.  Python 3 changed to
  # not required, but didn't include a required option until 3.7.  Setting
  # the required member works in all versions (and setting dest name).
  sub_parsers = parser.add_subparsers(dest='action')
  sub_parsers.required = True

  sub_parser = sub_parsers.add_parser('repack',
      help='Combines several .pak files into one.')
  sub_parser.add_argument('output_pak_file', help='File to create.')
  sub_parser.add_argument('input_pak_files', nargs='+',
      help='Input .pak files.')
  sub_parser.add_argument(
      '--allowlist',
      help='Path to a allowlist used to filter output pak file resource IDs.')
  sub_parser.add_argument(
      '--suppress-removed-key-output',
      action='store_true',
      help='Do not log which keys were removed by the allowlist.')
  sub_parser.add_argument('--compress', dest='compress', action='store_true',
      default=False, help='Compress output_pak_file using gzip.')
  sub_parser.set_defaults(func=_RepackMain)

  sub_parser = sub_parsers.add_parser('extract', help='Extracts pak file')
  sub_parser.add_argument('pak_file')
  sub_parser.add_argument('-o', '--output-dir', default='.',
                          help='Directory to extract to.')
  sub_parser.add_argument('--raw',
                          action='store_true',
                          help='Do not decompress when extracting.')
  sub_parser.add_argument('--brotli',
                          help='Path to brotli executable. Needed only to '
                          'decompress brotli-compressed entries. For a '
                          'chromium checkout, find in your output directory')
  sub_parser.add_argument(
      '-t',
      '--textual-id',
      action='store_true',
      help='Use textual resource ID (name) (from .info file) as filenames.')
  sub_parser.set_defaults(func=_ExtractMain)

  sub_parser = sub_parsers.add_parser('create',
      help='Creates pak file from extracted directory.')
  sub_parser.add_argument('output_pak_file', help='File to create.')
  sub_parser.add_argument('-i', '--input-dir', default='.',
                          help='Directory to create from.')
  sub_parser.set_defaults(func=_CreateMain)

  sub_parser = sub_parsers.add_parser('print',
      help='Prints all pak IDs and contents. Useful for diffing.')
  sub_parser.add_argument('pak_file')
  sub_parser.add_argument('--output', type=argparse.FileType('w'),
      default=sys.stdout,
      help='The resource list path to write (default stdout)')
  sub_parser.add_argument('--no-decode', dest='decode', action='store_false',
      default=True, help='Do not print entry data.')
  sub_parser.add_argument(
      '-t',
      '--textual-id',
      action='store_true',
      help='Print textual ID (name) (from .info file) instead of the ID.')
  sub_parser.set_defaults(func=_PrintMain)

  sub_parser = sub_parsers.add_parser('list-id',
      help='Outputs all resource IDs to a file.')
  sub_parser.add_argument('pak_file')
  sub_parser.add_argument('--output', type=argparse.FileType('w'),
      default=sys.stdout,
      help='The resource list path to write (default stdout)')
  sub_parser.add_argument(
      '-t',
      '--textual-id',
      action='store_true',
      help='Print the textual resource ID (from .info file).')
  sub_parser.add_argument(
      '-p',
      '--path',
      action='store_true',
      help='Print the resource path (from .info file).')
  sub_parser.set_defaults(func=_ListMain)

  args = parser.parse_args()
  args.func(args)


if __name__ == '__main__':
  main()
