#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# 'top'-like memory/network polling for Android apps.

import argparse
import curses
import logging
import os
import re
import sys
import time

from operator import sub

_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import device_errors
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_PATH, 'build', 'android'))
import devil_chromium

class Utils(object):
  """A helper class to hold various utility methods."""

  @staticmethod
  def FindLines(haystack, needle):
    """A helper method to find lines in |haystack| that contain the string
    |needle|."""
    return [ hay for hay in haystack if needle in hay ]

class Validator(object):
  """A helper class with validation methods for argparse."""

  @staticmethod
  def ValidatePath(path):
    """An argparse validation method to make sure a file path is writable."""
    if os.path.exists(path):
      return path
    elif os.access(os.path.dirname(path), os.W_OK):
      return path
    raise argparse.ArgumentTypeError("%s is an invalid file path" % path)

  @staticmethod
  def ValidatePdfPath(path):
    """An argparse validation method to make sure a pdf file path is writable.
    Validates a file path to make sure it is writable and also appends '.pdf' if
    necessary."""
    if os.path.splitext(path)[-1].lower() != 'pdf':
      path = path + '.pdf'
    return Validator.ValidatePath(path)

  @staticmethod
  def ValidateNonNegativeNumber(val):
    """An argparse validation method to make sure a number is not negative."""
    ival = int(val)
    if ival < 0:
      raise argparse.ArgumentTypeError("%s is a negative integer" % val)
    return ival

class Timer(object):
  """A helper class to track timestamps based on when this program was
  started"""
  starting_time = time.time()

  @staticmethod
  def GetTimestamp():
    """A helper method to return the time (in seconds) since this program was
    started."""
    return time.time() - Timer.starting_time

class DeviceHelper(object):
  """A helper class with various generic device interaction methods."""

  @staticmethod
  def __GetUserIdForProcessName(adb, process_name):
    """Returns the userId of the application associated by |pid| or None if
    not found."""
    try:
      process_name = process_name.split(':')[0]
      cmd = ['dumpsys', 'package', process_name]
      user_id_lines = adb.RunShellCommand(
          cmd, large_output=True, check_return=True)
      user_id_lines = Utils.FindLines(user_id_lines, 'userId=')

      if not user_id_lines:
        return None

      columns = re.split('\s+|=', user_id_lines[0].strip())

      if len(columns) >= 2:
        return columns[1]
    except device_errors.AdbShellCommandFailedError:
      pass
    return None

  @staticmethod
  def GetDeviceModel(adb):
    """Returns the model of the device with the |adb| connection."""
    return adb.GetProp('ro.product.model').strip()

  @staticmethod
  def GetDeviceToTrack(preset=None):
    """Returns a device serial to connect to.  If |preset| is specified it will
    return |preset| if it is connected and |None| otherwise.  If |preset| is not
    specified it will return the first connected device."""
    devices = [d.adb.GetDeviceSerial()
               for d in device_utils.DeviceUtils.HealthyDevices()]
    if not devices:
      return None

    if preset:
      return preset if preset in devices else None

    return devices[0]

  @staticmethod
  def GetPidsToTrack(adb, default_pid=None, process_filter=None):
    """Returns a list of tuples of (userid, pids, process name) based on the
    input arguments.  If |default_pid| is specified it will return that pid if
    it exists.  If |process_filter| is specified it will return the pids of
    processes with that string in the name. If both are specified it will
    intersect the two.  The returned result is sorted based on userid."""
    pids = []
    try:
      for process in adb.ListProcesses(process_filter):
        if default_pid and process.pid != default_pid:
          continue
        userid = DeviceHelper.__GetUserIdForProcessName(adb, process.name)
        pids.append((userid, process.pid, process.name))
    except device_errors.AdbShellCommandFailedError as exc:
      logging.warning('Error getting PIDs to track: %s', exc)
    return sorted(pids, key=lambda tup: tup[0])

