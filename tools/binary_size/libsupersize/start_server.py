# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a server to let the user interact with supersize using a web UI."""

import BaseHTTPServer
import logging
import os
import SimpleHTTPServer


class SupersizeHTTPRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler,
                                  object):
  # Directory to serve files from
  serve_from = None
  # Path to data file
  data_file_path = None
  before_file_path = None

  #override
  def translate_path(self, path):
    f = super(SupersizeHTTPRequestHandler, self).translate_path(path)
    relative_path = os.path.relpath(f, os.getcwd())
    if relative_path in ['data.ndjson', 'data.size']:
      return SupersizeHTTPRequestHandler.data_file_path
    if relative_path == 'before.size':
      return SupersizeHTTPRequestHandler.before_file_path
    else:
      return os.path.join(SupersizeHTTPRequestHandler.serve_from, relative_path)


def AddArguments(parser):
  parser.add_argument('report_file',
                      help='Path to a custom html_report data file to load.')
  parser.add_argument(
      '-b',
      '--before_file',
      type=str,
      default='',
      help=('Path to a "before" .size file to diff against. If present, '
            'report_file should also be a .size file.'))
  parser.add_argument('-p', '--port', type=int, default=8000,
                      help='Port for the HTTP server')
  parser.add_argument('-a', '--address', default='localhost',
                      help='Address for the HTTP server')


def Run(args, _parser):
  logging.info('Starting server')
  server_addr = (args.address, args.port)

  static_files = os.path.join(os.path.dirname(__file__), 'static')

  SupersizeHTTPRequestHandler.serve_from = static_files
  SupersizeHTTPRequestHandler.data_file_path = args.report_file
  SupersizeHTTPRequestHandler.before_file_path = args.before_file
  SupersizeHTTPRequestHandler.extensions_map['.wasm'] = 'application/wasm'
  httpd = BaseHTTPServer.HTTPServer(server_addr, SupersizeHTTPRequestHandler)

  sa = httpd.socket.getsockname()
  is_ndjson = args.report_file.endswith('ndjson')
  data_file = 'data.ndjson' if is_ndjson else 'data.size'
  maybe_before_file = '&before_url=before.size' if args.before_file else ''
  logging.warning(
      'Server ready at http://%s:%d/viewer.html?load_url=' + data_file +
      maybe_before_file, sa[0], sa[1])
  httpd.serve_forever()
