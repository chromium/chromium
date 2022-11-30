# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from py_utils import cloud_storage

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_TRACE_DIR = os.path.join(_SCRIPT_DIR, 'traces')

HTML_URL_PREFIX = ('https://storage.cloud.google.com/chrome-telemetry-output/')


def _GetSubpathInBucket(html_url):
  """Returns the path minus the HTML_URL_PREFIX.

  Given https://storage.../chrome-telemetry-output/foo/bar/trace.html,
  it returns foo/bar/trace.html."""
  if not html_url.startswith(HTML_URL_PREFIX):
    raise Exception('Html trace url must start with %s' % HTML_URL_PREFIX)
  return html_url.replace(HTML_URL_PREFIX, "")


def _GetProtoTraceLinkFromTraceEventsDir(link_prefix):
  """Returns the first proto trace in |link_prefix|/trace/traceEvents/"""
  proto_link_prefix = '/'.join([link_prefix, 'trace/traceEvents/**'])
  try:
    for link in cloud_storage.List(cloud_storage.TELEMETRY_OUTPUT,
                                   proto_link_prefix):
      if link.endswith('.pb.gz') or link.endswith('.pb'):
        return link[1:]  # Strip the initial '/'.
  except cloud_storage.NotFoundError as e:
    # This directory doesn't exist at all.
    raise cloud_storage.NotFoundError('No URLs match the prefix %s: %s' %
                                      (proto_link_prefix, str(e)))
  # The directory exists, but no proto trace found.
  raise cloud_storage.NotFoundError(
      'Proto trace not found in cloud storage. Path: %s.' % proto_link_prefix)


def GetFileExtension(file_path):
  """Given foo/bar/baz.pb.gz, returns '.pb.gz'."""
  # Get the filename only because the directory names can contain "." like
  # "v8.browsing".
  filename = file_path.split('/')[-1]
  first_dot_index = filename.find('.')
  if first_dot_index == -1:
    return ''
  return filename[first_dot_index:]


def GetLocalTraceFileName(html_url):
  """Returns a local filename derived from the html trace url.

  Given https://storage.../chrome-telemetry-output/foo/bar/trace.html, it
  returns foo_bar_trace as the local filename. The filename does not contain
  extensions. It's up to the caller to add .html or .pb etc."""
  subpath = _GetSubpathInBucket(html_url)
  extension = GetFileExtension(subpath)
  no_extension_subpath = subpath[:-len(extension)]
  return '_'.join(no_extension_subpath.split('/'))


def FindProtoTracePath(html_url):
  """
  Finds the proto trace path given a html trace url.

  In the simple case foo/bar/trace.pb is the proto trace for foo/bar/trace.html.
  But sometimes that's not available so we have to look for a .pb.gz file in a
  special directory."""
  subpath = _GetSubpathInBucket(html_url)
  if subpath.endswith('trace.html'):
    proto_path = subpath.replace('trace.html', 'trace.pb')
    if cloud_storage.Exists(cloud_storage.TELEMETRY_OUTPUT, proto_path):
      return proto_path
    proto_path += '.gz'
    if cloud_storage.Exists(cloud_storage.TELEMETRY_OUTPUT, proto_path):
      return proto_path

  directory_path = '/'.join(subpath.split('/')[:-1])
  return _GetProtoTraceLinkFromTraceEventsDir(directory_path)


def DownloadHtmlTrace(html_url, download_dir=DEFAULT_TRACE_DIR):
  """Downloads html trace given the url. Returns local path.

  Skips downloading if file was already downloaded once."""
  local_filename = os.path.join(download_dir, GetLocalTraceFileName(html_url))
  local_path = local_filename + '.html'
  if os.path.exists(local_path):
    logging.info('%s already downloaded. Skipping.' % local_path)
    return local_path

  remote_path = _GetSubpathInBucket(html_url)
  if not cloud_storage.Exists(cloud_storage.TELEMETRY_OUTPUT, remote_path):
    raise cloud_storage.NotFoundError(
        'HTML trace %s not found in cloud storage.' % html_url)

  cloud_storage.Get(cloud_storage.TELEMETRY_OUTPUT, remote_path, local_path)
  return local_path


def DownloadProtoTrace(html_url, download_dir=DEFAULT_TRACE_DIR):
  """Downloads the associated proto trace for html trace url. Returns path.

  Skips downloading if file was already downloaded once."""
  local_filename = os.path.join(download_dir, GetLocalTraceFileName(html_url))
  for local_path in [local_filename + '.pb', local_filename + '.pb.gz']:
    if os.path.exists(local_path):
      logging.info('%s already downloaded. Skipping.' % local_path)
      return local_path

  remote_path = FindProtoTracePath(html_url)
  extension = GetFileExtension(remote_path)
  local_path = local_filename + extension

  cloud_storage.Get(cloud_storage.TELEMETRY_OUTPUT, remote_path, local_path)
  return local_path
