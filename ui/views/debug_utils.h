// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DEBUG_UTILS_H_
#define UI_VIEWS_DEBUG_UTILS_H_

#include <sstream>
#include <string>

#include "ui/gfx/native_ui_types.h"
#include "ui/views/views_export.h"

namespace views {

class View;
class Widget;

// Log the focus traversal hierarchy.
VIEWS_EXPORT void PrintFocusHierarchy(const View* view);

// Log the information of the widget to |out|. |detailed| controls the amount of
// information logged.
VIEWS_EXPORT void PrintWidgetInformation(const Widget& widget,
                                         bool detailed,
                                         std::ostringstream* out);

// Prints the window hierarchy (Aura only) and Widget information for the given
// native window.
VIEWS_EXPORT void PrintWindowHierarchy(gfx::NativeWindow window,
                                       std::ostringstream* out);

// Prints the ui::Layer hierarchy for the given native window.
// Uses ui::PrintLayerHierarchy to print the layer tree.
VIEWS_EXPORT void PrintLayerHierarchy(gfx::NativeWindow window,
                                      std::ostringstream* out);

#if !defined(NDEBUG)
// Returns string containing a graph of the views hierarchy in graphViz DOT
// language (http://graphviz.org/). Can be called within debugger and saved
// to a file to compile/view.
VIEWS_EXPORT std::string PrintViewGraph(const View* view);
#endif

}  // namespace views

#endif  // UI_VIEWS_DEBUG_UTILS_H_
