# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A pure python library to interface with the Android Debug Bridge daemon.

This a lightweight implementation of the adb socket protocol, the same as the
one used by the adb client binary when communicating with its own server.
It can be used to run commands, push and pull files from/to Android devices,
requiring only that an adb daemon (adb start-server) is running on the host.
"""

from __future__ import print_function

import logging
import os
import pipes
import re
import socket
import stat
import struct


ADB_PORT = 5037
TIMEOUT = 5
ADB_NOT_RUNNING_MESSAGE = 'ADB daemon not running. Run \'adb start-server\'.'

"""Regular expression for matching the output of the 'getprop' Android command.
Sample input:

  [prop1]: [simple value]
  [prop2]: [multiline
  value]
"""
GETPROP_RE = re.compile(r'^\[([^\]]*)\]: \[(.*?)\]$', re.MULTILINE | re.DOTALL)


class ADBClientError(Exception):
  """ADB errors."""
  pass


class ADBHostSession(object):
  """A session handler to communicate with the adb daemon via a TCP socket.

  The design of adb requires that most high-level commands (shell, root, etc)
  must be run in distinct TCP connections (read: the TCP socket must be recycled
  continuously). However, many of these high-level commands typically require
  a variable number of low-level messages to be exchanged.
  This class abstracts the notion of a TCP session with the ADB host, hiding
  the underlying socket and data (de)serialization boilerplate, by means of
  scoped semantics, for instance:

  with ADBHostSession() as session:
    session.SendCmd(...)
    session.ReadMsg(...)
  """

  def __init__(self, transport=None):
    self._sock = None
    self._transport = transport

  def __enter__(self):
    try:
      self._sock = socket.create_connection(('127.0.0.1', ADB_PORT),
                                            timeout=TIMEOUT)
    except socket.error:
      raise ADBClientError(ADB_NOT_RUNNING_MESSAGE)
    if self._transport:
      self.SendCmd('host:transport:' + self._transport)
    return self

  def __exit__(self, exc_type, exc_value, exc_traceback):
    if exc_type is socket.error:
      raise ADBClientError(ADB_NOT_RUNNING_MESSAGE)
    try:
      self._sock.close()
    except Exception as e:
      logging.warn('ADB socket teardown: %s', e)

  def SendCmd(self, cmd):
    cmd = '%04x%s' % (len(cmd), cmd)
    self._sock.sendall(cmd.encode('ascii'))
    self.CheckAck()

  def ReadMsg(self):
    size = int(self._sock.recv(4), 16)
    return self._sock.recv(size)

  def SendCmdAndGetReply(self, cmd):
    self.SendCmd(cmd)
    return self.ReadMsg()

  def CheckAck(self):
    status = self._sock.recv(4)
    if status == 'OKAY':
      return
    elif status == 'FAIL':
      raise ADBClientError('FAIL ' + self.ReadMsg())
    else:
      raise ADBClientError(status or 'EMPTY ACK')

  def ReadAll(self):
    return ''.join(iter(lambda: self._sock.recv(4096), ''))

  def SendRaw(self, *args):
    for arg in args:
      if isinstance(arg, str):
        data = arg
      elif isinstance(arg, int):
        data = struct.pack('I', arg)
      else:
        assert False
      self._sock.sendall(data)

  def RecvRaw(self, size_fmt):
    """size_fmt can be either an integer (buf size) or a string (struct fmt)."""
    size = size_fmt if isinstance(size_fmt, int) else struct.calcsize(size_fmt)
    data = self._sock.recv(size)
    if isinstance(size_fmt, int):
      return data
    if len(data) != size:
      raise ADBClientError('Protocol error: expected %d bytes, got %d' % (
          size, len(data)))
    return struct.unpack(size_fmt, data)


class ADBDevice(object):
  """Handles the interaction with a specific Android device."""

  def __init__(self, serial):
    assert isinstance(serial, str), type(serial)
    self.serial = serial
    all_props = self.Shell(['getprop'])
    self._cached_props = dict(re.findall(GETPROP_RE, all_props))

  def GetProp(self, name, cached=False):
    if cached and name in self._cached_props:
      return self._cached_props[name]
    else:
      return self.Shell(['getprop', name]).rstrip('\r\n')

  def GetState(self):
    with ADBHostSession() as s:
      return s.SendCmdAndGetReply('host-serial:%s:get-state' % self.serial)

  def IsConnected(self):
    return self.GetState() == 'device'

  def Shell(self, cmd):
    # If cmd is a list (like in subprocess.call), quote and escape the args.
    if isinstance(cmd, list):
      cmd = ' '.join(pipes.quote(x) for x in cmd)

    with ADBHostSession(transport=self.serial) as s:
      s.SendCmd('shell:' + cmd)
      return s.ReadAll().replace('\r\n', '\n')  # Nobody likes carriage returns.

  def WaitForDevice(self):
    with ADBHostSession() as s:
      s.SendCmd('host-serial:%s:wait-for-any-device' % self.serial)
      return s.ReadAll() == 'OKAY'

  def RestartShellAsRoot(self):
    with ADBHostSession(transport=self.serial) as s:
      s.SendCmd('root:')
      return s.ReadAll()

  def RemountSystemPartition(self):
    with ADBHostSession(transport=self.serial) as s:
      s.SendCmd('remount:')
      resp = s.RecvRaw(64)
    if 'succeeded' not in resp.lower():
      raise ADBClientError('Remount failed: ' + resp)

  def Reboot(self):
    with ADBHostSession(transport=self.serial) as s:
      s.SendCmd('reboot:')

  def ForwardTCPPort(self, local_port, remote_port):
    with ADBHostSession() as s:
      s.SendCmd('host-serial:%s:forward:tcp:%s;tcp:%s' % (
          self.serial, local_port, remote_port))

  def DisableAllForwards(self):
    with ADBHostSession() as s:
      s.SendCmd('host-serial:%s:killforward-all' % self.serial)

  def Stat(self, device_path):
    device_path = device_path.encode('ascii')
    with ADBHostSession(transport=self.serial) as s:
      s.SendCmd('sync:')
      s.SendRaw('STAT', len(device_path), device_path)
      resp, mode, size, mtime = s.RecvRaw('4sIII')
      assert resp == 'STAT'
      return mode, size, mtime

  def FileExists(self, device_path):
    return self.Stat(device_path)[0] != 0

  def Push(self, host_path, device_path):
    if not os.path.isfile(host_path):
      raise ADBClientError('Can push only regular files')
    device_path = device_path.encode('ascii')
    device_stat = self.Stat(device_path)
    if device_stat[0] and not stat.S_ISREG(device_stat[0]):
      raise ADBClientError('Target %s exists but is not a file' % device_path)

    with ADBHostSession(transport=self.serial) as s:
      s.SendCmd('sync:')
      send_cmd = '%s,33206' % device_path  # adb supports only rw-rw-rw-.
      s.SendRaw('SEND', len(send_cmd), send_cmd)
      with open(host_path, 'rb') as fd:
        while True:
          data = fd.read(1490)  # Stay close to the MTU for best performance.
          if not data:
            break
          s.SendRaw('DATA', len(data), data)
      local_mtime = int(os.path.getmtime(host_path))
      s.SendRaw('DONE', local_mtime)
      s.CheckAck()
      s.SendRaw('QUIT', 0)

  def Pull(self, device_path, host_path, update_mtime=False):
    """Pulls a file from the device.

    Args:
      device_path: source path of the file to be pulled.
      host_path: destination path on the host.
      update_mtime: preserves the source file mtime if True.
    """
    if os.path.exists(host_path) and not os.path.isfile(host_path):
      raise ADBClientError('Target %s exists but is not a file' % host_path)
    device_path = device_path.encode('ascii')
    device_stat = self.Stat(device_path)
    if device_stat[0] and not stat.S_ISREG(device_stat[0]):
      raise ADBClientError('Source %s exists but is not a file' % device_path)

    with ADBHostSession(transport=self.serial) as s:
      s.SendCmd('sync:')
      s.SendRaw('RECV', len(device_path), device_path)
      with open(host_path, 'wb') as fd:
        while True:
          status, size = s.RecvRaw('4sI')
          if status == 'DONE':
            break
          if status != 'DATA':
            raise ADBClientError('Pull failed: ' + status)
          while size > 0:
            data = s.RecvRaw(size)
            if not data:
              raise ADBClientError('Pull failed: connection interrupted')
            fd.write(data)
            size -= len(data)
      if update_mtime:
        os.utime(host_path, (device_stat[2], device_stat[2]))
      s.SendRaw('QUIT', 0)

  def __str__(self):
    return 'ADBDevice [%s]' % self.serial

  __repr__ = __str__


def ListDevices():
  """Lists the connected devices. Returns a list of ADBDevice instances."""
  with ADBHostSession() as s:
    resp = s.SendCmdAndGetReply('host:devices')
  return [ADBDevice(serial=line.split('\t')[0]) for line in resp.splitlines()]


def GetDevice(serial=None):
  """Returns and ADBDevice given its serial.

  The first connected device is returned if serial is None.
  """
  devices = [d for d in ListDevices() if not serial or serial == d.serial]
  return devices[0] if devices else None


def _EndToEndTest():
  """Some minimal non-automated end-to-end testing."""
  import tempfile
  local_test_file = tempfile.mktemp()
  print('Starting test, please  make sure at least one device is connected')
  devices = ListDevices()
  print('Devices:', devices)
  device = GetDevice(devices[0].serial)
  assert device.IsConnected()
  device.RestartShellAsRoot()
  device.WaitForDevice()
  build_fingerprint = device.Shell(['getprop', 'ro.build.fingerprint']).strip()
  print('Build fingerprint', build_fingerprint)
  device.RemountSystemPartition()
  assert 'rw' in device.Shell('cat /proc/mounts | grep system')
  mode, size, _ = device.Stat('/system/bin/sh')
  assert mode == 0100755, oct(mode)
  assert size > 1024
  print('Pulling a large file')
  device.Pull('/system/lib/libwebviewchromium.so', local_test_file)
  print('Pushing a large file')
  device.Push(local_test_file, '/data/local/tmp/file name.so')
  remote_md5 = device.Shell('md5 /system/lib/libwebviewchromium.so')[:32]
  remote_md5_copy = device.Shell(['md5', '/data/local/tmp/file name.so'])[:32]
  size = device.Stat('/data/local/tmp/file name.so')[1]
  assert size == os.path.getsize(local_test_file)
  device.Shell(['rm', '/data/local/tmp/file name.so'])
  print('Remote MD5 of the original file is', remote_md5)
  print('Remote MD5 of the copied file is', remote_md5_copy)
  os.unlink(local_test_file)
  assert remote_md5 == remote_md5_copy
  print('[TEST PASSED]')


if __name__ == '__main__':
  _EndToEndTest()
