// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ASSISTANT_STRUCTURE_H_
#define UI_ACCESSIBILITY_AX_ASSISTANT_STRUCTURE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace ui {

struct AXTreeUpdate;

struct AssistantNode {
  AssistantNode();
  AssistantNode(const AssistantNode& other);
  AssistantNode& operator=(const AssistantNode&) = delete;
  ~AssistantNode();

  std::vector<int32_t> children_indices;

  // Geometry of the view in pixels
  gfx::Rect rect;

  // Geometry of the view in pixels, unclipped.
  gfx::Rect unclipped_rect;

  // Geometry of the view in absolute pixels relative to the page, unclipped.
  gfx::Rect page_absolute_rect;

  // Text of the view.
  std::u16string text;

  // Text properties
  float text_size;
  uint32_t color;
  uint32_t bgcolor;
  bool bold;
  bool italic;
  bool underline;
  bool line_through;

  // Selected portion of the text.
  std::optional<gfx::Range> selection;

  // Fake Android view class name of the element.  Each node is assigned
  // a closest approximation of Android's views to keep the server happy.
  std::string class_name;

  // HTML and CSS attributes.
  std::string html_tag;
  std::string css_display;

  // HTML attributes: map from lowercase ASCII HTML attribute name to value.
  base::StringPairs html_attributes;

  // Accessibility functionality of the node inferred from DOM or based on HTML
  // role attribute.
  std::optional<std::string> role;
};

struct AssistantTree {
  AssistantTree();
  AssistantTree(const AssistantTree& other);
  AssistantTree& operator=(const AssistantTree&) = delete;
  ~AssistantTree();

  std::vector<std::unique_ptr<AssistantNode>> nodes;
};

std::unique_ptr<AssistantTree> CreateAssistantTree(const AXTreeUpdate& update);

std::u16string AXUrlBaseText(std::u16string url);
const char* AXRoleToAndroidClassName(ax::mojom::Role role, bool has_parent);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ASSISTANT_STRUCTURE_H_
