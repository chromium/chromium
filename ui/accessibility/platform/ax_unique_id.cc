// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_unique_id.h"

#include <unordered_set>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"

namespace ui {

namespace {

// Returns the container of assigned IDs.
std::unordered_set<int32_t>& GetAssignedIds() {
  static base::NoDestructor<std::unordered_set<int32_t>> assigned_ids;
  return *assigned_ids;
}

}  // namespace

AXUniqueId::~AXUniqueId() {
  if (id_ != kInvalidId) {
    GetAssignedIds().erase(id_);
  }
}

// static
int32_t AXUniqueId::GetNextAXUniqueId(int32_t max_id) {
  static int32_t current_id = 0;
  static bool has_wrapped = false;

  auto& assigned_ids = GetAssignedIds();
  const int32_t prev_id = current_id;
  do {
    if (current_id >= max_id) {
      current_id = 1;
      has_wrapped = true;
    } else {
      ++current_id;
    }
    CHECK_NE(current_id, prev_id)
        << "There are over 2 billion available IDs, so the newly created ID "
           "cannot be equal to the most recently created ID.";
    // If it |has_wrapped| then we need to continue until we find the first
    // unassigned ID.
  } while (has_wrapped && base::Contains(assigned_ids, current_id));

  assigned_ids.insert(current_id);
  return current_id;
}

}  // namespace ui
