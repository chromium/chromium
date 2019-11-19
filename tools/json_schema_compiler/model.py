# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os.path

from json_parse import OrderedDict
from memoize import memoize


class ParseException(Exception):
  """Thrown when data in the model is invalid.
  """
  def __init__(self, parent, message):
    hierarchy = _GetModelHierarchy(parent)
    hierarchy.append(message)
    Exception.__init__(
        self, 'Model parse exception at:\n' + '\n'.join(hierarchy))


class Model(object):
  """Model of all namespaces that comprise an API.

  Properties:
  - |namespaces| a map of a namespace name to its model.Namespace
  """
  def __init__(self, allow_inline_enums=True):
    self._allow_inline_enums = allow_inline_enums
    self.namespaces = {}

  def AddNamespace(self,
                   json,
                   source_file,
                   include_compiler_options=False,
                   environment=None):
    """Add a namespace's json to the model and returns the namespace.
    """
    namespace = Namespace(json,
                          source_file,
                          include_compiler_options=include_compiler_options,
                          environment=environment,
                          allow_inline_enums=self._allow_inline_enums)
    self.namespaces[namespace.name] = namespace
    return namespace


def CreateFeature(name, model):
  if isinstance(model, dict):
    return SimpleFeature(name, model)
  return ComplexFeature(name, [SimpleFeature(name, child) for child in model])


class ComplexFeature(object):
  """A complex feature which may be made of several simple features.

  Properties:
  - |name| the name of the feature
  - |unix_name| the unix_name of the feature
  - |feature_list| a list of simple features which make up the feature
  """
  def __init__(self, feature_name, features):
    self.name = feature_name
    self.unix_name = UnixName(self.name)
    self.feature_list = features

class SimpleFeature(object):
  """A simple feature, which can make up a complex feature, as specified in
  files such as chrome/common/extensions/api/_permission_features.json.

  Properties:
  - |name| the name of the feature
  - |unix_name| the unix_name of the feature
  - |channel| the channel where the feature is released
  - |extension_types| the types which can use the feature
  - |whitelist| a list of extensions allowed to use the feature
  """
  def __init__(self, feature_name, feature_def):
    self.name = feature_name
    self.unix_name = UnixName(self.name)
    self.channel = feature_def['channel']
    self.extension_types = feature_def['extension_types']
    self.whitelist = feature_def.get('whitelist')


class Namespace(object):
  """An API namespace.

  Properties:
  - |name| the name of the namespace
  - |description| the description of the namespace
  - |deprecated| a reason and possible alternative for a deprecated api
  - |unix_name| the unix_name of the namespace
  - |source_file| the file that contained the namespace definition
  - |source_file_dir| the directory component of |source_file|
  - |source_file_filename| the filename component of |source_file|
  - |platforms| if not None, the list of platforms that the namespace is
                available to
  - |types| a map of type names to their model.Type
  - |functions| a map of function names to their model.Function
  - |events| a map of event names to their model.Function
  - |properties| a map of property names to their model.Property
  - |compiler_options| the compiler_options dict, only not empty if
                       |include_compiler_options| is True
  """
  def __init__(self,
               json,
               source_file,
               include_compiler_options=False,
               environment=None,
               allow_inline_enums=True):
    self.name = json['namespace']
    if 'description' not in json:
      # TODO(kalman): Go back to throwing an error here.
      print('%s must have a "description" field. This will appear '
                       'on the API summary page.' % self.name)
      json['description'] = ''
    self.description = json['description']
    self.deprecated = json.get('deprecated', None)
    self.unix_name = UnixName(self.name)
    self.source_file = source_file
    self.source_file_dir, self.source_file_filename = os.path.split(source_file)
    self.short_filename = os.path.basename(source_file).split('.')[0]
    self.parent = None
    self.allow_inline_enums = allow_inline_enums
    self.platforms = _GetPlatforms(json)
    toplevel_origin = Origin(from_client=True, from_json=True)
    self.types = _GetTypes(self, json, self, toplevel_origin)
    self.functions = _GetFunctions(self, json, self)
    self.events = _GetEvents(self, json, self)
    self.properties = _GetProperties(self, json, self, toplevel_origin)
    if include_compiler_options:
      self.compiler_options = json.get('compiler_options', {})
    else:
      self.compiler_options = {}
    self.environment = environment
    self.documentation_options = json.get('documentation_options', {})


