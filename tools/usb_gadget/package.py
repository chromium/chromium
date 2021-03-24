#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility to package and upload the USB gadget framework.
"""

import argparse
import hashlib
import os
import StringIO
import urllib2
import zipfile


def MakeZip(directory=None, files=None):
  """Construct a zip file.

  Args:
    directory: Include Python source files from this directory
    files: Include these files

  Returns:
    A tuple of the buffer containing the zip file and its MD5 hash.
  """
  buf = StringIO.StringIO()
  archive = zipfile.PyZipFile(buf, 'w')
  if directory is not None:
    archive.writepy(directory)
  if files is not None:
    for f in files:
      archive.write(f, os.path.basename(f))
  archive.close()
  content = buf.getvalue()
  buf.close()
  md5 = hashlib.md5(content).hexdigest()
  return content, md5


def EncodeBody(filename, buf):
  return '\r\n'.join([
      '--foo',
      'Content-Disposition: form-data; name="file"; filename="{}"'
      .format(filename),
      'Content-Type: application/octet-stream',
      '',
      buf,
      '--foo--',
      ''
  ])


def UploadZip(content, md5, host):
  filename = 'usb_gadget-{}.zip'.format(md5)
  req = urllib2.Request(url='http://{}/update'.format(host),
                        data=EncodeBody(filename, content))
  req.add_header('Content-Type', 'multipart/form-data; boundary=foo')
  urllib2.urlopen(req)


def main():
  parser = argparse.ArgumentParser(
      description='Package (and upload) the USB gadget framework.')
  parser.add_argument(
      '--dir', type=str, metavar='DIR',
      help='package all Python files from DIR')
  parser.add_argument(
      '--zip-file', type=str, metavar='FILE',
      help='save package as FILE')
  parser.add_argument(
      '--hash-file', type=str, metavar='FILE',
      help='save package hash as FILE')
  parser.add_argument(
      '--upload', type=str, metavar='HOST[:PORT]',
      help='upload package to target system')
  parser.add_argument(
      'files', metavar='FILE', type=str, nargs='*',
      help='source files')

  args = parser.parse_args()

  content, md5 = MakeZip(directory=args.dir, files=args.files)
  if args.zip_file:
    with open(args.zip_file, 'wb') as zip_file:
      zip_file.write(content)
  if args.hash_file:
    with open(args.hash_file, 'wb') as hash_file:
      hash_file.write(md5)
  if args.upload:
    UploadZip(content, md5, args.upload)


if __name__ == '__main__':
  main()
