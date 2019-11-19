#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Test server for testing Negotiate Authorization

This is a minimal dummy server for testing HTTP Negotiate Authorization.
It is currently only used for manual testing.

To use:
- Start on host.
- Use chrome://inspect to forward port 8080 from the target device to
the host.
- From Chrome on the target connect to localhost:8080.
- If HTTP Negotiate is working correctly the page should load as
"Talked to SPNEGO Authenticator"

Please see //tools/android/kerberos/README.md for detailed instructions.

"""

# TODO(dgn) Replace with an EmbeddedTestServer based server in the test apk once
# the java version is ready. See http://crbug.com/488192

from __future__ import print_function

import time
import BaseHTTPServer


HOST_NAME = 'localhost'
PORT_NUMBER = 8080

SUCCESS_HTML = ('<html><head>'
                '<title>SPNEGO Negotiation completed!</title>'
                '</head><body>'
                '<p><b><big>Talked to SPNEGO Authenticator</big></b></p>'
                '</body></html>')

FAILURE_HTML = ('<html><head>'
                '<title>Not Authorized</title>'
                '</head><body>'
                '<p><big>Did not talk to SPNEGO Authenticator</big></p>'
                '</body></html>')

WRONG_HEADER_HTML = ('<html><head>'
                     '<title>Not Authorized</title>'
                     '</head><body>'
                     '<p><big>Bad header value. Found "%s"</big></p>'
                     '</body></html>')


class MyHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  def do_HEAD(self):
    self.send_response(401, 'negotiate')
    self.send_header('WWW-Authenticate', 'negotiate')
    self.end_headers()

  def do_GET(self):
    """Respond to a GET request."""
    print('Path: ' + self.path)
    print(self.headers)
    auth_header = self.headers.getheader('Authorization')

    if not auth_header:
      self.send_response(401)
      self.send_header('WWW-Authenticate', 'negotiate')
      self.send_header('Content-type', 'text/html')
      self.end_headers()
      self.wfile.write(FAILURE_HTML)
    elif not auth_header.startswith('Negotiate '):
      self.send_response(403)
      self.send_header('Content-type', 'text/html')
      self.end_headers()
      self.wfile.write(WRONG_HEADER_HTML % auth_header)
    else:
      self.send_response(200)
      self.send_header('Content-type', 'text/html')
      self.end_headers()
      self.wfile.write(SUCCESS_HTML)


if __name__ == '__main__':
  server_class = BaseHTTPServer.HTTPServer
  httpd = server_class((HOST_NAME, PORT_NUMBER), MyHandler)
  print('%s Server Starts - %s:%s' % (time.asctime(), HOST_NAME, PORT_NUMBER))
  try:
    httpd.serve_forever()
  except KeyboardInterrupt:
    pass
  httpd.server_close()
  print('%s Server Stops - %s:%s' % (time.asctime(), HOST_NAME, PORT_NUMBER))