class Origin(object):
  """Stores the possible origin of model object as a pair of bools. These are:

  |from_client| indicating that instances can originate from users of
                generated code (for example, function results), or
  |from_json|   indicating that instances can originate from the JSON (for
                example, function parameters)

  It is possible for model objects to originate from both the client and json,
  for example Types defined in the top-level schema, in which case both
  |from_client| and |from_json| would be True.
  """
  def __init__(self, from_client=False, from_json=False):
    if not from_client and not from_json:
      raise ValueError('One of from_client or from_json must be true')
    self.from_client = from_client
    self.from_json = from_json


class Type(object):
  """A Type defined in the json.

  Properties:
  - |name| the type name
  - |namespace| the Type's namespace
  - |description| the description of the type (if provided)
  - |properties| a map of property unix_names to their model.Property
  - |functions| a map of function names to their model.Function
  - |events| a map of event names to their model.Event
  - |origin| the Origin of the type
  - |property_type| the PropertyType of this Type
  - |item_type| if this is an array, the type of items in the array
  - |simple_name| the name of this Type without a namespace
  - |additional_properties| the type of the additional properties, if any is
                            specified
  """
  def __init__(self,
               parent,
               name,
               json,
               namespace,
               origin):
    self.name = name
    self.namespace = namespace
    self.simple_name = _StripNamespace(self.name, namespace)
    self.unix_name = UnixName(self.name)
    self.description = json.get('description', None)
    self.jsexterns = json.get('jsexterns', None)
    self.origin = origin
    self.parent = parent
    self.instance_of = json.get('isInstanceOf', None)

    # TODO(kalman): Only objects need functions/events/properties, but callers
    # assume that all types have them. Fix this.
    self.functions = _GetFunctions(self, json, namespace)
    self.events = _GetEvents(self, json, namespace)
    self.properties = _GetProperties(self, json, namespace, origin)

    json_type = json.get('type', None)
    if json_type == 'array':
      self.property_type = PropertyType.ARRAY
      self.item_type = Type(
          self, '%sType' % name, json['items'], namespace, origin)
    elif '$ref' in json:
      self.property_type = PropertyType.REF
      self.ref_type = json['$ref']
    elif 'enum' in json and json_type == 'string':
      if not namespace.allow_inline_enums and not isinstance(parent, Namespace):
        raise ParseException(
            self,
            'Inline enum "%s" found in namespace "%s". These are not allowed. '
                'See crbug.com/472279' % (name, namespace.name))
      self.property_type = PropertyType.ENUM
      self.enum_values = [EnumValue(value) for value in json['enum']]
      self.cpp_enum_prefix_override = json.get('cpp_enum_prefix_override', None)
    elif json_type == 'any':
      self.property_type = PropertyType.ANY
    elif json_type == 'binary':
      self.property_type = PropertyType.BINARY
    elif json_type == 'boolean':
      self.property_type = PropertyType.BOOLEAN
    elif json_type == 'integer':
      self.property_type = PropertyType.INTEGER
    elif (json_type == 'double' or
          json_type == 'number'):
      self.property_type = PropertyType.DOUBLE
    elif json_type == 'string':
      self.property_type = PropertyType.STRING
    elif 'choices' in json:
      self.property_type = PropertyType.CHOICES
      def generate_type_name(type_json):
        if 'items' in type_json:
          return '%ss' % generate_type_name(type_json['items'])
        if '$ref' in type_json:
          return type_json['$ref']
        if 'type' in type_json:
          return type_json['type']
        return None
      self.choices = [
          Type(self,
               generate_type_name(choice) or 'choice%s' % i,
               choice,
               namespace,
               origin)
          for i, choice in enumerate(json['choices'])]
    elif json_type == 'object':
      if not (
          'isInstanceOf' in json or
          'properties' in json or
          'additionalProperties' in json or
          'functions' in json or
          'events' in json):
        raise ParseException(self, name + " has no properties or functions")
      self.property_type = PropertyType.OBJECT
      additional_properties_json = json.get('additionalProperties', None)
      if additional_properties_json is not None:
        self.additional_properties = Type(self,
                                          'additionalProperties',
                                          additional_properties_json,
                                          namespace,
                                          origin)
      else:
        self.additional_properties = None
    elif json_type == 'function':
      self.property_type = PropertyType.FUNCTION
      # Sometimes we might have an unnamed function, e.g. if it's a property
      # of an object. Use the name of the property in that case.
      function_name = json.get('name', name)
      self.function = Function(self, function_name, json, namespace, origin)
    else:
      raise ParseException(self, 'Unsupported JSON type %s' % json_type)


