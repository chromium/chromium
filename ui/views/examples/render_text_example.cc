// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/render_text_example.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_span.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace views::examples {

namespace {

template <typename T>
struct NamedValue {
  const char* name;
  T value;
};

template <typename T>
class OptionComboboxModel : public ui::ComboboxModel {
 public:
  explicit OptionComboboxModel(base::span<const NamedValue<T>> options)
      : options_(options) {}

  OptionComboboxModel(const OptionComboboxModel&) = delete;
  OptionComboboxModel& operator=(const OptionComboboxModel&) = delete;

  ~OptionComboboxModel() override = default;

  // ui::ComboboxModel:
  size_t GetItemCount() const override { return options_.size(); }
  std::u16string GetItemAt(size_t index) const override {
    return base::ASCIIToUTF16(options_[index].name);
  }

 private:
  const base::raw_span<const NamedValue<T>> options_;
};

// The controls start at RenderText's defaults for a left-to-right UI.

constexpr auto kSamples = std::to_array<NamedValue<const char16_t*>>({
    {"Short", u"The quick brown fox jumps over the lazy dog."},
    {"Long",
     u"Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do "
     u"eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad "
     u"minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip "
     u"ex ea commodo consequat."},
    {"Email", u"firstname.lastname@subdomain.example.com"},
    {"RTL Hebrew",
     u"\u05e9\u05dc\u05d5\u05dd \u05d4\u05e2\u05d5\u05dc\u05dd! "
     u"\u05e9\u05dc\u05d5\u05dd \u05d4\u05e2\u05d5\u05dc\u05dd! "
     u"\u05e9\u05dc\u05d5\u05dd \u05d4\u05e2\u05d5\u05dc\u05dd!"},
    {"Mixed bidi",
     u"qwerty \u0627\u0644\u0631\u0626\u064a\u0633\u064a\u0629 asdfgh"},
    {"Multiple lines",
     u"First line\\nSecond line, longer than the first\\nThird line"},
});

constexpr auto kFonts = std::to_array<NamedValue<const char*>>({
    {"Default", nullptr},
    {"sans-serif", "sans-serif"},
    {"serif", "serif"},
    {"monospace", "monospace"},
    {"Roboto", "Roboto"},
    {"Noto Sans", "Noto Sans"},
    {"Noto Serif", "Noto Serif"},
});

constexpr auto kWeights = std::to_array<NamedValue<gfx::Font::Weight>>({
    {"Thin", gfx::Font::Weight::THIN},
    {"Extra Light", gfx::Font::Weight::EXTRA_LIGHT},
    {"Light", gfx::Font::Weight::LIGHT},
    {"Normal", gfx::Font::Weight::NORMAL},
    {"Medium", gfx::Font::Weight::MEDIUM},
    {"Semibold", gfx::Font::Weight::SEMIBOLD},
    {"Bold", gfx::Font::Weight::BOLD},
    {"Extra Bold", gfx::Font::Weight::EXTRA_BOLD},
    {"Black", gfx::Font::Weight::BLACK},
});
constexpr size_t kDefaultWeightIndex = 3;

constexpr auto kColors = std::to_array<NamedValue<std::optional<SkColor>>>({
    {"Default", std::nullopt},
    {"Red", gfx::kGoogleRed600},
    {"Green", gfx::kGoogleGreen600},
    {"Blue", gfx::kGoogleBlue600},
    {"Yellow", gfx::kGoogleYellow600},
});

constexpr auto kRangeColors =
    std::to_array<NamedValue<std::optional<SkColor>>>({
        {"None", std::nullopt},
        {"Red", gfx::kGoogleRed600},
        {"Green", gfx::kGoogleGreen600},
        {"Blue", gfx::kGoogleBlue600},
        {"Yellow", gfx::kGoogleYellow600},
    });

constexpr auto kElideBehaviors = std::to_array<NamedValue<gfx::ElideBehavior>>({
    {"No elide", gfx::NO_ELIDE},
    {"Truncate", gfx::TRUNCATE},
    {"Elide head", gfx::ELIDE_HEAD},
    {"Elide middle", gfx::ELIDE_MIDDLE},
    {"Elide tail", gfx::ELIDE_TAIL},
    {"Elide email", gfx::ELIDE_EMAIL},
    {"Fade tail", gfx::FADE_TAIL},
});

constexpr auto kWhitespaceElisions =
    std::to_array<NamedValue<std::optional<bool>>>({
        {"Default", std::nullopt},
        {"Elide", true},
        {"Keep", false},
    });

// gfx::ELIDE_LONG_WORDS is not listed because RenderText does not support it.
constexpr auto kWordWrapBehaviors =
    std::to_array<NamedValue<gfx::WordWrapBehavior>>({
        {"Ignore long words", gfx::IGNORE_LONG_WORDS},
        {"Truncate long words", gfx::TRUNCATE_LONG_WORDS},
        {"Wrap long words", gfx::WRAP_LONG_WORDS},
    });

constexpr auto kHorizontalAlignments =
    std::to_array<NamedValue<gfx::HorizontalAlignment>>({
        {"Left", gfx::ALIGN_LEFT},
        {"Center", gfx::ALIGN_CENTER},
        {"Right", gfx::ALIGN_RIGHT},
        {"To head", gfx::ALIGN_TO_HEAD},
    });

constexpr auto kVerticalAlignments =
    std::to_array<NamedValue<gfx::VerticalAlignment>>({
        {"Top", gfx::ALIGN_TOP},
        {"Middle", gfx::ALIGN_MIDDLE},
        {"Bottom", gfx::ALIGN_BOTTOM},
    });
constexpr size_t kDefaultVerticalAlignmentIndex = 1;

constexpr int kMaxFontSize = 200;
constexpr int kMaxMaxLines = 100;

// Spacing between rows of controls, and between controls in a row.
constexpr int kSpacing = 8;

// Initial height of the view that draws the text.
constexpr int kTextViewHeight = 160;

template <typename T, size_t N>
const T& GetSelected(const Combobox* combobox,
                     const std::array<NamedValue<T>, N>& options) {
  return options[combobox->GetSelectedIndex().value()].value;
}

// Returns the number in `field`, or `fallback` if it is not a number.
int GetInt(const Textfield* field, int fallback) {
  int value = 0;
  return base::StringToInt(field->GetText(), &value) ? value : fallback;
}

// Adds a horizontal row of controls to `container`.
View* AddRow(View* container) {
  auto* row = container->AddChildView(std::make_unique<View>());
  row
      ->SetLayoutManager(std::make_unique<BoxLayout>(
          BoxLayout::Orientation::kHorizontal, gfx::Insets(), kSpacing))
      ->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  return row;
}

template <typename T, size_t N>
Combobox* AddCombobox(View* row,
                      const std::u16string& name,
                      const std::array<NamedValue<T>, N>& options,
                      base::RepeatingClosure callback,
                      size_t selected_index = 0) {
  row->AddChildView(std::make_unique<Label>(name));
  auto* combobox = row->AddChildView(std::make_unique<Combobox>(
      std::make_unique<OptionComboboxModel<T>>(options)));
  combobox->SetSelectedIndex(selected_index);
  combobox->SetCallback(std::move(callback));
  combobox->GetViewAccessibility().SetName(name);
  return combobox;
}

Checkbox* AddCheckbox(View* row,
                      const std::u16string& name,
                      bool checked,
                      base::RepeatingClosure callback) {
  auto* checkbox =
      row->AddChildView(std::make_unique<Checkbox>(name, std::move(callback)));
  checkbox->SetChecked(checked);
  return checkbox;
}

Textfield* AddTextfield(
    View* row,
    const std::u16string& name,
    const std::u16string& text,
    int width_in_chars,
    base::RepeatingClosure callback,
    std::vector<base::CallbackListSubscription>& subscriptions) {
  row->AddChildView(std::make_unique<Label>(name));
  auto* textfield = row->AddChildView(std::make_unique<Textfield>());
  textfield->SetText(text);
  textfield->SetDefaultWidthInChars(width_in_chars);
  textfield->GetViewAccessibility().SetName(name);
  subscriptions.push_back(
      textfield->AddTextChangedCallback(std::move(callback)));
  return textfield;
}

}  // namespace

