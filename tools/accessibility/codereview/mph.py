#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
  Upload comments to gerrit.

  mph = "make Peter happy".
  Previously called mch ("make Casey happy").
"""

import optparse
import re
import sys

import codereview_parser

try:
  import gerrit_util
  import git_cl
except:
  print('depot_tools not found; try appending the module path to your' +
        ' python path')
  sys.exit(1)


def DieWithError(message):
  print(message, file=sys.stderr)
  sys.exit(1)


# These are additions to gerrit_util.py from depot_tools not yet ready to send
# out for review.
def CreateDraft(host, change, revision, path, line, msg=''):
  """Create a draft  gerrit comment."""
  path = 'changes/%s/revisions/%s/drafts' % (change, revision)
  body = {'path': path, 'line': line, 'message': msg, 'unresolved': True}
  conn = gerrit_util.CreateHttpConn(host, path, reqtype='PUT', body=body)
  return gerrit_util.ReadHttpJsonResponse(conn)


def SetReview(host, change, revision, msg, lgtm, comments):
  """Sets a review  in gerrit."""
  path = 'changes/%s/revisions/%s/review' % (change, revision)
  body = {'message': msg, 'comments': comments}
  if lgtm:
    body['labels'] = {'code-review': 1}

  conn = gerrit_util.CreateHttpConn(host, path, reqtype='POST', body=body)
  return gerrit_util.ReadHttpJsonResponse(conn)


class GerritParser(codereview_parser.Parser):

  def __init__(self, file):
    codereview_parser.Parser.__init__(self, file)
    self._HOST = 'chromium-review.googlesource.com'
    self._issue_number = 0
    self._patchset = 0
    self._change_id = ''
    self._revision_id = ""
    self._overall_comment = ''
    self._comments = {}

  def OnError(self, msg):
    DieWithError(msg)

  def OnPreambleLine(self, line):
    matcher = re.match('Issue: (\d+), patchset: (\d+)', line)
    if matcher:
      self._issue_number = int(matcher.groups()[0])
      self._patchset = int(matcher.groups()[1])
      self._change_id = gerrit_util.GetChange(self._HOST,
                                              self._issue_number)['change_id']
      self._revision_id = gerrit_util.GetChangeCurrentRevision(
          self._HOST, self._change_id)[0]['current_revision']

  def OnFinishPreamble(self):
    pass

  def OnOverallComment(self, comment):
    self._overall_comment = comment

  def OnFileComment(self, path, line, text, comment):
    if not path in self._comments:
      self._comments[path] = []

    self._comments[path].append({
        'message': comment,
        'line': line,
        'unresolved': True
    })

  def OnParseFinished(self):
    is_lg = re.match(".*lgtm.*", self._overall_comment, re.IGNORECASE)
    SetReview(self._HOST, self._change_id, self._revision_id,
              self._overall_comment, is_lg, self._comments)
    print('Done')

  def Parse(self):
    codereview_parser.Parser.Parse(self)
    self.OnParseFinished()

  def add_comment(self, issue, message, add_as_reviewer=False):
    max_message = 10000
    tail = '...\n(message too large)'
    if len(message) > max_message:
      message = message[:max_message - len(tail)] + tail

    issue_props = self._rd.get_issue_properties(self._issue_number, None)
    reviewers = ','.join(issue_props['reviewers'])
    cc = ','.join(issue_props['cc'])
    self._rd.post('/%d/publish' % issue,
                  [('xsrf_token', self._rd.xsrf_token()), ('message', message),
                   ('message_only', 'False'),
                   ('add_as_reviewer', str(bool(add_as_reviewer))),
                   ('reviewers', reviewers), ('cc', cc), ('send_mail', 'True'),
                   ('no_redirect', 'True')])


def ProcessFile(file):
  parser = GerritParser(file)
  parser.Parse()


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('-f', '--file')
  options, args = parser.parse_args(argv)
  file_name = options.file
  if file_name is None:
    if len(argv) != 1:
      parser.print_help()
      DieWithError('Review file not specified')
    file_name = argv[0]
  with open(file_name) as file:
    ProcessFile(file)


if __name__ == '__main__':
  main(sys.argv[1:])
