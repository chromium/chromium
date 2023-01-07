# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Platform-specific utility methods shared by several scripts."""

import os
import re
import subprocess
import sys

import google.path_utils

# Cache a single cygpath process for use throughout, even across instances of
# the PlatformUtility class.
_cygpath_proc = None

class PlatformUtility(object):
  def __init__(self, base_dir):
    """Args:
         base_dir: a directory above which third_party/cygwin can be found,
             used to locate the cygpath executable for path conversions.
    """
    self._cygwin_root = None
    self._base_dir = base_dir

  def _CygwinRoot(self):
    """Returns the full path to third_party/cygwin/."""
    if not self._cygwin_root:
      self._cygwin_root = google.path_utils.FindUpward(self._base_dir,
                                                       'third_party', 'cygwin')
    return self._cygwin_root

  def _PathToExecutable(self, executable):
    """Returns the full path to an executable in Cygwin's bin dir."""
    return os.path.join(self._CygwinRoot(), 'bin', executable)

  def GetAbsolutePath(self, path, force=False):
    """Returns an absolute windows path. If platform is cygwin, converts it to
    windows style using cygpath.

    For performance reasons, we use a single cygpath process, shared among all
    instances of this class. Otherwise Python can run out of file handles.
    """
    if not force and sys.platform != "cygwin":
      return os.path.abspath(path)
    global _cygpath_proc
    if not _cygpath_proc:
      cygpath_command = [self._PathToExecutable("cygpath.exe"),
                         "-a", "-m", "-f", "-"]
      _cygpath_proc = subprocess.Popen(cygpath_command,
                                       stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE)
    _cygpath_proc.stdin.write(path + "\n")
    return _cygpath_proc.stdout.readline().rstrip()

  def GetFilesystemRoot(self):
    """Returns the root directory of the file system."""
    return os.environ['SYSTEMDRIVE'] + '\\'

  def GetTempDirectory(self):
    """Returns the file system's base temp directory, or the filesystem root
    if the standard temp directory can't be determined.

    Note that this does not use a random subdirectory, so it's not
    intrinsically secure.  If you need a secure subdir, use the tempfile
    package.
    """
    return os.environ.get('TEMP', self.GetFilesystemRoot())

  def FilenameToUri(self, path, use_http=False, use_ssl=False, port=8000):
    """Convert a Windows style path to a URI.

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
      path = path.replace("\\", "/")
      return "%s://127.0.0.1:%s/%s" % (protocol, str(port), path)
    return "file:///" + self.GetAbsolutePath(path)

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
               command for Apache 2.x as opposed to Apache 1.3.x
    """

    if document_root:
      document_root = GetCygwinPath(document_root)
    exe_name = "httpd"
    cert_file = ""
    if apache2:
      exe_name = "httpd2"
      cert_file = google.path_utils.FindUpward(self._base_dir, 'tools',
                                               'python', 'google',
                                               'httpd_config', 'httpd2.pem')
    httpd_vars = {
      "httpd_executable_path": GetCygwinPath(
          os.path.join(self._CygwinRoot(), "usr", "sbin", exe_name)),
      "httpd_conf_path": GetCygwinPath(httpd_conf_path),
      "ssl_certificate_file": GetCygwinPath(cert_file),
      "document_root" : document_root,
      "server_root": GetCygwinPath(os.path.join(self._CygwinRoot(), "usr")),
      "mime_types_path": GetCygwinPath(mime_types_path),
      "output_dir": GetCygwinPath(output_dir),
      "bindir": GetCygwinPath(os.path.join(self._CygwinRoot(), "bin")),
      "user": os.environ.get("USERNAME", os.environ.get("USER", "")),
    }
    if not httpd_vars["user"]:
      # Failed to get the username from the environment; use whoami.exe
      # instead.
      proc = subprocess.Popen(self._PathToExecutable("whoami.exe"),
                              stdout=subprocess.PIPE)
      httpd_vars["user"] = proc.stdout.read().strip()

    if not httpd_vars["user"]:
      raise Exception("Failed to get username.")

    google.path_utils.MaybeMakeDirectory(output_dir)

    # We have to wrap the command in bash because the cygwin environment
    # is required for httpd to run.
    # -C: process directive before reading config files
    # -c: process directive after reading config files
    # Apache wouldn't run CGIs with permissions==700 unless we add
    # -c User "<username>"
    bash = self._PathToExecutable("bash.exe")
    httpd_cmd_string = (
      ' PATH=%(bindir)s %(httpd_executable_path)s'
      ' -f %(httpd_conf_path)s'
      ' -c \'TypesConfig "%(mime_types_path)s"\''
      ' -c \'CustomLog "%(output_dir)s/access_log.txt" common\''
      ' -c \'ErrorLog "%(output_dir)s/error_log.txt"\''
      ' -c \'PidFile "%(output_dir)s/httpd.pid"\''
      ' -C \'User "%(user)s"\''
      ' -C \'ServerRoot "%(server_root)s"\''
    )
    if apache2:
      httpd_cmd_string = ('export CYGWIN=server;' + httpd_cmd_string +
          ' -c \'SSLCertificateFile "%(ssl_certificate_file)s"\'')
    if document_root:
      httpd_cmd_string += ' -C \'DocumentRoot "%(document_root)s"\''

    httpd_cmd = [bash, "-c", httpd_cmd_string % httpd_vars]
    return httpd_cmd

  def GetStopHttpdCommand(self):
    """Returns a list of strings that contains the command line+args needed to
    stop the http server used in the http tests.
    """
    # Force kill (/f) *all* httpd processes.  This has the side effect of
    # killing httpd processes that we didn't start.
    return ["taskkill.exe", "/f", "/im", "httpd*"]

###########################################################################
# This method is specific to windows, expected to be used only by *_win.py
# files.

def GetCygwinPath(path):
  """Convert a Windows path to a cygwin path.

  The cygpath utility insists on converting paths that it thinks are Cygwin
  root paths to what it thinks the correct roots are.  So paths such as
  "C:\b\slave\webkit-release-kjs\build\third_party\cygwin\bin" are converted to
  plain "/usr/bin".  To avoid this, we do the conversion manually.

  The path is expected to be an absolute path, on any drive.
  """
  drive_regexp = re.compile(r'([a-z]):[/\\]', re.IGNORECASE)
  def LowerDrive(matchobj):
    return '/cygdrive/%s/' % matchobj.group(1).lower()
  path = drive_regexp.sub(LowerDrive, path)
  return path.replace('\\', '/')
