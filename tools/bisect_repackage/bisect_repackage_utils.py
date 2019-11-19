# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Set of basic operations/utilities that are used by repacakging tool.

These functions were mostly imported from build/scripts/common/chromium_utils
and build/scripts/common/slave_utils.
"""

from __future__ import print_function

import errno
import os
import re
import shutil
import subprocess
import sys
import time
import zipfile


CREDENTIAL_ERROR_MESSAGE = ('You are attempting to access protected data with '
                            'no configured credentials')


class ExternalError(Exception):
  pass


def IsWindows():
  return sys.platform == 'cygwin' or sys.platform.startswith('win')


def IsLinux():
  return sys.platform.startswith('linux')


def IsMac():
  return sys.platform.startswith('darwin')

WIN_LINK_FUNC = None

try:
  if sys.platform.startswith('win'):
    import ctypes
    # There's 4 possibilities on Windows for links:
    # 1. Symbolic file links;
    # 2. Symbolic directory links;
    # 3. Hardlinked files;
    # 4. Junctioned directories.
    # (Hardlinked directories don't really exist.)
    #
    # 7-Zip does not handle symbolic file links as we want (it puts the
    # content of the link, not what it refers to, and reports "CRC Error" on
    # extraction). It does work as expected for symbolic directory links.
    # Because the majority of the large files are in the root of the staging
    # directory, we do however need to handle file links, so we do this with
    # hardlinking. Junctioning requires a huge whack of code, so we take the
    # slightly odd tactic of using #2 and #3, but not #1 and #4. That is,
    # hardlinks for files, but symbolic links for directories.
    def _WIN_LINK_FUNC(src, dst):
      print('linking %s -> %s' % (src, dst))
      if os.path.isdir(src):
        if not ctypes.windll.kernel32.CreateSymbolicLinkA(
            str(dst), str(os.path.abspath(src)), 1):
          raise ctypes.WinError()
      else:
        if not ctypes.windll.kernel32.CreateHardLinkA(str(dst), str(src), 0):
          raise ctypes.WinError()
    WIN_LINK_FUNC = _WIN_LINK_FUNC
except ImportError:
  # If we don't have ctypes or aren't on Windows, leave WIN_LINK_FUNC as None.
  pass


class PathNotFound(Exception):
  pass


def IsGitCommitHash(regex_match):
  """Checks if match is correct SHA1 hash."""
  matched_re = re.match(r'^[0-9,A-F]{40}$', regex_match.upper())
  if matched_re: return True
  return False


def IsCommitPosition(regex_match):
  """Checks if match is correct revision(Cp number) format."""
  matched_re = re.match(r'^[0-9]{6}$', regex_match)
  if matched_re: return True
  return False


def MaybeMakeDirectory(*path):
  """Creates an entire path, if it doesn't already exist."""
  file_path = os.path.join(*path)
  try:
    os.makedirs(file_path)
  except OSError, e:
    if e.errno != errno.EEXIST:
      raise


def RemovePath(*path):
  """Removes the file or directory at 'path', if it exists."""
  file_path = os.path.join(*path)
  if os.path.exists(file_path):
    if os.path.isdir(file_path):
      RemoveDirectory(file_path)
    else:
      RemoveFile(file_path)


def MoveFile(path, new_path):
  """Moves the file located at 'path' to 'new_path', if it exists."""
  try:
    RemoveFile(new_path)
    os.rename(path, new_path)
  except OSError, e:
    if e.errno != errno.ENOENT:
      raise


def RemoveFile(*path):
  """Removes the file located at 'path', if it exists."""
  file_path = os.path.join(*path)
  try:
    os.remove(file_path)
  except OSError, e:
    if e.errno != errno.ENOENT:
      raise


def CheckDepotToolsInPath():
  delimiter = ';' if sys.platform.startswith('win') else ':'
  path_list = os.environ['PATH'].split(delimiter)
  for path in path_list:
    if path.rstrip(os.path.sep).endswith('depot_tools'):
      return path
  return None


def RunGsutilCommand(args):
  gsutil_path = CheckDepotToolsInPath()
  if gsutil_path is None:
    print ('Follow the instructions in this document '
           'http://dev.chromium.org/developers/how-tos/install-depot-tools'
           ' to install depot_tools and then try again.')
    sys.exit(1)
  gsutil_path = os.path.join(gsutil_path, 'third_party', 'gsutil', 'gsutil')
  gsutil = subprocess.Popen([sys.executable, gsutil_path] + args,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            env=None)
  stdout, stderr = gsutil.communicate()
  if gsutil.returncode:
    if (re.findall(r'status[ |=]40[1|3]', stderr) or
        stderr.startswith(CREDENTIAL_ERROR_MESSAGE)):
      print('Follow these steps to configure your credentials and try'
            ' running the bisect-builds.py again.:\n'
            '  1. Run "python %s config" and follow its instructions.\n'
            '  2. If you have a @google.com account, use that account.\n'
            '  3. For the project-id, just enter 0.' % gsutil_path)
      sys.exit(1)
    else:
      raise Exception('Error running the gsutil command: %s' % stderr)
  return stdout


