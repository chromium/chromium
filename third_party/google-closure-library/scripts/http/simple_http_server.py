#!/usr/bin/env python

# Copyright 2018 The Closure Library Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Simple HTTP server.
"""

from __future__ import print_function
import SimpleHTTPServer
import SocketServer

PORT = 8080


# Simple server to respond to both POST and GET requests. POST requests will
# just respond as normal GETs.
class ServerHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):

  def do_GET(self):
    SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

  def do_POST(self):
    SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)


Handler = ServerHandler

# Allows use to restart server immediately after restarting it.
SocketServer.ThreadingTCPServer.allow_reuse_address = True

httpd = SocketServer.TCPServer(("", PORT), Handler)

print("Serving at: http://%s:%s" % ("localhost", PORT))
httpd.serve_forever()