class Function(object):
  """A Function defined in the API.

  Properties:
  - |name| the function name
  - |platforms| if not None, the list of platforms that the function is
                available to
  - |params| a list of parameters to the function (order matters). A separate
             parameter is used for each choice of a 'choices' parameter
  - |deprecated| a reason and possible alternative for a deprecated function
  - |description| a description of the function (if provided)
  - |callback| the callback parameter to the function. There should be exactly
               one
  - |optional| whether the Function is "optional"; this only makes sense to be
               present when the Function is representing a callback property
  - |simple_name| the name of this Function without a namespace
  - |returns| the return type of the function; None if the function does not
    return a value
  """
  def __init__(self,
               parent,
               name,
               json,
               namespace,
               origin):
    self.name = name
    self.simple_name = _StripNamespace(self.name, namespace)
    self.platforms = _GetPlatforms(json)
    self.params = []
    self.description = json.get('description')
    self.deprecated = json.get('deprecated')
    self.callback = None
    self.optional = json.get('optional', False)
    self.parent = parent
    self.nocompile = json.get('nocompile')
    self.nodefine = json.get('nodefine')
    options = json.get('options', {})
    self.conditions = options.get('conditions', [])
    self.actions = options.get('actions', [])
    self.supports_listeners = options.get('supportsListeners', True)
    self.supports_rules = options.get('supportsRules', False)
    self.supports_dom = options.get('supportsDom', False)

    def GeneratePropertyFromParam(p):
      return Property(self, p['name'], p, namespace, origin)

    self.filters = [GeneratePropertyFromParam(filter_instance)
                    for filter_instance in json.get('filters', [])]
    callback_param = None
    params = json.get('parameters', [])
    for i, param in enumerate(params):
      if i == len(params) - 1 and param.get('type') == 'function':
        callback_param = param
      else:
        # Treat all intermediate function arguments as properties. Certain APIs,
        # such as the webstore, have these.
        self.params.append(GeneratePropertyFromParam(param))

    if callback_param:
      self.callback = Function(self,
                               callback_param['name'],
                               callback_param,
                               namespace,
                               Origin(from_client=True))

    self.returns = None
    if 'returns' in json:
      self.returns = Type(self,
                          '%sReturnType' % name,
                          json['returns'],
                          namespace,
                          origin)


