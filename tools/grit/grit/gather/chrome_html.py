# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prepares a Chrome HTML file by inlining resources and adding references to
high DPI resources and removing references to unsupported scale factors.

This is a small gatherer that takes a HTML file, looks for src attributes
and inlines the specified file, producing one HTML file with no external
dependencies. It recursively inlines the included files. When inlining CSS
image files this script also checks for the existence of high DPI versions
of the inlined file including those on relevant platforms. Unsupported scale
factors are also removed from existing image sets to support explicitly
referencing all available images.
"""


import os
import re

from grit import lazy_re
from grit import util
from grit.format import html_inline
from grit.gather import interface


# Distribution string to replace with distribution.
DIST_SUBSTR = '%DISTRIBUTION%'


# Matches a chrome theme source URL.
_THEME_SOURCE = lazy_re.compile(
    r'(?P<baseurl>chrome://theme/IDR_[A-Z0-9_]*)(?P<query>\?.*)?')
# Pattern for matching CSS url() function.
_CSS_URL_PATTERN = r'url\((?P<quote>"|\'|)(?P<filename>[^"\'()]*)(?P=quote)\)'
# Matches CSS url() functions with the capture group 'filename'.
_CSS_URL = lazy_re.compile(_CSS_URL_PATTERN)
# Matches one or more CSS image urls used in given properties.
_CSS_IMAGE_URLS = lazy_re.compile(
    r'(?P<attribute>content|background|[\w-]*-image):\s*'
        r'(?P<urls>(' + _CSS_URL_PATTERN + r'\s*,?\s*)+)')
# Matches CSS image sets.
_CSS_IMAGE_SETS = lazy_re.compile(
    r'(?P<attribute>content|background|[\w-]*-image):[ ]*'
        r'(-webkit-)?image-set\((?P<images>'
        r'(\s*,?\s*url\((?P<quote>"|\'|)[^"\'()]*(?P=quote)\)[ ]*[0-9.]*x)*)\)',
    re.MULTILINE)
# Matches a single image in a CSS image set with the capture group scale.
_CSS_IMAGE_SET_IMAGE = lazy_re.compile(r'\s*,?\s*'
    r'url\((?P<quote>"|\'|)[^"\'()]*(?P=quote)\)[ ]*(?P<scale>[0-9.]*x)',
    re.MULTILINE)
_HTML_IMAGE_SRC = lazy_re.compile(
    r'<img[^>]+src=\"(?P<filename>[^">]*)\"[^>]*>')

def GetImageList(
    base_path, filename, scale_factors, distribution,
    filename_expansion_function=None):
  """Generate the list of images which match the provided scale factors.

  Takes an image filename and checks for files of the same name in folders
  corresponding to the supported scale factors. If the file is from a
  chrome://theme/ source, inserts supported @Nx scale factors as high DPI
  versions.

  Args:
    base_path: path to look for relative file paths in
    filename: name of the base image file
    scale_factors: a list of the supported scale factors (i.e. ['2x'])
    distribution: string that should replace %DISTRIBUTION%

  Returns:
    array of tuples containing scale factor and image (i.e.
        [('1x', 'image.png'), ('2x', '2x/image.png')]).
  """
  # Any matches for which a chrome URL handler will serve all scale factors
  # can simply request all scale factors.
  theme_match = _THEME_SOURCE.match(filename)
  if theme_match:
    images = [('1x', filename)]
    for scale_factor in scale_factors:
      scale_filename = "%s@%s" % (theme_match.group('baseurl'), scale_factor)
      if theme_match.group('query'):
        scale_filename += theme_match.group('query')
      images.append((scale_factor, scale_filename))
    return images

  if filename.find(':') != -1:
    # filename is probably a URL, only return filename itself.
    return [('1x', filename)]

  filename = filename.replace(DIST_SUBSTR, distribution)
  if filename_expansion_function:
    filename = filename_expansion_function(filename)
  filepath = os.path.join(base_path, filename)
  images = [('1x', filename)]

  for scale_factor in scale_factors:
    # Check for existence of file and add to image set.
    scale_path = os.path.split(os.path.join(base_path, filename))
    scale_image_path = os.path.join(scale_path[0], scale_factor, scale_path[1])
    if os.path.isfile(scale_image_path):
      # HTML/CSS always uses forward slashed paths.
      parts = filename.rsplit('/', 1)
      if len(parts) == 1:
        path = ''
      else:
        path = parts[0] + '/'
      scale_image_name = path + scale_factor + '/' + parts[-1]
      images.append((scale_factor, scale_image_name))
  return images


def GenerateImageSet(images, quote):
  """Generates a image-set for the provided list of images.

  Args:
    images: an array of tuples giving scale factor and file path
            (i.e. [('1x', 'image.png'), ('2x', '2x/image.png')]).
    quote: a string giving the quotation character to use (i.e. "'")

  Returns:
    string giving a image-set rule referencing the provided images.
        (i.e. 'image-set(url('image.png') 1x, url('2x/image.png') 2x)')
  """
  imageset = []
  for (scale_factor, filename) in images:
    imageset.append("url(%s%s%s) %s" % (quote, filename, quote, scale_factor))
  return "image-set(%s)" % (', '.join(imageset))


def UrlToImageSet(
    src_match, base_path, scale_factors, distribution,
    filename_expansion_function=None):
  """Regex replace function which replaces url() with image-set.

  Takes a regex match for url('path'). If the file is local, checks for
  files of the same name in folders corresponding to the supported scale
  factors. If the file is from a chrome://theme/ source, inserts the
  supported @Nx scale factor request. In either case inserts a
  image-set rule to fetch the appropriate image for the current scale factor.

  Args:
    src_match: regex match object from _CSS_URLS
    base_path: path to look for relative file paths in
    scale_factors: a list of the supported scale factors (i.e. ['2x'])
    distribution: string that should replace %DISTRIBUTION%.

  Returns:
    string
  """
  quote = src_match.group('quote')
  filename = src_match.group('filename')
  image_list = GetImageList(
      base_path, filename, scale_factors, distribution,
      filename_expansion_function=filename_expansion_function)

  # Don't modify the source if there is only one image.
  if len(image_list) == 1:
    return src_match.group(0)

  return GenerateImageSet(image_list, quote)


def InsertImageSet(
    src_match, base_path, scale_factors, distribution,
    filename_expansion_function=None):
  """Regex replace function which inserts image-set rules.

  Takes a regex match for `property: url('path')[, url('path')]+`.
  Replaces one or more occurances of the match with image set rules.

  Args:
    src_match: regex match object from _CSS_IMAGE_URLS
    base_path: path to look for relative file paths in
    scale_factors: a list of the supported scale factors (i.e. ['2x'])
    distribution: string that should replace %DISTRIBUTION%.

  Returns:
    string
  """
  attr = src_match.group('attribute')
  urls = _CSS_URL.sub(
      lambda m: UrlToImageSet(m, base_path, scale_factors, distribution,
                              filename_expansion_function),
      src_match.group('urls'))

  return "%s: %s" % (attr, urls)


def InsertImageStyle(
    src_match, base_path, scale_factors, distribution,
    filename_expansion_function=None):
  """Regex replace function which adds a content style to an <img>.

  Takes a regex match from _HTML_IMAGE_SRC and replaces the attribute with a CSS
  style which defines the image set.
  """
  filename = src_match.group('filename')
  image_list = GetImageList(
      base_path, filename, scale_factors, distribution,
      filename_expansion_function=filename_expansion_function)

  # Don't modify the source if there is only one image or image already defines
  # a style.
  if src_match.group(0).find(" style=\"") != -1 or len(image_list) == 1:
    return src_match.group(0)

  return "%s style=\"content: %s;\">" % (src_match.group(0)[:-1],
                                        GenerateImageSet(image_list, "'"))


def InsertImageSets(
    filepath, text, scale_factors, distribution,
    filename_expansion_function=None):
  """Helper function that adds references to external images available in any of
  scale_factors in CSS backgrounds.
  """
  # Add high DPI urls for css attributes: content, background,
  # or *-image or <img src="foo">.
  return _CSS_IMAGE_URLS.sub(
      lambda m: InsertImageSet(
          m, filepath, scale_factors, distribution,
          filename_expansion_function=filename_expansion_function),
      _HTML_IMAGE_SRC.sub(
          lambda m: InsertImageStyle(
              m, filepath, scale_factors, distribution,
              filename_expansion_function=filename_expansion_function),
          text))


def RemoveImagesNotIn(scale_factors, src_match):
  """Regex replace function which removes images for scale factors not in
  scale_factors.

  Takes a regex match for _CSS_IMAGE_SETS. For each image in the group images,
  checks if this scale factor is in scale_factors and if not, removes it.

  Args:
    scale_factors: a list of the supported scale factors (i.e. ['1x', '2x'])
    src_match: regex match object from _CSS_IMAGE_SETS

  Returns:
    string
  """
  attr = src_match.group('attribute')
  images = _CSS_IMAGE_SET_IMAGE.sub(
      lambda m: m.group(0) if m.group('scale') in scale_factors else '',
      src_match.group('images'))
  return "%s: image-set(%s)" % (attr, images)


def RemoveImageSetImages(text, scale_factors):
  """Helper function which removes images in image sets not in the list of
  supported scale_factors.
  """
  return _CSS_IMAGE_SETS.sub(
      lambda m: RemoveImagesNotIn(scale_factors, m), text)


def ProcessImageSets(
    filepath, text, scale_factors, distribution,
    filename_expansion_function=None):
  """Helper function that adds references to external images available in other
  scale_factors and removes images from image-sets in unsupported scale_factors.
  """
  # Explicitly add 1x to supported scale factors so that it is not removed.
  supported_scale_factors = ['1x']
  supported_scale_factors.extend(scale_factors)
  return InsertImageSets(
      filepath,
      RemoveImageSetImages(text, supported_scale_factors),
      scale_factors,
      distribution,
      filename_expansion_function=filename_expansion_function)


class ChromeHtml(interface.GathererBase):
  """Represents an HTML document processed for Chrome WebUI.

  HTML documents used in Chrome WebUI have local resources inlined and
  automatically insert references to high DPI assets used in CSS properties
  with the use of the -webkit-image-set value. References to unsupported scale
  factors in image sets are also removed. This does not generate any
  translateable messages and instead generates a single DataPack resource.
  """

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self.allow_external_script_ = False
    self.flatten_html_ = False
    self.preprocess_only_ = False
    # 1x resources are implicitly already in the source and do not need to be
    # added.
    self.scale_factors_ = []
    self.filename_expansion_function = None

  def SetAttributes(self, attrs):
    self.allow_external_script_ = ('allowexternalscript' in attrs and
                                   attrs['allowexternalscript'] == 'true')
    self.preprocess_only_ = ('preprocess' in attrs and
                             attrs['preprocess'] == 'true')
    self.flatten_html_ = (self.preprocess_only_ or ('flattenhtml' in attrs and
                           attrs['flattenhtml'] == 'true'))

  def SetDefines(self, defines):
    if 'scale_factors' in defines:
      self.scale_factors_ = defines['scale_factors'].split(',')

  def GetText(self):
    """Returns inlined text of the HTML document."""
    return self.inlined_text_

  def GetTextualIds(self):
    return [self.extkey]

  def GetData(self, lang, encoding):
    """Returns inlined text of the HTML document."""
    ret = self.inlined_text_
    if encoding == util.BINARY:
      ret = ret.encode('utf-8')
    return ret

  def GetHtmlResourceFilenames(self):
    """Returns a set of all filenames inlined by this file."""
    if self.flatten_html_:
      return html_inline.GetResourceFilenames(
          self.grd_node.ToRealPath(self.GetInputPath()),
          self.grd_node,
          allow_external_script=self.allow_external_script_,
          rewrite_function=lambda fp, t, d: ProcessImageSets(
              fp, t, self.scale_factors_, d,
              filename_expansion_function=self.filename_expansion_function),
          filename_expansion_function=self.filename_expansion_function)
    return []

  def Translate(self, lang, pseudo_if_not_available=True,
                skeleton_gatherer=None, fallback_to_english=False):
    """Returns this document translated."""
    return self.inlined_text_

  def SetFilenameExpansionFunction(self, fn):
    self.filename_expansion_function = fn

  def Parse(self):
    """Parses and inlines the represented file."""

    filename = self.GetInputPath()
    # If there is a grd_node, prefer its GetInputPath(), as that may do more
    # processing to make the call to ToRealPath() below work correctly.
    if self.grd_node:
      filename = self.grd_node.GetInputPath()
    if self.filename_expansion_function:
      filename = self.filename_expansion_function(filename)
    # Hack: some unit tests supply an absolute path and no root node.
    if not os.path.isabs(filename):
      filename = self.grd_node.ToRealPath(filename)
    if self.flatten_html_:
      self.inlined_text_ = html_inline.InlineToString(
          filename,
          self.grd_node,
          allow_external_script = self.allow_external_script_,
          strip_whitespace=True,
          preprocess_only = self.preprocess_only_,
          rewrite_function=lambda fp, t, d: ProcessImageSets(
              fp, t, self.scale_factors_, d,
              filename_expansion_function=self.filename_expansion_function),
          filename_expansion_function=self.filename_expansion_function)
    else:
      distribution = html_inline.GetDistribution()
      self.inlined_text_ = ProcessImageSets(
          os.path.dirname(filename),
          util.ReadFile(filename, 'utf-8'),
          self.scale_factors_,
          distribution,
          filename_expansion_function=self.filename_expansion_function)
