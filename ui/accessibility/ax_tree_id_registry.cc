// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_id_registry.h"

#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_host_delegate.h"

namespace ui {

// static
AXTreeIDRegistry* AXTreeIDRegistry::GetInstance() {
  return base::Singleton<AXTreeIDRegistry>::get();
}

AXTreeID AXTreeIDRegistry::GetOrCreateAXTreeID(int process_id, int routing_id) {
  FrameID frame_id(process_id, routing_id);
  auto it = frame_to_ax_tree_id_map_.find(frame_id);
  if (it != frame_to_ax_tree_id_map_.end())
    return it->second;

  AXTreeID new_id =
      AXTreeID::FromString(base::IntToString(++ax_tree_id_counter_));
  frame_to_ax_tree_id_map_[frame_id] = new_id;
  ax_tree_to_frame_id_map_[new_id] = frame_id;

  return new_id;
}

AXTreeIDRegistry::FrameID AXTreeIDRegistry::GetFrameID(AXTreeID ax_tree_id) {
  auto it = ax_tree_to_frame_id_map_.find(ax_tree_id);
  if (it != ax_tree_to_frame_id_map_.end())
    return it->second;

  return FrameID(-1, -1);
}

AXTreeID AXTreeIDRegistry::GetOrCreateAXTreeID(AXHostDelegate* delegate) {
  for (auto it : id_to_host_delegate_) {
    if (it.second == delegate)
      return it.first;
  }
  AXTreeID new_id =
      AXTreeID::FromString(base::IntToString(++ax_tree_id_counter_));
  id_to_host_delegate_[new_id] = delegate;
  return new_id;
}

AXHostDelegate* AXTreeIDRegistry::GetHostDelegate(AXTreeID ax_tree_id) {
  auto it = id_to_host_delegate_.find(ax_tree_id);
  if (it == id_to_host_delegate_.end())
    return nullptr;
  return it->second;
}

void AXTreeIDRegistry::SetDelegateForID(AXHostDelegate* delegate, AXTreeID id) {
  id_to_host_delegate_[id] = delegate;
}

void AXTreeIDRegistry::RemoveAXTreeID(AXTreeID ax_tree_id) {
  auto frame_it = ax_tree_to_frame_id_map_.find(ax_tree_id);
  if (frame_it != ax_tree_to_frame_id_map_.end()) {
    frame_to_ax_tree_id_map_.erase(frame_it->second);
    ax_tree_to_frame_id_map_.erase(frame_it);
    return;
  }

  auto action_it = id_to_host_delegate_.find(ax_tree_id);
  if (action_it != id_to_host_delegate_.end())
    id_to_host_delegate_.erase(action_it);
}

AXTreeIDRegistry::AXTreeIDRegistry() : ax_tree_id_counter_(-1) {
  // Always populate default desktop tree value (0 -> 0, 0).
  GetOrCreateAXTreeID(0, 0);
}

AXTreeIDRegistry::~AXTreeIDRegistry() {}

}  // namespace ui
