# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""WSGI application to manage a USB gadget.
"""

from __future__ import print_function

import datetime
import hashlib
import re
import subprocess
import sys
import time
import urllib2

from tornado import httpserver
from tornado import ioloop
from tornado import web

import default_gadget

VERSION_PATTERN = re.compile(r'.*usb_gadget-([a-z0-9]{32})\.zip')

address = None
chip = None
claimed_by = None
default = default_gadget.DefaultGadget()
gadget = None
hardware = None
interface = None
port = None


def SwitchGadget(new_gadget):
  if chip.IsConfigured():
    chip.Destroy()

  global gadget
  gadget = new_gadget
  gadget.AddStringDescriptor(3, address)
  chip.Create(gadget)


class VersionHandler(web.RequestHandler):

  def get(self):
    version = 'unpackaged'
    for path in sys.path:
      match = VERSION_PATTERN.match(path)
      if match:
        version = match.group(1)
        break

    self.write(version)


class UpdateHandler(web.RequestHandler):

  def post(self):
    fileinfo = self.request.files['file'][0]

    match = VERSION_PATTERN.match(fileinfo['filename'])
    if match is None:
      self.write('Filename must contain MD5 hash.')
      self.set_status(400)
      return

    content = fileinfo['body']
    md5sum = hashlib.md5(content).hexdigest()
    if md5sum != match.group(1):
      self.write('File hash does not match.')
      self.set_status(400)
      return

    filename = 'usb_gadget-{}.zip'.format(md5sum)
    with open(filename, 'wb') as f:
      f.write(content)

    args = ['/usr/bin/python', filename,
            '--interface', interface,
            '--port', str(port),
            '--hardware', hardware]
    if claimed_by is not None:
      args.extend(['--start-claimed', claimed_by])

    print('Reloading with version {}...'.format(md5sum))

    global http_server
    if chip.IsConfigured():
      chip.Destroy()
    http_server.stop()

    child = subprocess.Popen(args, close_fds=True)

    while True:
      child.poll()
      if child.returncode is not None:
        self.write('New package exited with error {}.'
                   .format(child.returncode))
        self.set_status(500)

        http_server = httpserver.HTTPServer(app)
        http_server.listen(port)
        SwitchGadget(gadget)
        return

      try:
        f = urllib2.urlopen('http://{}/version'.format(address))
        if f.getcode() == 200:
          # Update complete, wait 1 second to make sure buffers are flushed.
          io_loop = ioloop.IOLoop.instance()
          io_loop.add_timeout(datetime.timedelta(seconds=1), io_loop.stop)
          return
      except urllib2.URLError:
        pass
      time.sleep(0.1)


class ClaimHandler(web.RequestHandler):

  def post(self):
    global claimed_by

    if claimed_by is None:
      claimed_by = self.get_argument('session_id')
    else:
      self.write('Device is already claimed by "{}".'.format(claimed_by))
      self.set_status(403)


class UnclaimHandler(web.RequestHandler):

  def post(self):
    global claimed_by
    claimed_by = None
    if gadget != default:
      SwitchGadget(default)


class UnconfigureHandler(web.RequestHandler):

  def post(self):
    SwitchGadget(default)


class DisconnectHandler(web.RequestHandler):

  def post(self):
    if chip.IsConfigured():
      chip.Destroy()


class ReconnectHandler(web.RequestHandler):

  def post(self):
    if not chip.IsConfigured():
      chip.Create(gadget)


app = web.Application([
    (r'/version', VersionHandler),
    (r'/update', UpdateHandler),
    (r'/claim', ClaimHandler),
    (r'/unclaim', UnclaimHandler),
    (r'/unconfigure', UnconfigureHandler),
    (r'/disconnect', DisconnectHandler),
    (r'/reconnect', ReconnectHandler),
])

http_server = httpserver.HTTPServer(app)
