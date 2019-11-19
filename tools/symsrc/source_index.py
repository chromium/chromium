#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Usage: Run with the '--help' flag to see how to use this script.
This tool will take a PDB on the command line, extract the source files that
were used in building the PDB, query the source server for which repository
and revision these files are at, and then finally write this information back
into the PDB in a format that the debugging tools understand.  This allows for
automatic source debugging, as all of the information is contained in the PDB,
and the debugger can go out and fetch the source files.

You most likely want to run these immediately after a build, since the source
input files need to match the generated PDB, and we want the correct
revision information for the exact files that were used for the build.

Here's a quick overview of what this script does:
  - Extract the list of source files listed in the PDB, i.e. all the source
    files that have been used to produce the matching binary. This list contains
    some files for which the source code is accessible (e.g. files from the
    Chromium repo) and some from private repos (e.g. files that have been used
    to build the CRT static library that we link against).
  - Iterate over the list of files from the previous step, from here there's a
    few different possibilities:
      - This file is coming from a public Git repository (e.g. Chromium), in
        this case this script will list all the files that are contained in this
        repository and index them all at once (and then remove them from the
        file list it's iterating over).
      - This file is a generated file produced during the build. It will likely
        be living in a subdirectory of the build directory, if the "--build-dir"
        flag has been passed to this flag this flag this directory will be
        automatically ignored.
      - The directory containing this file isn't part of a Git checkout, this
        file will be excluded and all the files from this directory will get
        added to an exclusion list. Specifying the toolchain directory (via the
        "--toolchain-dir" flag) allow automatically skipping all the files from
        the VS toolchain directory (e.g. all the CRT files), this is much faster
        than using Git to check if these files are from a Git repo.
      - This file doesn't exist on disk, which means that it is coming from a
        third party static library. This file will be ignored.
  - A map that associates each source file to a public URL will be added in a
    new stream in the PDB.

NOTE: Expected to run under a native win32 python, NOT cygwin.  All paths are
dealt with as win32 paths, since we have to interact with the Microsoft tools.
"""

from __future__ import print_function

try:
  # Python 3.x
  from urllib.parse import urlparse
except ImportError:
  # Python 2.x
  from urlparse import urlparse

import optparse
import os
import re
import subprocess
import sys
import tempfile
import time
import win32api

from collections import namedtuple

# Map that associates Git repository URLs with the URL that should be used to
# retrieve individual files from this repo. Entries in this map should have the
# following format:
#   {
#     'url': |path to public URL, with the {revision} and {file_path} tags|,
#     'base64': |boolean indicating if the files are base64 encoded|
#   }
#
# Here's an example of what the entry for the Chromium repo looks like:
#   {
#     'url': 'chromium.googlesource/+/{revision}/{file_path}?format=TEXT',
#     'base64': True
#   }
#
# The {revision} and {file_path} will be replaced by the appropriate values when
# building the source indexing map that gets added to the PDB.
#
# TODO(sebmarchand): Check if this is really needed, this is a legacy thing
# coming from when this script was used for SVN repos and we could probably do
# without it.
REPO_MAP = {}


# Regex matching a junction at it's printed by the 'dir' command.
# It usually looks like this when the junction has been created with mklink:
#
#    Directory of C:\a
#
#   07/23/2015  06:42 PM <JUNCTION>     b [C:\real_a\b]
#
# The junctions created with the 'junction' utility look almost the same, except
# for a leading '\??\' on the junction target:
#
#   07/23/2015  06:42 PM <JUNCTION>     b [\??\C:\real_a\b]
_DIR_JUNCTION_RE = re.compile(r"""
    .*<JUNCTION\>\s+(?P<dirname>[^ ]+)\s+\[(\\\?\?\\)?(?P<real_path>.*)\]
""", re.VERBOSE)


# A named tuple used to store the information about a repository.
#
# It contains the following members:
#     - repo: The URL of the repository;
#     - rev: The revision (or hash) of the current checkout.
#     - file_list: The list of files coming from this repository.
#     - root_path: The root path of this checkout.
#     - path_prefix: A prefix to apply to the filename of the files coming from
#         this repository.
RevisionInfo = namedtuple('RevisionInfo',
                          ['repo', 'rev', 'files', 'root_path', 'path_prefix'])


def GetCasedFilePath(filename):
  """Return the correctly cased path for a given filename"""
  return win32api.GetLongPathName(win32api.GetShortPathName(unicode(filename)))


def FindSrcSrvFile(filename, toolchain_dir):
  """Return the absolute path for a file in the srcsrv directory.

  If |toolchain_dir| is null then this will assume that the file is in this
  script's directory.
  """
  bin_dir = os.path.join(toolchain_dir, 'win_sdk', 'Debuggers', 'x64', 'srcsrv')
  assert(os.path.exists(bin_dir))
  return os.path.abspath(os.path.join(bin_dir, filename))


def RunCommand(*cmd, **kwargs):
  """Runs a command.

  Returns what have been printed to stdout by this command.

  kwargs:
    raise_on_failure: Indicates if an exception should be raised on failure, if
        set to false then the function will return None.
  """
  kwargs.setdefault('stdin', subprocess.PIPE)
  kwargs.setdefault('stdout', subprocess.PIPE)
  kwargs.setdefault('stderr', subprocess.PIPE)
  kwargs.setdefault('universal_newlines', True)
  raise_on_failure = kwargs.pop('raise_on_failure', True)

  proc = subprocess.Popen(cmd, **kwargs)
  ret, err = proc.communicate()
  if proc.returncode != 0:
    if raise_on_failure:
      print('Error: %s' % err)
      raise subprocess.CalledProcessError(proc.returncode, cmd)
    return

  ret = (ret or '').rstrip('\n')
  return ret


def ExtractSourceFiles(pdb_filename, toolchain_dir):
  """Extract a list of local paths of the source files from a PDB."""

  # Don't use |RunCommand| as it expect the return code to be 0 on success but
  # srctool returns the number of files instead.
  srctool = subprocess.Popen([FindSrcSrvFile('srctool.exe', toolchain_dir),
                              '-r', pdb_filename],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             universal_newlines=True)
  src_files, _ = srctool.communicate()

  if (not src_files or src_files.startswith("srctool: ") or
      srctool.returncode <= 0):
    raise Exception("srctool failed: " + src_files)
  return set(
      x.rstrip('\n').lower() for x in src_files.split('\n') if len(x) != 0)


def ReadSourceStream(pdb_filename, toolchain_dir):
  """Read the contents of the source information stream from a PDB."""
  pdbstr = subprocess.Popen([FindSrcSrvFile('pdbstr.exe', toolchain_dir),
                              '-r', '-s:srcsrv',
                              '-p:%s' % pdb_filename],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  data, _ = pdbstr.communicate()

  # Old version of pdbstr.exe return -1 when the source requested stream is
  # missing, while more recent ones return 1, use |abs| to workaround this.
  if (((pdbstr.returncode != 0 and abs(pdbstr.returncode) != 1) or
      data.startswith("pdbstr: "))):
    raise Exception("pdbstr failed: " + data)
  return data


def WriteSourceStream(pdb_filename, data, toolchain_dir):
  """Write the contents of the source information stream to a PDB."""
  # Write out the data to a temporary filename that we can pass to pdbstr.
  (f, fname) = tempfile.mkstemp()
  f = os.fdopen(f, "wb")
  f.write(data)
  f.close()

  srctool = subprocess.Popen([FindSrcSrvFile('pdbstr.exe', toolchain_dir),
                              '-w', '-s:srcsrv',
                              '-i:%s' % fname,
                              '-p:%s' % pdb_filename],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  data, _ = srctool.communicate()

  if ((srctool.returncode != 0 and srctool.returncode != -1) or
      data.startswith("pdbstr: ")):
    raise Exception("pdbstr failed: " + data)

  os.unlink(fname)


def ExtractGitInfo(local_filename):
  """Checks if a file is coming from a git repository and if so returns some
  information about it.

  Args:
    local_filename: The name of the file that we want to check.

  Returns:
    None if the file doesn't come from a git repository, otherwise it returns a
    RevisionInfo tuple.
  """
  # Starts by checking if this file is coming from a git repository. For that
  # we'll start by calling 'git info' on this file; for this to work we need to
  # make sure that the current working directory is correctly cased. It turns
  # out that even on Windows the casing of the path passed in the |cwd| argument
  # of subprocess.Popen matters and if it's not correctly cased then 'git info'
  # will return None even if the file is coming from a git repository. This
  # is not the case if we're just interested in checking if the path containing
  # |local_filename| is coming from a git repository, in this case the casing
  # doesn't matter.
  local_filename = GetCasedFilePath(local_filename)
  local_file_basename = os.path.basename(local_filename)
  local_file_dir = os.path.dirname(local_filename)
  file_info = RunCommand('git.bat', 'log', '-n', '1', local_file_basename,
                          cwd=local_file_dir, raise_on_failure=False)

  if not file_info:
    return

  # Get the revision of the master branch.
  rev = RunCommand('git.bat', 'rev-parse', 'HEAD', cwd=local_file_dir)

  # Get the url of the remote repository.
  repo = RunCommand('git.bat', 'config', '--get', 'remote.origin.url',
      cwd=local_file_dir)
  # If the repository point to a local directory then we need to run this
  # command one more time from this directory to get the repository url.
  if os.path.isdir(repo):
    repo = RunCommand('git.bat', 'config', '--get', 'remote.origin.url',
        cwd=repo)

  # Don't use the authenticated path.
  repo = repo.replace('googlesource.com/a/', 'googlesource.com/')

  # Get the relative file path for this file in the git repository.
  git_path = RunCommand('git.bat', 'ls-tree', '--full-name', '--name-only',
      'HEAD', local_file_basename, cwd=local_file_dir).replace('/','\\')

  if not git_path:
    return

  git_root_path = local_filename.replace(git_path, '')

  if repo not in REPO_MAP:
    # Automatically adds the project coming from a git GoogleCode or Github
    # repository to the repository map.
    if urlparse(repo).netloc.endswith('.googlesource.com'):
      # The files from these repositories are accessible via gitiles in a
      # base64 encoded format.
      REPO_MAP[repo] = {
          'url': '%s/+/{revision}/{file_path}?format=TEXT' % repo,
          'base64': True
      }
    elif urlparse(repo).netloc.endswith('github.com'):
      raw_url = '%s/{revision}/{file_path}' % repo.replace('.git', '').replace(
          'github.com', 'raw.githubusercontent.com')
      REPO_MAP[repo] = {
          'url': raw_url,
          'base64': False
      }

  # Get the list of files coming from this repository.
  git_file_list = RunCommand('git.bat', 'ls-tree', '--full-name', '--name-only',
      'HEAD', '-r', cwd=git_root_path)

  file_list = [x for x in git_file_list.splitlines() if len(x) != 0]

  return RevisionInfo(repo=repo, rev=rev, files=file_list,
      root_path=git_root_path, path_prefix=None)


def CheckForJunction(filename):
  """Check if a directory containing a file is a junction to another directory.

  If so return 3 values:
      - The real path to this file.
      - The root directory of this checkout relative to |filename| (i.e. not
        relative to the real path).
      - The sub directory of the repository that has been checked out.
  """
  # Process the path of |filename| from right to left until a junction has been
  # found.
  #
  # Here's an example of what this does, for this example
  # |filename| = 'C:/a/b/c/d.h' and 'C:/a' is a junction to 'C:/real_a/'.
  #
  # - We remove the filename part (we're only looking at directory junctions
  #   here), that left us with 'C:/a/b/c'.
  # - During the first iteration we take 'C:/a/b' as our current root value and
  #   'c' as the leaf value.
  # - We run the 'dir' command on 'C:/a/b' and we look for a junction to 'c'. As
  #   we don't find any we go up one directory. Now the current root is 'C:/a'
  #   and the current leaf value is 'b'.
  # - We run the 'dir' command on 'C:/a' and we look for a junction to 'b'. This
  #   time we find one so we return the following triplet:
  #     - C:/real_a/b/c/d.h   # The real file path.
  #     - C:/a                # The root directory containing this junction.
  #     - b                   # The name of the junction.
  cur_root, cur_leaf = os.path.split(os.path.dirname(filename))
  while cur_leaf:
    # Run the 'dir' command and look for a junction.
    dir_cmd = RunCommand('cmd', '/c', 'dir', cur_root)
    for entry in dir_cmd.splitlines():
      m = _DIR_JUNCTION_RE.match(entry)
      if not m:
        continue
      if not m.group('dirname') == cur_leaf:
        continue
      real_path = filename.replace(os.path.join(cur_root, cur_leaf),
                                    m.group('real_path'))
      # This should always be the case.
      # TODO(sebmarchand): Remove this check if it proves to be useless.
      if os.path.exists(real_path):
        return real_path, cur_root, cur_leaf
      else:
        print('Source indexing: error: Unexpected non existing file \'%s\'' %
              real_path)
        return None, None, None
    cur_root, cur_leaf = os.path.split(cur_root)
  return None, None, None


def IndexFilesFromRepo(
    local_filename, file_list, output_lines, follow_junctions):
  """Checks if a given file is a part of a Git repository  and index all the
  files from this repository if it's the case.

  Args:
    local_filename: The filename of the current file.
    file_list: The list of files that should be indexed.
    output_lines: The source indexing lines that will be appended to the PDB.
    follow_junctions: Indicates if we should try to index the files in a
        junction.

  Returns the number of indexed files.
  """
  indexed_files = 0
  patch_root = None
  # Try to extract the revision info for the current file.
  info = ExtractGitInfo(local_filename)

  # If we haven't been able to find information for this file it might be
  # because its path contains a junction to another directory. It can be the
  # case if you do a Git checkout in C:/real_a/ and you're adding a junction to
  # one of the subdirectories (lets say 'b') of this checkout in another
  # project (e.g. 'C:/a'), so you'll end up with a partial Git checkout in a
  # junction, and any Git command in the path of the junction won't work (or
  # it'll return information related to 'C:/a' instead of 'C:/real_a').
  if not info and follow_junctions:
    real_filename, patch_root, patch_leaf = CheckForJunction(local_filename)
    if real_filename:
      info = ExtractGitInfo(real_filename)

  # Don't try to index the internal sources.
  if not info or ('internal.googlesource.com' in info.repo):
    return 0

  repo = info.repo
  rev = info.rev
  files = info.files
  if patch_root:
    files = [x for x in files if x.startswith(patch_leaf)]
    root_path = patch_root.lower()
  else:
    root_path = info.root_path.lower()

  # Checks if we should index this file and if the source that we'll retrieve
  # will be base64 encoded.
  should_index = False
  base_64 = False
  if repo in REPO_MAP:
    should_index = True
    base_64 = REPO_MAP[repo].get('base64')
  else:
    repo = None

  # Iterates over the files from this repo and index them if needed.
  for file_iter in files:
    current_filename = file_iter.lower()
    full_file_path = os.path.normpath(os.path.join(root_path, current_filename))
    # Checks if the file is in the list of files to be indexed.
    if full_file_path in file_list:
      if should_index:
        source_url = ''
        current_file = file_iter
        # Prefix the filename with the prefix for this repository if needed.
        if info.path_prefix:
          current_file = os.path.join(info.path_prefix, current_file)
        source_url = REPO_MAP[repo].get('url').format(revision=rev,
            file_path=os.path.normpath(current_file).replace('\\', '/'))
        output_lines.append('%s*%s*%s*%s*%s' % (full_file_path, current_file,
            rev, source_url, 'base64.b64decode' if base_64 else ''))
        indexed_files += 1
        file_list.remove(full_file_path)

  # The input file should have been removed from the list of files to index.
  if indexed_files and local_filename in file_list:
    print('%s shouldn\'t be in the list of files to index anymore.' % \
        local_filename)
    # TODO(sebmarchand): Turn this into an exception once I've confirmed that
    #     this doesn't happen on the official builder.
    file_list.remove(local_filename)

  return indexed_files


def DirectoryIsPartOfPublicGitRepository(local_dir):
  # Checks if this directory is from a public Git checkout.
  info = RunCommand('git.bat', 'config', '--get', 'remote.origin.url',
      cwd=local_dir, raise_on_failure=False)
  if info:
    if 'internal.googlesource.com' in info:
      return False
    return True

  return False


def UpdatePDB(pdb_filename, verbose=True, build_dir=None, toolchain_dir=None,
              follow_junctions=False):
  """Update a pdb file with source information."""
  dir_exclusion_list = { }

  if build_dir:
    # Excluding the build directory allows skipping the generated files, for
    # Chromium this makes the indexing ~10x faster.
    build_dir = (os.path.normpath(build_dir)).lower()
    for directory, _, _ in os.walk(build_dir):
      dir_exclusion_list[directory.lower()] = True
    dir_exclusion_list[build_dir.lower()] = True

  if toolchain_dir:
    # Exclude the directories from the toolchain as we don't have revision info
    # for them.
    toolchain_dir = (os.path.normpath(toolchain_dir)).lower()
    for directory, _, _ in os.walk(toolchain_dir):
      dir_exclusion_list[directory.lower()] = True
    dir_exclusion_list[toolchain_dir.lower()] = True

  # Writes the header of the source index stream.
  #
  # Here's the description of the variables used in the SRC_* macros (those
  # variables have to be defined for every source file that we want to index):
  #   var1: The file path.
  #   var2: The name of the file without its path.
  #   var3: The revision or the hash of this file's repository.
  #   var4: The URL to this file.
  #   var5: (optional) The python method to call to decode this file, e.g. for
  #       a base64 encoded file this value should be 'base64.b64decode'.
  lines = [
    'SRCSRV: ini ------------------------------------------------',
    'VERSION=1',
    'INDEXVERSION=2',
    'VERCTRL=Subversion',
    'DATETIME=%s' % time.asctime(),
    'SRCSRV: variables ------------------------------------------',
    'SRC_EXTRACT_TARGET_DIR=%targ%\\%fnbksl%(%var2%)\\%var3%',
    'SRC_EXTRACT_TARGET=%SRC_EXTRACT_TARGET_DIR%\\%fnfile%(%var1%)',
    'SRC_EXTRACT_CMD=cmd /c "mkdir "%SRC_EXTRACT_TARGET_DIR%" & python -c '
        '"import urllib2, base64;'
        'url = \\\"%var4%\\\";'
        'u = urllib2.urlopen(url);'
        'open(r\\\"%SRC_EXTRACT_TARGET%\\\", \\\"wb\\\").write(%var5%('
            'u.read()))"',
    'SRCSRVTRG=%SRC_EXTRACT_TARGET%',
    'SRCSRVCMD=%SRC_EXTRACT_CMD%',
    'SRCSRV: source files ---------------------------------------',
  ]

  if ReadSourceStream(pdb_filename, toolchain_dir):
    raise Exception("PDB already has source indexing information!")

  filelist = ExtractSourceFiles(pdb_filename, toolchain_dir)
  number_of_files = len(filelist)
  indexed_files_total = 0

  t_init = time.time()
  t1 = t_init
  while filelist:
    filename = next(iter(filelist))
    filedir = os.path.dirname(filename)
    if verbose:
      print("[%d / %d] Processing: %s" % (number_of_files - len(filelist),
                                          number_of_files, filename))

    # Print a message every 60 seconds to make sure that the process doesn't
    # time out.
    if time.time() - t1 > 60:
      t1 = time.time()
      print("Still working, %d / %d files have been processed." %
            (number_of_files - len(filelist), number_of_files))

    # This directory is in the exclusion listed, either because it's not part of
    # a repository, or from one we're not interested in indexing.
    if dir_exclusion_list.get(filedir, False):
      if verbose:
        print("  skipping, directory is excluded.")
      filelist.remove(filename)
      continue

    # Skip the files that don't exist on the current machine.
    if not os.path.exists(filename):
      filelist.remove(filename)
      continue

    # Try to index the current file and all the ones coming from the same
    # repository.
    indexed_files = IndexFilesFromRepo(
        filename, filelist, lines, follow_junctions)
    if not indexed_files:
      if not DirectoryIsPartOfPublicGitRepository(filedir):
        dir_exclusion_list[filedir] = True
        if verbose:
          print("Adding %s to the exclusion list." % filedir)
      filelist.remove(filename)
      continue

    indexed_files_total += indexed_files

    if verbose:
      print("  %d files have been indexed." % indexed_files)

  print('Indexing took %d seconds' % (time.time() - t_init))

  lines.append('SRCSRV: end ------------------------------------------------')

  WriteSourceStream(pdb_filename, '\r\n'.join(lines), toolchain_dir)

  if verbose:
    print("%d / %d files have been indexed." % (indexed_files_total,
                                                number_of_files))


def main():
  parser = optparse.OptionParser()
  parser.add_option('-v', '--verbose', action='store_true', default=False)
  parser.add_option('--build-dir', help='The original build directory, if set '
      'all the files present in this directory (or one of its subdirectories) '
      'will be skipped.')
  parser.add_option('--toolchain-dir', help='The directory containing the VS '
      'toolchain that has been used for this build. All the files present in '
      'this directory (or one of its subdirectories) will be skipped.')
  parser.add_option('--follow-junctions', action='store_true',help='Indicates '
      'if the junctions should be followed while doing the indexing.',
      default=False)
  options, args = parser.parse_args()

  if not args:
    parser.error('Specify a pdb.')

  if not options.toolchain_dir:
    parser.error('The toolchain directory should be specified.')

  for pdb in args:
    UpdatePDB(pdb, options.verbose, options.build_dir, options.toolchain_dir,
        options.follow_junctions)

  return 0


if __name__ == '__main__':
  sys.exit(main())
