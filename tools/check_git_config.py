#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that attempts to push to a special git repository to verify that git
credentials are configured correctly. It also verifies that gclient solution is
configured to use git checkout.

It will be added as gclient hook shortly before Chromium switches to git and
removed after the switch.

When running as hook in *.corp.google.com network it will also report status
of the push attempt to the server (on appengine), so that chrome-infra team can
collect information about misconfigured Git accounts.
"""

from __future__ import print_function

import contextlib
import datetime
import errno
import getpass
import json
import logging
import netrc
import optparse
import os
import pprint
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import time
import urllib2
import urlparse


# Absolute path to src/ directory.
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Absolute path to a file with gclient solutions.
GCLIENT_CONFIG = os.path.join(os.path.dirname(REPO_ROOT), '.gclient')

# Incremented whenever some changes to scrip logic are made. Change in version
# will cause the check to be rerun on next gclient runhooks invocation.
CHECKER_VERSION = 1

# Do not attempt to upload a report after this date.
UPLOAD_DISABLE_TS = datetime.datetime(2014, 10, 1)

# URL to POST json with results to.
MOTHERSHIP_URL = (
    'https://chromium-git-access.appspot.com/'
    'git_access/api/v1/reports/access_check')

# Repository to push test commits to.
TEST_REPO_URL = 'https://chromium.googlesource.com/a/playground/access_test'

# Git-compatible gclient solution.
GOOD_GCLIENT_SOLUTION = {
  'name': 'src',
  'deps_file': 'DEPS',
  'managed': False,
  'url': 'https://chromium.googlesource.com/chromium/src.git',
}

# Possible chunks of git push response in case .netrc is misconfigured.
BAD_ACL_ERRORS = (
  '(prohibited by Gerrit)',
  'does not match your user account',
  'Git repository not found',
  'Invalid user name or password',
  'Please make sure you have the correct access rights',
)

# Git executable to call.
GIT_EXE = 'git.bat' if sys.platform == 'win32' else 'git'


def is_on_bot():
  """True when running under buildbot."""
  return os.environ.get('CHROME_HEADLESS') == '1'


def is_in_google_corp():
  """True when running in google corp network."""
  try:
    return socket.getfqdn().endswith('.corp.google.com')
  except socket.error:
    logging.exception('Failed to get FQDN')
    return False


def is_using_git():
  """True if git checkout is used."""
  return os.path.exists(os.path.join(REPO_ROOT, '.git', 'objects'))


def is_using_svn():
  """True if svn checkout is used."""
  return os.path.exists(os.path.join(REPO_ROOT, '.svn'))


def read_git_config(prop):
  """Reads git config property of src.git repo.

  Returns empty string in case of errors.
  """
  try:
    proc = subprocess.Popen(
        [GIT_EXE, 'config', prop], stdout=subprocess.PIPE, cwd=REPO_ROOT)
    out, _ = proc.communicate()
    return out.strip().decode('utf-8')
  except OSError as exc:
    if exc.errno != errno.ENOENT:
      logging.exception('Unexpected error when calling git')
    return ''


def read_netrc_user(netrc_obj, host):
  """Reads 'user' field of a host entry in netrc.

  Returns empty string if netrc is missing, or host is not there.
  """
  if not netrc_obj:
    return ''
  entry = netrc_obj.authenticators(host)
  if not entry:
    return ''
  return entry[0]


def get_git_version():
  """Returns version of git or None if git is not available."""
  try:
    proc = subprocess.Popen([GIT_EXE, '--version'], stdout=subprocess.PIPE)
    out, _ = proc.communicate()
    return out.strip() if proc.returncode == 0 else ''
  except OSError as exc:
    if exc.errno != errno.ENOENT:
      logging.exception('Unexpected error when calling git')
    return ''


def read_gclient_solution():
  """Read information about 'src' gclient solution from .gclient file.

  Returns tuple:
    (url, deps_file, managed)
    or
    (None, None, None) if no such solution.
  """
  try:
    env = {}
    execfile(GCLIENT_CONFIG, env, env)
    for sol in (env.get('solutions') or []):
      if sol.get('name') == 'src':
        return sol.get('url'), sol.get('deps_file'), sol.get('managed')
    return None, None, None
  except Exception:
    logging.exception('Failed to read .gclient solution')
    return None, None, None


def read_git_insteadof(host):
  """Reads relevant insteadOf config entries."""
  try:
    proc = subprocess.Popen([GIT_EXE, 'config', '-l'], stdout=subprocess.PIPE)
    out, _ = proc.communicate()
    lines = []
    for line in out.strip().split('\n'):
      line = line.lower()
      if 'insteadof=' in line and host in line:
        lines.append(line)
    return '\n'.join(lines)
  except OSError as exc:
    if exc.errno != errno.ENOENT:
      logging.exception('Unexpected error when calling git')
    return ''


def scan_configuration():
  """Scans local environment for git related configuration values."""
  # Git checkout?
  is_git = is_using_git()

  # On Windows HOME should be set.
  if 'HOME' in os.environ:
    netrc_path = os.path.join(
        os.environ['HOME'],
        '_netrc' if sys.platform.startswith('win') else '.netrc')
  else:
    netrc_path = None

  # Netrc exists?
  is_using_netrc = netrc_path and os.path.exists(netrc_path)

  # Read it.
  netrc_obj = None
  if is_using_netrc:
    try:
      netrc_obj = netrc.netrc(netrc_path)
    except Exception:
      logging.exception('Failed to read netrc from %s', netrc_path)
      netrc_obj = None

  # Read gclient 'src' solution.
  gclient_url, gclient_deps, gclient_managed = read_gclient_solution()

  return {
    'checker_version': CHECKER_VERSION,
    'is_git': is_git,
    'is_home_set': 'HOME' in os.environ,
    'is_using_netrc': is_using_netrc,
    'netrc_file_mode': os.stat(netrc_path).st_mode if is_using_netrc else 0,
    'git_version': get_git_version(),
    'platform': sys.platform,
    'username': getpass.getuser(),
    'git_user_email': read_git_config('user.email') if is_git else '',
    'git_user_name': read_git_config('user.name') if is_git else '',
    'git_insteadof': read_git_insteadof('chromium.googlesource.com'),
    'chromium_netrc_email':
        read_netrc_user(netrc_obj, 'chromium.googlesource.com'),
    'chrome_internal_netrc_email':
        read_netrc_user(netrc_obj, 'chrome-internal.googlesource.com'),
    'gclient_deps': gclient_deps,
    'gclient_managed': gclient_managed,
    'gclient_url': gclient_url,
  }


def last_configuration_path():
  """Path to store last checked configuration."""
  if is_using_git():
    return os.path.join(REPO_ROOT, '.git', 'check_git_push_access_conf.json')
  elif is_using_svn():
    return os.path.join(REPO_ROOT, '.svn', 'check_git_push_access_conf.json')
  else:
    return os.path.join(REPO_ROOT, '.check_git_push_access_conf.json')


def read_last_configuration():
  """Reads last checked configuration if it exists."""
  try:
    with open(last_configuration_path(), 'r') as f:
      return json.load(f)
  except (IOError, ValueError):
    return None


def write_last_configuration(conf):
  """Writes last checked configuration to a file."""
  try:
    with open(last_configuration_path(), 'w') as f:
      json.dump(conf, f, indent=2, sort_keys=True)
  except IOError:
    logging.exception('Failed to write JSON to %s', path)


@contextlib.contextmanager
def temp_directory():
  """Creates a temp directory, then nukes it."""
  tmp = tempfile.mkdtemp()
  try:
    yield tmp
  finally:
    try:
      shutil.rmtree(tmp)
    except (OSError, IOError):
      logging.exception('Failed to remove temp directory %s', tmp)


class Runner(object):
  """Runs a bunch of commands in some directory, collects logs from them."""

  def __init__(self, cwd, verbose):
    self.cwd = cwd
    self.verbose = verbose
    self.log = []

  def run(self, cmd):
    self.append_to_log('> ' + ' '.join(cmd))
    retcode = -1
    try:
      proc = subprocess.Popen(
          cmd,
          stdout=subprocess.PIPE,
          stderr=subprocess.STDOUT,
          cwd=self.cwd)
      out, _ = proc.communicate()
      out = out.strip()
      retcode = proc.returncode
    except OSError as exc:
      out = str(exc)
    if retcode:
      out += '\n(exit code: %d)' % retcode
    self.append_to_log(out)
    return retcode

  def append_to_log(self, text):
    if text:
      self.log.append(text)
      if self.verbose:
        logging.warning(text)


def check_git_config(conf, report_url, verbose):
  """Attempts to push to a git repository, reports results to a server.

  Returns True if the check finished without incidents (push itself may
  have failed) and should NOT be retried on next invocation of the hook.
  """
  # Don't even try to push if netrc is not configured.
  if not conf['chromium_netrc_email']:
    return upload_report(
        conf,
        report_url,
        verbose,
        push_works=False,
        push_log='',
        push_duration_ms=0)

  # Ref to push to, each user has its own ref.
  ref = 'refs/push-test/%s' % conf['chromium_netrc_email']

  push_works = False
  flake = False
  started = time.time()
  try:
    logging.warning('Checking push access to the git repository...')
    with temp_directory() as tmp:
      # Prepare a simple commit on a new timeline.
      runner = Runner(tmp, verbose)
      runner.run([GIT_EXE, 'init', '.'])
      if conf['git_user_name']:
        runner.run([GIT_EXE, 'config', 'user.name', conf['git_user_name']])
      if conf['git_user_email']:
        runner.run([GIT_EXE, 'config', 'user.email', conf['git_user_email']])
      with open(os.path.join(tmp, 'timestamp'), 'w') as f:
        f.write(str(int(time.time() * 1000)))
      runner.run([GIT_EXE, 'add', 'timestamp'])
      runner.run([GIT_EXE, 'commit', '-m', 'Push test.'])
      # Try to push multiple times if it fails due to issues other than ACLs.
      attempt = 0
      while attempt < 5:
        attempt += 1
        logging.info('Pushing to %s %s', TEST_REPO_URL, ref)
        ret = runner.run(
            [GIT_EXE, 'push', TEST_REPO_URL, 'HEAD:%s' % ref, '-f'])
        if not ret:
          push_works = True
          break
        if any(x in runner.log[-1] for x in BAD_ACL_ERRORS):
          push_works = False
          break
  except Exception:
    logging.exception('Unexpected exception when pushing')
    flake = True

  if push_works:
    logging.warning('Git push works!')
  else:
    logging.warning(
        'Git push doesn\'t work, which is fine if you are not a committer.')

  uploaded = upload_report(
      conf,
      report_url,
      verbose,
      push_works=push_works,
      push_log='\n'.join(runner.log),
      push_duration_ms=int((time.time() - started) * 1000))
  return uploaded and not flake


def check_gclient_config(conf):
  """Shows warning if gclient solution is not properly configured for git."""
  # Ignore configs that do not have 'src' solution at all.
  if not conf['gclient_url']:
    return
  current = {
    'name': 'src',
    'deps_file': conf['gclient_deps'] or 'DEPS',
    'managed': conf['gclient_managed'] or False,
    'url': conf['gclient_url'],
  }
  # After depot_tools r291592 both DEPS and .DEPS.git are valid.
  good = GOOD_GCLIENT_SOLUTION.copy()
  good['deps_file'] = current['deps_file']
  if current == good:
    return
  # Show big warning if url or deps_file is wrong.
  if current['url'] != good['url'] or current['deps_file'] != good['deps_file']:
    print('-' * 80)
    print('Your gclient solution is not set to use supported git workflow!')
    print()
    print('Your \'src\' solution (in %s):' % GCLIENT_CONFIG)
    print(pprint.pformat(current, indent=2))
    print()
    print('Correct \'src\' solution to use git:')
    print(pprint.pformat(good, indent=2))
    print()
    print('Please update your .gclient file ASAP.')
    print('-' * 80)
  # Show smaller (additional) warning about managed workflow.
  if current['managed']:
    print('-' * 80)
    print('You are using managed gclient mode with git, which was deprecated '
          'on 8/22/13:')
    print('https://groups.google.com/a/chromium.org/'
          'forum/#!topic/chromium-dev/n9N5N3JL2_U')
    print()
    print('It is strongly advised to switch to unmanaged mode. For more '
          'information about managed mode and reasons for its deprecation see:')
    print(
        'http://www.chromium.org/developers/how-tos/get-the-code/gclient-managed-mode'
    )
    print()
    print('There\'s also a large suite of tools to assist managing git '
          'checkouts.\nSee \'man depot_tools\' (or read '
          'depot_tools/man/html/depot_tools.html).')
    print('-' * 80)


def upload_report(
    conf, report_url, verbose, push_works, push_log, push_duration_ms):
  """Posts report to the server, returns True if server accepted it.

  Uploads the report only if script is running in Google corp network. Otherwise
  just prints the report.
  """
  report = conf.copy()
  report.update(
      push_works=push_works,
      push_log=push_log,
      push_duration_ms=push_duration_ms)

  as_bytes = json.dumps({'access_check': report}, indent=2, sort_keys=True)
  if verbose:
    print('Status of git push attempt:')
    print(as_bytes)

  # Do not upload it outside of corp or if server side is already disabled.
  if not is_in_google_corp() or datetime.datetime.now() > UPLOAD_DISABLE_TS:
    if verbose:
      print (
          'You can send the above report to chrome-git-migration@google.com '
          'if you need help to set up you committer git account.')
    return True

  req = urllib2.Request(
      url=report_url,
      data=as_bytes,
      headers={'Content-Type': 'application/json; charset=utf-8'})

  attempt = 0
  success = False
  while not success and attempt < 10:
    attempt += 1
    try:
      logging.warning(
          'Attempting to upload the report to %s...',
          urlparse.urlparse(report_url).netloc)
      resp = urllib2.urlopen(req, timeout=5)
      report_id = None
      try:
        report_id = json.load(resp)['report_id']
      except (ValueError, TypeError, KeyError):
        pass
      logging.warning('Report uploaded: %s', report_id)
      success = True
    except (urllib2.URLError, socket.error, ssl.SSLError) as exc:
      logging.warning('Failed to upload the report: %s', exc)
  return success


def main(args):
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '--running-as-hook',
      action='store_true',
      help='Set when invoked from gclient hook')
  parser.add_option(
      '--report-url',
      default=MOTHERSHIP_URL,
      help='URL to submit the report to')
  parser.add_option(
      '--verbose',
      action='store_true',
      help='More logging')
  options, args = parser.parse_args()
  if args:
    parser.error('Unknown argument %s' % args)
  logging.basicConfig(
      format='%(message)s',
      level=logging.INFO if options.verbose else logging.WARN)

  # When invoked not as a hook, always run the check.
  if not options.running_as_hook:
    config = scan_configuration()
    check_gclient_config(config)
    check_git_config(config, options.report_url, True)
    return 0

  # Always do nothing on bots.
  if is_on_bot():
    return 0

  # Read current config, verify gclient solution looks correct.
  config = scan_configuration()
  check_gclient_config(config)

  # Do not attempt to push from non-google owned machines.
  if not is_in_google_corp():
    logging.info('Skipping git push check: non *.corp.google.com machine.')
    return 0

  # Skip git push check if current configuration was already checked.
  if config == read_last_configuration():
    logging.info('Check already performed, skipping.')
    return 0

  # Run the check. Mark configuration as checked only on success. Ignore any
  # exceptions or errors. This check must not break gclient runhooks.
  try:
    ok = check_git_config(config, options.report_url, False)
    if ok:
      write_last_configuration(config)
    else:
      logging.warning('Check failed and will be retried on the next run')
  except Exception:
    logging.exception('Unexpected exception when performing git access check')
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
