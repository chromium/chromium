# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from google.protobuf.descriptor_pb2 import FieldDescriptorProto


_cpp_base_type_map = {
    FieldDescriptorProto.TYPE_DOUBLE: 'double',
    FieldDescriptorProto.TYPE_FLOAT: 'float',
    FieldDescriptorProto.TYPE_INT64: None,
    FieldDescriptorProto.TYPE_UINT64: None,
    FieldDescriptorProto.TYPE_INT32: 'int',
    FieldDescriptorProto.TYPE_FIXED64: None,
    FieldDescriptorProto.TYPE_FIXED32: None,
    FieldDescriptorProto.TYPE_BOOL: 'bool',
    FieldDescriptorProto.TYPE_STRING: 'std::string',
    FieldDescriptorProto.TYPE_GROUP: None,
    FieldDescriptorProto.TYPE_MESSAGE: None,
    FieldDescriptorProto.TYPE_BYTES: None,
    FieldDescriptorProto.TYPE_UINT32: None,
    FieldDescriptorProto.TYPE_ENUM: 'int',
    FieldDescriptorProto.TYPE_SFIXED32: None,
    FieldDescriptorProto.TYPE_SFIXED64: None,
    FieldDescriptorProto.TYPE_SINT32: None,
    FieldDescriptorProto.TYPE_SINT64: None,
}

# Maps c++ types to names used in base::Value methods (like GetDouble,
# GetInteger, etc).
_cpp_type_to_value_type_map = {
    'double': 'Double',
    'float': 'Double',
    'int': 'Int',
    'bool': 'Bool',
    'std::string': 'String',
}


_proto_path_to_file_map = {}


def RegisterProtoFile(proto_file):
  _proto_path_to_file_map[proto_file.Filename()] = proto_file
  RegisterTypesForFile(proto_file)


def GetProtoFileForFilename(filename):
  proto_file = _proto_path_to_file_map[filename]
  assert proto_file
  return proto_file


def GetCppPrimitiveType(type_name):
  return _cpp_base_type_map[type_name]


def GetCppValueType(primitive_type):
  return _cpp_type_to_value_type_map[primitive_type]


def GetCppValuePredicate(primitive_type, variable_name):
  if primitive_type == 'double' or primitive_type == 'float':
    return '({var}.is_int() || {var}.is_double())'.format(var=variable_name)
  else:
    return '{var}.is_{value_type}()'.format(
        var=variable_name, value_type=GetCppValueType(primitive_type).lower())


# TYPE_ENUM and TYPE_MESSAGE are supported, but their types depend on type_name.
_java_base_type_map = {
    FieldDescriptorProto.TYPE_DOUBLE: 'double',
    FieldDescriptorProto.TYPE_FLOAT: 'float',
    FieldDescriptorProto.TYPE_INT64: None,
    FieldDescriptorProto.TYPE_UINT64: None,
    FieldDescriptorProto.TYPE_INT32: 'int',
    FieldDescriptorProto.TYPE_FIXED64: None,
    FieldDescriptorProto.TYPE_FIXED32: None,
    FieldDescriptorProto.TYPE_BOOL: 'boolean',
    FieldDescriptorProto.TYPE_STRING: 'String',
    FieldDescriptorProto.TYPE_GROUP: None,
    FieldDescriptorProto.TYPE_MESSAGE: None,
    FieldDescriptorProto.TYPE_BYTES: None,
    FieldDescriptorProto.TYPE_UINT32: None,
    FieldDescriptorProto.TYPE_ENUM: None,
    FieldDescriptorProto.TYPE_SFIXED32: None,
    FieldDescriptorProto.TYPE_SFIXED64: None,
    FieldDescriptorProto.TYPE_SINT32: None,
    FieldDescriptorProto.TYPE_SINT64: None,
}


# Maps java primitive types to java types usable in containers (e.g.
# List<Integer>).
_java_primitive_to_object_map = {
    'boolean': 'Boolean',
    'int': 'Integer',
    'long': 'Long',
    'float': 'Float',
    'double': 'Double',
}


def GetJavaPrimitiveType(field_type):
  return _java_base_type_map[field_type]


def GetJavaObjectType(java_base_type):
  if not java_base_type in _java_primitive_to_object_map:
    return java_base_type
  return _java_primitive_to_object_map[java_base_type]


_proto_cpp_converter_class_map = {}
_proto_java_class_map = {}


def ResolveCppConverterType(s):
  if s.startswith('.'):
    s = s[1:]
    return _proto_cpp_converter_class_map[s]
  return s


def ResolveJavaClassType(s):
  if s.startswith('.'):
    s = s[1:]
    return _proto_java_class_map[s]
  return s


class QualifiedTypes(object):
  def __init__(self, proto, java, cpp_base, cpp_converter):
    self.proto = proto
    self.java = java

    # cpp_base is the standard protoc-generated class for this type.
    self.cpp_base = cpp_base
    self.cpp_converter = cpp_converter

  def Register(self):
    _proto_cpp_converter_class_map[self.proto] = self.cpp_converter
    _proto_java_class_map[self.proto] = self.java


def TitleCase(s):
  return ''.join(p[0].upper() + p[1:] for p in s.split('_'))


def QualifiedTypesForChild(name, parent_typenames):
  title_name = TitleCase(name)
  proto = parent_typenames.proto + '.' + name
  java = parent_typenames.java + '.' + title_name
  cpp_base = parent_typenames.cpp_base + '::' + title_name
  cpp_converter = parent_typenames.cpp_converter + '::' + title_name
  return QualifiedTypes(proto, java, cpp_base, cpp_converter)


def RegisterTypesForEnum(proto_enum):
  proto_enum.QualifiedTypes().Register()


def RegisterTypesForMessage(proto_message):
  proto_message.QualifiedTypes().Register()
  for enum in proto_message.GetEnums():
    RegisterTypesForEnum(enum)
  for child in proto_message.GetMessages():
    RegisterTypesForMessage(child)


def RegisterTypesForFile(proto_file):
  for enum in proto_file.GetEnums():
    RegisterTypesForEnum(enum)
  for message in proto_file.GetMessages():
    RegisterTypesForMessage(message)
