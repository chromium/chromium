#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility to package and upload the USB gadget framework.
"""

import argparse
import hashlib
import io
import os
import zipfile


try:
  from urllib.request import Request, urlopen
except ImportError:  # For Py2 compatibility
  from urllib2 import Request, urlopen


def MakeZip(directory=None, files=None, mtime=(2022, 11, 11, 11, 11, 11)):
  """Construct a zip file.

  Args:
    directory: Include Python source files from this directory
    files: Include these files
    mtime: A fixed modification time to assign all files in the zip file, for
           deterministic output.

  Returns:
    A tuple of the buffer containing the zip file and its MD5 hash.
  """
  buf = io.BytesIO()
  archive = zipfile.PyZipFile(buf, 'w')
  if directory is not None:
    archive.writepy(directory)
  if files is not None:
    for path in files:
      with open(path, 'rb') as f:
        file_info = zipfile.ZipInfo(os.path.basename(path), mtime)
        file_contents = f.read()
        archive.writestr(file_info, file_contents)
  archive.close()
  content = buf.getvalue()
  buf.close()
  md5 = hashlib.md5(content).hexdigest()
  return content, md5


def EncodeBody(filename, buf):
  return b'\r\n'.join([
      b'--foo',
      b'Content-Disposition: form-data; name="file"; filename="%s"' %
      filename,
      b'Content-Type: application/octet-stream',
      b'',
      buf,
      b'--foo--',
      b''
  ])


def UploadZip(content, md5, host):
  filename = b'usb_gadget-%s.zip' % md5.encode('utf-8')
  req = Request(url='http://{}/update'.format(host),
                data=EncodeBody(filename, content))
  req.add_header('Content-Type', 'multipart/form-data; boundary=foo')
  urlopen(req)


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
    with open(args.hash_file, 'w') as hash_file:
      hash_file.write(md5)
  if args.upload:
    UploadZip(content, md5, args.upload)


if __name__ == '__main__':
  main()
