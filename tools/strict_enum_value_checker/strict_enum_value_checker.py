# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class StrictEnumValueChecker(object):
  """Verify that changes to enums are valid.

  This class is used to check enums where reordering or deletion is not allowed,
  and additions must be at the end of the enum, just prior to some "boundary"
  entry. See comments at the top of the "extension_function_histogram_value.h"
  file in chrome/browser/extensions for an example what are considered valid
  changes. There are situations where this class gives false positive warnings,
  i.e. it warns even though the edit is legitimate. Since the class warns using
  prompt warnings, the user can always choose to continue. The main point is to
  attract the attention to all (potentially or not) invalid edits.

  """
  def __init__(self, input_api, output_api, start_marker, end_marker, path):
    self.input_api = input_api
    self.output_api = output_api
    self.start_marker = start_marker
    self.end_marker = end_marker
    self.path = path
    self.results = []

  class EnumRange(object):
    """Represents a range of line numbers (1-based)"""
    def __init__(self, first_line, last_line):
      self.first_line = first_line
      self.last_line = last_line

    def Count(self):
      return self.last_line - self.first_line + 1

    def Contains(self, line_num):
      return self.first_line <= line_num and line_num <= self.last_line

  def LogInfo(self, message):
    self.input_api.logging.info(message)
    return

  def LogDebug(self, message):
    self.input_api.logging.debug(message)
    return

  def ComputeEnumRangeInContents(self, contents):
    """Returns an |EnumRange| object representing the line extent of the
    enum members in |contents|. The line numbers are 1-based,
    compatible with line numbers returned by AffectedFile.ChangeContents().
    |contents| is a list of strings reprenting the lines of a text file.

    If either start_marker or end_marker cannot be found in
    |contents|, returns None and emits detailed warnings about the problem.

    """
    first_enum_line = 0
    last_enum_line = 0
    line_num = 1  # Line numbers are 1-based
    for line in contents:
      if line.startswith(self.start_marker):
        first_enum_line = line_num + 1
      elif line.startswith(self.end_marker):
        last_enum_line = line_num
      line_num += 1

    if first_enum_line == 0:
      self.EmitWarning("The presubmit script could not find the start of the "
                       "enum definition (\"%s\"). Did the enum definition "
                       "change?" % self.start_marker)
      return None

    if last_enum_line == 0:
      self.EmitWarning("The presubmit script could not find the end of the "
                       "enum definition (\"%s\"). Did the enum definition "
                       "change?" % self.end_marker)
      return None

    if first_enum_line >= last_enum_line:
      self.EmitWarning("The presubmit script located the start of the enum "
                       "definition (\"%s\" at line %d) *after* its end "
                       "(\"%s\" at line %d). Something is not quite right."
                       % (self.start_marker, first_enum_line,
                          self.end_marker, last_enum_line))
      return None

    self.LogInfo("Line extent of (\"%s\") enum definition: "
                 "first_line=%d, last_line=%d."
                 % (self.start_marker, first_enum_line, last_enum_line))
    return self.EnumRange(first_enum_line, last_enum_line)

  def ComputeEnumRangeInNewFile(self, affected_file):
    return self.ComputeEnumRangeInContents(affected_file.NewContents())

  def GetLongMessage(self, local_path):
    return str("The file \"%s\" contains the definition of the "
               "(\"%s\") enum which should be edited in specific ways "
               "only - *** read the comments at the top of the header file ***"
               ". There are changes to the file that may be incorrect and "
               "warrant manual confirmation after review. Note that this "
               "presubmit script can not reliably report the nature of all "
               "types of invalid changes, especially when the diffs are "
               "complex. For example, an invalid deletion may be reported "
               "whereas the change contains a valid rename."
               % (local_path, self.start_marker))

  def EmitWarning(self, message, line_number=None, line_text=None):
    """Emits a presubmit prompt warning containing the short message
    |message|. |item| is |LOCAL_PATH| with optional |line_number| and
    |line_text|.

    """
    if line_number is not None and line_text is not None:
      item = "%s(%d): %s" % (self.path, line_number, line_text)
    elif line_number is not None:
      item = "%s(%d)" % (self.path, line_number)
    else:
      item = self.path
    long_message = self.GetLongMessage(self.path)
    self.LogInfo(message)
    self.results.append(
      self.output_api.PresubmitPromptWarning(message, [item], long_message))

  def CollectRangesInsideEnumDefinition(self, affected_file,
                                        first_line, last_line):
    """Returns a list of triplet (line_start, line_end, line_text) of ranges of
    edits changes. The |line_text| part is the text at line |line_start|.
    Since it used only for reporting purposes, we do not need all the text
    lines in the range.

    """
    results = []
    previous_line_number = 0
    previous_range_start_line_number = 0
    previous_range_start_text = ""

    def addRange():
      tuple = (previous_range_start_line_number,
               previous_line_number,
               previous_range_start_text)
      results.append(tuple)

    for line_number, line_text in affected_file.ChangedContents():
      if first_line <= line_number and line_number <= last_line:
        self.LogDebug("Line change at line number " + str(line_number) + ": " +
                      line_text)
        # Start a new interval if none started
        if previous_range_start_line_number == 0:
          previous_range_start_line_number = line_number
          previous_range_start_text = line_text
        # Add new interval if we reached past the previous one
        elif line_number != previous_line_number + 1:
          addRange()
          previous_range_start_line_number = line_number
          previous_range_start_text = line_text
        previous_line_number = line_number

    # Add a last interval if needed
    if previous_range_start_line_number != 0:
        addRange()
    return results

  def CheckForFileDeletion(self, affected_file):
    """Emits a warning notification if file has been deleted """
    if not affected_file.NewContents():
      self.EmitWarning("The file seems to be deleted in the changelist. If "
                       "your intent is to really delete the file, the code in "
                       "PRESUBMIT.py should be updated to remove the "
                       "|StrictEnumValueChecker| class.");
      return False
    return True

  def GetDeletedLinesFromScmDiff(self, affected_file):
    """Return a list of of line numbers (1-based) corresponding to lines
    deleted from the new source file (if they had been present in it). Note
    that if multiple contiguous lines have been deleted, the returned list will
    contain contiguous line number entries. To prevent false positives, we
    return deleted line numbers *only* from diff chunks which decrease the size
    of the new file.

    Note: We need this method because we have access to neither the old file
    content nor the list of "delete" changes from the current presubmit script
    API.

    """
    results = []
    line_num = 0
    deleting_lines = False
    for line in affected_file.GenerateScmDiff().splitlines():
      # Parse the unified diff chunk optional section heading, which looks like
      # @@ -l,s +l,s @@ optional section heading
      m = self.input_api.re.match(
        r"^@@ \-([0-9]+)\,([0-9]+) \+([0-9]+)\,([0-9]+) @@", line)
      if m:
        old_line_num = int(m.group(1))
        old_size = int(m.group(2))
        new_line_num = int(m.group(3))
        new_size = int(m.group(4))
        line_num = new_line_num
        # Return line numbers only from diff chunks decreasing the size of the
        # new file
        deleting_lines = old_size > new_size
        continue
      if not line.startswith("-"):
        line_num += 1
      if deleting_lines and line.startswith("-") and not line.startswith("--"):
        results.append(line_num)
    return results

  def CheckForEnumEntryDeletions(self, affected_file):
    """Look for deletions inside the enum definition. We currently use a
    simple heuristics (not 100% accurate): if there are deleted lines inside
    the enum definition, this might be a deletion.

    """
    range_new = self.ComputeEnumRangeInNewFile(affected_file)
    if not range_new:
      return False

    is_ok = True
    for line_num in self.GetDeletedLinesFromScmDiff(affected_file):
      if range_new.Contains(line_num):
        self.EmitWarning("It looks like you are deleting line(s) from the "
                         "enum definition. This should never happen.",
                         line_num)
        is_ok = False
    return is_ok

  def CheckForEnumEntryInsertions(self, affected_file):
    range = self.ComputeEnumRangeInNewFile(affected_file)
    if not range:
      return False

    first_line = range.first_line
    last_line = range.last_line

    # Collect the range of changes inside the enum definition range.
    is_ok = True
    for line_start, line_end, line_text in \
          self.CollectRangesInsideEnumDefinition(affected_file,
                                                 first_line,
                                                 last_line):
      # The only edit we consider valid is adding 1 or more entries *exactly*
      # at the end of the enum definition. Every other edit inside the enum
      # definition will result in a "warning confirmation" message.
      #
      # TODO(rpaquay): We currently cannot detect "renames" of existing entries
      # vs invalid insertions, so we sometimes will warn for valid edits.
      is_valid_edit = (line_end == last_line - 1)

      self.LogDebug("Edit range in new file at starting at line number %d and "
                    "ending at line number %d: valid=%s"
                    % (line_start, line_end, is_valid_edit))

      if not is_valid_edit:
        self.EmitWarning("The change starting at line %d and ending at line "
                         "%d is *not* located *exactly* at the end of the "
                         "enum definition. Unless you are renaming an "
                         "existing entry, this is not a valid change, as new "
                         "entries should *always* be added at the end of the "
                         "enum definition, right before the \"%s\" "
                         "entry." % (line_start, line_end, self.end_marker),
                         line_start,
                         line_text)
        is_ok = False
    return is_ok

  def PerformChecks(self, affected_file):
    if not self.CheckForFileDeletion(affected_file):
      return
    if not self.CheckForEnumEntryDeletions(affected_file):
      return
    if not self.CheckForEnumEntryInsertions(affected_file):
      return

  def ProcessHistogramValueFile(self, affected_file):
    self.LogInfo("Start processing file \"%s\"" % affected_file.LocalPath())
    self.PerformChecks(affected_file)
    self.LogInfo("Done processing file \"%s\"" % affected_file.LocalPath())

  def Run(self):
    for file in self.input_api.AffectedFiles(include_deletes=True):
      if file.LocalPath() == self.path:
        self.ProcessHistogramValueFile(file)
    return self.results
