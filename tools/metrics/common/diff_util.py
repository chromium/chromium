# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for prompting user if changes automatically applied to some
user-managed files are correct.
"""

from __future__ import print_function

import logging
import os
import sys
import webbrowser

from difflib import HtmlDiff
from tempfile import NamedTemporaryFile


def PromptUserToAcceptDiff(old_text, new_text, prompt):
  """Displays a difference in two strings (old and new file contents) to the
  user and asks whether the new version is acceptable.

  Args:
    old_text: A string containing old file contents.
    new_text: A string containing new file contents.
    prompt: Text that should be displayed to the user, asking whether the new
            file contents should be accepted.

  Returns:
    True is user accepted the changes or there were no changes, False otherwise.
  """
  logging.info('Computing diff...')
  if old_text == new_text:
    logging.info('No changes detected')
    return True
  html_diff = HtmlDiff(wrapcolumn=80).make_file(
      old_text.splitlines(), new_text.splitlines(), fromdesc='Original',
      todesc='Updated', context=True, numlines=5)
  temp = NamedTemporaryFile(suffix='.html', delete=False)
  try:
    html_diff = html_diff.encode()
    temp.write(html_diff)
    temp.close()  # Close the file so the browser process can access it.
    webbrowser.open('file://' + temp.name)
    print(prompt)
    if sys.version_info.major == 2:
      response = raw_input('(Y/n): ').strip().lower()
    else:
      response = input('(Y/n): ').strip().lower()
  finally:
    temp.close()  # May be called on already closed file.
    os.remove(temp.name)
  return response == 'y' or response == ''
