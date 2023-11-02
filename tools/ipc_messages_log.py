#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""""Processes a log file and resolves IPC message identifiers.

Resolves IPC messages of the form [unknown type NNNNNN] to named IPC messages.

e.g. logfile containing

I/stderr  ( 3915): ipc 3915.3.1370207904 2147483647 S [unknown type 66372]

will be transformed to:

I/stderr  ( 3915): ipc 3915.3.1370207904 2147483647 S ViewMsg_SetCSSColors

In order to find the message header files efficiently, it requires that
Chromium is checked out using git.
"""

from __future__ import print_function

import optparse
import os
import re
import subprocess
import sys


def _SourceDir():
  """Get chromium's source directory."""
  return os.path.join(sys.path[0], '..')


def _ReadLines(f):
  """Read from file f and generate right-stripped lines."""
  for line in f:
    yield line.rstrip()


def _GetMsgStartTable():
  """Read MsgStart enumeration from ipc/ipc_message_utils.h.

  Determines the message type identifiers by reading.
  header file ipc/ipc_message_utils.h and looking for
  enum IPCMessageStart.  Assumes following code format in header file:
  enum IPCMessageStart {
     Type1MsgStart ...,
     Type2MsgStart,
  };

  Returns:
      A dictionary mapping StartName to enumeration value.
  """
  ipc_message_file = _SourceDir() + '/ipc/ipc_message_utils.h'
  ipc_message_lines = _ReadLines(open(ipc_message_file))
  is_msg_start = False
  count = 0
  msg_start_table = dict()
  for line in ipc_message_lines:
    if is_msg_start:
      if line.strip() == '};':
        break
      msgstart_index = line.find('MsgStart')
      msg_type = line[:msgstart_index] + 'MsgStart'
      msg_start_table[msg_type.strip()] = count
      count+=1
    elif line.strip() == 'enum IPCMessageStart {':
      is_msg_start = True

  return msg_start_table


def _FindMessageHeaderFiles():
  """Look through the source directory for *_messages.h."""
  os.chdir(_SourceDir())
  pipe = subprocess.Popen(['git', 'ls-files', '--', '*_messages.h'],
                          stdout=subprocess.PIPE)
  return _ReadLines(pipe.stdout)


def _GetMsgId(msg_start, line_number, msg_start_table):
  """Construct the meessage id given the msg_start and the line number."""
  hex_str = '%x%04x' % (msg_start_table[msg_start], line_number)
  return int(hex_str, 16)


def _ReadHeaderFile(f, msg_start_table, msg_map):
  """Read a header file and construct a map from message_id to message name."""
  msg_def_re = re.compile(
      '^IPC_(?:SYNC_)?MESSAGE_[A-Z0-9_]+\(([A-Za-z0-9_]+).*')
  msg_start_re = re.compile(
      '^\s*#define\s+IPC_MESSAGE_START\s+([a-zA-Z0-9_]+MsgStart).*')
  msg_start = None
  msg_name = None
  line_number = 0

  for line in f:
    line_number+=1
    match = re.match(msg_start_re, line)
    if match:
      msg_start = match.group(1)
      # print("msg_start = " + msg_start)
    match = re.match(msg_def_re, line)
    if match:
      msg_name = match.group(1)
      # print("msg_name = " + msg_name)
    if msg_start and msg_name:
      msg_id = _GetMsgId(msg_start, line_number, msg_start_table)
      msg_map[msg_id] = msg_name
  return msg_map


def _ResolveMsg(msg_type, msg_map):
  """Fully resolve a message type to a name."""
  if msg_type in msg_map:
    return msg_map[msg_type]
  else:
    return '[Unknown message %d (0x%x)]x' % (msg_type, msg_type)


def _ProcessLog(f, msg_map):
  """Read lines from f and resolve the IPC messages according to msg_map."""
  unknown_msg_re = re.compile('\[unknown type (\d+)\]')
  for line in f:
    line = line.rstrip()
    match = re.search(unknown_msg_re, line)
    if match:
      line = re.sub(unknown_msg_re,
                    _ResolveMsg(int(match.group(1)), msg_map),
                    line)
    print(line)


def _GetMsgMap():
  """Returns a dictionary mapping from message number to message name."""
  msg_start_table = _GetMsgStartTable()
  msg_map = dict()
  for header_file in _FindMessageHeaderFiles():
    _ReadHeaderFile(open(header_file),
                    msg_start_table,
                    msg_map)
  return msg_map


def main():
  """Processes one or more log files with IPC logging messages.

     Replaces '[unknown type NNNNNN]' with resolved
     IPC messages.

     Reads from standard input if no log files specified on the
     command line.
  """
  parser = optparse.OptionParser('usage: %prog [LOGFILE...]')
  (_, args) = parser.parse_args()

  msg_map = _GetMsgMap()
  log_files = args

  if log_files:
    for log_file in log_files:
      _ProcessLog(open(log_file), msg_map)
  else:
    _ProcessLog(sys.stdin, msg_map)


if __name__ == '__main__':
  main()
