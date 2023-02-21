#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""protoc plugin to create C++ reader/writer for JSON-encoded protobufs

The reader/writer use Chrome's base::Values.
"""

import os
import sys

from util import plugin_protos, types, writer


class CppConverterWriter(writer.CodeWriter):
  def WriteProtoFile(self, proto_file, output_dir):
    err = proto_file.CheckSupported()
    if err:
      self.AddError(err)
      return

    self.WriteCStyleHeader()

    self.Output('#include "{output_dir}{generated_pb_h}"',
                output_dir=output_dir + '/' if output_dir else '',
                generated_pb_h=proto_file.CppBaseHeader())
    self.Output('')

    # import is not supported
    assert [] == proto_file.GetDependencies()

    self.Output('// base dependencies')
    self.Output('#include "base/values.h"')
    self.Output('')
    self.Output('#include <memory>')
    self.Output('#include <string>')
    self.Output('#include <utility>')
    self.Output('')

    namespaces = proto_file.ProtoNamespaces() + ['json']
    for name in namespaces:
      self.Output('namespace {name} {{', name=name)
      self.IncreaseIndent()

    for message in proto_file.GetMessages():
      self.WriteMessage(message)

    # Nothing to do for enums

    for name in namespaces:
      self.DecreaseIndent()
      self.Output('}}')

  def WriteMessage(self, message):
    self.Output('class {class_name} {{',
                class_name=message.CppConverterClassName())
    self.Output(' public:')
    with self.AddIndent():
      for nested_class in message.GetMessages():
        self.WriteMessage(nested_class)

      generated_class_name = message.QualifiedTypes().cpp_base
      # Nothing to write for enums.

      self.Output(
          'static bool ReadFromValue(const base::Value& dict_value, {generated_class_name}* message) {{\n'
          '  const base::Value::Dict* dict = dict_value.GetIfDict();\n'
          '  if (!dict) goto error;\n'
          '',
          generated_class_name=generated_class_name)

      with self.AddIndent():
        for field_proto in message.GetFields():
          self.WriteFieldRead(field_proto)

      self.Output(
          '  return true;\n'
          '\n'
          'error:\n'
          '  return false;\n'
          '}}\n'
          '\n'
          'static base::Value WriteToValue(const {generated_class_name}& message) {{\n'
          '  base::Value::Dict dict;\n'
          '',
          generated_class_name=generated_class_name)

      with self.AddIndent():
        for field_proto in message.GetFields():
          self.FieldWriteToValue(field_proto)

      self.Output(
          '  return base::Value(std::move(dict));\n'
          '',
          generated_class_name=generated_class_name)
      self.Output('}}')

    self.Output('}};')
    self.Output('')

  def FieldWriteToValue(self, field):
    if field.IsRepeated():
      self.Output('{{')
    else:
      self.Output('if (message.has_{field_name}()) {{\n', field_name=field.name)

    with self.AddIndent():
      if field.IsRepeated():
        self.RepeatedMemberFieldWriteToValue(field)
      else:
        self.OptionalMemberFieldWriteToValue(field)

    self.Output('}}')

  def RepeatedMemberFieldWriteToValue(self, field):
    if field.IsClassType():
      self.Output(
          'const auto& repeated_field = message.{field_name}();\n'
          'base::Value::List field_list;\n'
          'field_list.reserve(repeated_field.size());\n'
          'for (const auto& element : repeated_field) {{\n'
          '  field_list.Append(\n'
          '      {inner_class_converter}::WriteToValue(element));\n'
          '}}\n'
          'dict.Set("{field_number}", std::move(field_list));\n',
          field_number=field.JavascriptIndex(),
          field_name=field.name,
          inner_class_converter=field.CppConverterType()
      )
    else:
      self.Output(
          'const auto& repeated_field = message.{field_name}();\n'
          'base::Value::List field_list;\n'
          'field_list.reserve(repeated_field.size());\n'
          'for (const auto& element : repeated_field) {{\n'
          '  field_list.Append(element);\n'
          '}}\n'
          'dict.Set("{field_number}", std::move(field_list));\n',
          field_number=field.JavascriptIndex(),
          field_name=field.name
      )

  def OptionalMemberFieldWriteToValue(self, field):
    if field.IsClassType():
      self.Output(
          'dict.Set("{field_number}",\n'
          '         {inner_class_converter}::WriteToValue(\n'
          '             message.{field_name}()));\n',
          field_number=field.JavascriptIndex(),
          field_name=field.name,
          inner_class_converter=field.CppConverterType()
      )
    else:
      self.Output(
          'dict.Set("{field_number}", message.{field_name}());\n',
          field_number=field.JavascriptIndex(),
          field_name=field.name,
          value_type=field.CppValueType()
      )

  def WriteFieldRead(self, field):
    self.Output('if (const auto* value = dict->Find("{field_number}")) {{',
                field_number=field.JavascriptIndex())

    with self.AddIndent():
      if field.IsRepeated():
        self.RepeatedMemberFieldRead(field)
      else:
        self.OptionalMemberFieldRead(field)

    self.Output('}}')

  def RepeatedMemberFieldRead(self, field):
    self.Output(
        'if (!value->is_list()) {{\n'
        '  goto error;\n'
        '}}\n'
        'for (const auto& element : value->GetList()) {{\n'
    )

    with self.AddIndent():
      if field.IsClassType():
        self.Output(
            'if (!{inner_class_parser}::ReadFromValue(element, message->add_{field_name}())) {{\n'
            '  goto error;\n'
            '}}\n',
            field_name=field.name,
            inner_class_parser=field.CppConverterType()
        )
      else:
        self.Output(
            'if (!{predicate}) {{\n'
            '  goto error;\n'
            '}}\n'
            'message->add_{field_name}(element.Get{value_type}());\n',
            field_name=field.name,
            value_type=field.CppValueType(),
            predicate=field.CppValuePredicate('element')
        )

    self.Output('}}\n')

  def OptionalMemberFieldRead(self, field):
    if field.IsClassType():
      self.Output(
          'if (!{inner_class_parser}::ReadFromValue(*value, message->mutable_{field_name}())) {{\n'
          '  goto error;\n'
          '}}\n',
          field_number=field.JavascriptIndex(),
          field_name=field.name,
          inner_class_parser=field.CppConverterType()
      )
    else:
      self.Output(
          'if (!{predicate}) {{\n'
          '  goto error;\n'
          '}}\n'
          'message->set_{field_name}(value->Get{value_type}());\n',
          field_name=field.name,
          value_type=field.CppValueType(),
          predicate=field.CppValuePredicate('(*value)')
      )


def Indented(s, indent=2):
  return '\n'.join((' ' * indent) + p for p in s.rstrip('\n').split('\n'))


def SetBinaryStdio():
  import platform
  if platform.system() == 'Windows':
    import msvcrt
    msvcrt.setmode(sys.stdin.fileno(), os.O_BINARY)
    msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)


def ReadRequestFromStdin():
  stream = sys.stdin if sys.version_info[0] < 3 else sys.stdin.buffer
  data = stream.read()
  return plugin_protos.PluginRequestFromString(data)


def main():
  SetBinaryStdio()
  request = ReadRequestFromStdin()
  response = plugin_protos.PluginResponse()

  output_dir = request.GetArgs().get('output_dir', '')

  for proto_file in request.GetAllFiles():
    types.RegisterProtoFile(proto_file)

    cppwriter = CppConverterWriter()
    cppwriter.WriteProtoFile(proto_file, output_dir)

    converter_filename = proto_file.CppConverterFilename()
    if output_dir:
      converter_filename = os.path.join(output_dir,
                                        os.path.split(converter_filename)[1])

    response.AddFileWithContent(converter_filename, cppwriter.GetValue())
    if cppwriter.GetErrors():
      response.AddError('\n'.join(cppwriter.GetErrors()))

  response.WriteToStdout()


if __name__ == '__main__':
  main()