def GSutilList(bucket):
  query = '%s/' %(bucket)
  stdout = RunGsutilCommand(['ls', query])
  return [url[len(query):].strip('/') for url in stdout.splitlines()]


def GSUtilDownloadFile(src, dst):
  command = ['cp', src, dst]
  return RunGsutilCommand(command)


def GSUtilCopy(source, dest):
  if not source.startswith('gs://') and not source.startswith('file://'):
    source = 'file://' + source
  if not dest.startswith('gs://') and not dest.startswith('file://'):
    dest = 'file://' + dest
  command = ['cp']
  command.extend([source, dest])
  return RunGsutilCommand(command)


def RunCommand(cmd, cwd=None):
  """Runs the given command and returns the exit code.

  Args:
    cmd: list of command arguments.
    cwd: working directory to execute the command, or None if the current
         working directory should be used.

  Returns:
    The exit code of the command.
  """
  process = subprocess.Popen(cmd, cwd=cwd)
  process.wait()
  return process.returncode


def CopyFileToDir(src_path, dest_dir, dest_fn=None, link_ok=False):
  """Copies the file found at src_path to the dest_dir directory, with metadata.

  If dest_fn is specified, the src_path is copied to that name in dest_dir,
  otherwise it is copied to a file of the same name.

  Raises PathNotFound if either the file or the directory is not found.
  """
  # Verify the file and directory separately so we can tell them apart and
  # raise PathNotFound rather than shutil.copyfile's IOError.
  if not os.path.isfile(src_path):
    raise PathNotFound('Unable to find file %s' % src_path)
  if not os.path.isdir(dest_dir):
    raise PathNotFound('Unable to find dir %s' % dest_dir)
  src_file = os.path.basename(src_path)
  if dest_fn:
    # If we have ctypes and the caller doesn't mind links, use that to
    # try to make the copy faster on Windows. http://crbug.com/418702.
    if link_ok and WIN_LINK_FUNC:
      WIN_LINK_FUNC(src_path, os.path.join(dest_dir, dest_fn))
    else:
      shutil.copy2(src_path, os.path.join(dest_dir, dest_fn))
  else:
    shutil.copy2(src_path, os.path.join(dest_dir, src_file))


def RemoveDirectory(*path):
  """Recursively removes a directory, even if it's marked read-only.

  Remove the directory located at *path, if it exists.

  shutil.rmtree() doesn't work on Windows if any of the files or directories
  are read-only, which svn repositories and some .svn files are.  We need to
  be able to force the files to be writable (i.e., deletable) as we traverse
  the tree.

  Even with all this, Windows still sometimes fails to delete a file, citing
  a permission error (maybe something to do with antivirus scans or disk
  indexing).  The best suggestion any of the user forums had was to wait a
  bit and try again, so we do that too.  It's hand-waving, but sometimes it
  works. :/
  """
  file_path = os.path.join(*path)
  if not os.path.exists(file_path):
    return

  if sys.platform == 'win32':
    # Give up and use cmd.exe's rd command.
    file_path = os.path.normcase(file_path)
    for _ in xrange(3):
      print('RemoveDirectory running %s' % (' '.join(
          ['cmd.exe', '/c', 'rd', '/q', '/s', file_path])))
      if not subprocess.call(['cmd.exe', '/c', 'rd', '/q', '/s', file_path]):
        break
      print('  Failed')
      time.sleep(3)
    return

  def RemoveWithRetry_non_win(rmfunc, path):
    if os.path.islink(path):
      return os.remove(path)
    else:
      return rmfunc(path)

  remove_with_retry = RemoveWithRetry_non_win

  def RmTreeOnError(function, path, excinfo):
    r"""This works around a problem whereby python 2.x on Windows has no ability
    to check for symbolic links.  os.path.islink always returns False.  But
    shutil.rmtree will fail if invoked on a symbolic link whose target was
    deleted before the link.  E.g., reproduce like this:
    > mkdir test
    > mkdir test\1
    > mklink /D test\current test\1
    > python -c "import chromium_utils; chromium_utils.RemoveDirectory('test')"
    To avoid this issue, we pass this error-handling function to rmtree.  If
    we see the exact sort of failure, we ignore it.  All other failures we re-
    raise.
    """

    exception_type = excinfo[0]
    exception_value = excinfo[1]
    # If shutil.rmtree encounters a symbolic link on Windows, os.listdir will
    # fail with a WindowsError exception with an ENOENT errno (i.e., file not
    # found).  We'll ignore that error.  Note that WindowsError is not defined
    # for non-Windows platforms, so we use OSError (of which it is a subclass)
    # to avoid lint complaints about an undefined global on non-Windows
    # platforms.
    if (function is os.listdir) and issubclass(exception_type, OSError):
      if exception_value.errno == errno.ENOENT:
        # File does not exist, and we're trying to delete, so we can ignore the
        # failure.
        print('WARNING:  Failed to list %s during rmtree.  Ignoring.\n' % path)
      else:
        raise
    else:
      raise

  for root, dirs, files in os.walk(file_path, topdown=False):
    # For POSIX:  making the directory writable guarantees removability.
    # Windows will ignore the non-read-only bits in the chmod value.
    os.chmod(root, 0770)
    for name in files:
      remove_with_retry(os.remove, os.path.join(root, name))
    for name in dirs:
      remove_with_retry(lambda p: shutil.rmtree(p, onerror=RmTreeOnError),
                        os.path.join(root, name))

  remove_with_retry(os.rmdir, file_path)


