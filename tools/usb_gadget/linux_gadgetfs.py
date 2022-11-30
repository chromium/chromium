# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Linux gadgetfs glue.

Exposes a USB gadget using a USB peripheral controller on Linux. The userspace
ABI is documented here:

https://github.com/torvalds/linux/blob/master/drivers/usb/gadget/inode.c
"""

from __future__ import print_function

import errno
import multiprocessing
import os
import struct

from tornado import ioloop

import usb_constants
import usb_descriptors

GADGETFS_NOP = 0
GADGETFS_CONNECT = 1
GADGETFS_DISCONNECT = 2
GADGETFS_SETUP = 3
GADGETFS_SUSPEND = 4

BULK = 0x01
INTERRUPT = 0x02
ISOCHRONOUS = 0x04

USB_TRANSFER_TYPE_TO_MASK = {
    usb_constants.TransferType.BULK: BULK,
    usb_constants.TransferType.INTERRUPT: INTERRUPT,
    usb_constants.TransferType.ISOCHRONOUS: ISOCHRONOUS
}

IN = 0x01
OUT = 0x02

HARDWARE = {
    'beaglebone-black': (
        'musb-hdrc',  # Gadget controller name,
        {
            0x01: ('ep1out', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x81: ('ep1in', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x02: ('ep2out', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x82: ('ep2in', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x03: ('ep3out', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x83: ('ep3in', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x04: ('ep4out', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x84: ('ep4in', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x05: ('ep5out', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x85: ('ep5in', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x06: ('ep6out', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x86: ('ep6in', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x07: ('ep7out', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x87: ('ep7in', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x08: ('ep8out', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x88: ('ep8in', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x09: ('ep9out', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x89: ('ep9in', BULK | INTERRUPT | ISOCHRONOUS, 512),
            0x0A: ('ep10out', BULK | INTERRUPT | ISOCHRONOUS, 64),
            0x8A: ('ep10in', BULK | INTERRUPT | ISOCHRONOUS, 256),
            0x0B: ('ep11out', BULK | INTERRUPT | ISOCHRONOUS, 64),
            0x8B: ('ep11in', BULK | INTERRUPT | ISOCHRONOUS, 256),
            0x0C: ('ep12out', BULK | INTERRUPT | ISOCHRONOUS, 64),
            0x8C: ('ep12in', BULK | INTERRUPT | ISOCHRONOUS, 256),
            0x0D: ('ep13', BULK | INTERRUPT | ISOCHRONOUS, 4096),
            0x8D: ('ep13', BULK | INTERRUPT | ISOCHRONOUS, 4096),
            0x0E: ('ep14', BULK | INTERRUPT | ISOCHRONOUS, 1024),
            0x8E: ('ep14', BULK | INTERRUPT | ISOCHRONOUS, 1024),
            0x0F: ('ep15', BULK | INTERRUPT | ISOCHRONOUS, 1024),
            0x8F: ('ep15', BULK | INTERRUPT | ISOCHRONOUS, 1024),
        }
    )
}


class LinuxGadgetfs(object):
  """Linux gadgetfs-based gadget driver.
  """

  def __init__(self, hardware, mountpoint='/dev/gadget'):
    """Initialize bindings to the Linux gadgetfs interface.

    Args:
      hardware: Hardware type.
      mountpoint: Gadget filesystem mount point.
    """
    self._chip, self._hw_eps = HARDWARE[hardware]
    self._ep_dir = mountpoint
    self._gadget = None
    self._fd = None
    # map from bEndpointAddress to hardware ep name and open file descriptor
    self._ep_fds = {}
    self._io_loop = ioloop.IOLoop.current()

  def Create(self, gadget):
    """Bind a gadget to the USB peripheral controller."""
    self._gadget = gadget
    self._fd = os.open(os.path.join(self._ep_dir, self._chip), os.O_RDWR)
    buf = ''.join([struct.pack('=I', 0),
                   gadget.GetFullSpeedConfigurationDescriptor().Encode(),
                   gadget.GetHighSpeedConfigurationDescriptor().Encode(),
                   gadget.GetDeviceDescriptor().Encode()])
    os.write(self._fd, buf)
    self._io_loop.add_handler(self._fd, self.HandleEvent, self._io_loop.READ)

  def Destroy(self):
    """Unbind the gadget from the USB peripheral controller."""
    self.Disconnected()
    self._io_loop.remove_handler(self._fd)
    os.close(self._fd)
    self._gadget = None
    self._fd = None

  def IsConfigured(self):
    return self._gadget is not None

  def HandleEvent(self, unused_fd, unused_events):
    buf = os.read(self._fd, 12)
    event_type, = struct.unpack_from('=I', buf, 8)

    if event_type == GADGETFS_NOP:
      print('NOP')
    elif event_type == GADGETFS_CONNECT:
      speed, = struct.unpack('=Ixxxxxxxx', buf)
      self.Connected(speed)
    elif event_type == GADGETFS_DISCONNECT:
      self.Disconnected()
    elif event_type == GADGETFS_SETUP:
      request_type, request, value, index, length = struct.unpack(
          '<BBHHHxxxx', buf)
      self.HandleSetup(request_type, request, value, index, length)
    elif event_type == GADGETFS_SUSPEND:
      print('SUSPEND')
    else:
      print('Unknown gadgetfs event type:', event_type)

  def Connected(self, speed):
    print('CONNECT speed={}'.format(speed))
    self._gadget.Connected(self, speed)

  def Disconnected(self):
    print('DISCONNECT')
    for endpoint_addr in self._ep_fds.keys():
      self.StopEndpoint(endpoint_addr)
    self._ep_fds.clear()
    self._gadget.Disconnected()

  def HandleSetup(self, request_type, request, value, index, length):
    print('SETUP bmRequestType=0x{:02X} bRequest=0x{:02X} wValue=0x{:04X} '
          'wIndex=0x{:04X} wLength={}'.format(request_type, request, value,
                                              index, length))

    if request_type & usb_constants.Dir.IN:
      data = self._gadget.ControlRead(
          request_type, request, value, index, length)
      if data is None:
        print('SETUP STALL')
        try:
          os.read(self._fd, 0)  # Backwards I/O stalls the pipe.
        except OSError, e:
          # gadgetfs always returns EL2HLT which we should ignore.
          if e.errno != errno.EL2HLT:
            raise
      else:
        os.write(self._fd, data)
    else:
      data = ''
      if length:
        data = os.read(self._fd, length)
      result = self._gadget.ControlWrite(
          request_type, request, value, index, data)
      if result is None:
        print('SETUP STALL')
        try:
          os.write(self._fd, '')  # Backwards I/O stalls the pipe.
        except OSError, e:
          # gadgetfs always returns EL2HLT which we should ignore.
          if e.errno != errno.EL2HLT:
            raise
      elif not length:
        # Only empty OUT transfers can be ACKed.
        os.read(self._fd, 0)

  def StartEndpoint(self, endpoint_desc):
    """Activate an endpoint.

    To enable a hardware endpoint the appropriate endpoint file must be opened
    and the endpoint descriptors written to it. Linux requires both full- and
    high-speed descriptors to be written for a high-speed device but since the
    endpoint is always reinitialized after disconnect only the high-speed
    endpoint will be valid in this case.

    Args:
      endpoint_desc: Endpoint descriptor.

    Raises:
      RuntimeError: If the hardware endpoint is in use or the configuration
          is not supported by the hardware.
    """
    endpoint_addr = endpoint_desc.bEndpointAddress
    name, hw_ep_type, hw_ep_size = self._hw_eps[endpoint_addr]

    if name in self._ep_fds:
      raise RuntimeError('Hardware endpoint {} already in use.'.format(name))

    ep_type = USB_TRANSFER_TYPE_TO_MASK[
        endpoint_desc.bmAttributes & usb_constants.TransferType.MASK]
    ep_size = endpoint_desc.wMaxPacketSize

    if not hw_ep_type & ep_type:
      raise RuntimeError('Hardware endpoint {} does not support this transfer '
                         'type.'.format(name))
    elif hw_ep_size < ep_size:
      raise RuntimeError('Hardware endpoint {} only supports a maximum packet '
                         'size of {}, {} requested.'
                         .format(name, hw_ep_size, ep_size))

    fd = os.open(os.path.join(self._ep_dir, name), os.O_RDWR)

    buf = struct.pack('=I', 1)
    if self._gadget.GetSpeed() == usb_constants.Speed.HIGH:
      # The full speed endpoint descriptor will not be used but Linux requires
      # one to be provided.
      full_speed_endpoint = usb_descriptors.EndpointDescriptor(
          bEndpointAddress=endpoint_desc.bEndpointAddress,
          bmAttributes=0,
          wMaxPacketSize=0,
          bInterval=0)
      buf = ''.join([buf, full_speed_endpoint.Encode(), endpoint_desc.Encode()])
    else:
      buf = ''.join([buf, endpoint_desc.Encode()])
    os.write(fd, buf)

    pipe_r, pipe_w = multiprocessing.Pipe(False)
    child = None

    # gadgetfs doesn't support polling on the endpoint file descriptors (why?)
    # so we have to start background threads for each.
    if endpoint_addr & usb_constants.Dir.IN:
      def WriterProcess():
        while True:
          data = pipe_r.recv()
          written = os.write(fd, data)
          print('IN bEndpointAddress=0x{:02X} length={}'
                .format(endpoint_addr, written))

      child = multiprocessing.Process(target=WriterProcess)
      self._ep_fds[endpoint_addr] = fd, child, pipe_w
    else:
      def ReceivePacket(unused_fd, unused_events):
        data = pipe_r.recv()
        print('OUT bEndpointAddress=0x{:02X} length={}'
              .format(endpoint_addr, len(data)))
        self._gadget.ReceivePacket(endpoint_addr, data)

      def ReaderProcess():
        while True:
          data = os.read(fd, ep_size)
          pipe_w.send(data)

      child = multiprocessing.Process(target=ReaderProcess)
      pipe_fd = pipe_r.fileno()
      self._io_loop.add_handler(pipe_fd, ReceivePacket, self._io_loop.READ)
      self._ep_fds[endpoint_addr] = fd, child, pipe_r

    child.start()
    print('Started endpoint 0x{:02X}.'.format(endpoint_addr))

  def StopEndpoint(self, endpoint_addr):
    """Deactivate the given endpoint."""
    fd, child, pipe = self._ep_fds.pop(endpoint_addr)
    pipe_fd = pipe.fileno()
    child.terminate()
    child.join()
    if not endpoint_addr & usb_constants.Dir.IN:
      self._io_loop.remove_handler(pipe_fd)
    os.close(fd)
    print('Stopped endpoint 0x{:02X}.'.format(endpoint_addr))

  def SendPacket(self, endpoint_addr, data):
    """Send a packet on the given endpoint."""
    _, _, pipe = self._ep_fds[endpoint_addr]
    pipe.send(data)

  def HaltEndpoint(self, endpoint_addr):
    """Signal a stall condition on the given endpoint."""
    fd, _ = self._ep_fds[endpoint_addr]
    # Reverse I/O direction sets the halt condition on the pipe.
    try:
      if endpoint_addr & usb_constants.Dir.IN:
        os.read(fd, 0)
      else:
        os.write(fd, '')
    except OSError, e:
      # gadgetfs always returns EBADMSG which we should ignore.
      if e.errno != errno.EBADMSG:
        raise
