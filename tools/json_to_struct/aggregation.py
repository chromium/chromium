# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from dataclasses import dataclass
from enum import Enum


class AggregationKind(Enum):
  """Allowed aggregation types for data that uses "aggregate" declaration."""
  NONE = "none"
  ARRAY = "array"
  MAP = "map"


@dataclass
class AggregationDetails:
  """Aggregation rules, if specified by the processed JSON file."""
  kind: AggregationKind
  name: str
  export_items: bool


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
  # Temporarily support both aggregation types until we clean them up.
  # TODO(b:404850650): clean this up after migrating all structs to new
  # declaration.
  generate_array = description.get('generate_array', None)
  if generate_array:
    return AggregationDetails(AggregationKind.ARRAY,
                              generate_array.get('array_name'),
                              export_items=True)

  aggregation = description.get('aggregation', {})
  kind = AggregationKind(aggregation.get('type', 'none'))
  name = aggregation.get('name', None)
  export_items = aggregation.get('export_items', True)

  if kind != AggregationKind.NONE and not name:
    raise Exception("Aggregation container needs a `name`.")

  return AggregationDetails(kind, name, export_items)
