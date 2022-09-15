#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to help start/stop a local apache http server."""

from __future__ import print_function

import logging
import optparse
import os
import subprocess
import sys
import time
import urllib

import google.path_utils
import google.platform_utils

class HttpdNotStarted(Exception): pass

def UrlIsAlive(url):
  """Checks to see if we get an http response from |url|.
  We poll the url 5 times with a 1 second delay.  If we don't
  get a reply in that time, we give up and assume the httpd
  didn't start properly.

  Args:
    url: The URL to check.
  Return:
    True if the url is alive.
  """
  wait_time = 5
  while wait_time > 0:
    try:
      response = urllib.urlopen(url)
      # Server is up and responding.
      return True
    except IOError:
      pass
    wait_time -= 1
    # Wait a second and try again.
    time.sleep(1)

  return False

def ApacheConfigDir(start_dir):
  """Returns a path to the directory holding the Apache config files."""
  return google.path_utils.FindUpward(start_dir, 'tools', 'python',
                                      'google', 'httpd_config')


def GetCygserverPath(start_dir, apache2=False):
  """Returns the path to the directory holding cygserver.exe file."""
  cygserver_path = None
  if apache2:
    cygserver_path = google.path_utils.FindUpward(start_dir, 'third_party',
                                                  'cygwin', 'usr', 'sbin')
  return cygserver_path


def StartServer(document_root=None, output_dir=None, apache2=False):
  """Starts a local server on port 8000 using the basic configuration files.

  Args:
    document_root: If present, specifies the document root for the server;
        otherwise, the filesystem's root (e.g., C:/ or /) will be used.
    output_dir: If present, specifies where to put server logs; otherwise,
        they'll be placed in the system's temp dir (e.g., $TEMP or /tmp).
    apache2: boolean if true will cause this function to configure
             for Apache 2.x as opposed to Apache 1.3.x

  Returns: the ApacheHttpd object that was created
  """
  script_dir = google.path_utils.ScriptDir()
  platform_util = google.platform_utils.PlatformUtility(script_dir)
  if not output_dir:
    output_dir = platform_util.GetTempDirectory()
  if not document_root:
    document_root = platform_util.GetFilesystemRoot()
  apache_config_dir = ApacheConfigDir(script_dir)
  if apache2:
    httpd_conf_path = os.path.join(apache_config_dir, 'httpd2.conf')
  else:
    httpd_conf_path = os.path.join(apache_config_dir, 'httpd.conf')
  mime_types_path = os.path.join(apache_config_dir, 'mime.types')
  start_cmd = platform_util.GetStartHttpdCommand(output_dir,
                                                 httpd_conf_path,
                                                 mime_types_path,
                                                 document_root,
                                                 apache2=apache2)
  stop_cmd = platform_util.GetStopHttpdCommand()
  httpd = ApacheHttpd(start_cmd, stop_cmd, [8000],
                      cygserver_path=GetCygserverPath(script_dir, apache2))
  httpd.StartServer()
  return httpd


def StopServers(apache2=False):
  """Calls the platform's stop command on a newly created server, forcing it
  to stop.

  The details depend on the behavior of the platform stop command. For example,
  it's often implemented to kill all running httpd processes, as implied by
  the name of this function.

  Args:
    apache2: boolean if true will cause this function to configure
             for Apache 2.x as opposed to Apache 1.3.x
  """
  script_dir = google.path_utils.ScriptDir()
  platform_util = google.platform_utils.PlatformUtility(script_dir)
  httpd = ApacheHttpd('', platform_util.GetStopHttpdCommand(), [],
                      cygserver_path=GetCygserverPath(script_dir, apache2))
  httpd.StopServer(force=True)


class ApacheHttpd(object):
  def __init__(self, start_command, stop_command, port_list,
               cygserver_path=None):
    """Args:
        start_command: command list to call to start the httpd
        stop_command: command list to call to stop the httpd if one has been
            started.  May kill all httpd processes running on the machine.
        port_list: list of ports expected to respond on the local machine when
            the server has been successfully started.
        cygserver_path: Path to cygserver.exe. If specified, exe will be started
            with server as well as stopped when server is stopped.
    """
    self._http_server_proc = None
    self._start_command = start_command
    self._stop_command = stop_command
    self._port_list = port_list
    self._cygserver_path = cygserver_path

  def StartServer(self):
    if self._http_server_proc:
      return
    if self._cygserver_path:
      cygserver_exe = os.path.join(self._cygserver_path, "cygserver.exe")
      cygbin = google.path_utils.FindUpward(cygserver_exe, 'third_party',
                                            'cygwin', 'bin')
      env = os.environ
      env['PATH'] += ";" + cygbin
      subprocess.Popen(cygserver_exe, env=env)
    logging.info('Starting http server')
    self._http_server_proc = subprocess.Popen(self._start_command)

    # Ensure that the server is running on all the desired ports.
    for port in self._port_list:
      if not UrlIsAlive('http://127.0.0.1:%s/' % str(port)):
        raise HttpdNotStarted('Failed to start httpd on port %s' % str(port))

  def StopServer(self, force=False):
    """If we started an httpd.exe process, or if force is True, call
    self._stop_command (passed in on init so it can be platform-dependent).
    This will presumably kill it, and may also kill any other httpd.exe
    processes that are running.
    """
    if force or self._http_server_proc:
      logging.info('Stopping http server')
      kill_proc = subprocess.Popen(self._stop_command,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
      logging.info('%s\n%s' % (kill_proc.stdout.read(),
                               kill_proc.stderr.read()))
      self._http_server_proc = None
      if self._cygserver_path:
        subprocess.Popen(["taskkill.exe", "/f", "/im", "cygserver.exe"],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)


def main():
  # Provide some command line params for starting/stopping the http server
  # manually.
  option_parser = optparse.OptionParser()
  option_parser.add_option('-k', '--server', help='Server action (start|stop)')
  option_parser.add_option('-r', '--root', help='Document root (optional)')
  option_parser.add_option('-a', '--apache2', action='store_true',
      default=False, help='Starts Apache 2 instead of Apache 1.3 (default). '
                          'Ignored on Mac (apache2 is used always)')
  options, args = option_parser.parse_args()

  if not options.server:
    print("Usage: %s -k {start|stop} [-r document_root] [--apache2]" %
          sys.argv[0])
    return 1

  document_root = None
  if options.root:
    document_root = options.root

  if 'start' == options.server:
    StartServer(document_root, apache2=options.apache2)
  else:
    StopServers(apache2=options.apache2)


if '__main__' == __name__:
  sys.exit(main())