class NetworkHelper(object):
  """A helper class to query basic network usage of an application."""
  @staticmethod
  def QueryNetwork(adb, userid):
    """Queries the device for network information about the application with a
    user id of |userid|.  It will return a list of values:
    [ Download Background, Upload Background, Download Foreground, Upload
    Foreground ].  If the application is not found it will return
    [ 0, 0, 0, 0 ]."""
    results = [0, 0, 0, 0]

    if not userid:
      return results

    try:
      # Parsing indices for scanning a row from /proc/net/xt_qtaguid/stats.
      # The application id
      userid_idx = 3

      # Whether or not the transmission happened with the application in the
      # background (0) or foreground (1).
      bg_or_fg_idx = 4

      # The number of bytes received.
      rx_idx = 5

      # The number of bytes sent.
      tx_idx = 7

      net_lines = adb.ReadFile('/proc/net/xt_qtaguid/stats').splitlines()
      net_lines = Utils.FindLines(net_lines, userid)
      for line in net_lines:
        data = re.split('\s+', line.strip())
        if data[userid_idx] != userid:
          continue

        dst_idx_offset = None
        if data[bg_or_fg_idx] == '0':
          dst_idx_offset = 0
        elif data[bg_or_fg_idx] == '1':
          dst_idx_offset = 2

        if dst_idx_offset is None:
          continue

        results[dst_idx_offset] = round(float(data[rx_idx]) / 1000.0, 2)
        results[dst_idx_offset + 1] = round(float(data[tx_idx]) / 1000.0, 2)
    except device_errors.AdbShellCommandFailedError:
      pass
    return results

class MemoryHelper(object):
  """A helper class to query basic memory usage of a process."""

  @staticmethod
  def QueryMemory(adb, pid):
    """Queries the device for memory information about the process with a pid of
    |pid|.  It will query Native, Dalvik, and Pss memory of the process.  It
    returns a list of values: [ Native, Pss, Dalvik ].  If the process is not
    found it will return [ 0, 0, 0 ]."""
    results = [0, 0, 0]

    mem_lines = adb.RunShellCommand(
        ['dumpsys', 'meminfo', pid], check_return=True)
    for line in mem_lines:
      match = re.split('\s+', line.strip())

      # Skip data after the 'App Summary' line.  This is to fix builds where
      # they have more entries that might match the other conditions.
      if len(match) >= 2 and match[0] == 'App' and match[1] == 'Summary':
        break

      result_idx = None
      query_idx = None
      if match[0] == 'Native' and match[1] == 'Heap':
        result_idx = 0
        query_idx = -2
      elif match[0] == 'Dalvik' and match[1] == 'Heap':
        result_idx = 2
        query_idx = -2
      elif match[0] == 'TOTAL':
        result_idx = 1
        query_idx = 1

      # If we already have a result, skip it and don't overwrite the data.
      if result_idx is not None and results[result_idx] != 0:
        continue

      if result_idx is not None and query_idx is not None:
        results[result_idx] = round(float(match[query_idx]) / 1000.0, 2)
    return results

