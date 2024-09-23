// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/text_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/examples/example_combobox_model.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views::examples {

namespace {

// Number of columns in the view layout.
constexpr int kNumColumns = 10;

// Toggles bit |flag| on |flags| based on state of |checkbox|.
void SetFlagFromCheckbox(Checkbox* checkbox, int* flags, int flag) {
  if (checkbox->GetChecked())
    *flags |= flag;
  else
    *flags &= ~flag;
}

}  // namespace

// TextExample's content view, which draws stylized string.
class TextExample::TextExampleView : public View {
  METADATA_HEADER(TextExampleView, View)

 public:
  TextExampleView() = default;
  TextExampleView(const TextExampleView&) = delete;
  TextExampleView& operator=(const TextExampleView&) = delete;
  ~TextExampleView() override = default;

  void OnPaint(gfx::Canvas* canvas) override {
    View::OnPaint(canvas);
    const gfx::Rect bounds = GetContentsBounds();
    const SkColor color =
        GetColorProvider()->GetColor(ui::kColorTextfieldForeground);
    canvas->DrawStringRectWithFlags(text_, font_list_, color, bounds, flags_);
  }

  int GetFlags() const { return flags_; }
  void SetFlags(int flags) {
    flags_ = flags;
    SchedulePaint();
  }

  void SetText(const std::u16string& text) {
    text_ = text;
    SchedulePaint();
  }

  void SetElide(gfx::ElideBehavior elide) {
    elide_ = elide;
    SchedulePaint();
  }

  int GetStyle() const { return font_list_.GetFontStyle(); }
  void SetStyle(int style) {
    base_font_ = base_font_.DeriveWithStyle(style);
    font_list_ = font_list_.DeriveWithStyle(style);
    SchedulePaint();
  }

  gfx::Font::Weight GetWeight() const { return font_list_.GetFontWeight(); }
  void SetWeight(gfx::Font::Weight weight) {
    font_list_ = base_font_.DeriveWithWeight(weight);
    SchedulePaint();
  }

 protected:
  void OnThemeChanged() override {
    View::OnThemeChanged();
    SetBorder(CreateSolidBorder(
        1, GetColorProvider()->GetColor(ui::kColorFocusableBorderUnfocused)));
  }

 private:
  // The font used for drawing the text.
  gfx::FontList font_list_;

  // The font without any bold attributes. Mac font APIs struggle to derive UI
  // fonts from a base font that isn't NORMAL or BOLD.
  gfx::FontList base_font_;

  // The text to draw.
  std::u16string text_;

  // Text flags for passing to |DrawStringRect()|.
  int flags_ = 0;

  // The eliding, fading, or truncating behavior.
  gfx::ElideBehavior elide_ = gfx::NO_ELIDE;
};

BEGIN_METADATA(TextExample, TextExampleView)
END_METADATA

TextExample::TextExample()
    : ExampleBase(l10n_util::GetStringUTF8(IDS_TEXT_STYLE_LABEL).c_str()) {}

TextExample::~TextExample() = default;

Checkbox* TextExample::AddCheckbox(View* parent, const char* name) {
  return parent->AddChildView(std::make_unique<Checkbox>(
      base::ASCIIToUTF16(name),
      base::BindRepeating(&TextExample::UpdateStyle, base::Unretained(this))));
}

Combobox* TextExample::AddCombobox(View* parent,
                                   std::u16string name,
                                   base::span<const char* const> items,
                                   void (TextExample::*combobox_callback)()) {
  parent->AddChildView(std::make_unique<Label>(name));
  auto* combobox = parent->AddChildView(std::make_unique<Combobox>(
      std::make_unique<ExampleComboboxModel>(items)));
  combobox->SetProperty(kTableColAndRowSpanKey, gfx::Size(kNumColumns - 1, 1));
  combobox->SetCallback(
      base::BindRepeating(combobox_callback, base::Unretained(this)));
  combobox->GetViewAccessibility().SetName(name);
  return combobox;
}

