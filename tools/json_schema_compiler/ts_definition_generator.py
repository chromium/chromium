# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generator that produces a definition file for typescript.

Note: This is a work in progress, and generated definitions may need tweaking.
      See bug: crbug.com/1203307
This script is currently run manually.
"""

import datetime
import os
import subprocess
import tempfile

from code_util import Code
from js_util import JsUtil
from model import *
from schema_util import *

CHROMIUM_SRC = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", ".."))


class TsDefinitionGenerator(object):

  def Generate(self, namespace):
    return _Generator(namespace).Generate()


class _Generator(object):

  def __init__(self, namespace: Namespace):
    self._namespace = namespace
    self._events_required = False
    self._js_util = JsUtil()

  def Generate(self):
    main_code = Code()
    body_code = Code()
    # Generate the definition first to determine if an import is required.
    self._AppendDefinitionBody(body_code)
    # Create copyright header.
    self._AppendChromiumHeader(main_code)
    # Create file overview.
    self._AppendFileOverview(main_code)
    # Create import area.
    self._AppendImportArea(main_code)
    # Create namespaces.
    namespaces_to_close = self._OpenNamespaces(main_code)
    # Append definitions.
    main_code.Concat(body_code)
    # Close namespaces.
    self._CloseNamespaces(main_code, namespaces_to_close)
    # Cleanup a little.
    main_code.TrimTrailingNewlines()
    # If events are needed, add the import.
    if self._events_required:
      main_code.Substitute(
          {"imports": "import {ChromeEvent} from './chrome_event.js';"})
    else:
      main_code.Substitute({"imports": ""})
    main_code = self._ClangFormat(main_code)
    # Final new line.
    main_code.Append()
    return main_code

  def _AppendChromiumHeader(self, c: Code):
    c.Append(f"""// Copyright {datetime.date.today().year} The Chromium Authors
    // Use of this source code is governed by a BSD-style license that can be
    // found in the LICENSE file.""")
    c.Append()

  def _AppendFileOverview(self, c: Code):
    c.Append("""/**
     * @fileoverview Definitions for chrome.{name} API
     * Generated from: {file}
     * run `tools/json_schema_compiler/compiler.py {file} -g ts_definitions` to
     * regenerate.
     */""".format(name=self._namespace.name, file=self._namespace.source_file))
    c.Append()

  def _AppendImportArea(self, c: Code):
    # Assume these declarations will be placed in tools/typescript/definitions.
    c.Append("%(imports)s")
    c.Append()

  def _OpenNamespaces(self, c: Code):
    namespaces_opened = 2
    declare_or_export = "declare"
    # If adding an import the definition file becomes a module.
    # If that happens we must declare something global specifically.
    # Otherwise the definition file is considered global by default.
    if self._events_required:
      c.Sblock("declare global {")
      namespaces_opened += 1
      c.Append()
      declare_or_export = "export"
    c.Sblock(f"{declare_or_export} namespace chrome {{")
    c.Append()
    c.Sblock(f"export namespace {self._namespace.name} {{")
    c.Append()
    return namespaces_opened

  def _AppendDefinitionBody(self, c: Code):
    # Add namespace level properties.
    for prop in self._namespace.properties.values():
      type_name = self._ExtractType(prop.type_)
      # If the ref type has additional properties, do a namespace merge.
      prop_type: Type = prop.type_
      if (len(prop_type.properties) > 0
          and prop_type.property_type == PropertyType.REF):
        type_name = self._AppendInterfaceForProperty(c, prop, type_name)
      c.Append(f"export const {prop.name}: {type_name};")
      c.Append()
    # Add types.
    for type in self._namespace.types.values():
      self._AppendType(c, type)
    # Add namespace level functions.
    for func in self._namespace.functions.values():
      self._AppendFunction(c, func)
    # Add Events.
    for event in self._namespace.events.values():
      event_type = self._ExtractFunctionType(event)
      c.Append(f"export const {event.name}: ChromeEvent<{event_type}>;")
      c.Append()
      self._events_required = True

  def _CloseNamespaces(self, c: Code, to_close: int):
    for i in range(to_close):
      c.Eblock("}")

  def _AppendFunction(self, c: Code, func):
    params = self._ExtractFunctionParams(func)
    ret_type = self._ExtractFunctionReturnType(func)
    c.Append(f"export function {func.name}({params}): {ret_type};")
    c.Append()

  # This appends an local only interface to allow for additional
  # properties on an already defined type.
  def _AppendInterfaceForProperty(self, c: Code, prop: Property,
                                  prop_type_name):
    if prop.deprecated:
      return
    prop_type = prop.type_
    interface_name = f"{prop.name}_{prop_type_name}"
    # The names of these interfaces are not in pascal case.
    # They are unexported though which results in the correct behavior.
    c.Append("// eslint-disable-next-line @typescript-eslint/naming-convention")
    c.Sblock(f"interface {interface_name} extends {prop_type_name}{{")
    for prop in prop_type.properties.values():
      type_name = self._ExtractType(prop.type_)
      c.Append(f"readonly {prop.name}: {type_name};")
    # Add interface functions.
    for func in prop_type.functions.values():
      self._AppendFunction(c, func)
    # Add Events.
    for event in prop_type.events.values():
      event_type = self._ExtractFunctionType(event)
      c.Append(f"readonly {event.name}: ChromeEvent<{event_type}>;")
      self._events_required = True
    c.Eblock("}")
    return interface_name

  def _AppendType(self, c: Code, type: Type):
    if type.property_type is PropertyType.ENUM:
      self._AppendEnum(c, type)
    elif type.property_type is PropertyType.OBJECT:
      self._AppendInterface(c, type)
    elif type.property_type.is_fundamental:
      # Type alias
      c.Append(f"export type {type.name} = {type.property_type.name};")
      c.Append()
    elif (type.property_type is PropertyType.ARRAY
          or type.property_type is PropertyType.CHOICES):
      ts_type = self._ExtractType(type)
      c.Append(f"export type {type.name} = {ts_type};")
      c.Append()
    else:
      # Adding this for things we may not have accounted for here.
      c.Append(
          f"// TODO({os.getlogin()}) -- {type.name}: {type.property_type.name}")

  def _AppendInterface(self, c: Code, interface: Type):
    c.Sblock(f"export interface {interface.name} {{")
    # Add interface properties.
    for property in interface.properties.values():
      c.Append(self._ExtractPropertyDefinition(property))
    # Add interface functions.
    func: Function
    for func in interface.functions.values():
      c.Append(f"{func.name}{self._ExtractFunctionType(func, ':')};")
    # Add interface events.
    for evnt in interface.events.values():
      event_type = self._ExtractFunctionType(evnt)
      c.Append(f"{evnt.name}: ChromeEvent<{event_type}>;")
      self._events_required = True
    c.Eblock("}")
    c.Append()

  def _AppendEnum(self, c: Code, enum):
    c.Sblock(f"export enum {enum.name} {{")
    for v in enum.enum_values:
      c.Append(f"{self._js_util.GetPropertyName(v.name)} = '{v.name}',")
    c.Eblock("}")
    c.Append()

  def _AppendClass(self, c: Code, class_type: Type):
    c.Sblock(f"export class {class_type.name} {{")
    for property in class_type.properties.values():
      c.Append(self._ExtractPropertyDefinition(property))
    # Add class functions.
    func: Function
    for func in class_type.functions.values():
      c.Append(f"{func.name}{self._ExtractFunctionType(func, ':')};")
    # Add class events.
    for evnt in class_type.events.values():
      event_type = self._ExtractFunctionType(evnt)
      c.Append(f"{evnt.name}: ChromeEvent<{event_type}>;")
      self._events_required = True
    c.Eblock("}")

  def _ExtractFunctionReturnType(self, func: Function):
    ret_type = "void"
    if func.returns is not None:
      ret_type = self._ExtractType(func.returns)
    elif (func.returns_async is not None
          and func.returns_async.can_return_promise):
      ret_type = f"Promise<{self._ExtractPromiseType(func.returns_async)}>"
    return ret_type

  # Extracts the code required to define a type.
  # Uses recursion to get types within types.
  def _ExtractType(self, type: Type):
    if type is None:
      return "void"
    if type.property_type in (PropertyType.INTEGER, PropertyType.DOUBLE):
      return "number"
    elif type.property_type is PropertyType.OBJECT:
      return self._ExtractObjectDefinition(type)
    elif type.property_type is PropertyType.REF:
      return type.ref_type
    elif type.property_type is PropertyType.CHOICES:
      type_list = ""
      for i, choice in enumerate(type.choices):
        if i != 0:
          type_list += "|"
        type_list += self._ExtractType(choice)
      return type_list
    elif type.property_type is PropertyType.ARRAY:
      if type.item_type.property_type is PropertyType.OBJECT:
        element_type = self._ExtractType(type.item_type)
        # Trying to idenfity non-simple elements to use the syntax:
        # Array<string | number>
        # Array<{prop: string}>
        # Array<() => void>
        if '|' in element_type or '(' in element_type or '{' in element_type:
          return f"Array<{element_type}>"

        # For simple type use like the syntax: string[]
        return f"{element_type}[]"
      elif type.item_type.property_type is PropertyType.CHOICES:
        return f"({self._ExtractType(type.item_type)})[]"
      else:
        return f"{self._ExtractType(type.item_type)}[]"
    elif type.property_type.is_fundamental:
      return type.property_type.name
    elif type.property_type is PropertyType.FUNCTION:
      return self._ExtractFunctionType(type.function)
    elif type.property_type is PropertyType.ANY:
      return "any"
    elif type.property_type is PropertyType.BINARY:
      return "ArrayBuffer"
    else:
      # Added for accounting for unknown objects.
      return f"unknown /*TODO({os.getlogin()})*/"

  def _ExtractPropertyDefinition(self, prop: Property, terminator=";"):
    q_mark = "?" if prop.optional else ""
    type_name = self._ExtractType(prop.type_)
    return f"{prop.name}{q_mark}: {type_name}{terminator}"

  # Extracts the function type as an arrow function.
  # The delimiter can be changed so this can be used for interface / object
  # members.
  def _ExtractFunctionType(self, func: Function, return_delim=" =>"):
    params = self._ExtractFunctionParams(func)
    ret_type = self._ExtractFunctionReturnType(func)
    return f"({params}){return_delim} {ret_type}"

  # Extracts an object definition.
  def _ExtractObjectDefinition(self, obj: Type):
    if obj.instance_of:
      return obj.instance_of

    # If there are no specific properties on the object then we should expect
    # and object of random keys with specific values.
    if len(obj.properties) == 0:
      value_type = self._ExtractType(obj.additional_properties)
      return "{[key:string]: %s,}" % value_type

    ## Otherwise we will build a definition similar to an interface
    obj_code = Code()
    obj_code.Append("{")
    for property in obj.properties.values():
      obj_code.Append(self._ExtractPropertyDefinition(property, ","))
    func: Function
    for func in obj.functions.values():
      obj_code.Append(f"{func.name}{self._ExtractFunctionType(func, ':')};")
    obj_code.Append("}")
    for evnt in obj.events.values():
      event_type = self._ExtractFunctionType(evnt)
      obj_code.Append(f"{evnt.name}: ChromeEvent<{event_type}>;")
      self._events_required = True
    return obj_code.Render()

  # Extracts parameters from a function as a string representation.
  # Example = "p1: string, p2: number, p3: any".
  def _ExtractFunctionParams(self, func: Function):
    param_str = self._ExtractParams(func.params)

    # When the return async isn't a promise, we append it as a return callback
    # at the end of the parameters.
    use_callback = (func.returns_async
                    and not func.returns_async.can_return_promise)
    if use_callback:
      callback_params = self._ExtractParams(func.returns_async.params)
      if param_str:
        param_str += ", "

      param_str += f"{func.returns_async.name} "
      if func.returns_async.optional:
        param_str += "?"
      param_str += f": ({callback_params}) => void"

    return param_str

  def _ExtractParams(self, params: list):
    param_str = ""
    required_index = -1
    for i, param in reversed(list(enumerate(params))):
      if not param.optional:
        required_index = i
        break
    for i, param in enumerate(params):
      q_mark = "?" if param.optional and not i < required_index else ""
      type_name = self._ExtractType(param.type_)
      # Typescript doesn't allow an optional before a required param.
      # In this case append | undefined to the parameter.
      if i < required_index and param.optional:
        type_name += "|undefined"
      param_str += f"{param.name}{q_mark}: {type_name}"
      if i < len(params) - 1:
        param_str += ", "
    return param_str

  # Extracts the type from a promise.
  def _ExtractPromiseType(self, async_return: ReturnsAsync):
    retval = "void"
    # Assume that there is at most only one param since functions can only
    # return one thing. This includes those that are async and use a promise to
    # return a value. It could also be 0 for void return type.
    assert len(async_return.params) <= 1
    for ret in async_return.params:
      retval = self._ExtractType(ret.type_)
      if ret.optional:
        retval += "|undefined"
    return retval

  def _ClangFormat(self, c: Code, level=0):
    # temp = tempfile.NamedTemporaryFile("w", encoding="utf-8", suffix=".js")
    # f_name = temp.name
    with tempfile.NamedTemporaryFile("w",
                                     encoding="utf-8",
                                     suffix=".js",
                                     delete=False) as f:
      f.write(c.Render())
      f_name = f.name
    script_path = self._GetChromiumClangFormatScriptPath()
    style_path = self._GetChromiumClangFormatStylePath()
    cmd = (f'python3 {script_path} --fallback-style=none '
           f'--style=file:{style_path} "{f_name}"')
    p = subprocess.Popen(cmd,
                         cwd=CHROMIUM_SRC,
                         encoding="utf-8",
                         shell=True,
                         stdout=subprocess.PIPE)
    out = p.communicate()[0]
    out_code = Code()
    out_code.Append(out)
    os.remove(f_name)
    return out_code

  def _GetChromiumClangFormatScriptPath(self):
    return os.path.join(CHROMIUM_SRC, "third_party", "depot_tools",
                        "clang_format.py")

  def _GetChromiumClangFormatStylePath(self):
    return os.path.join(CHROMIUM_SRC, ".clang-format")
