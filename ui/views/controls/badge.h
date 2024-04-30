// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BADGE_H_
#define UI_VIEWS_CONTROLS_BADGE_H_

#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

// A badge that displays a small piece of infromational text on a square blue
// background.
class VIEWS_EXPORT Badge : public View {
  METADATA_HEADER(Badge, View)

 public:
  explicit Badge(const std::u16string& text = std::u16string());

  Badge(const Badge&) = delete;
  Badge& operator=(const Badge&) = delete;

  ~Badge() override;

  const std::u16string& GetText() const;
  void SetText(const std::u16string& text);

  // View:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  std::u16string text_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, Badge, View)
VIEW_BUILDER_PROPERTY(std::u16string, Text)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, Badge)

#endif  // UI_VIEWS_CONTROLS_BADGE_H_
