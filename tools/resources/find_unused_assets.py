#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds unused assets in Chromium code base.

This script is experimental. While it tries to discover known usages, the assets
identified should still be manually verified. It currently only looks for PNG
files.

The script also ignores several directory trees, such as third_party, docs, and
testing directories.

Usage:
  Either execute the script from the chromium/src directory or specify a
  chromium/src directory via the --src_dir flag.

  Example:
    $ tools/resources/find_unused_assets.py

  To output the list of unused assets to a file, use the --output_unused_files
  flag.

  Example:
    $ tools/resources/find_unused_assets.py \
      --output_unused_files=/tmp/unused_files.txt
"""

import argparse
from concurrent import futures
import functools
from html import parser as html_parser
import itertools
import json
import logging
import os
import pprint
import re
import subprocess
import sys
import threading
from typing import Iterable, List, Optional, Sequence, Set, Text, Tuple
from xml.etree import ElementTree as ET

IGNORED_PATHSPECS = [
    ':!**/docs/**',
    ':!**/pdfium/**',
    ':!**/test/**',
    ':!**/testdata/**',
    ':!**/testing/**',
    ':!**/vectorcanvastest/**',
    ':!**/*unittest/**',
    ':!docs/**',
    ':!native_client_sdk/**',
    ':!testing/**',
    ':!third_party/**',
    ':!tools/perf/page_sets/**',
]

logger = logging.getLogger(__name__)


def init_logger():
  logger.setLevel(logging.DEBUG)
  ch = logging.StreamHandler()
  ch.setLevel(logging.INFO)
  formatter = logging.Formatter(
      '%(levelname)s %(asctime)s %(filename)s:%(lineno)d] %(message)s')
  ch.setFormatter(formatter)
  logger.addHandler(ch)


def parse_args():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--src_dir',
      help='Optional chromium/src directory to analyze. If not specified, '
      'defaults to current working directory.')
  parser.add_argument('--output_unused_files',
                      help='Filepath to output the list of unused files.')
  parser.add_argument('--output_all_png_files',
                      help='Filepath to output the list of all PNG files.')
  return parser.parse_args()


def list_files(pathspecs: Sequence[str]) -> List[str]:
  cmd = ['git', 'ls-files'] + list(pathspecs) + IGNORED_PATHSPECS
  return subprocess.check_output(cmd).decode('utf-8').strip().split('\n')


def get_all_ext_files(extension: str) -> List[str]:
  return list_files([f'**/*.{extension}'])


def find_filepath_usage(fpath: str, segs: int = 1):
  """Finds file usage based on filepath.

  Args:
    fpath: The relative filepath.
    segs: The number of filepath segments to use for checking.

  Returns:
    True if usage found, False otherwise.
  """
  pattern = r'[\\/]'.join(fpath.split(os.sep)[-segs:])
  pattern = pattern.replace('\\', '\\\\')
  pattern = pattern.replace('.', '\\.')
  found = grep_pattern(pattern, pathspecs=tuple(IGNORED_PATHSPECS))
  if not found:
    logger.info('Found unused file: %s', fpath)
  return found


@functools.lru_cache()
def grep_pattern(pattern: Text,
                 pathspecs: Optional[Sequence[Text]] = None,
                 fixed: bool = False) -> List[Text]:
  """Greps for the specified pattern.

  Args:
    pattern: Regex pattern to find.
    pathspecs: Optional sequence of pathspecs to limit the search.
    fixed: Whether the pattern should be treated as a fixed string instead of a
      regex pattern.

  Returns:
    A list of matching filepaths.
  """
  pathspecs = pathspecs or []
  cmd = ['git', 'grep', '-I', '-w', '-l', '-e', pattern]
  if fixed:
    cmd.append('-F')
  cmd.append('--')
  cmd.extend(pathspecs)
  try:
    return subprocess.check_output(cmd).decode('utf-8').strip().split('\n')
  except subprocess.CalledProcessError as ex:
    if ex.returncode == 1:
      return []
    else:
      logger.exception('Got error when grepping: %s', pattern)
      raise


def find_usage_of_java_res(fpath: Text,
                           pathspecs: Optional[Iterable[Text]] = None
                           ) -> List[Text]:
  """Finds usage of a Java image resource.

  Args:
    fpath: The filepath to find.
    pathspecs: Optional pathspecs to look for usages.

  Returns:
    A list of filepaths that use |fpath|.
  """
  pathspecs = pathspecs or []
  common_pathspecs = list(pathspecs) + [':!**/*.gni', ':!**/*.gn'
                                        ] + IGNORED_PATHSPECS
  basename = os.path.basename(fpath).rsplit('.')[0]
  dirname = os.path.dirname(fpath)
  if 'drawable' in dirname:
    res_type = 'drawable'
  elif 'mipmap' in dirname:
    res_type = 'mipmap'
  else:
    raise ValueError(f'Could not parse resource type from filepath: {fpath}')

  pathspec_prefixes = [
      'android_webview',
      'chrome/android',
      'chrome/browser',
      'components',
      'content/public/android',
      'content/shell/android',
      'remoting/android',
      'tools/android',
      'ui/android',
      'weblayer/browser',
      'weblayer/shell',
  ]

  xml_pattern = f'@{res_type}/{basename}'
  found = grep_pattern(
      xml_pattern,
      pathspecs=tuple([p + '/**/*.xml'
                       for p in pathspec_prefixes] + common_pathspecs),
      fixed=True)

  java_pattern = f'R.{res_type}.{basename}'
  found = found or grep_pattern(
      java_pattern,
      pathspecs=tuple([p + '/**/*.java' for p in pathspec_prefixes] + [
          'chrome/browser/android/resource_id.h',
          'components/resources/android/*.h'
      ] + common_pathspecs),
      fixed=True)

  if not found:
    logger.info('Found unused file: %s', fpath)
  return found


def filter_usages_of_java_res(png_files: Iterable[Text], path_pattern: Text
                              ) -> Tuple[Set[Text], Set[Text]]:
  """Filters and finds used and unused Java image resources.

  Args:
    png_files: The paths of image files to filter through and find usages of.
    path_pattern: The regex pattern to filter filepaths.

  Returns:
    A tuple of (used, unused) filepaths.
  """
  used_png_files = set()
  unused_png_files = set()
  containing_files = set()
  matcher = re.compile(path_pattern)
  for png_file in sorted(png_files):
    matches = matcher.search(png_file)
    if not matches:
      continue
    found = find_usage_of_java_res(png_file)
    if found:
      used_png_files.add(png_file)
      containing_files |= set(found)
    else:
      unused_png_files.add(png_file)
  logger.info('Found %d PNG files used under %s dirs.', len(used_png_files),
              path_pattern)
  logger.info('Found %d unused PNG files under %s dirs.', len(unused_png_files),
              path_pattern)
  logger.debug('Containing files:\n%s',
               pprint.pformat(sorted(containing_files)))
  return used_png_files, unused_png_files


def find_images_by_filepath_string(filepaths: Iterable[Text],
                                   all_png_files: Set[Text]
                                   ) -> Tuple[Set[Text], Set[Text]]:
  """Finds images used and maybe unused by looking at filepath strings.

  The maybe unused files are determined by the fact that they're not referenced
  by any of the files in |filepaths| that are in their ancestor directories.

  Args:
    filepaths: The paths of the files that contain filepath strings.
    all_png_files: The set of all PNG files, used to filter results and
      find unused files.

  Returns:
    A tuple of (used_files, maybe_unused_files).
  """
  used_files = set()
  maybe_unused_files = set()
  ignored_files = set()
  png_filepath_matcher = re.compile(r'"([^/][^"]+\.png)"')
  for filepath in filepaths:
    rel_dir = os.path.dirname(filepath)
    used_by_current_file = set()
    with open(filepath, 'rb') as f:
      content = f.read().decode('utf-8')
      for match in png_filepath_matcher.finditer(content):
        img_relpath = match.group(1)
        rooted_img_path = os.path.join(rel_dir, img_relpath)
        if 'test' in os.path.dirname(img_relpath) or '$' in img_relpath:
          logger.debug('Ignoring %s', rooted_img_path)
          ignored_files.add(img_relpath)
          continue
        if rooted_img_path in all_png_files:
          used_by_current_file.add(rooted_img_path)
        else:
          logger.warning('PNG file %s does not exist, reffed in %s as %s',
                         rooted_img_path, filepath, img_relpath)
    used_files |= used_by_current_file
    # Consider any images in subdirectories that were not used by this file as
    # unused. Nested BUILD.gn files will be handled outside the loop.
    maybe_unused_files |= set(
        f for f in all_png_files
        if f.startswith(rel_dir) and f not in used_by_current_file)
  # Handles nested BUILD.gn files.
  maybe_unused_files -= used_files
  maybe_unused_files -= ignored_files
  return used_files, maybe_unused_files


def find_images_used_by_grd(all_png_files: Set[Text]) -> Set[Text]:
  """Finds image files referenced by grd/grdp.

  Args:
    all_png_files: The set of all PNG files, used to filter results.

  Returns:
    A set of image filepaths that are referenced by grd/grdp.
  """
  used_files = set()
  grd_files = set(get_all_ext_files('grd') + get_all_ext_files('grdp'))
  for grd_file in grd_files:
    grd_dir = os.path.dirname(grd_file)
    cur_relpaths = set()
    grd_root = ET.parse(grd_file).getroot()
    for elem in itertools.chain(grd_root.iter('include'),
                                grd_root.iter('structure')):
      relpath = elem.get('file')
      if relpath and relpath.endswith('.png'):
        relpath = relpath.replace('\\', '/')
        relpath = relpath.replace(
            '${input_tools_root_dir}',
            'third_party/google_input_tools/src/chrome/os')
        if relpath.startswith('${root_src_dir}'):
          relpath = relpath[len('${root_src_dir}') + 1]
        if relpath.startswith('/') or '$' in relpath:
          logger.error('When processing %s got weird relpath: %s', grd_file,
                       relpath)
          raise ValueError('Unexpected relpath!')
        rooted_filepath = os.path.normpath(os.path.join(grd_dir, relpath))
        if rooted_filepath in all_png_files:
          used_files.add(rooted_filepath)
        cur_relpaths.add(relpath)
    for relpath in cur_relpaths:
      pattern = re.compile(grd_dir + r'/(default_\d+_percent/)?' + relpath)
      for png_file in all_png_files:
        if pattern.match(png_file):
          used_files.add(png_file)
  return used_files


def find_images_used_by_html(all_png_files: Set[Text]) -> Set[Text]:
  """Finds images used by HTML img tags.

  Args:
    all_png_files: The set of all PNG files, used to filter results.

  Returns:
    A set of image filepaths referenced by HTML img tags.
  """
  current_relpaths = []

  class ImgHTMLParser(html_parser.HTMLParser):
    def handle_starttag(self, tag, attrs):
      if tag != 'img':
        return
      for name, value in attrs:
        if name != 'src' or not value.endswith('.png'):
          continue
        current_relpaths.append(value)

  used_files = set()
  ignored_src_pattern = re.compile(r'^(https?://)')
  html_files = set(get_all_ext_files('html'))
  for html_file in html_files:
    current_relpaths.clear()
    parser = ImgHTMLParser()
    with open(html_file, 'rb') as f:
      try:
        parser.feed(f.read().decode('utf-8'))
      except UnicodeDecodeError:
        logger.error('Failed to decode html file: %s', html_file)
        continue
    html_dir = os.path.dirname(html_file)
    for src_relpath in current_relpaths:
      if ignored_src_pattern.match(src_relpath):
        continue
      rooted_src_path = normalize_resource_url_path(html_dir, src_relpath)
      if '{{static}}' in src_relpath:
        # I don't know how to handle this yet.
        continue
      if rooted_src_path not in all_png_files:
        logger.debug('PNG file %s does not exist, reffed in %s as %s',
                     rooted_src_path, html_file, src_relpath)
        continue
      used_files.add(rooted_src_path)
  return used_files


def find_images_used_by_css(all_png_files: Set[Text]) -> Set[Text]:
  """Finds images used by css.

  Args:
    all_png_files: The set of all PNG files, used to filter results.

  Returns:
    A set of image filepaths referenced by css.
  """
  used_files = set()
  css_url_pattern = re.compile(r'\burl\(([^\)]+\.png)\)')
  # Both .css and .html files can contain css.
  files_with_css = set(get_all_ext_files('css') + get_all_ext_files('html'))
  for css_file in files_with_css:
    css_dir = os.path.dirname(css_file)
    with open(css_file, 'rb') as f:
      for match in css_url_pattern.finditer(f.read().decode('utf-8')):
        url_relpath = match.group(1)
        # TODO(aluh): Figure out if url references in css is relative to the
        # css file (current assumption) or the including html file.
        rooted_url_path = normalize_resource_url_path(css_dir, url_relpath)
        if rooted_url_path not in all_png_files:
          logger.debug('PNG file %s does not exist, reffed in %s as %s',
                       rooted_url_path, css_file, url_relpath)
          continue
        used_files.add(rooted_url_path)
  return used_files


def normalize_resource_url_path(rel_dir: Text, url: Text) -> Text:
  """Joins the relative root directory with the URL path and normalizes it.

  It handles the special case of the 'chrome://' web resource URL.

  Args:
    rel_dir: The relative directory from the source root.
    url: The URL path of the asset.

  Returns:
    The joined and normalized filepath.
  """
  if url.startswith('chrome://'):
    rooted_path = os.path.join('ui/webui', url[len('chrome://'):])
  else:
    rooted_path = os.path.join(rel_dir, url)
  return os.path.normpath(rooted_path)


def find_images_used_by_markdown(all_png_files: Set[Text]) -> Set[Text]:
  """Finds images used by markdown files.

  Args:
    all_png_files: The set of all PNG files, used to filter results.

  Returns:
    A set of image filepaths used by markdown files.
  """
  md_url_pattern = re.compile(r'\(([^\s\)]+\.png)[\s\)]')
  md_files = list_files(['**/README', '**/*.md'])
  used_files = set()
  for md_file in md_files:
    file_dir = os.path.dirname(md_file)
    with open(md_file, 'rb') as f:
      for match in md_url_pattern.finditer(f.read().decode('utf-8')):
        url_relpath = match.group(1)
        if url_relpath.startswith('http'):
          continue
        if url_relpath.startswith('/'):
          rooted_filepath = url_relpath.lstrip('/')
        else:
          rooted_filepath = os.path.join(file_dir, url_relpath)
        rooted_filepath = os.path.normpath(rooted_filepath)
        if rooted_filepath in all_png_files:
          used_files.add(rooted_filepath)
        else:
          logger.warning('PNG file %s does not exist, reffed in %s as %s',
                         rooted_filepath, md_file, url_relpath)
  return used_files


def find_images_used_by_ios_imageset(all_png_files: Set[Text]
                                     ) -> Tuple[Set[Text], Set[Text]]:
  """Finds images used and unused by ios imagesets and friends.

  Args:
    all_png_files: The set of all PNG files, used to filter results.

  Returns:
    A tuple of (used, unused) image filepaths from ios imagesets.
  """
  used_files = set()
  unused_files = set()
  json_files = set(
      get_all_ext_files('imageset/Contents.json') +
      get_all_ext_files('appiconset/Contents.json'))
  for json_file in json_files:
    rel_dir = os.path.dirname(json_file)
    with open(json_file, 'rb') as f:
      j = json.load(f)
      for elem in j.get('images', []):
        img_relpath = elem.get('filename', '')
        if not img_relpath.endswith('.png'):
          continue
        rooted_img_path = os.path.join(rel_dir, img_relpath)
        if rooted_img_path not in all_png_files:
          logger.warning('PNG file %s does not exist, reffed in %s as %s',
                         rooted_img_path, json_file, img_relpath)
          continue
        else:
          used_files.add(rooted_img_path)

  imageset_dirs = set(os.path.dirname(f) for f in json_files)
  imageset_png_files = set()
  for imageset_dir in imageset_dirs:
    imageset_png_files |= set(
        list_files([imageset_dir + '/*.png', imageset_dir + '/**/*.png']))
  unused_files = (imageset_png_files & all_png_files) - used_files

  return used_files, unused_files


def main() -> None:
  init_logger()
  args = parse_args()

  src_dir = os.getcwd()
  if args.src_dir and os.path.isdir(args.src_dir):
    src_dir = os.path.realpath(args.src_dir)
    os.chdir(src_dir)
  logger.info('Searching directory: %s', src_dir)

  # Set of all PNG files. Should not be modified.
  all_png_files = set(get_all_ext_files('png'))
  logger.info('Found %d PNG files total.', len(all_png_files))
  if args.output_all_png_files:
    with open(args.output_all_png_files, 'w') as fout:
      for fp in sorted(all_png_files):
        fout.write(fp)
        fout.write('\n')

  # Working set of files that still need to be examined.
  png_files = set(all_png_files)
  # Files that are used.
  used_png_files = set()
  # Files that are likely to be unused.
  likely_unused_png_files = set()

  # Exclude files used by grd files.
  logger.info('Looking for usages by grd files...')
  used_by_grd = find_images_used_by_grd(all_png_files)
  logger.info('Found %d PNG files used by grd files.', len(used_by_grd))
  used_png_files |= used_by_grd
  png_files -= used_png_files

  # Exclude files used by HTML files.
  logger.info('Looking for usages by html files...')
  used_by_html = find_images_used_by_html(all_png_files)
  logger.info('Found %d PNG files used by html img tags.', len(used_by_html))
  used_png_files |= used_by_html
  png_files -= used_png_files

  # Exclude files used in CSS.
  logger.info('Looking for usages by css files...')
  used_by_css = find_images_used_by_css(all_png_files)
  logger.info('Found %d PNG files used in css.', len(used_by_css))
  used_png_files |= used_by_css
  png_files -= used_png_files

  # Find files used by markdown.
  logger.info('Looking for usages by markdown files...')
  used_by_markdown = find_images_used_by_markdown(all_png_files)
  logger.info('Found %d PNG files used by markdown.', len(used_by_markdown))
  used_png_files |= used_by_markdown
  png_files -= used_png_files

  # Find images used by ios imagesets.
  logger.info('Looking for usages by ios imagesets...')
  used_by_imageset, unused_by_imageset = find_images_used_by_ios_imageset(
      all_png_files)
  logger.info('Found %d PNG files used in json.', len(used_by_imageset))
  used_png_files |= used_by_imageset
  png_files -= used_png_files
  logger.info('Found %d likely unused imageset PNG files.',
              len(unused_by_imageset))
  likely_unused_png_files |= unused_by_imageset
  png_files -= likely_unused_png_files

  # Check for usages by filepath string.
  manifest_pathspecs = ['**/manifest.json', '**/*.gni', '**/BUILD.gn']
  for pathspec in manifest_pathspecs:
    logger.info('Looking for usages with pathspec: %s', pathspec)
    used_by_pathspec, unused_by_pathspec = find_images_by_filepath_string(
        list_files([pathspec]), all_png_files)
    used_png_files |= used_by_pathspec
    png_files -= used_png_files
    likely_unused_png_files |= unused_by_pathspec
    png_files -= likely_unused_png_files
    logger.info('Found %d used PNG files from pathspec: %s',
                len(used_by_pathspec), pathspec)
    logger.info('Found %d likely unused PNG files from pathspec: %s',
                len(unused_by_pathspec), pathspec)

  # Check java resources image files.
  logger.info('Looking for usages by java or android resources...')
  path_patterns = [r'/res\w*/(drawable|mipmap)']
  for path_pattern in path_patterns:
    used_java_res_png_files, unused_java_res_png_files = (
        filter_usages_of_java_res(all_png_files, path_pattern=path_pattern))
    used_png_files |= used_java_res_png_files
    png_files -= used_png_files
    likely_unused_png_files |= unused_java_res_png_files
    png_files -= likely_unused_png_files

  logger.info('Still have %d remaining files:', len(png_files))
  pprint.pprint(sorted(png_files))

  # Check remaining files.
  lock = threading.Lock()
  remaining_used_files = set()
  remaining_unused_files = set()
  logger.info('Searching for remaining files...')

  def find_filepath_usage_wrapper(fpath: Text):
    found = find_filepath_usage(fpath)
    with lock:
      if found:
        remaining_used_files.add(fpath)
      else:
        remaining_unused_files.add(fpath)

  with futures.ThreadPoolExecutor(max_workers=32) as executor:
    num_processed = 0
    for future in futures.as_completed(
        executor.submit(find_filepath_usage_wrapper, fpath)
        for fpath in png_files):
      num_processed += 1
      if num_processed % 20 == 0:
        logger.info('Checked %d files so far.', num_processed)
      if future.exception():
        logger.error('Got exception when finding usage: %s', future.exception())

  logger.info('Found %d used misc PNG files.', len(remaining_used_files))
  logger.info('Found %d unused misc PNG files.', len(remaining_unused_files))
  used_png_files |= remaining_used_files
  likely_unused_png_files |= remaining_unused_files
  png_files -= used_png_files

  # Prune likely unused files.
  logger.info('Pruning likely unused files...')
  likely_unused_png_files -= used_png_files

  # Summarize findings.
  likely_unused_png_files = sorted(likely_unused_png_files)
  logger.info('Summary:')
  logger.info('  Found %d used files out of %d.', len(used_png_files),
              len(all_png_files))
  logger.info('  Found %d likely unused files.', len(likely_unused_png_files))
  pprint.pprint(likely_unused_png_files)

  if args.output_unused_files:
    with open(os.path.realpath(args.output_unused_files), 'w') as fout:
      for fp in likely_unused_png_files:
        fout.write(fp)
        fout.write('\n')
    logger.info('Saved list of likely unused files to: %s',
                args.output_unused_files)


if __name__ == '__main__':
  sys.exit(main())