class Property(object):
  """A property of a type OR a parameter to a function.
  Properties:
  - |name| name of the property as in the json. This shouldn't change since
    it is the key used to access DictionaryValues
  - |unix_name| the unix_style_name of the property. Used as variable name
  - |optional| a boolean representing whether the property is optional
  - |description| a description of the property (if provided)
  - |type_| the model.Type of this property
  - |simple_name| the name of this Property without a namespace
  - |deprecated| a reason and possible alternative for a deprecated property
  """
  def __init__(self, parent, name, json, namespace, origin):
    """Creates a Property from JSON.
    """
    self.parent = parent
    self.name = name
    self._unix_name = UnixName(self.name)
    self._unix_name_used = False
    self.origin = origin
    self.simple_name = _StripNamespace(self.name, namespace)
    self.description = json.get('description', None)
    self.optional = json.get('optional', None)
    self.instance_of = json.get('isInstanceOf', None)
    self.deprecated = json.get('deprecated')

    # HACK: only support very specific value types.
    is_allowed_value = (
        '$ref' not in json and
        ('type' not in json or json['type'] == 'integer'
                            or json['type'] == 'number'
                            or json['type'] == 'string'))

    self.value = None
    if 'value' in json and is_allowed_value:
      self.value = json['value']
      if 'type' not in json:
        # Sometimes the type of the value is left out, and we need to figure
        # it out for ourselves.
        if isinstance(self.value, int):
          json['type'] = 'integer'
        elif isinstance(self.value, float):
          json['type'] = 'double'
        elif isinstance(self.value, basestring):
          json['type'] = 'string'
        else:
          # TODO(kalman): support more types as necessary.
          raise ParseException(
              parent,
              '"%s" is not a supported type for "value"' % type(self.value))

    self.type_ = Type(parent, name, json, namespace, origin)

  def GetUnixName(self):
    """Gets the property's unix_name. Raises AttributeError if not set.
    """
    if not self._unix_name:
      raise AttributeError('No unix_name set on %s' % self.name)
    self._unix_name_used = True
    return self._unix_name

  def SetUnixName(self, unix_name):
    """Set the property's unix_name. Raises AttributeError if the unix_name has
    already been used (GetUnixName has been called).
    """
    if unix_name == self._unix_name:
      return
    if self._unix_name_used:
      raise AttributeError(
          'Cannot set the unix_name on %s; '
          'it is already used elsewhere as %s' %
          (self.name, self._unix_name))
    self._unix_name = unix_name

  unix_name = property(GetUnixName, SetUnixName)

class EnumValue(object):
  """A single value from an enum.
  Properties:
  - |name| name of the property as in the json.
  - |description| a description of the property (if provided)
  """
  def __init__(self, json):
    if isinstance(json, dict):
      self.name = json['name']
      self.description = json.get('description')
    else:
      self.name = json
      self.description = None

  def CamelName(self):
    return CamelName(self.name)

class _Enum(object):
  """Superclass for enum types with a "name" field, setting up repr/eq/ne.
  Enums need to do this so that equality/non-equality work over pickling.
  """
  @staticmethod
  def GetAll(cls):
    """Yields all _Enum objects declared in |cls|.
    """
    for prop_key in dir(cls):
      prop_value = getattr(cls, prop_key)
      if isinstance(prop_value, _Enum):
        yield prop_value

  def __init__(self, name):
    self.name = name

  def __eq__(self, other):
    return type(other) == type(self) and other.name == self.name
  def __ne__(self, other):
    return not (self == other)

  def __repr__(self):
    return self.name

  def __str__(self):
    return repr(self)

  def __hash__(self):
    return hash(self.name)


class _PropertyTypeInfo(_Enum):
  def __init__(self, is_fundamental, name):
    _Enum.__init__(self, name)
    self.is_fundamental = is_fundamental

  def __repr__(self):
    return self.name

class PropertyType(object):
  """Enum of different types of properties/parameters.
  """
  ANY = _PropertyTypeInfo(False, "any")
  ARRAY = _PropertyTypeInfo(False, "array")
  BINARY = _PropertyTypeInfo(False, "binary")
  BOOLEAN = _PropertyTypeInfo(True, "boolean")
  CHOICES = _PropertyTypeInfo(False, "choices")
  DOUBLE = _PropertyTypeInfo(True, "double")
  ENUM = _PropertyTypeInfo(False, "enum")
  FUNCTION = _PropertyTypeInfo(False, "function")
  INT64 = _PropertyTypeInfo(True, "int64")
  INTEGER = _PropertyTypeInfo(True, "integer")
  OBJECT = _PropertyTypeInfo(False, "object")
  REF = _PropertyTypeInfo(False, "ref")
  STRING = _PropertyTypeInfo(True, "string")


