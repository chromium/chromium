#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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

debugger_port = 0
remote_port = 7777


class CORSRequestHandler(SimpleHTTPRequestHandler):
  def do_GET(self):
    if (self.path == "/discover.html"):
      try:
        contents = urllib.request.urlopen("http://localhost:" +
                                          str(remote_port) +
                                          "/json/version").read()
        self.send_response(200)

      except Exception:
        contents =\
        "\n Cannot connect to remote discovery page on localhost:"+\
             str(remote_port) +\
            "\n check for target command line parameter: \n" +\
            "        --remote-debugging-port=" + str(remote_port) +\
            "\n and if the target is a remote DUT  tunnel forwarding"+\
              " is required from local to remote : " + \
            "\n      ssh root@$DUT_IP -L " + \
            str(remote_port)+":localhost:" + str(remote_port)
        contents = bytes(contents, 'UTF-8')
        # Used error code 206 to prevent console logs every time
        # connection is unsuccessful.
        self.send_response(206)

      self.send_header("Content-type", "text/html")
      self.send_header("Content-length", len(contents))
      self.end_headers()
      self.wfile.write(contents)
    else:
      SimpleHTTPRequestHandler.do_GET(self)


if __name__ == '__main__':
  try:
    remote_port = int(sys.argv[1]) if len(sys.argv) > 1 else remote_port
    debugger_port = int(sys.argv[2]) if len(sys.argv) > 2 else debugger_port
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
    webbrowser.open("http://localhost:" + str(debugger_port) + "/app.html",
                    new=1,
                    autoraise=True)
    tpc_server.serve_forever()
  except KeyboardInterrupt:
    tpc_server.server_close()
    sys.exit()
