// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DEBUG_DEBUGGER_UTILS_H_
#define UI_VIEWS_DEBUG_DEBUGGER_UTILS_H_

#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/callback.h"

namespace views::debug {

// This class acts as a "view" over the View class. This has been done to allow
// debugger extensions to remnain resillient to structure and version changes in
// the code base.
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

void PrintViewHierarchy(std::ostream* out,
                        ViewDebugWrapper* view,
                        bool verbose = false,
                        int depth = -1,
                        size_t column_limit = 240);

}  // namespace views::debug

#endif  // UI_VIEWS_DEBUG_DEBUGGER_UTILS_H_
