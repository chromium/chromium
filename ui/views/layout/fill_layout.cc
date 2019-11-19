// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/fill_layout.h"

#include <algorithm>

namespace views {

FillLayout::FillLayout() = default;

FillLayout::~FillLayout() = default;

void FillLayout::Layout(View* host) {
  if (host->children().empty())
    return;

  for (View* child : host->children())
    child->SetBoundsRect(host->GetContentsBounds());
}

gfx::Size FillLayout::GetPreferredSize(const View* host) const {
  if (host->children().empty())
    return gfx::Size();

  gfx::Size preferred_size;
  for (View* child : host->children())
    preferred_size.SetToMax(child->GetPreferredSize());
  gfx::Rect rect(preferred_size);
  rect.Inset(-host->GetInsets());
  return rect.size();
}

int FillLayout::GetPreferredHeightForWidth(const View* host, int width) const {
  if (host->children().empty())
    return 0;

  const gfx::Insets insets = host->GetInsets();
  int preferred_height = 0;
  for (View* child : host->children()) {
    int cur_preferred_height = 0;
    cur_preferred_height =
        child->GetHeightForWidth(width - insets.width()) + insets.height();
    preferred_height = std::max(preferred_height, cur_preferred_height);
  }
  return preferred_height;
}

}  // namespace views
