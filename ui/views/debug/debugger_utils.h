// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DEBUG_DEBUGGER_UTILS_H_
#define UI_VIEWS_DEBUG_DEBUGGER_UTILS_H_

#include <string>
#include <tuple>
#include <vector>

namespace views {
namespace debug {

// This class acts as a "view" over the View class. This has been done to allow
// debugger extensions to remnain resillient to structure and version changes in
// the code base.
// TODO(tluk): Replace use of //ui/views/debug_utils.h with this.
class ViewDebugWrapper {
 public:
  // Tuple used to represent View bounds. Takes the form <x, y, width, height>.
  using BoundsTuple = std::tuple<int, int, int, int>;

  ViewDebugWrapper() = default;
  virtual ~ViewDebugWrapper() = default;

  virtual std::string GetViewClassName() = 0;
  virtual int GetID() = 0;
  virtual BoundsTuple GetBounds() = 0;
  virtual bool GetVisible() = 0;
  virtual bool GetNeedsLayout() = 0;
  virtual bool GetEnabled() = 0;
  virtual std::vector<ViewDebugWrapper*> GetChildren() = 0;
};

void PrintViewHierarchy(std::ostringstream* out,
                        ViewDebugWrapper* view,
                        int depth = -1,
                        size_t column_limit = 240);

}  // namespace debug
}  // namespace views

#endif  // UI_VIEWS_DEBUG_DEBUGGER_UTILS_H_
