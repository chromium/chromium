# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""USB echo gadget module.

This gadget has pairs of IN/OUT endpoints that echo packets back to the host.
"""

import uuid

import composite_gadget
import usb_constants
import usb_descriptors


class EchoCompositeFeature(composite_gadget.CompositeFeature):
  """Composite device feature that echos data back to the host.
  """

  def __init__(self, endpoints):
    """Create an echo gadget.
    """
    fs_interfaces = []
    hs_interfaces = []

    if len(endpoints) >= 1:
      iface_num, iface_string, in_endpoint, out_endpoint = endpoints[0]
      fs_intr_interface_desc = usb_descriptors.InterfaceDescriptor(
          bInterfaceNumber=iface_num,
          bInterfaceClass=usb_constants.DeviceClass.VENDOR,
          bInterfaceSubClass=0,
          bInterfaceProtocol=0,
          iInterface=iface_string,
      )
      fs_intr_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=out_endpoint,
          bmAttributes=usb_constants.TransferType.INTERRUPT,
          wMaxPacketSize=64,
          bInterval=1  # 1ms
      ))
      fs_intr_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=in_endpoint,
          bmAttributes=usb_constants.TransferType.INTERRUPT,
          wMaxPacketSize=64,
          bInterval=1  # 1ms
      ))
      fs_interfaces.append(fs_intr_interface_desc)

      hs_intr_interface_desc = usb_descriptors.InterfaceDescriptor(
          bInterfaceNumber=iface_num,
          bInterfaceClass=usb_constants.DeviceClass.VENDOR,
          bInterfaceSubClass=0,
          bInterfaceProtocol=0,
          iInterface=iface_string
      )
      hs_intr_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=out_endpoint,
          bmAttributes=usb_constants.TransferType.INTERRUPT,
          wMaxPacketSize=64,
          bInterval=4  # 1ms
      ))
      hs_intr_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=in_endpoint,
          bmAttributes=usb_constants.TransferType.INTERRUPT,
          wMaxPacketSize=64,
          bInterval=4  # 1ms
      ))
      hs_interfaces.append(hs_intr_interface_desc)

    if len(endpoints) >= 2:
      iface_num, iface_string, in_endpoint, out_endpoint = endpoints[1]
      fs_bulk_interface_desc = usb_descriptors.InterfaceDescriptor(
          bInterfaceNumber=iface_num,
          bInterfaceClass=usb_constants.DeviceClass.VENDOR,
          bInterfaceSubClass=0,
          bInterfaceProtocol=0,
          iInterface=iface_string
      )
      fs_bulk_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=out_endpoint,
          bmAttributes=usb_constants.TransferType.BULK,
          wMaxPacketSize=64,
          bInterval=0
      ))
      fs_bulk_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=in_endpoint,
          bmAttributes=usb_constants.TransferType.BULK,
          wMaxPacketSize=64,
          bInterval=0
      ))
      fs_interfaces.append(fs_bulk_interface_desc)

      hs_bulk_interface_desc = usb_descriptors.InterfaceDescriptor(
          bInterfaceNumber=iface_num,
          bInterfaceClass=usb_constants.DeviceClass.VENDOR,
          bInterfaceSubClass=0,
          bInterfaceProtocol=0,
          iInterface=iface_string
      )
      hs_bulk_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=out_endpoint,
          bmAttributes=usb_constants.TransferType.BULK,
          wMaxPacketSize=512,
          bInterval=0
      ))
      hs_bulk_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=in_endpoint,
          bmAttributes=usb_constants.TransferType.BULK,
          wMaxPacketSize=512,
          bInterval=0
      ))
      hs_interfaces.append(hs_bulk_interface_desc)

    if len(endpoints) >= 3:
      iface_num, iface_string, in_endpoint, out_endpoint = endpoints[2]
      fs_interfaces.append(usb_descriptors.InterfaceDescriptor(
          bInterfaceNumber=iface_num,
          bInterfaceClass=usb_constants.DeviceClass.VENDOR,
          bInterfaceSubClass=0,
          bInterfaceProtocol=0,
          iInterface=iface_string
      ))
      fs_isoc_interface_desc = usb_descriptors.InterfaceDescriptor(
          bInterfaceNumber=iface_num,
          bAlternateSetting=1,
          bInterfaceClass=usb_constants.DeviceClass.VENDOR,
          bInterfaceSubClass=0,
          bInterfaceProtocol=0,
          iInterface=iface_string
      )
      fs_isoc_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=out_endpoint,
          bmAttributes=usb_constants.TransferType.ISOCHRONOUS,
          wMaxPacketSize=1023,
          bInterval=1  # 1ms
      ))
      fs_isoc_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=in_endpoint,
          bmAttributes=usb_constants.TransferType.ISOCHRONOUS,
          wMaxPacketSize=1023,
          bInterval=1  # 1ms
      ))
      fs_interfaces.append(fs_isoc_interface_desc)

      hs_interfaces.append(usb_descriptors.InterfaceDescriptor(
          bInterfaceNumber=iface_num,
          bInterfaceClass=usb_constants.DeviceClass.VENDOR,
          bInterfaceSubClass=0,
          bInterfaceProtocol=0,
          iInterface=iface_string
      ))
      hs_isoc_interface_desc = usb_descriptors.InterfaceDescriptor(
          bInterfaceNumber=iface_num,
          bAlternateSetting=1,
          bInterfaceClass=usb_constants.DeviceClass.VENDOR,
          bInterfaceSubClass=0,
          bInterfaceProtocol=0,
          iInterface=iface_string
      )
      hs_isoc_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=out_endpoint,
          bmAttributes=usb_constants.TransferType.ISOCHRONOUS,
          wMaxPacketSize=512,
          bInterval=4  # 1ms
      ))
      hs_isoc_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
          bEndpointAddress=in_endpoint,
          bmAttributes=usb_constants.TransferType.ISOCHRONOUS,
          wMaxPacketSize=512,
          bInterval=4  # 1ms
      ))
      hs_interfaces.append(hs_isoc_interface_desc)

    super(EchoCompositeFeature, self).__init__(fs_interfaces, hs_interfaces)

  def ReceivePacket(self, endpoint, data):
    """Echo a packet back to the host.

    Args:
      endpoint: Incoming endpoint (must be an OUT pipe).
      data: Packet data.
    """
    assert endpoint & usb_constants.Dir.IN == 0

    self.SendPacket(endpoint | usb_constants.Dir.IN, data)


class EchoGadget(composite_gadget.CompositeGadget):
  """Echo gadget.
  """

  def __init__(self):
    """Create an echo gadget.
    """
    device_desc = usb_descriptors.DeviceDescriptor(
        idVendor=usb_constants.VendorID.GOOGLE,
        idProduct=usb_constants.ProductID.GOOGLE_ECHO_GADGET,
        bcdUSB=0x0200,
        iManufacturer=1,
        iProduct=2,
        iSerialNumber=3,
        bcdDevice=0x0100)

    feature = EchoCompositeFeature(
        endpoints=[(0, 4, 0x81, 0x01), (1, 5, 0x82, 0x02), (2, 6, 0x83, 0x03)])

    super(EchoGadget, self).__init__(device_desc, [feature])
    self.AddStringDescriptor(1, 'Google Inc.')
    self.AddStringDescriptor(2, 'Echo Gadget')
    self.AddStringDescriptor(3, '{:06X}'.format(uuid.getnode()))
    self.AddStringDescriptor(4, 'Interrupt Echo')
    self.AddStringDescriptor(5, 'Bulk Echo')
    self.AddStringDescriptor(6, 'Isochronous Echo')

    # Enable Microsoft OS Descriptors for Windows 8 and above.
    self.EnableMicrosoftOSDescriptorsV1(vendor_code=0x01)
    # These are used to force Windows to load WINUSB.SYS for the echo functions.
    self.SetMicrosoftCompatId(0, 'WINUSB')
    self.SetMicrosoftCompatId(1, 'WINUSB')
    self.SetMicrosoftCompatId(2, 'WINUSB')

    self.AddDeviceCapabilityDescriptor(usb_descriptors.ContainerIdDescriptor(
        ContainerID=uuid.uuid4().bytes_le))

def RegisterHandlers():
  """Registers web request handlers with the application server."""

  import server
  from tornado import web

  class WebConfigureHandler(web.RequestHandler):

    def post(self):
      server.SwitchGadget(EchoGadget())

  server.app.add_handlers('.*$', [
      (r'/echo/configure', WebConfigureHandler),
  ])
