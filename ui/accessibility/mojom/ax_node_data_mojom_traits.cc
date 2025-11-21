// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_node_data_mojom_traits.h"

#include "base/containers/flat_map.h"
#include "ui/accessibility/mojom/ax_relative_bounds.mojom-shared.h"
#include "ui/accessibility/mojom/ax_relative_bounds_mojom_traits.h"

namespace mojo {
namespace {
bool HasAnyHighlightEntries(std::vector<int32_t>& marker_types) {
  for (auto marker : marker_types) {
    if (marker & static_cast<int32_t>(ax::mojom::MarkerType::kHighlight)) {
      // Can stop looking once we know there is one highlight.
      return true;
    }
  }
  return false;
}
}  // namespace

// static
bool StructTraits<ax::mojom::AXBitsetDataDataView,
                  ui::AXBitset<ax::mojom::BoolAttribute>>::
    Read(ax::mojom::AXBitsetDataDataView data,
         ui::AXBitset<ax::mojom::BoolAttribute>* out) {
  *out = ui::AXBitset<ax::mojom::BoolAttribute>(data.set_bits(), data.values());
  return true;
}

// static
bool StructTraits<ax::mojom::AXNodeDataDataView, ui::AXNodeData>::Read(
    ax::mojom::AXNodeDataDataView data,
    ui::AXNodeData* out) {
  out->id = data.id();
  out->role = data.role();
  out->state = ui::AXStates(data.state());
  out->actions = data.actions();

  if (!data.ReadStringAttributes(&out->string_attributes.container())) {
    return false;
  }
  if (!data.ReadIntAttributes(&out->int_attributes.container())) {
    return false;
  }
  if (!data.ReadFloatAttributes(&out->float_attributes.container())) {
    return false;
  }

  std::optional<ui::AXBitset<ax::mojom::BoolAttribute>> bitset_from_mojo;
  if (!data.ReadBoolAttributesData(&bitset_from_mojo)) {
    return false;
  }

  if (bitset_from_mojo.has_value()) {
    out->bool_attributes = bitset_from_mojo.value();
  } else {
    std::optional<base::flat_map<ax::mojom::BoolAttribute, bool>> map_from_mojo;
    if (!data.ReadBoolAttributes(&map_from_mojo)) {
      return false;
    }
    if (map_from_mojo.has_value()) {
      for (const auto& [attr, value] : map_from_mojo.value()) {
        out->bool_attributes.Set(attr, value);
      }
    }
  }

  auto& intlist_attributes = out->intlist_attributes.container();
  if (!data.ReadIntlistAttributes(&intlist_attributes)) {
    return false;
  }

  // Enforce some invariants:
  //  If marker types are present, marker starts and ends must be present.
  //  If any marker type is a highlight, highlights must be present.
  if (auto types_it =
          intlist_attributes.find(ax::mojom::IntListAttribute::kMarkerTypes);
      types_it != intlist_attributes.end()) {
    auto starts_it =
        intlist_attributes.find(ax::mojom::IntListAttribute::kMarkerStarts);
    if (starts_it == intlist_attributes.end()) {
      return false;
    }
    auto ends_it =
        intlist_attributes.find(ax::mojom::IntListAttribute::kMarkerEnds);
    if (ends_it == intlist_attributes.end()) {
      return false;
    }
    auto& marker_types = types_it->second;
    auto& marker_starts = starts_it->second;
    auto& marker_ends = ends_it->second;
    if (marker_types.size() != marker_starts.size() ||
        marker_types.size() != marker_ends.size()) {
      return false;
    }
    if (HasAnyHighlightEntries(marker_types)) {
      auto highlight_types_it =
          intlist_attributes.find(ax::mojom::IntListAttribute::kHighlightTypes);
      if (highlight_types_it == intlist_attributes.end()) {
        return false;
      }
      auto& highlight_types = highlight_types_it->second;
      if (marker_types.size() != highlight_types.size()) {
        return false;
      }
    }
  }

  if (!data.ReadStringlistAttributes(&out->stringlist_attributes.container())) {
    return false;
  }

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
