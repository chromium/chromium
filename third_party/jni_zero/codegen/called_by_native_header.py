# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for calling Java methods from C++."""


def constants_enums(java_class, constant_fields):
  if not constant_fields:
    return ''
  sb = []
  sb.append(f'// Constants\n')
  sb.append(f'enum Java_{java_class.name}_constant_fields {{\n')
  sb.extend(f'  {c.name} = {c.value},\n' for c in constant_fields)
  sb.append('};\n\n')
  return ''.join(sb)
