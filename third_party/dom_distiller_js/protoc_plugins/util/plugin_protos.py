# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Provides simple state-less wrappers of the proto types used by plugins.

See https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.descriptor.pb
and https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.compiler.plugin.pb
"""

import os
import sys

SCRIPT_DIR = os.path.dirname(__file__)
SRC_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..', '..', '..'))

sys.path.insert(
    1, os.path.join(SRC_DIR, 'third_party', 'protobuf', 'python'))
sys.path.insert(
    1, os.path.join(SRC_DIR, 'third_party', 'protobuf', 'third_party', 'six'))
from google.protobuf.descriptor_pb2 import FieldDescriptorProto
from google.protobuf.compiler import plugin_pb2

from . import types


class PluginRequest(object):
  def __init__(self, proto):
    self.proto = proto

  def GetArgs(self):
    return dict((v.split('=') for v in self.proto.parameter.split(',')))

  def GetAllFiles(self):
    files = [ProtoFile(x) for x in self.proto.proto_file]
    for f in files:
      assert f.Filename() in self.proto.file_to_generate
    return files


def PluginRequestFromString(data):
  request_proto = plugin_pb2.CodeGeneratorRequest()
  request_proto.ParseFromString(data)
  return PluginRequest(request_proto)


class PluginResponse(object):
  def __init__(self):
    self.proto = plugin_pb2.CodeGeneratorResponse()

  def AddFileWithContent(self, filename, content):
    file_proto = self.proto.file.add()
    file_proto.name = filename
    file_proto.content = content

  def AddError(self, err):
    self.proto.error += err + '\n'

  def WriteToStdout(self):
    stream = sys.stdout if sys.version_info[0] < 3 else sys.stdout.buffer
    stream.write(self.proto.SerializeToString())
    stream.flush()


class ProtoFile(object):
  def __init__(self, proto):
    self.proto = proto
    self.qualified_types = types.QualifiedTypes(
        self.ProtoPackage(),
        self.JavaPackage() + '.' + self.JavaOuterClass(),
        self.CppBaseNamespace(),
        self.CppConverterNamespace()
    )

  def Filename(self):
    return self.proto.name

  def CheckSupported(self):
    if self.proto.service:
      return 'Services are not supported'

    if self.proto.extension:
      return 'Extensions are not supported'

    for child in self.GetMessages() + self.GetEnums():
      err = child.CheckSupported()
      if err:
        return err

  def ProtoPackage(self):
    return self.proto.package if self.proto.HasField('package') else ''

  def ProtoNamespaces(self):
    return self.ProtoPackage().split('.')

  def CppBaseNamespace(self):
    return '::'.join(self.ProtoNamespaces())

  def CppBaseHeader(self):
    assert self.proto.name.endswith('.proto')
    return self.proto.name[:-5] + 'pb.h'

  def CppConverterNamespace(self):
    return self.CppBaseNamespace() + '::json'

  def JavaPackage(self):
    if self.proto.options.HasField('java_package'):
      return self.proto.options.java_package
    else:
      return self.ProtoPackage()

  def GetMessages(self):
    return [ProtoMessage(n, self.qualified_types)
            for n in self.proto.message_type]

  def GetEnums(self):
    return [ProtoEnum(n, self.qualified_types) for n in self.proto.enum_type]

  def GetDependencies(self):
    # import is not supported
    assert [] == self.proto.dependency
    return [types.GetProtoFileForFilename(x) for x in self.proto.dependency]

  def JavaFilename(self):
    return '/'.join(self.JavaQualifiedOuterClass().split('.')) + '.java'

  def JavaOuterClass(self):
    if self.proto.options.HasField('java_outer_classname'):
      return self.proto.options.java_outer_classname
    basename, _ = os.path.splitext(os.path.basename(self.proto.name))
    return types.TitleCase(basename)

  def JavaQualifiedOuterClass(self):
    return self.qualified_types.java

  def CppConverterFilename(self):
    assert self.proto.name.endswith('.proto')
    return self.proto.name[:-6] + '_json_converter.h'


class ProtoMessage(object):
  def __init__(self, proto, parent_typenames):
    self.proto = proto
    self.qualified_types = types.QualifiedTypesForChild(
        proto.name, parent_typenames)

  def CheckSupported(self):
    if self.proto.extension_range:
      return 'Extensions are not supported: ' + self.proto.extension_range

    for child in self.GetFields() + self.GetMessages() + self.GetEnums():
      err = child.CheckSupported()
      if err:
        return err

  def QualifiedTypes(self):
    return self.qualified_types

  def JavaClassName(self):
    return types.TitleCase(self.proto.name)

  def CppConverterClassName(self):
    return types.TitleCase(self.proto.name)

  def GetFields(self):
    return [ProtoField(x) for x in self.proto.field]

  def GetMessages(self):
    return [ProtoMessage(n, self.qualified_types)
            for n in self.proto.nested_type]

  def GetEnums(self):
    return [ProtoEnum(n, self.qualified_types) for n in self.proto.enum_type]


class ProtoField(object):
  def __init__(self, field_proto):
    self.proto = field_proto
    self.name = field_proto.name

    if self.IsClassType() and not self.proto.HasField('type_name'):
      raise TypeError('expected type_name')

  def Extendee(self):
    return self.proto.extendee if self.proto.HasField('extendee') else None

  def IsOptional(self):
    return self.proto.label == FieldDescriptorProto.LABEL_OPTIONAL

  def IsRepeated(self):
    return self.proto.label == FieldDescriptorProto.LABEL_REPEATED

  def IsRequired(self):
    return self.proto.label == FieldDescriptorProto.LABEL_REQUIRED

  def IsClassType(self):
    return self.proto.type == FieldDescriptorProto.TYPE_MESSAGE

  def IsEnumType(self):
    return self.proto.type == FieldDescriptorProto.TYPE_ENUM

  def JavaType(self):
    if self.IsClassType():
      return types.ResolveJavaClassType(self.proto.type_name)
    elif self.IsEnumType():
      return 'int'
    else:
      return types.GetJavaPrimitiveType(self.proto.type)

  def JavaListType(self):
    return types.GetJavaObjectType(self.JavaType())

  def JavascriptIndex(self):
    return self.proto.number

  def JavaName(self):
    return types.TitleCase(self.name)

  def CppConverterType(self):
    return types.ResolveCppConverterType(self.proto.type_name)

  def CppPrimitiveType(self):
    assert not self.IsClassType()
    return types.GetCppPrimitiveType(self.proto.type)

  def CppValueType(self):
    return types.GetCppValueType(self.CppPrimitiveType())

  def CppValuePredicate(self, variable_name):
    return types.GetCppValuePredicate(self.CppPrimitiveType(), variable_name)

  def CheckSupported(self):
    if self.Extendee():
      return 'Unsupported field extension: ' + self.DebugString()

    if self.JavaType() is None:
      return 'Unsupported type for field: ' + self.DebugString()

    if self.IsRequired():
      return 'Required fields not supported: ' + self.DebugString()

    if self.proto.HasField('default_value'):
      return 'Default values are not supported: ' + self.DebugString()

    return None

  def DebugString(self):
    return '{name}, {type}, {extendee}'.format(
        name=self.name,
        type=self.proto.type,
        extendee=self.Extendee())


class ProtoEnum(object):
  def __init__(self, proto, parent_typenames):
    self.proto = proto
    self.qualified_types = types.QualifiedTypesForChild(
        proto.name, parent_typenames)

  def CheckSupported(self):
    if self.proto.HasField('options'):
      return 'Enum options are not supported: ' + self.DebugString()
    for val in self.Values():
      err = val.CheckSupported()
      if err:
        return err + ' ' + self.DebugString()

  def QualifiedTypes(self):
    return self.qualified_types

  def JavaName(self):
    return types.TitleCase(self.proto.name)

  def Values(self):
    return [ProtoEnumValue(x) for x in self.proto.value]


class ProtoEnumValue(object):
  def __init__(self, enum_value_proto):
    self.proto = enum_value_proto

  def GetName(self):
    return self.proto.name

  def GetValue(self):
    return self.proto.number

  def CheckSupported(self):
    if self.proto.HasField('options'):
      return 'Enum value options are not supported: {} {}'.format(
          self.proto.name, self.proto.value)
