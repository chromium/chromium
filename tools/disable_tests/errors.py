# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class UserError(Exception):
  """
  This class is for "expected" errors, which should fail this invocation of the
  tool, contain an error message suitable for printing to the user, but
  shouldn't prompt a message telling the user to file a bug.
  """


class InternalError(Exception):
  """
  This class is for exceptions which indicate a bug or deficiency in the tool,
  and which contain an error message suitable for printing to the user. Errors
  of this type will result in a bug link being shown to the user.
  """
