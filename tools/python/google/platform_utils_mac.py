# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Platform-specific utility methods shared by several scripts."""

import os
import subprocess

import google.path_utils


class PlatformUtility(object):
  def __init__(self, base_dir):
    """Args:
         base_dir: the base dir for running tests.
    """
    self._base_dir = base_dir
    self._httpd_cmd_string = None  # used for starting/stopping httpd
    self._bash = "/bin/bash"

  def _UnixRoot(self):
    """Returns the path to root."""
    return "/"

  def GetFilesystemRoot(self):
    """Returns the root directory of the file system."""
    return self._UnixRoot()

  def GetTempDirectory(self):
    """Returns the file system temp directory

    Note that this does not use a random subdirectory, so it's not
    intrinsically secure.  If you need a secure subdir, use the tempfile
    package.
    """
    return os.getenv("TMPDIR", "/tmp")

  def FilenameToUri(self, path, use_http=False, use_ssl=False, port=8000):
    """Convert a filesystem path to a URI.

    Args:
      path: For an http URI, the path relative to the httpd server's
          DocumentRoot; for a file URI, the full path to the file.
      use_http: if True, returns a URI of the form http://127.0.0.1:8000/.
          If False, returns a file:/// URI.
      use_ssl: if True, returns HTTPS URL (https://127.0.0.1:8000/).
          This parameter is ignored if use_http=False.
      port: The port number to append when returning an HTTP URI
    """
    if use_http:
      protocol = 'http'
      if use_ssl:
        protocol = 'https'
      return "%s://127.0.0.1:%d/%s" % (protocol, port, path)
    return "file://" + path

  def GetStartHttpdCommand(self, output_dir,
                           httpd_conf_path, mime_types_path,
                           document_root=None, apache2=False):
    """Prepares the config file and output directory to start an httpd server.
    Returns a list of strings containing the server's command line+args.

    Args:
      output_dir: the path to the server's output directory, for log files.
          It will be created if necessary.
      httpd_conf_path: full path to the httpd.conf file to be used.
      mime_types_path: full path to the mime.types file to be used.
      document_root: full path to the DocumentRoot.  If None, the DocumentRoot
          from the httpd.conf file will be used.  Note that the httpd.conf
          file alongside this script does not specify any DocumentRoot, so if
          you're using that one, be sure to specify a document_root here.
      apache2: boolean if true will cause this function to return start
               command for Apache 2.x as opposed to Apache 1.3.x. This flag
               is ignored on Mac (but preserved here for compatibility in
               function signature with win), where httpd2 is used always
    """

    exe_name = "httpd"
    cert_file = google.path_utils.FindUpward(self._base_dir, 'tools',
                                             'python', 'google',
                                             'httpd_config', 'httpd2.pem')
    ssl_enabled = os.path.exists('/etc/apache2/mods-enabled/ssl.conf')

    httpd_vars = {
      "httpd_executable_path":
          os.path.join(self._UnixRoot(), "usr", "sbin", exe_name),
      "httpd_conf_path": httpd_conf_path,
      "ssl_certificate_file": cert_file,
      "document_root" : document_root,
      "server_root": os.path.join(self._UnixRoot(), "usr"),
      "mime_types_path": mime_types_path,
      "output_dir": output_dir,
      "ssl_mutex": "file:"+os.path.join(output_dir, "ssl_mutex"),
      "user": os.environ.get("USER", "#%d" % os.geteuid()),
      "lock_file": os.path.join(output_dir, "accept.lock"),
    }

    google.path_utils.MaybeMakeDirectory(output_dir)

    # We have to wrap the command in bash
    # -C: process directive before reading config files
    # -c: process directive after reading config files
    # Apache wouldn't run CGIs with permissions==700 unless we add
    # -c User "<username>"
    httpd_cmd_string = (
      '%(httpd_executable_path)s'
      ' -f %(httpd_conf_path)s'
      ' -c \'TypesConfig "%(mime_types_path)s"\''
      ' -c \'CustomLog "%(output_dir)s/access_log.txt" common\''
      ' -c \'ErrorLog "%(output_dir)s/error_log.txt"\''
      ' -c \'PidFile "%(output_dir)s/httpd.pid"\''
      ' -C \'User "%(user)s"\''
      ' -C \'ServerRoot "%(server_root)s"\''
      ' -c \'LockFile "%(lock_file)s"\''
    )

    if document_root:
      httpd_cmd_string += ' -C \'DocumentRoot "%(document_root)s"\''

    if ssl_enabled:
      httpd_cmd_string += (
        ' -c \'SSLCertificateFile "%(ssl_certificate_file)s"\''
        ' -c \'SSLMutex "%(ssl_mutex)s"\''
      )

    # Save a copy of httpd_cmd_string to use for stopping httpd
    self._httpd_cmd_string = httpd_cmd_string % httpd_vars

    httpd_cmd = [self._bash, "-c", self._httpd_cmd_string]
    return httpd_cmd

  def GetStopHttpdCommand(self):
    """Returns a list of strings that contains the command line+args needed to
    stop the http server used in the http tests.

    This tries to fetch the pid of httpd (if available) and returns the
    command to kill it. If pid is not available, kill all httpd processes
    """

    if not self._httpd_cmd_string:
      return ["true"]   # Haven't been asked for the start cmd yet. Just pass.
    # Add a sleep after the shutdown because sometimes it takes some time for
    # the port to be available again.
    return [self._bash, "-c", self._httpd_cmd_string + ' -k stop && sleep 5']
