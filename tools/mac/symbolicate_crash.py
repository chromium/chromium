#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script can take an Apple-style CrashReporter log and symbolicate it. This
is useful for when a user's reports aren't being uploaded, for example.

Only versions 6, 7, 8, and 9 reports are supported. For more information on the
file format, reference this document:
  TN2123 <http://developer.apple.com/library/mac/#technotes/tn2004/tn2123.html>

Information on symbolication was gleaned from:
  <http://developer.apple.com/tools/xcode/symbolizingcrashdumps.html>
"""

from __future__ import print_function

import optparse
import os.path
import re
import subprocess
import sys

# Maps binary image identifiers to binary names (minus the .dSYM portion) found
# in the archive. These are the only objects that will be looked up.
SYMBOL_IMAGE_MAP = {
  'com.google.Chrome': 'Google Chrome.app',
  'com.google.Chrome.framework': 'Google Chrome Framework.framework',
  'com.google.Chrome.helper': 'Google Chrome Helper.app'
}

class CrashReport(object):
  """A parsed representation of an Apple CrashReport text file."""
  def __init__(self, file_name):
    super(CrashReport, self).__init__()
    self.report_info = {}
    self.threads = []
    self._binary_images = {}

    fd = open(file_name, 'r')
    self._ParseHeader(fd)

    # Try and get the report version. If it's not a version we handle, abort.
    self.report_version = int(self.report_info['Report Version'])
    # Version 6: 10.5 and 10.6 crash report
    # Version 7: 10.6 spindump report
    # Version 8: 10.7 spindump report
    # Version 9: 10.7 crash report
    valid_versions = (6, 7, 8, 9)
    if self.report_version not in valid_versions:
      raise Exception("Only crash reports of versions %s are accepted." %
          str(valid_versions))

    # If this is a spindump (version 7 or 8 report), use a special parser. The
    # format is undocumented, but is similar to version 6. However, the spindump
    # report contains user and kernel stacks for every process on the system.
    if self.report_version == 7 or self.report_version == 8:
      self._ParseSpindumpStack(fd)
    else:
      self._ParseStack(fd)

    self._ParseBinaryImages(fd)
    fd.close()

  def Symbolicate(self, symbol_path):
    """Symbolicates a crash report stack trace."""
    # In order to be efficient, collect all the offsets that will be passed to
    # atos by the image name.
    offsets_by_image = self._CollectAddressesForImages(SYMBOL_IMAGE_MAP.keys())

    # For each image, run atos with the list of addresses.
    for image_name, addresses in offsets_by_image.items():
      # If this image was not loaded or is in no stacks, skip.
      if image_name not in self._binary_images or not len(addresses):
        continue

      # Combine the |image_name| and |symbol_path| into the path of the dSYM.
      dsym_file = self._GetDSymPath(symbol_path, image_name)

      # From the list of 2-Tuples of (frame, address), create a list of just
      # addresses.
      address_list = map(lambda x: x[1], addresses)

      # Look up the load address of the image.
      binary_base = self._binary_images[image_name][0]

      # This returns a list of just symbols. The indices will match up with the
      # list of |addresses|.
      symbol_names = self._RunAtos(binary_base, dsym_file, address_list)
      if not symbol_names:
        print('Error loading symbols for ' + image_name)
        continue

      # Attaches a list of symbol names to stack frames. This assumes that the
      # order of |addresses| has stayed the same as |symbol_names|.
      self._AddSymbolsToFrames(symbol_names, addresses)

  def _ParseHeader(self, fd):
    """Parses the header section of a crash report, which contains the OS and
    application version information."""
    # The header is made up of different sections, depending on the type of
    # report and the report version. Almost all have a format of a key and
    # value separated by a colon. Accumulate all of these artifacts into a
    # dictionary until the first thread stack is reached.
    thread_re = re.compile('^[ \t]*Thread ([a-f0-9]+)')
    line = ''
    while not thread_re.match(line):
      # Skip blank lines. There are typically three or four sections separated
      # by newlines in the header.
      line = line.strip()
      if line:
        parts = line.split(':', 1)
        # Certain lines in different report versions don't follow the key-value
        # format, so skip them.
        if len(parts) == 2:
          # There's a varying amount of space padding after the ':' to align all
          # the values; strip that.
          self.report_info[parts[0]] = parts[1].lstrip()
      line = fd.readline()

    # When this loop exits, the header has been read in full. However, the first
    # thread stack heading has been read past. Seek backwards from the current
    # position by the length of the line so that it is re-read when
    # _ParseStack() is entered.
    fd.seek(-len(line), os.SEEK_CUR)

  def _ParseStack(self, fd):
    """Parses the stack dump of a crash report and creates a list of threads
    and their stack traces."""
    # Compile a regex that matches the start of a thread stack. Note that this
    # must be specific to not include the thread state section, which comes
    # right after all the stack traces.
    line_re = re.compile('^Thread ([0-9]+)( Crashed)?:(.*)')

    # On entry into this function, the fd has been walked up to the "Thread 0"
    # line.
    line = fd.readline().rstrip()
    in_stack = False
    thread = None
    while line_re.match(line) or in_stack:
      # Check for start of the thread stack.
      matches = line_re.match(line)

      if not line.strip():
        # A blank line indicates a break in the thread stack.
        in_stack = False
      elif matches:
        # If this is the start of a thread stack, create the CrashThread.
        in_stack = True
        thread = CrashThread(matches.group(1))
        thread.name = matches.group(3)
        thread.did_crash = matches.group(2) != None
        self.threads.append(thread)
      else:
        # All other lines are stack frames.
        thread.stack.append(self._ParseStackFrame(line))
      # Read the next line.
      line = fd.readline()

  def _ParseStackFrame(self, line):
    """Takes in a single line of text and transforms it into a StackFrame."""
    frame = StackFrame(line)

    # A stack frame is in the format of:
    # |<frame-number> <binary-image> 0x<address> <symbol> <offset>|.
    regex = '^([0-9]+) +(.+)[ \t]+(0x[0-9a-f]+) (.*) \+ ([0-9]+)$'
    matches = re.match(regex, line)
    if matches is None:
      return frame

    # Create a stack frame with the information extracted from the regex.
    frame.frame_id = matches.group(1)
    frame.image = matches.group(2)
    frame.address = int(matches.group(3), 0)  # Convert HEX to an int.
    frame.original_symbol = matches.group(4)
    frame.offset = matches.group(5)
    frame.line = None
    return frame

  def _ParseSpindumpStack(self, fd):
    """Parses a spindump stack report. In this format, each thread stack has
    both a user and kernel trace. Only the user traces are symbolicated."""

    # The stack trace begins with the thread header, which is identified by a
    # HEX number. The thread names appear to be incorrect in spindumps.
    user_thread_re = re.compile('^  Thread ([0-9a-fx]+)')

    # When this method is called, the fd has been walked right up to the first
    # line.
    line = fd.readline()
    in_user_stack = False
    in_kernel_stack = False
    thread = None
    frame_id = 0
    while user_thread_re.match(line) or in_user_stack or in_kernel_stack:
      # Check for the start of a thread.
      matches = user_thread_re.match(line)

      if not line.strip():
        # A blank line indicates the start of a new thread. The blank line comes
        # after the kernel stack before a new thread header.
        in_kernel_stack = False
      elif matches:
        # This is the start of a thread header. The next line is the heading for
        # the user stack, followed by the actual trace.
        thread = CrashThread(matches.group(1))
        frame_id = 0
        self.threads.append(thread)
        in_user_stack = True
        line = fd.readline()  # Read past the 'User stack:' header.
      elif line.startswith('  Kernel stack:'):
        # The kernel stack header comes immediately after the last frame (really
        # the top frame) in the user stack, without a blank line.
        in_user_stack = False
        in_kernel_stack = True
      elif in_user_stack:
        # If this is a line while in the user stack, parse it as a stack frame.
        thread.stack.append(self._ParseSpindumpStackFrame(line))
      # Loop with the next line.
      line = fd.readline()

    # When the loop exits, the file has been read through the 'Binary images:'
    # header. Seek backwards so that _ParseBinaryImages() does the right thing.
    fd.seek(-len(line), os.SEEK_CUR)

  def _ParseSpindumpStackFrame(self, line):
    """Parses a spindump-style stackframe."""
    frame = StackFrame(line)

    # The format of the frame is either:
    # A: |<space><steps> <symbol> + <offset> (in <image-name>) [<address>]|
    # B: |<space><steps> ??? (in <image-name> + <offset>) [<address>]|
    regex_a = '^([ ]+[0-9]+) (.*) \+ ([0-9]+) \(in (.*)\) \[(0x[0-9a-f]+)\]'
    regex_b = '^([ ]+[0-9]+) \?\?\?( \(in (.*) \+ ([0-9]+)\))? \[(0x[0-9a-f]+)\]'

    # Create the stack frame with the information extracted from the regex.
    matches = re.match(regex_a, line)
    if matches:
      frame.frame_id = matches.group(1)[4:]  # Remove some leading spaces.
      frame.original_symbol = matches.group(2)
      frame.offset = matches.group(3)
      frame.image = matches.group(4)
      frame.address = int(matches.group(5), 0)
      frame.line = None
      return frame

    # If pattern A didn't match (which it will most of the time), try B.
    matches = re.match(regex_b, line)
    if matches:
      frame.frame_id = matches.group(1)[4:]  # Remove some leading spaces.
      frame.image = matches.group(3)
      frame.offset = matches.group(4)
      frame.address = int(matches.group(5), 0)
      frame.line = None
      return frame

    # Otherwise, this frame could not be matched and just use the raw input.
    frame.line = frame.line.strip()
    return frame

  def _ParseBinaryImages(self, fd):
    """Parses out the binary images section in order to get the load offset."""
    # The parser skips some sections, so advance until the "Binary Images"
    # header is reached.
    while not fd.readline().lstrip().startswith("Binary Images:"): pass

    # Create a regex to match the lines of format:
    # |0x<start> - 0x<end> <binary-image> <version> (<version>) <<UUID>> <path>|
    image_re = re.compile(
        '[ ]*(0x[0-9a-f]+) -[ \t]+(0x[0-9a-f]+) [+ ]([a-zA-Z0-9._\-]+)')

    # This section is in this format:
    # |<start address> - <end address> <image name>|.
    while True:
      line = fd.readline()
      if not line.strip():
        # End when a blank line is hit.
        return
      # Match the line to the regex.
      match = image_re.match(line)
      if match:
        # Store the offsets by image name so it can be referenced during
        # symbolication. These are hex numbers with leading '0x', so int() can
        # convert them to decimal if base=0.
        address_range = (int(match.group(1), 0), int(match.group(2), 0))
        self._binary_images[match.group(3)] = address_range

  def _CollectAddressesForImages(self, images):
    """Iterates all the threads and stack frames and all the stack frames that
    are in a list of binary |images|. The result is a dictionary, keyed by the
    image name that maps to a list of tuples. Each is a 2-Tuple of
    (stack_frame, address)"""
    # Create the collection and initialize it with empty lists for each image.
    collection = {}
    for image in images:
      collection[image] = []

    # Perform the iteration.
    for thread in self.threads:
      for frame in thread.stack:
        image_name = self._ImageForAddress(frame.address)
        if image_name in images:
          # Replace the image name in the frame in case it was elided.
          frame.image = image_name
          collection[frame.image].append((frame, frame.address))

    # Return the result.
    return collection

  def _ImageForAddress(self, address):
    """Given a PC address, returns the bundle identifier of the image in which
    the address resides."""
    for image_name, address_range in self._binary_images.items():
      if address >= address_range[0] and address <= address_range[1]:
        return image_name
    return None

  def _GetDSymPath(self, base_path, image_name):
    """Takes a base path for the symbols and an image name. It looks the name up
    in SYMBOL_IMAGE_MAP and creates a full path to the dSYM in the bundle."""
    image_file = SYMBOL_IMAGE_MAP[image_name]
    return os.path.join(base_path, image_file + '.dSYM', 'Contents',
        'Resources', 'DWARF',
        os.path.splitext(image_file)[0])  # Chop off the extension.

  def _RunAtos(self, load_address, dsym_file, addresses):
    """Runs the atos with the provided arguments. |addresses| is used as stdin.
    Returns a list of symbol information in the same order as |addresses|."""
    args = ['atos', '-l', str(load_address), '-o', dsym_file]

    # Get the arch type. This is of the format |X86 (Native)|.
    if 'Code Type' in self.report_info:
      arch = self.report_info['Code Type'].lower().split(' ')
      if len(arch) == 2:
        arch = arch[0]
        if arch == 'x86':
          # The crash report refers to i386 as x86, but atos doesn't know what
          # that is.
          arch = 'i386'
        args.extend(['-arch', arch])

    proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    addresses = map(hex, addresses)
    (stdout, stderr) = proc.communicate(' '.join(addresses))
    if proc.returncode:
      return None
    return stdout.rstrip().split('\n')

  def _AddSymbolsToFrames(self, symbols, address_tuples):
    """Takes a single value (the list) from _CollectAddressesForImages and does
    a smart-zip with the data returned by atos in |symbols|. Note that the
    indices must match for this to succeed."""
    if len(symbols) != len(address_tuples):
      print('symbols do not match')

    # Each line of output from atos is in this format:
    # |<symbol> (in <image>) (<file>:<line>)|.
    line_regex = re.compile('(.+) \(in (.+)\) (\((.+):([0-9]+)\))?')

    # Zip the two data sets together.
    for i in range(len(symbols)):
      symbol_parts = line_regex.match(symbols[i])
      if not symbol_parts:
        continue  # Error.
      frame = address_tuples[i][0]
      frame.symbol = symbol_parts.group(1)
      frame.image = symbol_parts.group(2)
      frame.file_name = symbol_parts.group(4)
      frame.line_number = symbol_parts.group(5)


class CrashThread(object):
  """A CrashThread represents a stacktrace of a single thread """
  def __init__(self, thread_id):
    super(CrashThread, self).__init__()
    self.thread_id = thread_id
    self.name = None
    self.did_crash = False
    self.stack = []

  def __repr__(self):
    name = ''
    if self.name:
      name = ': ' + self.name
    return 'Thread ' + self.thread_id + name + '\n' + \
        '\n'.join(map(str, self.stack))


class StackFrame(object):
  """A StackFrame is owned by a CrashThread."""
  def __init__(self, line):
    super(StackFrame, self).__init__()
    # The original line. This will be set to None if symbolication was
    # successfuly.
    self.line = line

    self.frame_id = 0
    self.image = None
    self.address = 0x0
    self.original_symbol = None
    self.offset = 0x0
    # The following members are set after symbolication.
    self.symbol = None
    self.file_name = None
    self.line_number = 0

  def __repr__(self):
    # If symbolication failed, just use the original line.
    if self.line:
      return '  %s' % self.line

    # Use different location information depending on symbolicated data.
    location = None
    if self.file_name:
      location = ' - %s:%s' % (self.file_name, self.line_number)
    else:
      location = ' + %s' % self.offset

    # Same with the symbol information.
    symbol = self.original_symbol
    if self.symbol:
      symbol = self.symbol

    return '  %s\t0x%x\t[%s\t%s]\t%s' % (self.frame_id, self.address,
        self.image, location, symbol)


def PrettyPrintReport(report):
  """Takes a crash report and prints it like the crash server would."""
  print('Process    : ' + report.report_info['Process'])
  print('Version    : ' + report.report_info['Version'])
  print('Date       : ' + report.report_info['Date/Time'])
  print('OS Version : ' + report.report_info['OS Version'])
  print()
  if 'Crashed Thread' in report.report_info:
    print('Crashed Thread : ' + report.report_info['Crashed Thread'])
    print()
  if 'Event' in report.report_info:
    print('Event      : ' + report.report_info['Event'])
    print()

  for thread in report.threads:
    print()
    if thread.did_crash:
      exc_type = report.report_info['Exception Type'].split(' ')[0]
      exc_code = report.report_info['Exception Codes'].replace('at', '@')
      print('*CRASHED* ( ' + exc_type + ' / ' + exc_code + ' )')
    # Version 7 reports have spindump-style output (with a stepped stack trace),
    # so remove the first tab to get better alignment.
    if report.report_version == 7:
      for line in repr(thread).split('\n'):
        print(line.replace('\t', '  ', 1))
    else:
      print(thread)


def Main(args):
  """Program main."""
  parser = optparse.OptionParser(
      usage='%prog [options] symbol_path crash_report',
      description='This will parse and symbolicate an Apple CrashReporter v6-9 '
          'file.')
  parser.add_option('-s', '--std-path', action='store_true', dest='std_path',
                    help='With this flag, the symbol_path is a containing '
                    'directory, in which a dSYM files are stored in a '
                    'directory named by the version. Example: '
                    '[symbolicate_crash.py -s ./symbols/ report.crash] will '
                    'look for dSYMs in ./symbols/15.0.666.0/ if the report is '
                    'from that verison.')
  (options, args) = parser.parse_args(args[1:])

  # Check that we have something to symbolicate.
  if len(args) != 2:
    parser.print_usage()
    return 1

  report = CrashReport(args[1])
  symbol_path = None

  # If not using the standard layout, this is a full path to the symbols.
  if not options.std_path:
    symbol_path = args[0]
  # Otherwise, use the report version to locate symbols in a directory.
  else:
    # This is in the format of |M.N.B.P (B.P)|. Get just the part before the
    # space.
    chrome_version = report.report_info['Version'].split(' ')[0]
    symbol_path = os.path.join(args[0], chrome_version)

  # Check that the symbols exist.
  if not os.path.isdir(symbol_path):
    print('Symbol path %s is not a directory' % symbol_path, file=sys.stderr)
    return 2

  print('Using symbols from ' + symbol_path, file=sys.stderr)
  print('=' * 80, file=sys.stderr)

  report.Symbolicate(symbol_path)
  PrettyPrintReport(report)
  return 0


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
