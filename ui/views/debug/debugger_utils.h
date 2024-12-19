// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DEBUG_DEBUGGER_UTILS_H_
#define UI_VIEWS_DEBUG_DEBUGGER_UTILS_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/callback.h"

namespace views::debug {

// This class acts as a "view" over the View class. This has been done to allow
// debugger extensions to remain resilient to structure and version changes in
// the codebase.
// TODO(tluk): Replace use of //ui/views/debug_utils.h with this.
class ViewDebugWrapper {
 public:
  // Tuple used to represent View bounds. Takes the form <x, y, width, height>.
  using BoundsTuple = std::tuple<int, int, int, int>;
  // Callback function used to iterate through all metadata properties.
  using PropCallback =
      base::RepeatingCallback<void(const std::string&, const std::string&)>;

  ViewDebugWrapper() = default;
  virtual ~ViewDebugWrapper() = default;

  virtual std::string GetViewClassName() = 0;
  virtual int GetID() = 0;
  virtual BoundsTuple GetBounds() = 0;
  virtual bool GetVisible() = 0;
  virtual bool GetNeedsLayout() = 0;
  virtual bool GetEnabled() = 0;
  virtual std::vector<ViewDebugWrapper*> GetChildren() = 0;
  virtual void ForAllProperties(PropCallback callback) {}
  virtual std::optional<intptr_t> GetAddress();
};

std::string PrintViewHierarchy(ViewDebugWrapper* view, bool verbose = false);

}  // namespace views::debug

#endif  // UI_VIEWS_DEBUG_DEBUGGER_UTILS_H_
