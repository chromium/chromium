// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/debug_utils.h"

#include <ostream>

#include "base/logging.h"
#include "ui/views/view.h"

#if !defined(NDEBUG)
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/transform_util.h"
#endif

namespace views {
namespace {
void PrintViewHierarchyImp(const View* view,
                           size_t indent,
                           std::ostringstream* out) {
  *out << std::string(indent, ' ');
  *out << view->GetClassName();
  *out << ' ';
  *out << view->GetID();
  *out << ' ';
  *out << view->x() << "," << view->y() << ",";
  *out << view->bounds().right() << "," << view->bounds().bottom();
  *out << ' ';
  *out << view;
  *out << '\n';

  for (const View* child : view->children())
    PrintViewHierarchyImp(child, indent + 2, out);
}

void PrintFocusHierarchyImp(const View* view,
                            size_t indent,
                            std::ostringstream* out) {
  *out << std::string(indent, ' ');
  *out << view->GetClassName();
  *out << ' ';
  *out << view->GetID();
  *out << ' ';
  *out << view->GetClassName();
  *out << ' ';
  *out << view;
  *out << '\n';

  if (!view->children().empty())
    PrintFocusHierarchyImp(view->children().front(), indent + 2, out);

  const View* next_focusable = view->GetNextFocusableView();
  if (next_focusable)
    PrintFocusHierarchyImp(next_focusable, indent, out);
}

#if !defined(NDEBUG)
std::string PrintViewGraphImpl(const View* view) {
  // 64-bit pointer = 16 bytes of hex + "0x" + '\0' = 19.
  const size_t kMaxPointerStringLength = 19;

  std::string result;

  // Node characteristics.
  char p[kMaxPointerStringLength];

  const std::string class_name(view->GetClassName());
  size_t base_name_index = class_name.find_last_of('/');
  if (base_name_index == std::string::npos)
    base_name_index = 0;
  else
    base_name_index++;

  constexpr size_t kBoundsBufferSize = 512;
  char bounds_buffer[kBoundsBufferSize];

  // Information about current node.
  base::snprintf(p, kBoundsBufferSize, "%p", view);
  result.append("  N");
  result.append(p + 2);
  result.append(" [label=\"");

  result.append(class_name.substr(base_name_index).c_str());

  base::snprintf(bounds_buffer, kBoundsBufferSize,
                 "\\n bounds: (%d, %d), (%dx%d)", view->bounds().x(),
                 view->bounds().y(), view->bounds().width(),
                 view->bounds().height());
  result.append(bounds_buffer);

  gfx::DecomposedTransform decomp;
  if (!view->GetTransform().IsIdentity() &&
      gfx::DecomposeTransform(&decomp, view->GetTransform())) {
    base::snprintf(bounds_buffer, kBoundsBufferSize,
                   "\\n translation: (%f, %f)", decomp.translate[0],
                   decomp.translate[1]);
    result.append(bounds_buffer);

    base::snprintf(bounds_buffer, kBoundsBufferSize, "\\n rotation: %3.2f",
                   gfx::RadToDeg(std::acos(decomp.quaternion.w()) * 2));
    result.append(bounds_buffer);

    base::snprintf(bounds_buffer, kBoundsBufferSize,
                   "\\n scale: (%2.4f, %2.4f)", decomp.scale[0],
                   decomp.scale[1]);
    result.append(bounds_buffer);
  }

  result.append("\"");
  if (!view->parent())
    result.append(", shape=box");
  if (view->layer()) {
    if (view->layer()->has_external_content())
      result.append(", color=green");
    else
      result.append(", color=red");

    if (view->layer()->fills_bounds_opaquely())
      result.append(", style=filled");
  }
  result.append("]\n");

  // Link to parent.
  if (view->parent()) {
    char pp[kMaxPointerStringLength];

    base::snprintf(pp, kMaxPointerStringLength, "%p", view->parent());
    result.append("  N");
    result.append(pp + 2);
    result.append(" -> N");
    result.append(p + 2);
    result.append("\n");
  }

  for (const View* child : view->children())
    result.append(PrintViewGraphImpl(child));

  return result;
}
#endif

}  // namespace

void PrintViewHierarchy(const View* view) {
  std::ostringstream out;
  out << "View hierarchy:\n";
  PrintViewHierarchyImp(view, 0, &out);
  // Error so users in the field can generate and upload logs.
  LOG(ERROR) << out.str();
}

void PrintFocusHierarchy(const View* view) {
  std::ostringstream out;
  out << "Focus hierarchy:\n";
  PrintFocusHierarchyImp(view, 0, &out);
  // Error so users in the field can generate and upload logs.
  LOG(ERROR) << out.str();
}

#if !defined(NDEBUG)
std::string PrintViewGraph(const View* view) {
  return "digraph {\n" + PrintViewGraphImpl(view) + "}\n";
}
#endif

}  // namespace views