class GraphicsHelper(object):
  """A helper class to query basic graphics memory usage of a process."""

  # TODO(dtrainor): Find a generic way to query/fall back for other devices.
  # Is showmap consistently reliable?
  __NV_MAP_MODELS = ['Xoom']
  __NV_MAP_FILE_LOCATIONS = ['/d/nvmap/generic-0/clients',
                             '/d/nvmap/iovmm/clients']

  __SHOWMAP_MODELS = ['Nexus S',
                      'Nexus S 4G',
                      'Galaxy Nexus',
                      'Nexus 4',
                      'Nexus 5',
                      'Nexus 7']
  __SHOWMAP_KEY_MATCHES = ['/dev/pvrsrvkm',
                           '/dev/kgsl-3d0']

  @staticmethod
  def __QueryShowmap(adb, pid):
    """Attempts to query graphics memory via the 'showmap' command.  It will
    look for |self.__SHOWMAP_KEY_MATCHES| entries to try to find one that
    represents the graphics memory usage.  Will return this as a single entry
    array of [ Graphics ].  If not found, will return [ 0 ]."""
    try:
      mem_lines = adb.RunShellCommand(['showmap', '-t', pid], check_return=True)
      for line in mem_lines:
        match = re.split('[ ]+', line.strip())
        if match[-1] in GraphicsHelper.__SHOWMAP_KEY_MATCHES:
          return [ round(float(match[2]) / 1000.0, 2) ]
    except device_errors.AdbShellCommandFailedError:
      pass
    return [ 0 ]

  @staticmethod
  def __NvMapPath(adb):
    """Attempts to find a valid NV Map file on the device.  It will look for a
    file in |self.__NV_MAP_FILE_LOCATIONS| and see if one exists.  If so, it
    will return it."""
    for nv_file in GraphicsHelper.__NV_MAP_FILE_LOCATIONS:
      if adb.PathExists(nv_file):
        return nv_file
    return None

  @staticmethod
  def __QueryNvMap(adb, pid):
    """Attempts to query graphics memory via the NV file map method.  It will
    find a possible NV Map file from |self.__NvMapPath| and try to parse the
    graphics memory from it.  Will return this as a single entry array of
    [ Graphics ].  If not found, will return [ 0 ]."""
    nv_file = GraphicsHelper.__NvMapPath(adb)
    if nv_file:
      mem_lines = adb.ReadFile(nv_file).splitlines()
      for line in mem_lines:
        match = re.split(' +', line.strip())
        if match[2] == pid:
          return [ round(float(match[3]) / 1000000.0, 2) ]
    return [ 0 ]

  @staticmethod
  def QueryVideoMemory(adb, pid):
    """Queries the device for graphics memory information about the process with
    a pid of |pid|.  Not all devices are currently supported.  If possible, this
    will return a single entry array of [ Graphics ].  Otherwise it will return
    [ 0 ].

    Please see |self.__NV_MAP_MODELS| and |self.__SHOWMAP_MODELS|
    to see if the device is supported.  For new devices, see if they can be
    supported by existing methods and add their entry appropriately.  Also,
    please add any new way of querying graphics memory as they become
    available."""
    model = DeviceHelper.GetDeviceModel(adb)
    if model in GraphicsHelper.__NV_MAP_MODELS:
      return GraphicsHelper.__QueryNvMap(adb, pid)
    elif model in GraphicsHelper.__SHOWMAP_MODELS:
      return GraphicsHelper.__QueryShowmap(adb, pid)
    return [ 0 ]

class DeviceSnapshot(object):
  """A class holding a snapshot of memory and network usage for various pids
  that are being tracked.  If |show_mem| is True, this will track memory usage.
  If |show_net| is True, this will track network usage.

  Attributes:
    pids:      A list of tuples (userid, pid, process name) that should be
              tracked.
    memory:    A map of entries of pid => memory consumption array.  Right now
               the indices are [ Native, Pss, Dalvik, Graphics ].
    network:   A map of entries of userid => network consumption array.  Right
               now the indices are [ Download Background, Upload Background,
               Download Foreground, Upload Foreground ].
    timestamp: The amount of time (in seconds) between when this program started
               and this snapshot was taken.
  """

  def __init__(self, adb, pids, show_mem, show_net):
    """Creates an instances of a DeviceSnapshot with an |adb| device connection
    and a list of (pid, process name) tuples."""
    super(DeviceSnapshot, self).__init__()

    self.pids = pids
    self.memory = {}
    self.network = {}
    self.timestamp = Timer.GetTimestamp()

    for (userid, pid, name) in pids:
      if show_mem:
        self.memory[pid] = self.__QueryMemoryForPid(adb, pid)

      if show_net and userid not in self.network:
        self.network[userid] = NetworkHelper.QueryNetwork(adb, userid)

  @staticmethod
  def __QueryMemoryForPid(adb, pid):
    """Queries the |adb| device for memory information about |pid|.  This will
    return a list of memory values that map to [ Native, Pss, Dalvik,
    Graphics ]."""
    results = MemoryHelper.QueryMemory(adb, pid)
    results.extend(GraphicsHelper.QueryVideoMemory(adb, pid))
    return results

  def __GetProcessNames(self):
    """Returns a list of all of the process names tracked by this snapshot."""
    return [tuple[2] for tuple in self.pids]

  def HasResults(self):
    """Whether or not this snapshot was tracking any processes."""
    return self.pids

  def GetPidInfo(self):
    """Returns a list of (userid, pid, process name) tuples that are being
    tracked in this snapshot."""
    return self.pids

  def GetNameForPid(self, search_pid):
    """Returns the process name of a tracked |search_pid|.  This only works if
    |search_pid| is tracked by this snapshot."""
    for (userid, pid, name) in self.pids:
      if pid == search_pid:
        return name
    return None

  def GetUserIdForPid(self, search_pid):
    """Returns the application userId for an associated |pid|.  This only works
    if |search_pid| is tracked by this snapshot and the application userId is
    queryable."""
    for (userid, pid, name) in self.pids:
      if pid == search_pid:
        return userid
    return None

  def IsFirstPidForUserId(self, search_pid):
    """Returns whether or not |search_pid| is the first pid in the |pids| with
    the associated application userId.  This is used to determine if network
    statistics should be shown for this pid or if they have already been shown
    for a pid associated with this application."""
    prev_userid = None
    for idx, (userid, pid, name) in enumerate(self.pids):
      if pid == search_pid:
        return prev_userid != userid
      prev_userid = userid
    return False

  def GetMemoryResults(self, pid):
    """Returns a list of entries about the memory usage of the process specified
    by |pid|.  This will be of the format [ Native, Pss, Dalvik, Graphics ]."""
    if pid in self.memory:
      return self.memory[pid]
    return None

  def GetNetworkResults(self, userid):
    """Returns a list of entries about the network usage of the application
    specified by |userid|.  This will be of the format [ Download Background,
    Upload Background, Download Foreground, Upload Foreground ]."""
    if userid in self.network:
      return self.network[userid]
    return None

  def GetLongestNameLength(self):
    """Returns the length of the longest process name tracked by this
    snapshot."""
    return len(max(self.__GetProcessNames(), key=len))

  def GetTimestamp(self):
    """Returns the time since program start that this snapshot was taken."""
    return self.timestamp