void TextExample::CreateExampleView(View* container) {
  auto* box_layout = container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(8), 20, false));
  auto* table_container = container->AddChildView(std::make_unique<View>());
  TableLayout* layout =
      table_container->SetLayoutManager(std::make_unique<TableLayout>());
  layout->AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStretch, 0.1f,
                    TableLayout::ColumnSize::kUsePreferred, 0, 0);
  for (int i = 0; i < kNumColumns - 1; i++) {
    layout->AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch,
                      0.1f, TableLayout::ColumnSize::kUsePreferred, 0, 0);
  }
  layout->AddRows(6, TableLayout::kFixedSize);

  constexpr auto kHorizontalAligments = std::to_array<const char* const>({
      "Default",
      "Left",
      "Center",
      "Right",
  });
  h_align_cb_ = AddCombobox(table_container, u"H-Align", kHorizontalAligments,
                            &TextExample::AlignComboboxChanged);

  constexpr auto kElideBehaviors =
      std::to_array<const char* const>({"Elide", "No Elide"});
  eliding_cb_ = AddCombobox(table_container, u"Eliding", kElideBehaviors,
                            &TextExample::ElideComboboxChanged);

  constexpr auto kPrefixOptions = std::to_array<const char* const>({
      "Default",
      "Show",
      "Hide",
  });
  prefix_cb_ = AddCombobox(table_container, u"Prefix", kPrefixOptions,
                           &TextExample::PrefixComboboxChanged);

  constexpr auto kTextExamples = std::to_array<const char* const>({
      "Short",
      "Long",
      "Ampersands",
      "RTL Hebrew",
  });
  text_cb_ = AddCombobox(table_container, u"Example Text", kTextExamples,
                         &TextExample::TextComboboxChanged);

  constexpr auto kWeightLabels = std::to_array<const char* const>({
      "Thin",
      "Extra Light",
      "Light",
      "Normal",
      "Medium",
      "Semibold",
      "Bold",
      "Extra Bold",
      "Black",
  });
  weight_cb_ = AddCombobox(table_container, u"Font Weight", kWeightLabels,
                           &TextExample::WeightComboboxChanged);
  weight_cb_->SelectValue(u"Normal");

  multiline_checkbox_ = AddCheckbox(table_container, "Multiline");
  break_checkbox_ = AddCheckbox(table_container, "Character Break");
  italic_checkbox_ = AddCheckbox(table_container, "Italic");
  underline_checkbox_ = AddCheckbox(table_container, "Underline");
  strike_checkbox_ = AddCheckbox(table_container, "Strike");

  auto* fill_container = container->AddChildView(std::make_unique<View>());
  box_layout->SetFlexForView(fill_container, 1);
  fill_container->SetLayoutManager(std::make_unique<FillLayout>());
  fill_container->SetBorder(CreateEmptyBorder(gfx::Insets::VH(0, 8)));

  text_view_ =
      fill_container->AddChildView(std::make_unique<TextExampleView>());

  TextComboboxChanged();  // Sets initial text content.
}

void TextExample::UpdateStyle() {
  int flags = text_view_->GetFlags();
  int style = text_view_->GetStyle();
  SetFlagFromCheckbox(multiline_checkbox_, &flags, gfx::Canvas::MULTI_LINE);
  SetFlagFromCheckbox(break_checkbox_, &flags,
                      gfx::Canvas::CHARACTER_BREAKABLE);
  SetFlagFromCheckbox(italic_checkbox_, &style, gfx::Font::ITALIC);
  SetFlagFromCheckbox(underline_checkbox_, &style, gfx::Font::UNDERLINE);
  SetFlagFromCheckbox(strike_checkbox_, &style, gfx::Font::STRIKE_THROUGH);
  text_view_->SetFlags(flags);
  text_view_->SetStyle(style);
}