// A View that draws a RenderText and outlines its display rect.
class RenderTextExample::RenderTextView : public View {
  METADATA_HEADER(RenderTextView, View)

 public:
  // `update_callback` is run when the bounds or the theme of this view change.
  explicit RenderTextView(base::RepeatingClosure update_callback)
      : update_callback_(std::move(update_callback)) {
    SetBorder(CreateSolidBorder(1, ui::kColorFocusableBorderUnfocused));
  }

  RenderTextView(const RenderTextView&) = delete;
  RenderTextView& operator=(const RenderTextView&) = delete;

  ~RenderTextView() override = default;

  gfx::RenderText* render_text() { return render_text_.get(); }

  // View:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override {
    // The layout stretches this view; this only sets its initial height.
    return {0, kTextViewHeight};
  }

  void OnPaint(gfx::Canvas* canvas) override {
    View::OnPaint(canvas);
    render_text_->Draw(canvas);

    // The display rect can differ from the bounds of this view, so outline it.
    gfx::RectF outline(render_text_->display_rect());
    outline.Inset(0.5f);
    canvas->DrawRect(outline,
                     GetColorProvider()->GetColor(ui::kColorAlertHighSeverity));
  }

  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    update_callback_.Run();
  }

  void OnThemeChanged() override {
    View::OnThemeChanged();
    update_callback_.Run();
  }

 private:
  base::RepeatingClosure update_callback_;
  std::unique_ptr<gfx::RenderText> render_text_ =
      gfx::RenderText::CreateRenderText();
};

