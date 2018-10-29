// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/property_utils.h"

#include "base/logging.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"

namespace aura {
namespace {

client::WindowType UiWindowTypeToWindowType(ws::mojom::WindowType type) {
  switch (type) {
    case ws::mojom::WindowType::WINDOW:
      return client::WINDOW_TYPE_NORMAL;
    case ws::mojom::WindowType::PANEL:
      return client::WINDOW_TYPE_PANEL;
    case ws::mojom::WindowType::CONTROL:
      return client::WINDOW_TYPE_CONTROL;
    case ws::mojom::WindowType::WINDOW_FRAMELESS:
    case ws::mojom::WindowType::POPUP:
    case ws::mojom::WindowType::BUBBLE:
    case ws::mojom::WindowType::DRAG:
      return client::WINDOW_TYPE_POPUP;
    case ws::mojom::WindowType::MENU:
      return client::WINDOW_TYPE_MENU;
    case ws::mojom::WindowType::TOOLTIP:
      return client::WINDOW_TYPE_TOOLTIP;
    case ws::mojom::WindowType::UNKNOWN:
      return client::WINDOW_TYPE_UNKNOWN;
  }
  NOTREACHED();
  return client::WINDOW_TYPE_UNKNOWN;
}

}  // namespace

void SetWindowType(Window* window, ws::mojom::WindowType window_type) {
  if (window_type == ws::mojom::WindowType::UNKNOWN)
    return;
  window->SetProperty(client::kWindowTypeKey, window_type);
  window->SetType(UiWindowTypeToWindowType(window_type));
}

ws::mojom::WindowType GetWindowTypeFromProperties(
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  auto iter =
      properties.find(ws::mojom::WindowManager::kWindowType_InitProperty);
  if (iter == properties.end())
    return ws::mojom::WindowType::UNKNOWN;
  return static_cast<ws::mojom::WindowType>(
      mojo::ConvertTo<int32_t>(iter->second));
}

ws::mojom::OcclusionState WindowOcclusionStateToMojom(
    Window::OcclusionState input) {
  switch (input) {
    case Window::OcclusionState::UNKNOWN:
      return ws::mojom::OcclusionState::kUnknown;
    case Window::OcclusionState::VISIBLE:
      return ws::mojom::OcclusionState::kVisible;
    case Window::OcclusionState::OCCLUDED:
      return ws::mojom::OcclusionState::kOccluded;
    case Window::OcclusionState::HIDDEN:
      return ws::mojom::OcclusionState::kHidden;
  }
  NOTREACHED();
  return ws::mojom::OcclusionState::kUnknown;
}

Window::OcclusionState WindowOcclusionStateFromMojom(
    ws::mojom::OcclusionState input) {
  switch (input) {
    case ws::mojom::OcclusionState::kUnknown:
      return Window::OcclusionState::UNKNOWN;
    case ws::mojom::OcclusionState::kVisible:
      return aura::Window::OcclusionState::VISIBLE;
    case ws::mojom::OcclusionState::kOccluded:
      return Window::OcclusionState::OCCLUDED;
    case ws::mojom::OcclusionState::kHidden:
      return Window::OcclusionState::HIDDEN;
  }
  NOTREACHED();
  return Window::OcclusionState::UNKNOWN;
}

}  // namespace aura