def MakeZip(output_dir, archive_name, file_list, file_relative_dir, dir_in_zip,
            raise_error=True, remove_archive_directory=True, strip_files=None,
            ignore_sub_folder=False):
  """Packs files into a new zip archive.

  Files are first copied into a directory within the output_dir named for
  the archive_name, which will be created if necessary and emptied if it
  already exists.  The files are then then packed using archive names
  relative to the output_dir.  That is, if the zipfile is unpacked in place,
  it will create a directory identical to the new archive_name directory, in
  the output_dir.  The zip file will be named as the archive_name, plus
  '.zip'.

  Args:
    output_dir: Absolute path to the directory in which the archive is to
      be created.
    archive_dir: Subdirectory of output_dir holding files to be added to
      the new zipfile.
    file_list: List of paths to files or subdirectories, relative to the
      file_relative_dir.
    file_relative_dir: Absolute path to the directory containing the files
      and subdirectories in the file_list.
    dir_in_zip: Directory where the files are archived into.
    raise_error: Whether to raise a PathNotFound error if one of the files in
      the list is not found.
    remove_archive_directory: Whether to remove the archive staging directory
      before copying files over to it.
    strip_files: List of executable files to strip symbols when zipping

  Returns:
    A tuple consisting of (archive_dir, zip_file_path), where archive_dir
    is the full path to the newly created archive_name subdirectory.

  Raises:
    PathNotFound if any of the files in the list is not found, unless
    raise_error is False, in which case the error will be ignored.
  """

  start_time = time.clock()
  # Collect files into the archive directory.
  archive_dir = os.path.join(output_dir, dir_in_zip)
  print('output_dir: %s, archive_name: %s' % (output_dir, archive_name))
  print('archive_dir: %s, remove_archive_directory: %s, exists: %s' %
        (archive_dir, remove_archive_directory, os.path.exists(archive_dir)))
  if remove_archive_directory and os.path.exists(archive_dir):
    # Move it even if it's not a directory as expected. This can happen with
    # FILES.cfg archive creation where we create an archive staging directory
    # that is the same name as the ultimate archive name.
    if not os.path.isdir(archive_dir):
      print('Moving old "%s" file to create same name directory.' % archive_dir)
      previous_archive_file = '%s.old' % archive_dir
      MoveFile(archive_dir, previous_archive_file)
    else:
      print('Removing %s' % archive_dir)
      RemoveDirectory(archive_dir)
      print('Now, os.path.exists(%s): %s' % (archive_dir,
                                             os.path.exists(archive_dir)))
  MaybeMakeDirectory(archive_dir)
  for needed_file in file_list:
    needed_file = needed_file.rstrip()
    # These paths are relative to the file_relative_dir.  We need to copy
    # them over maintaining the relative directories, where applicable.
    src_path = os.path.join(file_relative_dir, needed_file)
    dirname, basename = os.path.split(needed_file)
    try:
      if os.path.isdir(src_path):
        if WIN_LINK_FUNC:
          WIN_LINK_FUNC(src_path, os.path.join(archive_dir, needed_file))
        else:
          shutil.copytree(src_path, os.path.join(archive_dir, needed_file),
                          symlinks=True)
      elif dirname != '' and basename != '':
        dest_dir = os.path.join(archive_dir, dirname)
        MaybeMakeDirectory(dest_dir)
        CopyFileToDir(src_path, dest_dir, basename, link_ok=True)
        if strip_files and basename in strip_files:
          cmd = ['strip', os.path.join(dest_dir, basename)]
          RunCommand(cmd)
      else:
        CopyFileToDir(src_path, archive_dir, basename, link_ok=True)
        if strip_files and basename in strip_files:
          cmd = ['strip', os.path.join(archive_dir, basename)]
          RunCommand(cmd)
    except PathNotFound:
      if raise_error:
        raise
  end_time = time.clock()
  print(
      'Took %f seconds to create archive directory.' % (end_time - start_time))

  # Pack the zip file.
  output_file = os.path.join(output_dir, '%s.zip' % archive_name)
  previous_file = os.path.join(output_dir, '%s_old.zip' % archive_name)
  MoveFile(output_file, previous_file)

  # If we have 7z, use that as it's much faster. See http://crbug.com/418702.
  windows_zip_cmd = None
  if os.path.exists('C:\\Program Files\\7-Zip\\7z.exe'):
    windows_zip_cmd = ['C:\\Program Files\\7-Zip\\7z.exe', 'a', '-y', '-mx1']

  # On Windows we use the python zip module; on Linux and Mac, we use the zip
  # command as it will handle links and file bits (executable).  Which is much
  # easier then trying to do that with ZipInfo options.
  start_time = time.clock()
  if IsWindows() and not windows_zip_cmd:
    print('Creating %s' % output_file)

    def _Addfiles(to_zip_file, dirname, files_to_add):
      for this_file in files_to_add:
        archive_name = this_file
        this_path = os.path.join(dirname, this_file)
        if os.path.isfile(this_path):
          # Store files named relative to the outer output_dir.
          archive_name = this_path.replace(output_dir + os.sep, '')
          if os.path.getsize(this_path) == 0:
            compress_method = zipfile.ZIP_STORED
          else:
            compress_method = zipfile.ZIP_DEFLATED
          to_zip_file.write(this_path, archive_name, compress_method)
          print('Adding %s' % archive_name)

    zip_file = zipfile.ZipFile(output_file, 'w', zipfile.ZIP_DEFLATED,
                               allowZip64=True)
    try:
      os.path.walk(archive_dir, _Addfiles, zip_file)
    finally:
      zip_file.close()
  else:
    if IsMac() or IsLinux():
      zip_cmd = ['zip', '-yr1']
    else:
      zip_cmd = windows_zip_cmd
    if ignore_sub_folder:
      zip_cmd.extend(['-j'])
    saved_dir = os.getcwd()
    os.chdir(os.path.dirname(archive_dir))
    command = zip_cmd + [output_file, os.path.basename(archive_dir)]
    result = RunCommand(command)
    os.chdir(saved_dir)
    if result and raise_error:
      raise ExternalError('zip failed: %s => %s' %
                          (str(command), result))
  end_time = time.clock()
  print('Took %f seconds to create zip.' % (end_time - start_time))
  return (archive_dir, output_file)


