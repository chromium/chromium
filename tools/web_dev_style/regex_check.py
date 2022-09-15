# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def RegexCheck(re, line_number, line, regex, msg):
  """Searches for |regex| in |line| to check for a particular style
     violation, returning a message like the one below if the regex matches.
     The |regex| must have exactly one capturing group so that the relevant
     part of |line| can be highlighted. If more groups are needed, use
     "(?:...)" to make a non-capturing group. Sample message:

       line 6: Use var instead of const.
           const foo = bar();
           ^^^^^
  """

  def _highlight(match):
    """Takes a start position and a length, and produces a row of '^'s to
       highlight the corresponding part of a string.
    """
    return match.start(1) * ' ' + (match.end(1) - match.start(1)) * '^'

  match = re.search(regex, line)
  if match:
    assert len(match.groups()) == 1
    return '  line %d: %s\n%s\n%s' % (line_number, msg, line, _highlight(match))
  return ''
