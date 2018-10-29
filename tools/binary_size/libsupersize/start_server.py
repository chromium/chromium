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

  #override
  def translate_path(self, path):
    f = super(SupersizeHTTPRequestHandler, self).translate_path(path)
    relative_path = os.path.relpath(f, os.getcwd())
    if relative_path == 'data.ndjson':
      return SupersizeHTTPRequestHandler.data_file_path
    else:
      return os.path.join(SupersizeHTTPRequestHandler.serve_from, relative_path)


def AddArguments(parser):
  parser.add_argument('report_file',
                      help='Path to a custom html_report data file to load.')
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
  httpd = BaseHTTPServer.HTTPServer(server_addr, SupersizeHTTPRequestHandler)

  sa = httpd.socket.getsockname()
  logging.warning(
      'Server ready at http://%s:%d/viewer.html?load_url=data.ndjson',
      sa[0], sa[1])
  httpd.serve_forever()