class OutputBeautifier(object):
  """A helper class to beautify the memory output to various destinations.

  Attributes:
    can_color: Whether or not the output should include ASCII color codes to
               make it look nicer.  Default is |True|.  This is disabled when
               writing to a file or a graph.
    overwrite: Whether or not the output should overwrite the previous output.
               Default is |True|.  This is disabled when writing to a file or a
               graph.
  """

  __MEMORY_COLUMN_TITLES = ['Native',
                            'Pss',
                            'Dalvik',
                            'Graphics']

  __NETWORK_COLUMN_TITLES = ['Bg Rx',
                             'Bg Tx',
                             'Fg Rx',
                             'Fg Tx']

  __TERMINAL_COLORS = {'ENDC': 0,
                       'BOLD': 1,
                       'GREY30': 90,
                       'RED': 91,
                       'DARK_YELLOW': 33,
                       'GREEN': 92}

  def __init__(self, can_color=True, overwrite=True):
    """Creates an instance of an OutputBeautifier."""
    super(OutputBeautifier, self).__init__()
    self.can_color = can_color
    self.overwrite = overwrite

    self.lines_printed = 0
    self.printed_header = False

  @staticmethod
  def __FindPidsForSnapshotList(snapshots):
    """Find the set of unique pids across all every snapshot in |snapshots|."""
    pids = set()
    for snapshot in snapshots:
      for (userid, pid, name) in snapshot.GetPidInfo():
        pids.add((userid, pid, name))
    return pids

  @staticmethod
  def __TermCode(num):
    """Escapes a terminal code.  See |self.__TERMINAL_COLORS| for a list of some
    terminal codes that are used by this program."""
    return '\033[%sm' % num

  @staticmethod
  def __PadString(string, length, left_align):
    """Pads |string| to at least |length| with spaces.  Depending on
    |left_align| the padding will appear at either the left or the right of the
    original string."""
    return (('%' if left_align else '%-') + str(length) + 's') % string

  @staticmethod
  def __GetDiffColor(delta):
    """Returns a color based on |delta|.  Used to color the deltas between
    different snapshots."""
    if not delta or delta == 0.0:
      return 'GREY30'
    elif delta < 0:
      return 'GREEN'
    elif delta > 0:
      return 'RED'

  @staticmethod
  def __CleanRound(val, precision):
    """Round |val| to |precision|.  If |precision| is 0, completely remove the
    decimal point."""
    return int(val) if precision == 0 else round(float(val), precision)

  def __ColorString(self, string, color):
    """Colors |string| based on |color|.  |color| must be in
    |self.__TERMINAL_COLORS|.  Returns the colored string or the original
    string if |self.can_color| is |False| or the |color| is invalid."""
    if not self.can_color or not color or not self.__TERMINAL_COLORS[color]:
      return string

    return '%s%s%s' % (
        self.__TermCode(self.__TERMINAL_COLORS[color]),
        string,
        self.__TermCode(self.__TERMINAL_COLORS['ENDC']))

  def __PadAndColor(self, string, length, left_align, color):
    """A helper method to both pad and color the string.  See
    |self.__ColorString| and |self.__PadString|."""
    return self.__ColorString(
      self.__PadString(string, length, left_align), color)

  def __OutputLine(self, line):
    """Writes a line to the screen.  This also tracks how many times this method
    was called so that the screen can be cleared properly if |self.overwrite| is
    |True|."""
    sys.stdout.write(line + '\n')
    if self.overwrite:
      self.lines_printed += 1

  def __ClearScreen(self):
    """Clears the screen based on the number of times |self.__OutputLine| was
    called."""
    if self.lines_printed == 0 or not self.overwrite:
      return

    key_term_up = curses.tparm(curses.tigetstr('cuu1'))
    key_term_clear_eol = curses.tparm(curses.tigetstr('el'))
    key_term_go_to_bol = curses.tparm(curses.tigetstr('cr'))

    sys.stdout.write(key_term_go_to_bol)
    sys.stdout.write(key_term_clear_eol)

    for i in range(self.lines_printed):
      sys.stdout.write(key_term_up)
      sys.stdout.write(key_term_clear_eol)
    self.lines_printed = 0

  def __PrintPidLabelHeader(self, snapshot):
    """Returns a header string with columns Pid and Name."""
    if not snapshot or not snapshot.HasResults():
      return

    name_length = max(8, snapshot.GetLongestNameLength())

    header = self.__PadString('Pid', 8, True) + ' '
    header += self.__PadString('Name', name_length, False)
    header = self.__ColorString(header, 'BOLD')
    return header

  def __PrintTimestampHeader(self):
    """Returns a header string with a Timestamp column."""
    header = self.__PadString('Timestamp', 8, False)
    header = self.__ColorString(header, 'BOLD')
    return header

  def __PrintMemoryStatsHeader(self):
    """Returns a header string for memory usage statistics."""
    headers = ''
    for header in self.__MEMORY_COLUMN_TITLES:
      headers += self.__PadString(header, 8, True) + ' '
      headers += self.__PadString('(mB)', 8, False)
    return self.__ColorString(headers, 'BOLD')

  def __PrintNetworkStatsHeader(self):
    """Returns a header string for network usage statistics."""
    headers = ''
    for header in self.__NETWORK_COLUMN_TITLES:
      headers += self.__PadString(header, 8, True) + ' '
      headers += self.__PadString('(kB)', 8, False)
    return self.__ColorString(headers, 'BOLD')

  def __PrintTrailingHeader(self, snapshot):
    """Returns a header string for the header trailer (includes timestamp)."""
    if not snapshot or not snapshot.HasResults():
      return

    header = '(' + str(round(snapshot.GetTimestamp(), 2)) + 's)'
    return self.__ColorString(header, 'BOLD')

  def __PrintArrayWithDeltas(self, results, old_results, precision=2):
    """Helper method to return a string of statistics with their deltas.  This
    takes two arrays and prints out "current (current - old)" for all entries in
    the arrays."""
    if not results:
      return
    deltas = [0] * len(results)
    if old_results:
      assert len(old_results) == len(results)
      deltas = list(map(sub, results, old_results))
    output = ''
    for idx, val in enumerate(results):
      round_val = self.__CleanRound(val, precision)
      round_delta = self.__CleanRound(deltas[idx], precision)
      output += self.__PadString(str(round_val), 8, True) + ' '
      output += self.__PadAndColor('(' + str(round_delta) + ')', 8, False,
          self.__GetDiffColor(deltas[idx]))

    return output

  def __PrintPidLabelStats(self, pid, snapshot):
    """Returns a string that includes the columns pid and process name for
    the specified |pid|.  This lines up with the associated header."""
    if not snapshot or not snapshot.HasResults():
      return

    name_length = max(8, snapshot.GetLongestNameLength())
    name = snapshot.GetNameForPid(pid)

    output = self.__PadAndColor(pid, 8, True, 'DARK_YELLOW') + ' '
    output += self.__PadAndColor(name, name_length, False, None)
    return output

  def __PrintTimestampStats(self, snapshot):
    """Returns a string that includes the timestamp of the |snapshot|.  This
    lines up with the associated header."""
    if not snapshot or not snapshot.HasResults():
      return

    timestamp_length = max(8, len("Timestamp"))
    timestamp = round(snapshot.GetTimestamp(), 2)

    output = self.__PadString(str(timestamp), timestamp_length, True)
    return output

  def __PrintMemoryStats(self, pid, snapshot, prev_snapshot):
    """Returns a string that includes memory statistics of the |snapshot|.  This
    lines up with the associated header."""
    if not snapshot or not snapshot.HasResults():
      return

    results = snapshot.GetMemoryResults(pid)
    if not results:
      return

    old_results = prev_snapshot.GetMemoryResults(pid) if prev_snapshot else None
    return self.__PrintArrayWithDeltas(results, old_results, 2)

  def __PrintNetworkStats(self, userid, snapshot, prev_snapshot):
    """Returns a string that includes network statistics of the |snapshot|. This
    lines up with the associated header."""
    if not snapshot or not snapshot.HasResults():
      return

    results = snapshot.GetNetworkResults(userid)
    if not results:
      return

    old_results = None
    if prev_snapshot:
      old_results = prev_snapshot.GetNetworkResults(userid)
    return self.__PrintArrayWithDeltas(results, old_results, 0)

  def __PrintNulledNetworkStats(self):
    """Returns a string that includes empty network statistics.  This lines up
    with the associated header.  This is used when showing statistics for pids
    that share the same application userId.  Network statistics should only be
    shown once for each application userId."""
    stats = ''
    for title in self.__NETWORK_COLUMN_TITLES:
      stats += self.__PadString('-', 8, True) + ' '
      stats += self.__PadString('', 8, True)
    return stats

  def __PrintHeaderHelper(self,
                          snapshot,
                          show_labels,
                          show_timestamp,
                          show_mem,
                          show_net,
                          show_trailer):
    """Helper method to concat various header entries together into one header.
    This will line up with a entry built by __PrintStatsHelper if the same
    values are passed to it."""
    titles = []
    if show_labels:
      titles.append(self.__PrintPidLabelHeader(snapshot))

    if show_timestamp:
      titles.append(self.__PrintTimestampHeader())

    if show_mem:
      titles.append(self.__PrintMemoryStatsHeader())

    if show_net:
      titles.append(self.__PrintNetworkStatsHeader())

    if show_trailer:
      titles.append(self.__PrintTrailingHeader(snapshot))

    return ' '.join(titles)

  def __PrintStatsHelper(self,
                         pid,
                         snapshot,
                         prev_snapshot,
                         show_labels,
                         show_timestamp,
                         show_mem,
                         show_net):
    """Helper method to concat various stats entries together into one line.
    This will line up with a header built by __PrintHeaderHelper if the same
    values are passed to it."""
    stats = []
    if show_labels:
      stats.append(self.__PrintPidLabelStats(pid, snapshot))

    if show_timestamp:
      stats.append(self.__PrintTimestampStats(snapshot))

    if show_mem:
      stats.append(self.__PrintMemoryStats(pid, snapshot, prev_snapshot))

    if show_net:
      userid = snapshot.GetUserIdForPid(pid)
      show_userid = snapshot.IsFirstPidForUserId(pid)
      if userid and show_userid:
        stats.append(self.__PrintNetworkStats(userid, snapshot, prev_snapshot))
      else:
        stats.append(self.__PrintNulledNetworkStats())

    return ' '.join(stats)

  def PrettyPrint(self, snapshot, prev_snapshot, show_mem=True, show_net=True):
    """Prints |snapshot| to the console.  This will show memory and/or network
    deltas between |snapshot| and |prev_snapshot|.  This will also either color
    or overwrite the previous entries based on |self.can_color| and
    |self.overwrite|.  If |show_mem| is True, this will attempt to show memory
    statistics.  If |show_net| is True, this will attempt to show network
    statistics."""
    self.__ClearScreen()

    if not snapshot or not snapshot.HasResults():
      self.__OutputLine("No results...")
      return

    # Output Format
    show_label = True
    show_timestamp = False
    show_trailer = True

    self.__OutputLine(self.__PrintHeaderHelper(snapshot,
                                               show_label,
                                               show_timestamp,
                                               show_mem,
                                               show_net,
                                               show_trailer))

    for (userid, pid, name) in snapshot.GetPidInfo():
      self.__OutputLine(self.__PrintStatsHelper(pid,
                                                snapshot,
                                                prev_snapshot,
                                                show_label,
                                                show_timestamp,
                                                show_mem,
                                                show_net))

  def PrettyFile(self,
                 file_path,
                 snapshots,
                 diff_against_start,
                 show_mem=True,
                 show_net=True):
    """Writes |snapshots| (a list of DeviceSnapshots) to |file_path|.
    |diff_against_start| determines whether or not the snapshot deltas are
    between the first entry and all entries or each previous entry.  This output
    will not follow |self.can_color| or |self.overwrite|.  If |show_mem| is
    True, this will attempt to show memory statistics.  If |show_net| is True,
    this will attempt to show network statistics."""
    if not file_path or not snapshots:
      return

    # Output Format
    show_label = False
    show_timestamp = True
    show_trailer = False

    pids = self.__FindPidsForSnapshotList(snapshots)

    # Disable special output formatting for file writing.
    can_color = self.can_color
    self.can_color = False

    with open(file_path, 'w') as out:
      for (userid, pid, name) in pids:
        out.write(name + ' (' + str(pid) + '):\n')
        out.write(self.__PrintHeaderHelper(None,
                                           show_label,
                                           show_timestamp,
                                           show_mem,
                                           show_net,
                                           show_trailer))
        out.write('\n')

        prev_snapshot = None
        for snapshot in snapshots:
          has_mem = show_mem and snapshot.GetMemoryResults(pid) is not None
          has_net = show_net and snapshot.GetNetworkResults(userid) is not None
          if not has_mem and not has_net:
            continue
          out.write(self.__PrintStatsHelper(pid,
                                            snapshot,
                                            prev_snapshot,
                                            show_label,
                                            show_timestamp,
                                            show_mem,
                                            show_net))
          out.write('\n')
          if not prev_snapshot or not diff_against_start:
            prev_snapshot = snapshot
        out.write('\n\n')

    # Restore special output formatting.
    self.can_color = can_color

  def PrettyGraph(self, file_path, snapshots):
    """Creates a pdf graph of |snapshots| (a list of DeviceSnapshots) at
    |file_path|.  This currently only shows memory stats and no network
    stats."""
    # Import these here so the rest of the functionality doesn't rely on
    # matplotlib
    from matplotlib import pyplot
    from matplotlib.backends.backend_pdf import PdfPages

    if not file_path or not snapshots:
      return

    pids = self.__FindPidsForSnapshotList(snapshots)

    pp = PdfPages(file_path)
    for (userid, pid, name) in pids:
      figure = pyplot.figure()
      ax = figure.add_subplot(1, 1, 1)
      ax.set_xlabel('Time (s)')
      ax.set_ylabel('MB')
      ax.set_title(name + ' (' + pid + ')')

      mem_list = [[] for x in range(len(self.__MEMORY_COLUMN_TITLES))]
      timestamps = []

      for snapshot in snapshots:
        results = snapshot.GetMemoryResults(pid)
        if not results:
          continue

        timestamps.append(round(snapshot.GetTimestamp(), 2))

        assert len(results) == len(self.__MEMORY_COLUMN_TITLES)
        for idx, result in enumerate(results):
          mem_list[idx].append(result)

      colors = []
      for data in mem_list:
        colors.append(ax.plot(timestamps, data)[0])
        for i in range(len(timestamps)):
          ax.annotate(data[i], xy=(timestamps[i], data[i]))
      figure.legend(colors, self.__MEMORY_COLUMN_TITLES)
      pp.savefig()
    pp.close()

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--process',
                      dest='procname',
                      help="A (sub)string to match against process names.")
  parser.add_argument('-p',
                      '--pid',
                      dest='pid',
                      type=Validator.ValidateNonNegativeNumber,
                      help='Which pid to scan for.')
  parser.add_argument('-d',
                      '--device',
                      dest='device',
                      help='Device serial to scan.')
  parser.add_argument('-t',
                      '--timelimit',
                      dest='timelimit',
                      type=Validator.ValidateNonNegativeNumber,
                      help='How long to track memory in seconds.')
  parser.add_argument('-f',
                      '--frequency',
                      dest='frequency',
                      default=0,
                      type=Validator.ValidateNonNegativeNumber,
                      help='How often to poll in seconds.')
  parser.add_argument('-s',
                      '--diff-against-start',
                      dest='diff_against_start',
                      action='store_true',
                      help='Whether or not to always compare against the'
                           ' original memory values for deltas.')
  parser.add_argument('-b',
                      '--boring-output',
                      dest='dull_output',
                      action='store_true',
                      help='Whether or not to dull down the output.')
  parser.add_argument('-k',
                      '--keep-results',
                      dest='no_overwrite',
                      action='store_true',
                      help='Keeps printing the results in a list instead of'
                           ' overwriting the previous values.')
  parser.add_argument('-g',
                      '--graph-file',
                      dest='graph_file',
                      type=Validator.ValidatePdfPath,
                      help='PDF file to save graph of memory stats to.')
  parser.add_argument('-o',
                      '--text-file',
                      dest='text_file',
                      type=Validator.ValidatePath,
                      help='File to save memory tracking stats to.')
  parser.add_argument('-m',
                      '--memory',
                      dest='show_mem',
                      action='store_true',
                      help='Whether or not to show memory stats. True by'
                           ' default unless --n is specified.')
  parser.add_argument('-n',
                      '--net',
                      dest='show_net',
                      action='store_true',
                      help='Whether or not to show network stats. False by'
                           ' default.')

  args = parser.parse_args()

  # Add a basic filter to make sure we search for something.
  if not args.procname and not args.pid:
    args.procname = 'chrome'

  # Make sure we show memory stats if nothing was specifically requested.
  if not args.show_net and not args.show_mem:
    args.show_mem = True

  devil_chromium.Initialize()

  curses.setupterm()

  printer = OutputBeautifier(not args.dull_output, not args.no_overwrite)

  sys.stdout.write("Running... Hold CTRL-C to stop (or specify timeout).\n")
  try:
    last_time = time.time()

    adb = None
    old_snapshot = None
    snapshots = []
    while not args.timelimit or Timer.GetTimestamp() < float(args.timelimit):
      # Check if we need to track another device
      device = DeviceHelper.GetDeviceToTrack(args.device)
      if not device:
        adb = None
      elif not adb or device != str(adb):
        #adb = adb_wrapper.AdbWrapper(device)
        adb = device_utils.DeviceUtils(device)
        old_snapshot = None
        snapshots = []
        try:
          adb.EnableRoot()
        except device_errors.CommandFailedError:
          sys.stderr.write('Unable to run adb as root.\n')
          sys.exit(1)

      # Grab a snapshot if we have a device
      snapshot = None
      if adb:
        pids = DeviceHelper.GetPidsToTrack(adb, args.pid, args.procname)
        snapshot = None
        if pids:
          snapshot = DeviceSnapshot(adb, pids, args.show_mem, args.show_net)

      if snapshot and snapshot.HasResults():
        snapshots.append(snapshot)

      printer.PrettyPrint(snapshot, old_snapshot, args.show_mem, args.show_net)

      # Transfer state for the next iteration and sleep
      delay = max(1, args.frequency)
      if snapshot:
        delay = max(0, args.frequency - (time.time() - last_time))
      time.sleep(delay)

      last_time = time.time()
      if not old_snapshot or not args.diff_against_start:
        old_snapshot = snapshot
  except KeyboardInterrupt:
    pass

  if args.graph_file:
    printer.PrettyGraph(args.graph_file, snapshots)

  if args.text_file:
    printer.PrettyFile(args.text_file,
                       snapshots,
                       args.diff_against_start,
                       args.show_mem,
                       args.show_net)

if __name__ == '__main__':
  sys.exit(main(sys.argv))
