// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_MOJOM_AX_ASSISTANT_STRUCTURE_MOJOM_TRAITS_H_
#define UI_ACCESSIBILITY_MOJOM_AX_ASSISTANT_STRUCTURE_MOJOM_TRAITS_H_

#include <memory>
#include <optional>

#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom-shared.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

namespace mojo {

template <>
struct StructTraits<ax::mojom::AssistantTreeDataView,
                    std::unique_ptr<ui::AssistantTree>> {
  static bool IsNull(const std::unique_ptr<ui::AssistantTree>& ptr) {
    return !ptr;
  }

  static void SetToNull(std::unique_ptr<ui::AssistantTree>* output) {
    output->reset();
  }

  static const std::vector<std::unique_ptr<ui::AssistantNode>>& nodes(
      const std::unique_ptr<ui::AssistantTree>& tree) {
    return tree->nodes;
  }
  static bool Read(ax::mojom::AssistantTreeDataView data,
                   std::unique_ptr<ui::AssistantTree>* out);
};

template <>
struct StructTraits<ax::mojom::AssistantNodeDataView,
                    std::unique_ptr<ui::AssistantNode>> {
  static const std::vector<int32_t>& children_indices(
      const std::unique_ptr<ui::AssistantNode>& node) {
    return node->children_indices;
  }
  static const gfx::Rect& unclipped_rect(
      const std::unique_ptr<ui::AssistantNode>& node) {
    return node->unclipped_rect;
  }
  static const gfx::Rect& rect(const std::unique_ptr<ui::AssistantNode>& node) {
    return node->rect;
  }
  static const std::u16string& text(
      const std::unique_ptr<ui::AssistantNode>& node) {
    return node->text;
  }
  static float text_size(const std::unique_ptr<ui::AssistantNode>& node) {
    return node->text_size;
  }
  static uint32_t color(const std::unique_ptr<ui::AssistantNode>& node) {
    return node->color;
  }
  static uint32_t bgcolor(const std::unique_ptr<ui::AssistantNode>& node) {
    return node->bgcolor;
  }
  static bool bold(const std::unique_ptr<ui::AssistantNode>& node) {
    return node->bold;
  }
  static bool italic(const std::unique_ptr<ui::AssistantNode>& node) {
    return node->italic;
  }
  static bool underline(const std::unique_ptr<ui::AssistantNode>& node) {
    return node->underline;
  }
  static bool line_through(const std::unique_ptr<ui::AssistantNode>& node) {
    return node->line_through;
  }
  static const std::optional<gfx::Range>& selection(
      const std::unique_ptr<ui::AssistantNode>& node) {
    return node->selection;
  }
  static const std::string& class_name(
      const std::unique_ptr<ui::AssistantNode>& node) {
    return node->class_name;
  }
  static const std::optional<std::string>& role(
      const std::unique_ptr<ui::AssistantNode>& node) {
    return node->role;
  }
  static bool Read(ax::mojom::AssistantNodeDataView data,
                   std::unique_ptr<ui::AssistantNode>* out);
};

}  // namespace mojo

#endif  // UI_ACCESSIBILITY_MOJOM_AX_ASSISTANT_STRUCTURE_MOJOM_TRAITS_H_
