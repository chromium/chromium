#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import sys

if (sys.version_info < (3, )):
  print("FAILURE. Python 3 or greater required. Please run with \"python3\".")
  sys.exit(7)

from http.server import HTTPServer, SimpleHTTPRequestHandler, test
from functools import partial
import sys
import urllib.request
import socketserver
import webbrowser
import os
import argparse

debugger_port = 0
remote_port = 7777


class CORSRequestHandler(SimpleHTTPRequestHandler):
  def do_GET(self):
    if (self.path == "/discover.json"):
      remote_discovery_url = "http://localhost:{remote_port}/json/version".format(
          remote_port=remote_port)
      try:
        contents = urllib.request.urlopen(remote_discovery_url).read()

      except Exception:
        output = {
            "error":
            '''\
Cannot connect to remote discovery page on:
        {remote_discovery_url}
Check for target command line parameter:
        --remote-debugging-port={remote_port}
If the target is a remote DUT tunnel forwarding is required
from local to remote:
        ssh root@$DUT_IP -L {remote_port}:localhost:{remote_port}
            '''.format(remote_port=remote_port,
                       remote_discovery_url=remote_discovery_url)
        }
        contents = json.dumps(output)
        contents = bytes(contents, 'UTF-8')

      self.send_response(200)
      self.send_header("Content-type", "application/json")
      self.send_header("Content-length", len(contents))
      self.end_headers()
      self.wfile.write(contents)
    else:
      SimpleHTTPRequestHandler.do_GET(self)


if __name__ == '__main__':
  try:
    parser = argparse.ArgumentParser()
    parser.add_argument("remote_port", default=remote_port, type=int)
    parser.add_argument("debugger_port",
                        default=debugger_port,
                        type=int,
                        nargs='?')
    parser.add_argument("--nolaunch",
                        help="Disables launching in browser window",
                        action="store_true")
    args = parser.parse_args()
    remote_port = args.remote_port
    debugger_port = args.debugger_port
    # Creates a partial object that will behave like a function called with args
    # and kwargs, while overriding directory with the given path.
    Handler = partial(CORSRequestHandler,
                      directory=os.path.dirname(os.path.abspath(__file__)))
    socketserver.TCPServer.allow_reuse_address = True
    tpc_server = socketserver.TCPServer(("", debugger_port), Handler)
    # If socket is not specified it was assigned so we must grab it.
    if (debugger_port == 0):
      debugger_port = tpc_server.server_address[1]
    print("Server running on port", debugger_port)
    if not args.nolaunch:
      webbrowser.open("http://localhost:" + str(debugger_port) + "/app.html",
                      new=1,
                      autoraise=True)
    tpc_server.serve_forever()
  except KeyboardInterrupt:
    tpc_server.server_close()
    sys.exit()