def ExtractZip(filename, output_dir, extract_file_list=[], verbose=True):
  """Extract the zip archive in the output directory."""
  MaybeMakeDirectory(output_dir)

  # On Linux and Mac, we use the unzip command as it will
  # handle links and file bits (executable), which is much
  # easier then trying to do that with ZipInfo options.
  #
  # The Mac Version of unzip unfortunately does not support Zip64, whereas
  # the python module does, so we have to fallback to the python zip module
  # on Mac if the filesize is greater than 4GB.
  #
  # On Windows, try to use 7z if it is installed, otherwise fall back to python
  # zip module and pray we don't have files larger than 512MB to unzip.
  unzip_cmd = None
  if ((IsMac() and os.path.getsize(filename) < 4 * 1024 * 1024 * 1024)
      or IsLinux()):
    unzip_cmd = ['unzip', '-o']
  elif IsWindows() and os.path.exists('C:\\Program Files\\7-Zip\\7z.exe'):
    unzip_cmd = ['C:\\Program Files\\7-Zip\\7z.exe', 'x', '-y']

  if unzip_cmd:
    # Make sure path is absolute before changing directories.
    filepath = os.path.abspath(filename)
    saved_dir = os.getcwd()
    os.chdir(output_dir)
    command = unzip_cmd + [filepath]
    command.extend(extract_file_list)
    result = RunCommand(command)
    os.chdir(saved_dir)
    if result:
      raise ExternalError('unzip failed: %s => %s' % (str(command), result))
  else:
    assert IsWindows() or IsMac()
    zf = zipfile.ZipFile(filename)
    # TODO(hinoka): This can be multiprocessed.
    for name in zf.namelist():
      if verbose:
        print('Extracting %s' % name)
      zf.extract(name, output_dir)
      if IsMac():
        # Restore permission bits.
        os.chmod(os.path.join(output_dir, name),
                 zf.getinfo(name).external_attr >> 16L)