void TextExample::AlignComboboxChanged() {
  int flags = text_view_->GetFlags() &
              ~(gfx::Canvas::TEXT_ALIGN_LEFT | gfx::Canvas::TEXT_ALIGN_CENTER |
                gfx::Canvas::TEXT_ALIGN_RIGHT);
  switch (h_align_cb_->GetSelectedIndex().value()) {
    case 0:
      break;
    case 1:
      flags |= gfx::Canvas::TEXT_ALIGN_LEFT;
      break;
    case 2:
      flags |= gfx::Canvas::TEXT_ALIGN_CENTER;
      break;
    case 3:
      flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
      break;
  }
  text_view_->SetFlags(flags);
}

void TextExample::TextComboboxChanged() {
  switch (text_cb_->GetSelectedIndex().value()) {
    case 0:
      text_view_->SetText(u"The quick brown fox jumps over the lazy dog.");
      break;
    case 1:
      text_view_->SetText(
          u"Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do "
          u"eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut "
          u"enim ad minim veniam, quis nostrud exercitation ullamco laboris "
          u"nisi ut aliquip ex ea commodo consequat.\n"
          u"Duis aute irure dolor in reprehenderit in voluptate velit esse "
          u"cillum dolore eu fugiat nulla pariatur.\n"
          u"\n"
          u"Excepteur sint occaecat cupidatat non proident, sunt in culpa qui "
          u"officia deserunt mollit anim id est laborum.");
      break;
    case 2:
      text_view_->SetText(u"The quick && &brown fo&x jumps over the lazy dog.");
      break;
    case 3:
      text_view_->SetText(
          u"\x5e9\x5dc\x5d5\x5dd \x5d4\x5e2\x5d5\x5dc\x5dd! "
          u"\x5e9\x5dc\x5d5\x5dd \x5d4\x5e2\x5d5\x5dc\x5dd! "
          u"\x5e9\x5dc\x5d5\x5dd \x5d4\x5e2\x5d5\x5dc\x5dd! "
          u"\x5e9\x5dc\x5d5\x5dd \x5d4\x5e2\x5d5\x5dc\x5dd! "
          u"\x5e9\x5dc\x5d5\x5dd \x5d4\x5e2\x5d5\x5dc\x5dd! "
          u"\x5e9\x5dc\x5d5\x5dd \x5d4\x5e2\x5d5\x5dc\x5dd! "
          u"\x5e9\x5dc\x5d5\x5dd \x5d4\x5e2\x5d5\x5dc\x5dd! "
          u"\x5e9\x5dc\x5d5\x5dd \x5d4\x5e2\x5d5\x5dc\x5dd!");
      break;
  }
}

void TextExample::ElideComboboxChanged() {
  switch (eliding_cb_->GetSelectedIndex().value()) {
    case 0:
      text_view_->SetElide(gfx::ELIDE_TAIL);
      break;
    case 1:
      text_view_->SetElide(gfx::NO_ELIDE);
      break;
  }
}

void TextExample::PrefixComboboxChanged() {
  int flags = text_view_->GetFlags() &
              ~(gfx::Canvas::SHOW_PREFIX | gfx::Canvas::HIDE_PREFIX);
  switch (prefix_cb_->GetSelectedIndex().value()) {
    case 0:
      break;
    case 1:
      flags |= gfx::Canvas::SHOW_PREFIX;
      break;
    case 2:
      flags |= gfx::Canvas::HIDE_PREFIX;
      break;
  }
  text_view_->SetFlags(flags);
}

void TextExample::WeightComboboxChanged() {
  constexpr auto kFontWeights = std::to_array<gfx::Font::Weight>({
      gfx::Font::Weight::THIN,
      gfx::Font::Weight::EXTRA_LIGHT,
      gfx::Font::Weight::LIGHT,
      gfx::Font::Weight::NORMAL,
      gfx::Font::Weight::MEDIUM,
      gfx::Font::Weight::SEMIBOLD,
      gfx::Font::Weight::BOLD,
      gfx::Font::Weight::EXTRA_BOLD,
      gfx::Font::Weight::BLACK,
  });
  text_view_->SetWeight(kFontWeights[weight_cb_->GetSelectedIndex().value()]);
}

}  // namespace views::examples
