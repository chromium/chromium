# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements Gitiles' simpler auto linking.

This extention auto links basic URLs that aren't bracketed by <...>.

https://gerrit.googlesource.com/gitiles/+/master/java/com/google/gitiles/Linkifier.java
"""

from markdown.inlinepatterns import (AutolinkPattern, Pattern)
from markdown.extensions import Extension


# Best effort attempt to match URLs without matching past the end of the URL.
# The first "[]" is copied from Linkifier.java (safe, reserved, and unsafe
# characters). The second "[]" is similar to the first, but with English
# punctuation removed, since the gitiles parser treats these as punction in the
# sentence, rather than the final character of the URL.
AUTOLINK_RE = (r'(https?://[a-zA-Z0-9$_.+!*\',%;:@=?#/~<>-]+'
               r'[a-zA-Z0-9$_+*\'%@=#/~<-])')


class _GitilesSmartQuotesExtension(Extension):
  """Add Gitiles' simpler linkifier to Markdown."""
  def extendMarkdown(self, md, md_globals):
    md.inlinePatterns.add('gitilesautolink',
                          AutolinkPattern(AUTOLINK_RE, md),
                          '<autolink')


def makeExtension(*args, **kwargs):
  return _GitilesSmartQuotesExtension(*args, **kwargs)
