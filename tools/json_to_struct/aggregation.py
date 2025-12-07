# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from dataclasses import dataclass
from enum import Enum
from typing import Optional


class AggregationKind(Enum):
  """Allowed aggregation types for data that uses "aggregate" declaration."""
  NONE = "none"
  ARRAY = "array"
  MAP = "map"


@dataclass
class AggregationDetails:
  """Aggregation rules, if specified by the processed JSON file."""
  kind: AggregationKind
  name: Optional[str]
  export_items: bool
  elements: dict[str, str]
  map_key_type: Optional[str]

  def GetSortedArrayElements(self) -> list[str]:
    """Returns sorted list of names of all elements."""
    return sorted(self.elements.keys())

  def GetSortedMapElements(self) -> list[tuple[str, str]]:
    """Returns sorted mapping of all elements, including aliases."""
    keys = sorted(self.elements.keys())
    return [(key, self.elements[key]) for key in keys]


def GetAggregationDetails(description) -> AggregationDetails:
  """Extracts aggregation details from a JSON structure.

  This function processes a JSON data object to determine its aggregation
  properties. Aggregation details are specified within an optional "aggregation"
  descriptor in the JSON input. If the descriptor is missing, no aggregation is
  performed.

  **Aggregation Descriptor Structure:**
      - `type` (str): Defines the aggregation type. Options include:
          - `"none"`: No aggregation (default/implied if descriptor is missing).
          - `"array"`: Uses `std::span<Type>` for aggregation.
          - `"map"`: Uses `base::fixed_flat_map<std::string_view, Type>` for
            aggregation.
      - `name` (str): The name assigned to the generated array or map.
      - `export_items` (bool, optional): Whether aggregated items should be
        exported (defaults to `true`).
      - `map_aliases` (dict[str, str]) - if the aggregation `type` is set to
        `"map"`, this field allows specifying additional aliases - elements
        pointing to already defined structures.
      - `map_key_type` (str) - the type representing the map key. Must be a
        constexpr-constructible from const char[].

  **Default Behavior:**
      - If the `aggregation` descriptor is missing, the function defaults to:
          - `type = "none"`
          - `export_items = True`
      - The `name` field is only relevant when aggregation is `array` or `map`.

  **Parameters:**
      description (dict): input JSON data file (not schema).

  **Returns:**
      AggregationDetails populated with relevant fields. This is always
      returned, even if the data object does not include aggregation descriptor.
  """
  aggregation = description.get('aggregation', {})
  kind = AggregationKind(aggregation.get('type', 'none'))
  name = aggregation.get('name', None)
  export_items = aggregation.get('export_items', True)
  map_aliases = aggregation.get('map_aliases', {})
  map_key_type = None

  if kind != AggregationKind.NONE and not name:
    raise Exception("Aggregation container needs a `name`.")

  elements = {}
  for element in description.get('elements', {}).keys():
    elements.update({element: element})

  confirmed_aliases = {}
  if kind == AggregationKind.MAP:
    map_key_type = aggregation.get('map_key_type', 'std::string_view')
    for alias, element in map_aliases.items():
      # Confirmation check for duplicate entries.
      # Note: we do not need to verify duplicate aliases, because `map_aliases`
      # is already a dict - all keys should be unique.
      if elements.get(alias, None):
        raise Exception(f"Alias `{alias}` already defined as element.")

      # Detect that alias does not point to a valid element.
      if not elements.get(element, None):
        raise Exception(f"Aliased element `{element}` does not exist.")

      confirmed_aliases.update({alias: element})
    elements.update(confirmed_aliases)

  return AggregationDetails(kind, name, export_items, elements, map_key_type)


def GenerateCCAggregation(type_name: str,
                          aggregation: AggregationDetails) -> Optional[str]:
  """
    Generates C++ aggregation code based on the aggregation kind.

    Parameters:
        type_name (str): The type name to be used in the aggregation.
        aggregation (AggregationDetails): The aggregation details.

    Returns:
        Optional[str]: The generated C++ aggregation code if applicable, otherwise None.
    """
  if aggregation.kind == AggregationKind.ARRAY:
    return _GenerateCCArray(type_name, aggregation)

  if aggregation.kind == AggregationKind.MAP:
    return _GenerateCCMap(type_name, aggregation)

  return None


def _GenerateCCArray(type_name: str, aggregation: AggregationDetails) -> str:
  """
    Generates C++ code for an array aggregation.

    Parameters:
        type_name (str): The type name to be used in the aggregation.
        aggregation (AggregationDetails): The aggregation details.

    Returns:
        str: The generated C++ array aggregation code.
    """
  res = '\nconst '
  res += f'std::array<const {type_name}*, {len(aggregation.elements)}> '
  res += f'{aggregation.name} '

  res += '({{\n'
  for element_name in aggregation.elements.values():
    res += f'  &{element_name},\n'
  res += '}});\n'
  return res


def _GenerateCCMap(type_name: str, aggregation: AggregationDetails) -> str:
  """
    Generates C++ code for a map aggregation.

    Parameters:
        type_name (str): The type name to be used in the aggregation.
        aggregation (AggregationDetails): The aggregation details.

    Returns:
        str: The generated C++ map aggregation code.
    """
  key_type = aggregation.map_key_type

  res = '\nconst '
  res += f'base::fixed_flat_map<{aggregation.map_key_type}, '
  res += f'const {type_name}*, {len(aggregation.GetSortedMapElements())}> '
  res += f'{aggregation.name} =\n'
  res += f'    base::MakeFixedFlatMap<{key_type}, const {type_name}*>'

  res += '({\n'
  for (alias_name, element_name) in aggregation.GetSortedMapElements():
    res += f'  {{{key_type}("{alias_name}"), &{element_name}}},\n'
  res += '});\n'
  return res


def GenerateHHAggregation(type_name: str,
                          aggregation: AggregationDetails) -> Optional[str]:
  """
    Generates header file aggregation code based on the aggregation kind.

    Parameters:
        type_name (str): The type name to be used in the aggregation.
        aggregation (AggregationDetails): The aggregation details.

    Returns:
        Optional[str]: The generated header file aggregation code if applicable, otherwise None.
    """
  if aggregation.kind == AggregationKind.ARRAY:
    return _GenerateHHArray(type_name, aggregation)

  if aggregation.kind == AggregationKind.MAP:
    return _GenerateHHMap(type_name, aggregation)

  return None


def _GenerateHHArray(type_name: str, aggregation: AggregationDetails) -> str:
  """
    Generates header file code for an array aggregation.

    Parameters:
        type_name (str): The type name to be used in the aggregation.
        aggregation (AggregationDetails): The aggregation details.

    Returns:
        str: The generated header file array aggregation declaration.
    """
  res = '\nextern const '
  res += f'std::array<const {type_name}*, {len(aggregation.elements)}> '
  res += f'{aggregation.name};\n'
  return res


def _GenerateHHMap(type_name: str, aggregation: AggregationDetails) -> str:
  """
    Generates header file code for a map aggregation.

    Parameters:
        type_name (str): The type name to be used in the aggregation.
        aggregation (AggregationDetails): The aggregation details.

    Returns:
        str: The generated header file map aggregation declaration.
    """
  res = '\nextern const '
  res += f'base::fixed_flat_map<{aggregation.map_key_type}, '
  res += f'const {type_name}*, {len(aggregation.GetSortedMapElements())}> '
  res += f'{aggregation.name};\n'
  return res
