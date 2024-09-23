// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/multiline_example.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views::examples {

namespace {

gfx::Range ClampRange(gfx::Range range, size_t max) {
  range.set_start(std::min(range.start(), max));
  range.set_end(std::min(range.end(), max));
  return range;
}

// A Label with a clamped preferred width to demonstrate wrapping.
class PreferredSizeLabel : public Label {
  METADATA_HEADER(PreferredSizeLabel, Label)

 public:
  PreferredSizeLabel() = default;

  PreferredSizeLabel(const PreferredSizeLabel&) = delete;
  PreferredSizeLabel& operator=(const PreferredSizeLabel&) = delete;

  ~PreferredSizeLabel() override = default;

  // Label:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override {
    return gfx::Size(50,
                     Label::CalculatePreferredSize(available_size).height());
  }
};

BEGIN_METADATA(PreferredSizeLabel)
END_METADATA

}  // namespace

// A simple View that hosts a RenderText object.
class MultilineExample::RenderTextView : public View {
  METADATA_HEADER(RenderTextView, View)

 public:
  RenderTextView() : render_text_(gfx::RenderText::CreateRenderText()) {
    render_text_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    render_text_->SetMultiline(true);
    SetBorder(CreateThemedSolidBorder(
        2, ExamplesColorIds::kColorMultilineExampleBorder));
  }

  RenderTextView(const RenderTextView&) = delete;
  RenderTextView& operator=(const RenderTextView&) = delete;

  void OnPaint(gfx::Canvas* canvas) override {
    View::OnPaint(canvas);
    render_text_->Draw(canvas);
  }

  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override {
    int w = available_size.width().value_or(0);
    if (w == 0) {
      // Turn off multiline mode to get the single-line text size, which is the
      // preferred size for this view.
      render_text_->SetMultiline(false);
      gfx::Size size(render_text_->GetContentWidth(),
                     render_text_->GetStringSize().height());
      size.Enlarge(GetInsets().width(), GetInsets().height());
      render_text_->SetMultiline(true);
      return size;
    }

    const gfx::Rect old_rect = render_text_->display_rect();
    gfx::Rect rect = old_rect;
    rect.set_width(w - GetInsets().width());
    render_text_->SetDisplayRect(rect);
    int height = render_text_->GetStringSize().height() + GetInsets().height();
    render_text_->SetDisplayRect(old_rect);
    return gfx::Size(w, height);
  }

  void OnThemeChanged() override {
    View::OnThemeChanged();
    UpdateColors();
  }

  void SetText(const std::u16string& new_contents) {
    // Color and style the text inside |test_range| to test colors and styles.
    const size_t range_max = new_contents.length();
    gfx::Range bold_range = ClampRange(gfx::Range(4, 10), range_max);
    gfx::Range italic_range = ClampRange(gfx::Range(7, 13), range_max);

    render_text_->SetText(new_contents);
    render_text_->SetStyle(gfx::TEXT_STYLE_UNDERLINE, false);
    render_text_->SetStyle(gfx::TEXT_STYLE_STRIKE, false);
    render_text_->ApplyStyle(gfx::TEXT_STYLE_ITALIC, true, italic_range);
    render_text_->ApplyWeight(gfx::Font::Weight::BOLD, bold_range);
    UpdateColors();
    InvalidateLayout();
  }

  void SetMaxLines(int max_lines) {
    render_text_->SetMaxLines(max_lines);
    render_text_->SetElideBehavior(max_lines ? gfx::ELIDE_TAIL : gfx::NO_ELIDE);
  }

 private:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    render_text_->SetDisplayRect(GetContentsBounds());
  }

  void UpdateColors() {
    const auto* cp = GetColorProvider();
    if (!cp)
      return;
    render_text_->SetColor(
        cp->GetColor(ExamplesColorIds::kColorMultilineExampleForeground));
    render_text_->set_selection_color(cp->GetColor(
        ExamplesColorIds::kColorMultilineExampleSelectionForeground));
    render_text_->set_selection_background_focused_color(cp->GetColor(
        ExamplesColorIds::kColorMultilineExampleSelectionBackground));
    const size_t range_max = render_text_->text().length();
    gfx::Range color_range = ClampRange(gfx::Range(1, 21), range_max);
    render_text_->ApplyColor(
        cp->GetColor(ExamplesColorIds::kColorMultilineExampleColorRange),
        color_range);
    render_text_->ApplyStyle(gfx::TEXT_STYLE_UNDERLINE, true, color_range);
  }

  std::unique_ptr<gfx::RenderText> render_text_;
};

BEGIN_METADATA(MultilineExample, RenderTextView)
END_METADATA

MultilineExample::MultilineExample()
    : ExampleBase(GetStringUTF8(IDS_MULTILINE_SELECT_LABEL).c_str()) {}

MultilineExample::~MultilineExample() {
  if (textfield_) {
    textfield_->set_controller(nullptr);
  }
}

void MultilineExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(LayoutAlignment::kStart, LayoutAlignment::kCenter,
                  TableLayout::kFixedSize,
                  TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 1.0f,
                 TableLayout::ColumnSize::kFixed, 0, 0)
      .AddRows(4, TableLayout::kFixedSize);

  const std::u16string kTestString = u"qwertyالرئيسيةasdfgh";

  container->AddChildView(
      std::make_unique<Label>(GetStringUTF16(IDS_MULTILINE_RENDER_TEXT_LABEL)));
  render_text_view_ =
      container->AddChildView(std::make_unique<RenderTextView>());
  render_text_view_->SetText(kTestString);

  label_checkbox_ = container->AddChildView(std::make_unique<Checkbox>(
      GetStringUTF16(IDS_MULTILINE_LABEL),
      base::BindRepeating(
          [](MultilineExample* example) {
            example->label_->SetText(example->label_checkbox_->GetChecked()
                                         ? example->textfield_->GetText()
                                         : std::u16string());
          },
          base::Unretained(this))));
  label_checkbox_->SetChecked(true);
  label_checkbox_->SetRequestFocusOnPress(false);
  label_ = container->AddChildView(std::make_unique<PreferredSizeLabel>());
  label_->SetText(kTestString);
  label_->SetMultiLine(true);
  label_->SetBorder(CreateThemedSolidBorder(
      2, ExamplesColorIds::kColorMultilineExampleLabelBorder));

  elision_checkbox_ = container->AddChildView(std::make_unique<Checkbox>(
      GetStringUTF16(IDS_MULTILINE_ELIDE_LABEL),
      base::BindRepeating(
          [](MultilineExample* example) {
            example->render_text_view_->SetMaxLines(
                example->elision_checkbox_->GetChecked() ? 3 : 0);
          },
          base::Unretained(this))));
  elision_checkbox_->SetChecked(false);
  elision_checkbox_->SetRequestFocusOnPress(false);
  container->AddChildView(std::make_unique<View>());

  auto* label = container->AddChildView(
      std::make_unique<Label>(GetStringUTF16(IDS_MULTILINE_SAMPLE_TEXT_LABEL)));
  textfield_ = container->AddChildView(std::make_unique<Textfield>());
  textfield_->set_controller(this);
  textfield_->SetText(kTestString);
  textfield_->GetViewAccessibility().SetName(*label);
}

void MultilineExample::ContentsChanged(Textfield* sender,
                                       const std::u16string& new_contents) {
  render_text_view_->SetText(new_contents);
  if (label_checkbox_->GetChecked())
    label_->SetText(new_contents);
  example_view()->InvalidateLayout();
  example_view()->SchedulePaint();
}

}  // namespace views::examples
