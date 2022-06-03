// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DEBUG_UTILS_H_
#define UI_VIEWS_DEBUG_UTILS_H_

#include <string>

#include "ui/views/views_export.h"

namespace views {

class View;

// Log the view hierarchy.
VIEWS_EXPORT void PrintViewHierarchy(const View* view);

// Print the view hierarchy to |out|.
VIEWS_EXPORT void PrintViewHierarchy(const View* view, std::ostringstream* out);

// Log the focus traversal hierarchy.
VIEWS_EXPORT void PrintFocusHierarchy(const View* view);

#if !defined(NDEBUG)
// Returns string containing a graph of the views hierarchy in graphViz DOT
// language (http://graphviz.org/). Can be called within debugger and saved
// to a file to compile/view.
VIEWS_EXPORT std::string PrintViewGraph(const View* view);
#endif

}  // namespace views

#endif  // UI_VIEWS_DEBUG_UTILS_H_