BEGIN_METADATA(RenderTextExample, RenderTextView)
END_METADATA

RenderTextExample::RenderTextExample()
    : ExampleBase(
          l10n_util::GetStringUTF8(IDS_RENDER_TEXT_SELECT_LABEL).c_str()) {}

RenderTextExample::~RenderTextExample() = default;

void RenderTextExample::CreateExampleView(View* container) {
  auto* layout = container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(kSpacing), kSpacing));

  const base::RepeatingClosure update = base::BindRepeating(
      &RenderTextExample::Update, weak_ptr_factory_.GetWeakPtr());
  const int default_font_size = gfx::FontList().GetFontSize();

  View* row = AddRow(container);
  sample_cb_ =
      AddCombobox(row, u"Sample", kSamples,
                  base::BindRepeating(&RenderTextExample::OnSampleChanged,
                                      weak_ptr_factory_.GetWeakPtr()));
  text_field_ = AddTextfield(row, u"Text (\\n is a newline)", kSamples[0].value,
                             60, update, subscriptions_);
  static_cast<BoxLayout*>(row->GetLayoutManager())
      ->SetFlexForView(text_field_, 1);

  row = AddRow(container);
  font_cb_ = AddCombobox(row, u"Font", kFonts, update);
  font_size_field_ =
      AddTextfield(row, u"Size", base::NumberToString16(default_font_size), 4,
                   update, subscriptions_);
  weight_cb_ =
      AddCombobox(row, u"Weight", kWeights, update, kDefaultWeightIndex);
  italic_checkbox_ = AddCheckbox(row, u"Italic", false, update);
  underline_checkbox_ = AddCheckbox(row, u"Underline", false, update);
  strike_checkbox_ = AddCheckbox(row, u"Strike", false, update);

  row = AddRow(container);
  color_cb_ = AddCombobox(row, u"Color", kColors, update);
  range_color_cb_ = AddCombobox(row, u"Range color", kRangeColors, update);
  color_range_start_field_ =
      AddTextfield(row, u"Start", u"4", 4, update, subscriptions_);
  color_range_end_field_ =
      AddTextfield(row, u"End", u"12", 4, update, subscriptions_);

  row = AddRow(container);
  elide_cb_ = AddCombobox(row, u"Elide", kElideBehaviors, update);
  whitespace_elision_cb_ =
      AddCombobox(row, u"Whitespace elision", kWhitespaceElisions, update);
  clip_checkbox_ = AddCheckbox(row, u"Clip to display rect", true, update);

  row = AddRow(container);
  multiline_checkbox_ = AddCheckbox(row, u"Multiline", false, update);
  max_lines_field_ =
      AddTextfield(row, u"Max lines", u"0", 4, update, subscriptions_);
  word_wrap_cb_ = AddCombobox(row, u"Word wrap", kWordWrapBehaviors, update);

  row = AddRow(container);
  h_align_cb_ =
      AddCombobox(row, u"Horizontal alignment", kHorizontalAlignments, update);
  v_align_cb_ = AddCombobox(row, u"Vertical alignment", kVerticalAlignments,
                            update, kDefaultVerticalAlignmentIndex);
  cursor_checkbox_ = AddCheckbox(row, u"Cursor enabled", true, update);

  row = AddRow(container);
  fit_checkbox_ =
      AddCheckbox(row, u"Display rect fills the view", true, update);
  rect_x_field_ =
      AddTextfield(row, u"Display rect X", u"20", 5, update, subscriptions_);
  rect_y_field_ = AddTextfield(row, u"Y", u"20", 5, update, subscriptions_);
  rect_width_field_ =
      AddTextfield(row, u"Width", u"300", 5, update, subscriptions_);
  rect_height_field_ =
      AddTextfield(row, u"Height", u"60", 5, update, subscriptions_);

  row = AddRow(container);
  offset_x_field_ =
      AddTextfield(row, u"Display offset X", u"0", 5, update, subscriptions_);
  offset_y_field_ = AddTextfield(row, u"Y (top vertical alignment only)", u"0",
                                 5, update, subscriptions_);

  status_label_ = container->AddChildView(std::make_unique<Label>());
  status_label_->SetMultiLine(true);
  status_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // This must be created last because it calls Update() as soon as it is
  // attached to a widget, and Update() reads every control.
  text_view_ =
      container->AddChildView(std::make_unique<RenderTextView>(update));
  layout->SetFlexForView(text_view_, 1);

  Update();
}

