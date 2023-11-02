# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Microsoft OS 2.0 Descriptor generation utilities.

Classes to help generate Microsoft OS 2.0 descriptors.

Based on documentation here:
https://msdn.microsoft.com/en-us/library/windows/hardware/dn385747.aspx
"""

import uuid

import usb_constants
import usb_descriptors


class PlatformCapabilityDescriptor(usb_descriptors.Descriptor):
  """Microsoft OS 2.0 platform capability descriptor.
  """

  def __init__(self, **kwargs):
    super(PlatformCapabilityDescriptor, self).__init__(**kwargs)
    self._descriptor_set = None

  @property
  def descriptor_set_size(self):
    if self._descriptor_set is None:
      return 0
    return len(self._descriptor_set.Encode())

  def SetDescriptorSet(self, descriptor_set):
    self._descriptor_set = descriptor_set

PlatformCapabilityDescriptor.AddComputedField('bLength', 'B', 'struct_size')
PlatformCapabilityDescriptor.AddFixedField(
    'bDescriptorType', 'B', usb_constants.DescriptorType.DEVICE_CAPABILITY)
PlatformCapabilityDescriptor.AddFixedField(
    'bDevCapabilityType', 'B', usb_constants.CapabilityType.PLATFORM)
PlatformCapabilityDescriptor.AddFixedField('bReserved', 'B', 0)
PlatformCapabilityDescriptor.AddFixedField(
    'MS_OS_20_Platform_Capability_ID', '16s',
    uuid.UUID('{D8DD60DF-4589-4CC7-9CD2-659D9E648A9F}').bytes_le)
PlatformCapabilityDescriptor.AddField('dwWindowsVersion', 'I')
PlatformCapabilityDescriptor.AddComputedField(
    'wMSOSDescriptorSetTotalLength', 'H', 'descriptor_set_size')
PlatformCapabilityDescriptor.AddField('bMS_VendorCode', 'B')
PlatformCapabilityDescriptor.AddField('bAltEnumCode', 'B', default=0)


class DescriptorSetHeader(usb_descriptors.DescriptorContainer):
  """Microsoft OS 2.0 descriptor set header.
  """
  pass

DescriptorSetHeader.AddComputedField('wLength', 'H', 'struct_size')
DescriptorSetHeader.AddFixedField('wDescriptorType', 'H', 0x00)
DescriptorSetHeader.AddField('dwWindowsVersion', 'I')
DescriptorSetHeader.AddComputedField('wTotalLength', 'H', 'total_size')


class ConfigurationSubsetHeader(usb_descriptors.DescriptorContainer):
  """Microsoft OS 2.0 configuration subset header.
  """
  pass

ConfigurationSubsetHeader.AddComputedField('wLength', 'H', 'struct_size')
ConfigurationSubsetHeader.AddFixedField('wDescriptorType', 'H', 0x01)
ConfigurationSubsetHeader.AddField('bConfigurationValue', 'B')
ConfigurationSubsetHeader.AddFixedField('bReserved', 'B', 0)
ConfigurationSubsetHeader.AddComputedField('wTotalLength', 'H', 'total_size')


class FunctionSubsetHeader(usb_descriptors.DescriptorContainer):
  """Microsoft OS 2.0 function subset header.
  """
  pass

FunctionSubsetHeader.AddComputedField('wLength', 'H', 'struct_size')
FunctionSubsetHeader.AddFixedField('wDescriptorType', 'H', 0x02)
FunctionSubsetHeader.AddField('bFirstInterface', 'B')
FunctionSubsetHeader.AddFixedField('bReserved', 'B', 0)
FunctionSubsetHeader.AddComputedField('wSubsetLength', 'H', 'total_size')


class CompatibleId(usb_descriptors.Descriptor):
  """Microsoft OS 2.0 compatible ID descriptor.
  """
  pass

CompatibleId.AddComputedField('wLength', 'H', 'struct_size')
CompatibleId.AddFixedField('wDescriptorType', 'H', 0x03)
CompatibleId.AddField('CompatibleID', '8s')
CompatibleId.AddField('SubCompatibleID', '8s', default='')
