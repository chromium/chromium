// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/visual_manager.h"

#include <bitset>

#include "base/strings/string_number_conversions.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/visual_picker_glx.h"
#include "ui/gfx/x/xfixes.h"

namespace x11 {

VisualManager::VisualManager(Connection* connection) : connection_(connection) {
  auto atom_name =
      "_NET_WM_CM_S" + base::NumberToString(connection_->DefaultScreenId());
  compositor_atom_ = GetAtom(atom_name.c_str());

  auto& xfixes = connection_->xfixes();
  if (xfixes.present()) {
    auto mask = x11::XFixes::SelectionEventMask::SetSelectionOwner |
                x11::XFixes::SelectionEventMask::SelectionWindowDestroy |
                x11::XFixes::SelectionEventMask::SelectionClientClose;
    xfixes.SelectSelectionInput(connection_->default_root(), compositor_atom_,
                                mask);
    connection_->AddEventObserver(this);
  }

  if (auto response = connection_->GetSelectionOwner(compositor_atom_).Sync()) {
    compositor_owner_ = response->owner;
  }

  for (const auto& depth : connection_->default_screen().allowed_depths) {
    for (const auto& visual : depth.visuals) {
      visuals_[visual.visual_id] =
          std::make_unique<XVisualData>(connection_, depth.depth, &visual);
    }
  }

  ColorMap colormap;
  PickBestVisuals(connection, opaque_visual_id_, transparent_visual_id_);

  // Choose the opaque visual.
  if (opaque_visual_id_ == VisualId{}) {
    opaque_visual_id_ = connection->default_screen().root_visual;
  }
  // opaque_visual_id_ may be unset in headless environments
  if (opaque_visual_id_ != VisualId{}) {
    DCHECK(visuals_.find(opaque_visual_id_) != visuals_.end());
    ChooseVisualForWindow(false, nullptr, nullptr, &colormap, nullptr);
  }

  // Choose the transparent visual.
  if (transparent_visual_id_ == VisualId{}) {
    for (const auto& pair : visuals_) {
      // Why support only 8888 ARGB? Because it's all that GTK supports. In
      // gdkvisual-x11.cc, they look for this specific visual and use it for
      // all their alpha channel using needs.
      const auto& data = *pair.second;
      if (data.depth == 32 && data.info->red_mask == 0xff0000 &&
          data.info->green_mask == 0x00ff00 &&
          data.info->blue_mask == 0x0000ff) {
        transparent_visual_id_ = pair.first;
        break;
      }
    }
  }
  if (transparent_visual_id_ != VisualId{}) {
    DCHECK(visuals_.find(transparent_visual_id_) != visuals_.end());
    ChooseVisualForWindow(true, nullptr, nullptr, &colormap, nullptr);
  }
}

VisualManager::~VisualManager() {
  auto& xfixes = connection_->xfixes();
  if (xfixes.present()) {
    xfixes.SelectSelectionInput(connection_->default_root(), compositor_atom_);
    connection_->RemoveEventObserver(this);
  }
}

void VisualManager::ChooseVisualForWindow(bool want_argb_visual,
                                          VisualId* visual_id,
                                          uint8_t* depth,
                                          ColorMap* colormap,
                                          bool* visual_has_alpha) {
  bool use_argb = want_argb_visual && ArgbVisualAvailable();
  VisualId visual = use_argb ? transparent_visual_id_ : opaque_visual_id_;

  if (visual_id) {
    *visual_id = visual;
  }
  bool success = GetVisualInfo(visual, depth, colormap, visual_has_alpha);
  DCHECK(success);
}

bool VisualManager::GetVisualInfo(VisualId visual_id,
                                  uint8_t* depth,
                                  ColorMap* colormap,
                                  bool* visual_has_alpha) {
  DCHECK_NE(visual_id, VisualId{});
  auto it = visuals_.find(visual_id);
  if (it == visuals_.end()) {
    return false;
  }
  XVisualData& data = *it->second;
  const VisualType& info = *data.info;

  if (depth) {
    *depth = data.depth;
  }
  if (colormap) {
    bool is_default_visual =
        visual_id == connection_->default_root_visual().visual_id;
    *colormap = is_default_visual ? ColorMap{} : data.GetColormap(connection_);
  }
  if (visual_has_alpha) {
    auto popcount = [](auto x) {
      return std::bitset<8 * sizeof(decltype(x))>(x).count();
    };
    *visual_has_alpha = popcount(info.red_mask) + popcount(info.green_mask) +
                            popcount(info.blue_mask) <
                        static_cast<std::size_t>(data.depth);
  }
  return true;
}

bool VisualManager::ArgbVisualAvailable() const {
  return compositor_owner_ != x11::Window::None &&
         transparent_visual_id_ != VisualId{};
}

VisualManager::XVisualData::XVisualData(Connection* connection,
                                        uint8_t depth,
                                        const VisualType* info)
    : depth(depth), info(info) {}

// Do not free the colormap as this would uninstall the colormap even for
// non-Chromium clients.
VisualManager::XVisualData::~XVisualData() = default;

ColorMap VisualManager::XVisualData::GetColormap(Connection* connection) {
  if (colormap_ == ColorMap{}) {
    colormap_ = connection->GenerateId<ColorMap>();
    connection->CreateColormap({ColormapAlloc::None, colormap_,
                                connection->default_root(), info->visual_id});
    // In single-process mode, VisualManager may be used on multiple threads,
    // so we need to flush colormap creation early so that other threads are
    // able to use it.
    connection->Flush();
  }
  return colormap_;
}

void VisualManager::OnEvent(const x11::Event& event) {
  if (auto* selection_notify = event.As<x11::XFixes::SelectionNotifyEvent>()) {
    if (selection_notify->selection == compositor_atom_) {
      compositor_owner_ = selection_notify->owner;
    }
  }
}

}  // namespace x11