@memoize
def UnixName(name):
  '''Returns the unix_style name for a given lowerCamelCase string.
  '''
  unix_name = []
  for i, c in enumerate(name):
    if c.isupper() and i > 0 and name[i - 1] != '_':
      # Replace lowerUpper with lower_Upper.
      if name[i - 1].islower():
        unix_name.append('_')
      # Replace ACMEWidgets with ACME_Widgets
      elif i + 1 < len(name) and name[i + 1].islower():
        unix_name.append('_')
    if c == '.':
      # Replace hello.world with hello_world.
      unix_name.append('_')
    else:
      # Everything is lowercase.
      unix_name.append(c.lower())
  return ''.join(unix_name)


@memoize
def CamelName(snake):
  ''' Converts a snake_cased_string to a camelCasedOne. '''
  pieces = snake.split('_')
  camel = []
  for i, piece in enumerate(pieces):
    if i == 0:
      camel.append(piece)
    else:
      camel.append(piece.capitalize())
  return ''.join(camel)


def _StripNamespace(name, namespace):
  if name.startswith(namespace.name + '.'):
    return name[len(namespace.name + '.'):]
  return name


def _GetModelHierarchy(entity):
  """Returns the hierarchy of the given model entity."""
  hierarchy = []
  while entity is not None:
    hierarchy.append(getattr(entity, 'name', repr(entity)))
    if isinstance(entity, Namespace):
      hierarchy.insert(0, '  in %s' % entity.source_file)
    entity = getattr(entity, 'parent', None)
  hierarchy.reverse()
  return hierarchy


def _GetTypes(parent, json, namespace, origin):
  """Creates Type objects extracted from |json|.
  """
  types = OrderedDict()
  for type_json in json.get('types', []):
    type_ = Type(parent, type_json['id'], type_json, namespace, origin)
    types[type_.name] = type_
  return types


def _GetFunctions(parent, json, namespace):
  """Creates Function objects extracted from |json|.
  """
  functions = OrderedDict()
  for function_json in json.get('functions', []):
    function = Function(parent,
                        function_json['name'],
                        function_json,
                        namespace,
                        Origin(from_json=True))
    functions[function.name] = function
  return functions


def _GetEvents(parent, json, namespace):
  """Creates Function objects generated from the events in |json|.
  """
  events = OrderedDict()
  for event_json in json.get('events', []):
    event = Function(parent,
                     event_json['name'],
                     event_json,
                     namespace,
                     Origin(from_client=True))
    events[event.name] = event
  return events


def _GetProperties(parent, json, namespace, origin):
  """Generates Property objects extracted from |json|.
  """
  properties = OrderedDict()
  for name, property_json in json.get('properties', {}).items():
    properties[name] = Property(parent, name, property_json, namespace, origin)
  return properties


class _PlatformInfo(_Enum):
  def __init__(self, name):
    _Enum.__init__(self, name)


class Platforms(object):
  """Enum of the possible platforms.
  """
  CHROMEOS = _PlatformInfo("chromeos")
  CHROMEOS_TOUCH = _PlatformInfo("chromeos_touch")
  LINUX = _PlatformInfo("linux")
  MAC = _PlatformInfo("mac")
  WIN = _PlatformInfo("win")


def _GetPlatforms(json):
  if 'platforms' not in json or json['platforms'] == None:
    return None
  # Sanity check: platforms should not be an empty list.
  if not json['platforms']:
    raise ValueError('"platforms" cannot be an empty list')
  platforms = []
  for platform_name in json['platforms']:
    for platform_enum in _Enum.GetAll(Platforms):
      if platform_name == platform_enum.name:
        platforms.append(platform_enum)
        break
  return platforms
