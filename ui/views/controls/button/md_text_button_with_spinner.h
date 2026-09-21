// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_WITH_SPINNER_H_
#define UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_WITH_SPINNER_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/metadata/view_factory.h"

namespace views {

// A material design themed text button with a spinner displayed on the
// left side.
class VIEWS_EXPORT MdTextButtonWithSpinner : public MdTextButton {
  METADATA_HEADER(MdTextButtonWithSpinner, MdTextButton)

 public:
  explicit MdTextButtonWithSpinner(
      Button::PressedCallback callback = PressedCallback(),
      std::u16string_view text = {});

  MdTextButtonWithSpinner(const MdTextButtonWithSpinner&) = delete;
  MdTextButtonWithSpinner& operator=(const MdTextButtonWithSpinner&) = delete;
  ~MdTextButtonWithSpinner() override;

  void SetSpinnerVisible(bool visible);
  bool GetSpinnerVisible() const;

  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;

  Throbber* GetSpinnerForTesting() { return spinner_; }

  void UpdateColors() override;

 private:
  friend class MdTextButtonWithSpinnerTestPeer;

  void UpdateSpinnerColor();
  raw_ptr<Throbber> spinner_ = nullptr;
  bool spinner_visible_ = false;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, MdTextButtonWithSpinner, MdTextButton)
VIEW_BUILDER_PROPERTY(bool, SpinnerVisible)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, MdTextButtonWithSpinner)

#endif  // UI_VIEWS_CONTROLS_BUTTON_MD_TEXT_BUTTON_WITH_SPINNER_H_
