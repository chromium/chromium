// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_node_data_mojom_traits.h"

#include "base/containers/flat_map.h"
#include "ui/accessibility/mojom/ax_relative_bounds.mojom-shared.h"
#include "ui/accessibility/mojom/ax_relative_bounds_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<ax::mojom::AXNodeDataDataView, ui::AXNodeData>::Read(
    ax::mojom::AXNodeDataDataView data,
    ui::AXNodeData* out) {
  out->id = data.id();
  out->role = data.role();
  out->state = data.state();
  out->actions = data.actions();

  // TODO(dcheng): AXNodeData should probably just switch over to absl's
  // flat_hash_map for simplicity at some point.
  base::flat_map<ax::mojom::StringAttribute, std::string> string_attributes;
  if (!data.ReadStringAttributes(&string_attributes))
    return false;
  out->string_attributes = std::move(string_attributes).extract();

  base::flat_map<ax::mojom::IntAttribute, int32_t> int_attributes;
  if (!data.ReadIntAttributes(&int_attributes))
    return false;
  out->int_attributes = std::move(int_attributes).extract();

  base::flat_map<ax::mojom::FloatAttribute, float> float_attributes;
  if (!data.ReadFloatAttributes(&float_attributes))
    return false;
  out->float_attributes = std::move(float_attributes).extract();

  base::flat_map<ax::mojom::BoolAttribute, bool> bool_attributes;
  if (!data.ReadBoolAttributes(&bool_attributes))
    return false;
  out->bool_attributes = std::move(bool_attributes).extract();

  base::flat_map<ax::mojom::IntListAttribute, std::vector<int32_t>>
      intlist_attributes;
  if (!data.ReadIntlistAttributes(&intlist_attributes))
    return false;
  out->intlist_attributes = std::move(intlist_attributes).extract();

  base::flat_map<ax::mojom::StringListAttribute, std::vector<std::string>>
      stringlist_attributes;
  if (!data.ReadStringlistAttributes(&stringlist_attributes))
    return false;
  out->stringlist_attributes = std::move(stringlist_attributes).extract();

  base::flat_map<std::string, std::string> html_attributes;
  if (!data.ReadHtmlAttributes(&html_attributes))
    return false;
  out->html_attributes = std::move(html_attributes).extract();

  if (!data.ReadChildIds(&out->child_ids))
    return false;

  if (!data.ReadRelativeBounds(&out->relative_bounds))
    return false;

  return true;
}

}  // namespace mojo
