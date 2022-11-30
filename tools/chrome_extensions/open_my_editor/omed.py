#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
HTTP server for handling requests to open files.
"""

from bottle import Bottle, install, get, response, request, run
import logging
import sys
import sh

logfile = "/tmp/omed.log"
# Get it a path for log.
if len(sys.argv) == 2:
  logfile = sys.argv[1]

logging.basicConfig(filename=logfile, level=logging.INFO)
logger = logging.getLogger('omed')

def log(func):
  def wrapper(*args, **kwargs):
    logger.info(
        '%s %s %s %s' %
        (request.remote_addr, request.method, request.url, response.status))

    req = func(*args, **kwargs)
    return req
  return wrapper

install(log)

@get('/file')
def open_file():
  filepath = request.query.f
  line = request.query.l

  logger.info("open file: " + filepath + ":" + line)

  sh.myeditor("-f", filepath, "-l", line)
  return

@get('/files')
def open_files():
  filepaths = request.query.f

  logger.info("open files: " + filepaths)

  sh.myeditor("-m", filepaths)
  return

run(port=8989, host='127.0.0.1')
