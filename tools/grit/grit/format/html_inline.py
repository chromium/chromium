#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Flattens a HTML file by inlining its external resources.

This is a small script that takes a HTML file, looks for src attributes
and inlines the specified file, producing one HTML file with no external
dependencies. It recursively inlines the included files.
"""

from __future__ import print_function

import os
import re
import sys
import base64
import mimetypes

from grit import lazy_re
from grit import util
from grit.format import minifier

# There is a python bug that makes mimetypes crash if the Windows
# registry contains non-Latin keys ( http://bugs.python.org/issue9291
# ). Initing manually and blocking external mime-type databases will
# prevent that bug and if we add svg manually, it will still give us
# the data we need.
mimetypes.init([])
mimetypes.add_type('image/svg+xml', '.svg')

# webm video type is not always available if mimetype package is outdated.
mimetypes.add_type('video/webm', '.webm')

DIST_DEFAULT = 'chromium'
DIST_ENV_VAR = 'CHROMIUM_BUILD'
DIST_SUBSTR = '%DISTRIBUTION%'

# Matches beginning of an "if" block.
_BEGIN_IF_BLOCK = lazy_re.compile(
    r'<if [^>]*?expr=("(?P<expr1>[^">]*)"|\'(?P<expr2>[^\'>]*)\')[^>]*?>')

# Matches ending of an "if" block.
_END_IF_BLOCK = lazy_re.compile(r'</if>')

# Used by DoInline to replace various links with inline content.
_STYLESHEET_RE = lazy_re.compile(
    r'<link rel="stylesheet"[^>]+?href="(?P<filename>[^"]*)".*?>(\s*</link>)?',
    re.DOTALL)
_INCLUDE_RE = lazy_re.compile(
    r'(?P<comment>\/\/ )?<include[^>]+?'
    r'src=("(?P<file1>[^">]*)"|\'(?P<file2>[^\'>]*)\').*?>(\s*</include>)?',
    re.DOTALL)
_SRC_RE = lazy_re.compile(
    r'<(?!script)(?:[^>]+?\s)src="(?!\[\[|{{)(?P<filename>[^"\']*)"',
    re.MULTILINE)
# This re matches '<img srcset="..."' or '<source srcset="..."'
_SRCSET_RE = lazy_re.compile(
    r'<(img|source)\b(?:[^>]*?\s)srcset="(?!\[\[|{{|\$i18n{)'
    r'(?P<srcset>[^"\']*)"',
    re.MULTILINE)
# This re is for splitting srcset value string into "image candidate strings".
# Notes:
# - HTML 5.2 states that URL cannot start or end with comma.
# - the "descriptor" is either "width descriptor" or "pixel density descriptor".
#   The first one consists of "valid non-negative integer + letter 'x'",
#   the second one is formed of "positive valid floating-point number +
#   letter 'w'". As a reasonable compromise, we match a list of characters
#   that form both of them.
# Matches for example "img2.png 2x" or "img9.png 11E-2w".
_SRCSET_ENTRY_RE = lazy_re.compile(
    r'\s*(?P<url>[^,\s]\S+[^,\s])'
    r'(?:\s+(?P<descriptor>[\deE.-]+[wx]))?\s*'
    r'(?P<separator>,|$)',
    re.MULTILINE)
_ICON_RE = lazy_re.compile(
    r'<link rel="icon"\s(?:[^>]+?\s)?'
    r'href=(?P<quote>")(?P<filename>[^"\']*)\1',
    re.MULTILINE)


def GetDistribution():
  """Helper function that gets the distribution we are building.

  Returns:
    string
  """
  distribution = DIST_DEFAULT
  if DIST_ENV_VAR in os.environ:
    distribution = os.environ[DIST_ENV_VAR]
    if len(distribution) > 1 and distribution[0] == '_':
      distribution = distribution[1:].lower()
  return distribution

def ConvertFileToDataURL(filename, base_path, distribution, inlined_files,
    names_only):
  """Convert filename to inlined data URI.

  Takes a filename from ether "src" or "srcset", and attempts to read the file
  at 'filename'. Returns data URI as string with given file inlined.
  If it finds DIST_SUBSTR string in file name, replaces it with distribution.
  If filename contains ':', it is considered URL and not translated.

  Args:
    filename: filename string from ether src or srcset attributes.
    base_path: path that to look for files in
    distribution: string that should replace DIST_SUBSTR
    inlined_files: The name of the opened file is appended to this list.
    names_only: If true, the function will not read the file but just return "".
                It will still add the filename to |inlined_files|.

  Returns:
    string
  """
  if filename.find(':') != -1:
    # filename is probably a URL, which we don't want to bother inlining
    return filename

  filename = filename.replace(DIST_SUBSTR , distribution)
  filepath = os.path.normpath(os.path.join(base_path, filename))
  inlined_files.add(filepath)

  if names_only:
    return ""

  mimetype = mimetypes.guess_type(filename)[0]
  if mimetype is None:
    raise Exception('%s is of an an unknown type and '
                    'cannot be stored in a data url.' % filename)
  inline_data = base64.standard_b64encode(util.ReadFile(filepath, util.BINARY))
  return 'data:%s;base64,%s' % (mimetype, inline_data)


def SrcInlineAsDataURL(
    src_match, base_path, distribution, inlined_files, names_only=False,
    filename_expansion_function=None):
  """regex replace function.

  Takes a regex match for src="filename", attempts to read the file
  at 'filename' and returns the src attribute with the file inlined
  as a data URI. If it finds DIST_SUBSTR string in file name, replaces
  it with distribution.

  Args:
    src_match: regex match object with 'filename' named capturing group
    base_path: path that to look for files in
    distribution: string that should replace DIST_SUBSTR
    inlined_files: The name of the opened file is appended to this list.
    names_only: If true, the function will not read the file but just return "".
                It will still add the filename to |inlined_files|.

  Returns:
    string
  """
  filename = src_match.group('filename')
  if filename_expansion_function:
    filename = filename_expansion_function(filename)

  data_url = ConvertFileToDataURL(filename, base_path, distribution,
                                  inlined_files, names_only)

  if not data_url:
    return data_url

  prefix = src_match.string[src_match.start():src_match.start('filename')]
  suffix = src_match.string[src_match.end('filename'):src_match.end()]
  return prefix + data_url + suffix

def SrcsetInlineAsDataURL(
    srcset_match, base_path, distribution, inlined_files, names_only=False,
    filename_expansion_function=None):
  """regex replace function to inline files in srcset="..." attributes

  Takes a regex match for srcset="filename 1x, filename 2x, ...", attempts to
  read the files referenced by filenames and returns the srcset attribute with
  the files inlined as a data URI. If it finds DIST_SUBSTR string in file name,
  replaces it with distribution.

  Args:
    srcset_match: regex match object with 'srcset' named capturing group
    base_path: path that to look for files in
    distribution: string that should replace DIST_SUBSTR
    inlined_files: The name of the opened file is appended to this list.
    names_only: If true, the function will not read the file but just return "".
                It will still add the filename to |inlined_files|.

  Returns:
    string
  """
  srcset = srcset_match.group('srcset')

  if not srcset:
    return srcset_match.group(0)

  # HTML 5.2 defines srcset as a list of "image candidate strings".
  # Each of them consists of URL and descriptor.
  # _SRCSET_ENTRY_RE splits srcset into a list of URLs, descriptors and
  # commas.
  # The descriptor part will be None if that optional regex didn't match
  parts = _SRCSET_ENTRY_RE.split(srcset)

  if not parts:
    return srcset_match.group(0)

  # List of image candidate strings that will form new srcset="..."
  new_candidates = []

  # When iterating over split srcset we fill this parts of a single image
  # candidate string: [url, descriptor]
  candidate = [];

  # Each entry should consist of some text before the entry, the url,
  # the descriptor or None if the entry has no descriptor, a comma separator or
  # the end of the line, and finally some text after the entry (which is the
  # same as the text before the next entry).
  for i in range(0, len(parts) - 1, 4):
    before, url, descriptor, separator, after = parts[i:i+5]

    # There must be a comma-separated next entry or this must be the last entry.
    assert separator == "," or (separator == "" and i == len(parts) - 5), (
           "Bad srcset format in {}".format(srcset_match.group(0)))
    # Both before and after the entry must be empty
    assert before == after == "", (
           "Bad srcset format in {}".format(srcset_match.group(0)))

    if filename_expansion_function:
      filename = filename_expansion_function(url)
    else:
      filename = url

    data_url = ConvertFileToDataURL(filename, base_path, distribution,
                                    inlined_files, names_only)

    # This is not "names_only" mode
    if data_url:
      candidate = [data_url]
      if descriptor:
        candidate.append(descriptor)

      new_candidates.append(" ".join(candidate))

  prefix = srcset_match.string[srcset_match.start():
      srcset_match.start('srcset')]
  suffix = srcset_match.string[srcset_match.end('srcset'):srcset_match.end()]
  return prefix + ','.join(new_candidates) + suffix

class InlinedData:
  """Helper class holding the results from DoInline().

  Holds the inlined data and the set of filenames of all the inlined
  files.
  """
  def __init__(self, inlined_data, inlined_files):
    self.inlined_data = inlined_data
    self.inlined_files = inlined_files

def DoInline(
    input_filename, grd_node, allow_external_script=False,
    preprocess_only=False, names_only=False, strip_whitespace=False,
    rewrite_function=None, filename_expansion_function=None):
  """Helper function that inlines the resources in a specified file.

  Reads input_filename, finds all the src attributes and attempts to
  inline the files they are referring to, then returns the result and
  the set of inlined files.

  Args:
    input_filename: name of file to read in
    grd_node: html node from the grd file for this include tag
    preprocess_only: Skip all HTML processing, only handle <if> and <include>.
    names_only: |nil| will be returned for the inlined contents (faster).
    strip_whitespace: remove whitespace and comments in the input files.
    rewrite_function: function(filepath, text, distribution) which will be
        called to rewrite html content before inlining images.
    filename_expansion_function: function(filename) which will be called to
        rewrite filenames before attempting to read them.
  Returns:
    a tuple of the inlined data as a string and the set of filenames
    of all the inlined files
  """
  if filename_expansion_function:
    input_filename = filename_expansion_function(input_filename)
  input_filepath = os.path.dirname(input_filename)
  distribution = GetDistribution()

  # Keep track of all the files we inline.
  inlined_files = set()

  def SrcReplace(src_match, filepath=input_filepath,
                 inlined_files=inlined_files):
    """Helper function to provide SrcInlineAsDataURL with the base file path"""
    return SrcInlineAsDataURL(
        src_match, filepath, distribution, inlined_files, names_only=names_only,
        filename_expansion_function=filename_expansion_function)

  def SrcsetReplace(srcset_match, filepath=input_filepath,
                 inlined_files=inlined_files):
    """Helper function to provide SrcsetInlineAsDataURL with the base file
    path.
    """
    return SrcsetInlineAsDataURL(
        srcset_match, filepath, distribution, inlined_files,
        names_only=names_only,
        filename_expansion_function=filename_expansion_function)

  def GetFilepath(src_match, base_path = input_filepath):
    filename = [v for k, v in src_match.groupdict().items()
                if k.startswith('file') and v][0]

    if filename.find(':') != -1:
      # filename is probably a URL, which we don't want to bother inlining
      return None

    filename = filename.replace('%DISTRIBUTION%', distribution)
    if filename_expansion_function:
      filename = filename_expansion_function(filename)
    return os.path.normpath(os.path.join(base_path, filename))

  def IsConditionSatisfied(src_match):
    expr1 = src_match.group('expr1') or ''
    expr2 = src_match.group('expr2') or ''
    return grd_node is None or grd_node.EvaluateCondition(expr1 + expr2)

  def CheckConditionalElements(str):
    """Helper function to conditionally inline inner elements"""
    while True:
      begin_if = _BEGIN_IF_BLOCK.search(str)
      if begin_if is None:
        if _END_IF_BLOCK.search(str) is not None:
          raise Exception('Unmatched </if>')
        return str

      condition_satisfied = IsConditionSatisfied(begin_if)
      leading = str[0:begin_if.start()]
      content_start = begin_if.end()

      # Find matching "if" block end.
      count = 1
      pos = begin_if.end()
      while True:
        end_if = _END_IF_BLOCK.search(str, pos)
        if end_if is None:
          raise Exception('Unmatched <if>')

        next_if = _BEGIN_IF_BLOCK.search(str, pos)
        if next_if is None or next_if.start() >= end_if.end():
          count = count - 1
          if count == 0:
            break
          pos = end_if.end()
        else:
          count = count + 1
          pos = next_if.end()

      content = str[content_start:end_if.start()]
      trailing = str[end_if.end():]

      if condition_satisfied:
        str = leading + CheckConditionalElements(content) + trailing
      else:
        str = leading + trailing

  def InlineFileContents(src_match,
                         pattern,
                         inlined_files=inlined_files,
                         strip_whitespace=False):
    """Helper function to inline external files of various types"""
    filepath = GetFilepath(src_match)
    if filepath is None:
      return src_match.group(0)
    inlined_files.add(filepath)

    if names_only:
      inlined_files.update(GetResourceFilenames(
          filepath,
          grd_node,
          allow_external_script,
          rewrite_function,
          filename_expansion_function=filename_expansion_function))
      return ""
    # To recursively save inlined files, we need InlinedData instance returned
    # by DoInline.
    inlined_data_inst=DoInline(filepath, grd_node,
        allow_external_script=allow_external_script,
        strip_whitespace=strip_whitespace,
        filename_expansion_function=filename_expansion_function)

    inlined_files.update(inlined_data_inst.inlined_files)

    return pattern % inlined_data_inst.inlined_data;


  def InlineIncludeFiles(src_match):
    """Helper function to directly inline generic external files (without
       wrapping them with any kind of tags).
    """
    return InlineFileContents(src_match, '%s')

  def InlineScript(match):
    """Helper function to inline external script files"""
    attrs = (match.group('attrs1') + match.group('attrs2')).strip()
    if attrs:
      attrs = ' ' + attrs
    return InlineFileContents(match, '<script' + attrs + '>%s</script>',
                              strip_whitespace=True)

  def InlineCSSText(text, css_filepath):
    """Helper function that inlines external resources in CSS text"""
    filepath = os.path.dirname(css_filepath)
    # Allow custom modifications before inlining images.
    if rewrite_function:
      text = rewrite_function(filepath, text, distribution)
    text = InlineCSSImages(text, filepath)
    return InlineCSSImports(text, filepath)

  def InlineCSSFile(src_match, pattern, base_path=input_filepath):
    """Helper function to inline external CSS files.

    Args:
      src_match: A regular expression match with a named group named "filename".
      pattern: The pattern to replace with the contents of the CSS file.
      base_path: The base path to use for resolving the CSS file.

    Returns:
      The text that should replace the reference to the CSS file.
    """
    filepath = GetFilepath(src_match, base_path)
    if filepath is None:
      return src_match.group(0)

    # Even if names_only is set, the CSS file needs to be opened, because it
    # can link to images that need to be added to the file set.
    inlined_files.add(filepath)

    # Inline stylesheets included in this css file.
    text = _INCLUDE_RE.sub(InlineIncludeFiles,
                           util.ReadFile(filepath, util.BINARY))
    # When resolving CSS files we need to pass in the path so that relative URLs
    # can be resolved.

    return pattern % InlineCSSText(text, filepath)

  def GetUrlRegexString(postfix=''):
    """Helper function that returns a string for a regex that matches url('')
       but not url([[ ]]) or url({{ }}). Appends |postfix| to group names.
    """
    url_re = (r'url\((?!\[\[|{{)(?P<q%s>"|\'|)(?P<filename%s>[^"\'()]*)'
              r'(?P=q%s)\)')
    return url_re % (postfix, postfix, postfix)

  def InlineCSSImages(text, filepath=input_filepath):
    """Helper function that inlines external images in CSS backgrounds."""
    # Replace contents of url() for css attributes: content, background,
    # or *-image.
    property_re = r'(content|background|[\w-]*-image):[^;]*'
    # Replace group names to prevent duplicates when forming value_re.
    image_set_value_re = (r'image-set\(([ ]*' + GetUrlRegexString('2') +
        r'[ ]*[0-9.]*x[ ]*(,[ ]*)?)+\)')
    value_re = '(%s|%s)' % (GetUrlRegexString(), image_set_value_re)
    css_re = property_re + value_re
    return re.sub(css_re, lambda m: InlineCSSUrls(m, filepath), text)

  def InlineCSSUrls(src_match, filepath=input_filepath):
    """Helper function that inlines each url on a CSS image rule match."""
    # Replace contents of url() references in matches.
    return re.sub(GetUrlRegexString(),
                  lambda m: SrcReplace(m, filepath),
                  src_match.group(0))

  def InlineCSSImports(text, filepath=input_filepath):
    """Helper function that inlines CSS files included via the @import
       directive.
    """
    return re.sub(r'@import\s+' + GetUrlRegexString() + r';',
                  lambda m: InlineCSSFile(m, '%s', filepath),
                  text)


  flat_text = util.ReadFile(input_filename, util.BINARY)

  # Check conditional elements, remove unsatisfied ones from the file. We do
  # this twice. The first pass is so that we don't even bother calling
  # InlineScript, InlineCSSFile and InlineIncludeFiles on text we're eventually
  # going to throw out anyway.
  flat_text = CheckConditionalElements(flat_text)

  flat_text = _INCLUDE_RE.sub(InlineIncludeFiles, flat_text)

  if not preprocess_only:
    if strip_whitespace:
      flat_text = minifier.Minify(flat_text, input_filename)

    if not allow_external_script:
      # We need to inline css and js before we inline images so that image
      # references gets inlined in the css and js
      flat_text = re.sub(r'<script (?P<attrs1>.*?)src="(?P<filename>[^"\']*)"'
                         r'(?P<attrs2>.*?)></script>',
                         InlineScript,
                         flat_text)

    flat_text = _STYLESHEET_RE.sub(
        lambda m: InlineCSSFile(m, '<style>%s</style>'),
        flat_text)

  # Check conditional elements, second pass. This catches conditionals in any
  # of the text we just inlined.
  flat_text = CheckConditionalElements(flat_text)

  # Allow custom modifications before inlining images.
  if rewrite_function:
    flat_text = rewrite_function(input_filepath, flat_text, distribution)

  if not preprocess_only:
    flat_text = _SRC_RE.sub(SrcReplace, flat_text)
    flat_text = _SRCSET_RE.sub(SrcsetReplace, flat_text)

    # TODO(arv): Only do this inside <style> tags.
    flat_text = InlineCSSImages(flat_text)

    flat_text = _ICON_RE.sub(SrcReplace, flat_text)

  if names_only:
    flat_text = None  # Will contains garbage if the flag is set anyway.
  return InlinedData(flat_text, inlined_files)


def InlineToString(input_filename, grd_node, preprocess_only = False,
                   allow_external_script=False, strip_whitespace=False,
                   rewrite_function=None, filename_expansion_function=None):
  """Inlines the resources in a specified file and returns it as a string.

  Args:
    input_filename: name of file to read in
    grd_node: html node from the grd file for this include tag
  Returns:
    the inlined data as a string
  """
  try:
    return DoInline(
        input_filename,
        grd_node,
        preprocess_only=preprocess_only,
        allow_external_script=allow_external_script,
        strip_whitespace=strip_whitespace,
        rewrite_function=rewrite_function,
        filename_expansion_function=filename_expansion_function).inlined_data
  except IOError as e:
    raise Exception("Failed to open %s while trying to flatten %s. (%s)" %
                    (e.filename, input_filename, e.strerror))


def InlineToFile(input_filename, output_filename, grd_node):
  """Inlines the resources in a specified file and writes it.

  Reads input_filename, finds all the src attributes and attempts to
  inline the files they are referring to, then writes the result
  to output_filename.

  Args:
    input_filename: name of file to read in
    output_filename: name of file to be written to
    grd_node: html node from the grd file for this include tag
  Returns:
    a set of filenames of all the inlined files
  """
  inlined_data = InlineToString(input_filename, grd_node)
  with open(output_filename, 'wb') as out_file:
    out_file.write(inlined_data)


def GetResourceFilenames(filename,
                         grd_node,
                         allow_external_script=False,
                         rewrite_function=None,
                         filename_expansion_function=None):
  """For a grd file, returns a set of all the files that would be inline."""
  try:
    return DoInline(
        filename,
        grd_node,
        names_only=True,
        preprocess_only=False,
        allow_external_script=allow_external_script,
        strip_whitespace=False,
        rewrite_function=rewrite_function,
        filename_expansion_function=filename_expansion_function).inlined_files
  except IOError as e:
    raise Exception("Failed to open %s while trying to flatten %s. (%s)" %
                    (e.filename, filename, e.strerror))


def main():
  if len(sys.argv) <= 2:
    print("Flattens a HTML file by inlining its external resources.\n")
    print("html_inline.py inputfile outputfile")
  else:
    InlineToFile(sys.argv[1], sys.argv[2], None)

if __name__ == '__main__':
  main()