void RenderTextExample::OnSampleChanged() {
  // Changing the text runs Update().
  text_field_->SetText(GetSelected(sample_cb_, kSamples));
}

void RenderTextExample::Update() {
  // `text_view_` runs this while it is being added, which is before the
  // pointer to it is stored and may be before it has a widget. It runs this
  // again once it has a widget.
  if (!text_view_ || !text_view_->GetWidget()) {
    return;
  }
  gfx::RenderText* render_text = text_view_->render_text();

  // Text. SetText() clears colors and styles applied to ranges, so it comes
  // first.
  std::u16string text(text_field_->GetText());
  base::ReplaceSubstringsAfterOffset(&text, 0, u"\\n", u"\n");
  render_text->SetText(text);

  // Font. SetFontList() resets the weight and the styles, so they come after.
  const gfx::FontList default_font_list;
  const int font_size =
      std::clamp(GetInt(font_size_field_, default_font_list.GetFontSize()), 1,
                 kMaxFontSize);
  const char* font_name = GetSelected(font_cb_, kFonts);
  render_text->SetFontList(
      font_name ? gfx::FontList({font_name}, gfx::Font::NORMAL, font_size,
                                gfx::Font::Weight::NORMAL)
                : default_font_list.DeriveWithSizeDelta(
                      font_size - default_font_list.GetFontSize()));
  render_text->SetWeight(GetSelected(weight_cb_, kWeights));
  render_text->SetStyle(gfx::TEXT_STYLE_ITALIC, italic_checkbox_->GetChecked());
  render_text->SetStyle(gfx::TEXT_STYLE_UNDERLINE,
                        underline_checkbox_->GetChecked());
  render_text->SetStyle(gfx::TEXT_STYLE_STRIKE, strike_checkbox_->GetChecked());

  // Color.
  render_text->SetColor(GetSelected(color_cb_, kColors)
                            .value_or(text_view_->GetColorProvider()->GetColor(
                                ui::kColorLabelForeground)));
  const std::optional<SkColor> range_color =
      GetSelected(range_color_cb_, kRangeColors);
  color_range_start_field_->SetEnabled(range_color.has_value());
  color_range_end_field_->SetEnabled(range_color.has_value());
  const size_t range_start = std::min(
      static_cast<size_t>(std::max(GetInt(color_range_start_field_, 0), 0)),
      text.length());
  const size_t range_end = std::min(
      static_cast<size_t>(std::max(GetInt(color_range_end_field_, 0), 0)),
      text.length());
  if (range_color && range_start < range_end) {
    render_text->ApplyColor(*range_color, gfx::Range(range_start, range_end));
  }

  // Eliding and line breaking.
  render_text->SetElideBehavior(GetSelected(elide_cb_, kElideBehaviors));
  render_text->SetWhitespaceElision(
      GetSelected(whitespace_elision_cb_, kWhitespaceElisions));
  render_text->SetMultiline(multiline_checkbox_->GetChecked());
  render_text->SetMaxLines(
      std::clamp(GetInt(max_lines_field_, 0), 0, kMaxMaxLines));
  render_text->SetWordWrapBehavior(
      GetSelected(word_wrap_cb_, kWordWrapBehaviors));

  // Alignment, cursor and clipping.
  const gfx::VerticalAlignment vertical_alignment =
      GetSelected(v_align_cb_, kVerticalAlignments);
  render_text->SetHorizontalAlignment(
      GetSelected(h_align_cb_, kHorizontalAlignments));
  render_text->SetVerticalAlignment(vertical_alignment);
  render_text->SetCursorEnabled(cursor_checkbox_->GetChecked());
  render_text->set_clip_to_display_rect(clip_checkbox_->GetChecked());

  // Display rect.
  const bool fit = fit_checkbox_->GetChecked();
  rect_x_field_->SetEnabled(!fit);
  rect_y_field_->SetEnabled(!fit);
  rect_width_field_->SetEnabled(!fit);
  rect_height_field_->SetEnabled(!fit);
  const gfx::Rect display_rect =
      fit ? text_view_->GetContentsBounds()
          : gfx::Rect(GetInt(rect_x_field_, 0), GetInt(rect_y_field_, 0),
                      std::max(GetInt(rect_width_field_, 0), 0),
                      std::max(GetInt(rect_height_field_, 0), 0));
  render_text->SetDisplayRect(display_rect);

  // Display offset. This comes last because RenderText clamps the offset
  // using the text, the font, the alignment and the display rect. RenderText
  // only accepts a vertical offset when the text is aligned to the top.
  const bool has_vertical_offset = vertical_alignment == gfx::ALIGN_TOP;
  offset_y_field_->SetEnabled(has_vertical_offset);
  const gfx::Vector2d display_offset(
      GetInt(offset_x_field_, 0),
      has_vertical_offset ? GetInt(offset_y_field_, 0) : 0);
  render_text->SetDisplayOffset(display_offset);

  status_label_->SetText(base::UTF8ToUTF16(base::StringPrintf(
      "Display rect: %s    Display offset: requested %s, actual %s\n"
      "String size: %s    Content width: %d    Lines: %zu    "
      "Text length: %zu, displayed: %zu",
      display_rect.ToString().c_str(), display_offset.ToString().c_str(),
      render_text->GetUpdatedDisplayOffset().ToString().c_str(),
      render_text->GetStringSize().ToString().c_str(),
      render_text->GetContentWidth(), render_text->GetNumLines(), text.length(),
      render_text->GetDisplayText().length())));

  text_view_->SchedulePaint();
}

}  // namespace views::examples
