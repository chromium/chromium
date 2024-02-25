// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_EVENT_REWRITER_UTILS_H_
#define UI_EVENTS_ASH_EVENT_REWRITER_UTILS_H_

#include <tuple>

#include "ui/events/keycodes/dom/dom_code.h"

namespace ui::internal {

// Represents physical key for internal use of EventRewriter implementation
// to remember key state before rewriting.
struct PhysicalKey {
  DomCode code;
  int source_device_id;
  inline bool operator<(const PhysicalKey& other) const {
    return std::tie(code, source_device_id) <
           std::tie(other.code, other.source_device_id);
  }
};

}  // namespace ui::internal

#endif  // UI_EVENTS_ASH_EVENT_REWRITER_UTILS_H_
