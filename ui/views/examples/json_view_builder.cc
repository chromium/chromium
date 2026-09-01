// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/json_view_builder.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/table_model.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/slider.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/examples/views_canvas_example.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace views::examples {

class JsonTableModel : public ui::TableModel {
 public:
  JsonTableModel(std::vector<ui::TableColumn> columns,
                 std::vector<std::vector<std::u16string>> rows)
      : columns_(std::move(columns)), rows_(std::move(rows)) {}
  ~JsonTableModel() override = default;

  size_t RowCount() const override { return rows_.size(); }

  std::u16string GetText(size_t row, int column_id) const override {
    if (row >= rows_.size()) {
      return u"";
    }
    for (size_t col_idx = 0; col_idx < columns_.size(); ++col_idx) {
      if (columns_[col_idx].id == column_id) {
        if (col_idx < rows_[row].size()) {
          return rows_[row][col_idx];
        }
        break;
      }
    }
    return u"";
  }

  void SetObserver(ui::TableModelObserver* observer) override {
    observer_ = observer;
  }

  int CompareValues(size_t row1, size_t row2, int column_id) const override {
    std::u16string text1 = GetText(row1, column_id);
    std::u16string text2 = GetText(row2, column_id);
    return text1.compare(text2);
  }

 private:
  std::vector<ui::TableColumn> columns_;
  std::vector<std::vector<std::u16string>> rows_;
  raw_ptr<ui::TableModelObserver> observer_ = nullptr;
};

}  // namespace views::examples

DEFINE_UI_CLASS_PROPERTY_TYPE(views::examples::JsonTableModel*)

DEFINE_ENUM_CONVERTERS(
    views::InsetsMetric,
    {views::InsetsMetric::INSETS_CHECKBOX_RADIO_BUTTON,
     u"INSETS_CHECKBOX_RADIO_BUTTON"},
    {views::InsetsMetric::INSETS_DIALOG, u"INSETS_DIALOG"},
    {views::InsetsMetric::INSETS_DIALOG_BUTTON_ROW,
     u"INSETS_DIALOG_BUTTON_ROW"},
    {views::InsetsMetric::INSETS_DIALOG_SUBSECTION,
     u"INSETS_DIALOG_SUBSECTION"},
    {views::InsetsMetric::INSETS_DIALOG_TITLE, u"INSETS_DIALOG_TITLE"},
    {views::InsetsMetric::INSETS_DIALOG_FOOTNOTE, u"INSETS_DIALOG_FOOTNOTE"},
    {views::InsetsMetric::INSETS_TOOLTIP_BUBBLE, u"INSETS_TOOLTIP_BUBBLE"},
    {views::InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON,
     u"INSETS_VECTOR_IMAGE_BUTTON"},
    {views::InsetsMetric::INSETS_LABEL_BUTTON, u"INSETS_LABEL_BUTTON"},
    {views::InsetsMetric::INSETS_ICON_BUTTON, u"INSETS_ICON_BUTTON"})

DEFINE_ENUM_CONVERTERS(
    views::DistanceMetric,
    {views::DistanceMetric::DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE,
     u"DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE"},
    {views::DistanceMetric::DISTANCE_BUBBLE_PREFERRED_WIDTH,
     u"DISTANCE_BUBBLE_PREFERRED_WIDTH"},
    {views::DistanceMetric::DISTANCE_BUTTON_HORIZONTAL_PADDING,
     u"DISTANCE_BUTTON_HORIZONTAL_PADDING"},
    {views::DistanceMetric::DISTANCE_BUTTON_MAX_LINKABLE_WIDTH,
     u"DISTANCE_BUTTON_MAX_LINKABLE_WIDTH"},
    {views::DistanceMetric::DISTANCE_CLOSE_BUTTON_MARGIN,
     u"DISTANCE_CLOSE_BUTTON_MARGIN"},
    {views::DistanceMetric::DISTANCE_CONTROL_LIST_VERTICAL,
     u"DISTANCE_CONTROL_LIST_VERTICAL"},
    {views::DistanceMetric::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING,
     u"DISTANCE_CONTROL_VERTICAL_TEXT_PADDING"},
    {views::DistanceMetric::DISTANCE_TABLE_VERTICAL_TEXT_PADDING,
     u"DISTANCE_TABLE_VERTICAL_TEXT_PADDING"},
    {views::DistanceMetric::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH,
     u"DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH"},
    {views::DistanceMetric::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL,
     u"DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL"},
    {views::DistanceMetric::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT,
     u"DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT"},
    {views::DistanceMetric::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL,
     u"DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL"},
    {views::DistanceMetric::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT,
     u"DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT"},
    {views::DistanceMetric::DISTANCE_DROPDOWN_BUTTON_LABEL_ARROW_SPACING,
     u"DISTANCE_DROPDOWN_BUTTON_LABEL_ARROW_SPACING"},
    {views::DistanceMetric::DISTANCE_DROPDOWN_BUTTON_RIGHT_MARGIN,
     u"DISTANCE_DROPDOWN_BUTTON_RIGHT_MARGIN"},
    {views::DistanceMetric::DISTANCE_DROPDOWN_BUTTON_LEFT_MARGIN,
     u"DISTANCE_DROPDOWN_BUTTON_LEFT_MARGIN"},
    {views::DistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH,
     u"DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH"},
    {views::DistanceMetric::DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH,
     u"DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH"},
    {views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL,
     u"DISTANCE_RELATED_BUTTON_HORIZONTAL"},
    {views::DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL,
     u"DISTANCE_RELATED_CONTROL_HORIZONTAL"},
    {views::DistanceMetric::DISTANCE_RELATED_CONTROL_VERTICAL,
     u"DISTANCE_RELATED_CONTROL_VERTICAL"},
    {views::DistanceMetric::DISTANCE_RELATED_LABEL_HORIZONTAL,
     u"DISTANCE_RELATED_LABEL_HORIZONTAL"},
    {views::DistanceMetric::DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT,
     u"DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT"},
    {views::DistanceMetric::DISTANCE_MODAL_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT,
     u"DISTANCE_MODAL_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT"},
    {views::DistanceMetric::DISTANCE_TABLE_CELL_HORIZONTAL_MARGIN,
     u"DISTANCE_TABLE_CELL_HORIZONTAL_MARGIN"},
    {views::DistanceMetric::DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING,
     u"DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING"},
    {views::DistanceMetric::DISTANCE_UNRELATED_CONTROL_HORIZONTAL,
     u"DISTANCE_UNRELATED_CONTROL_HORIZONTAL"},
    {views::DistanceMetric::DISTANCE_UNRELATED_INFOBAR_CONTAINER_HORIZONTAL,
     u"DISTANCE_UNRELATED_INFOBAR_CONTAINER_HORIZONTAL"},
    {views::DistanceMetric::DISTANCE_UNRELATED_CONTROL_VERTICAL,
     u"DISTANCE_UNRELATED_CONTROL_VERTICAL"},
    {views::DistanceMetric::DISTANCE_VECTOR_ICON_PADDING,
     u"DISTANCE_VECTOR_ICON_PADDING"})

DEFINE_ENUM_CONVERTERS(
    views::style::TextContext,
    {views::style::TextContext::CONTEXT_BADGE, u"CONTEXT_BADGE"},
    {views::style::TextContext::CONTEXT_BUBBLE_FOOTER,
     u"CONTEXT_BUBBLE_FOOTER"},
    {views::style::TextContext::CONTEXT_BUTTON, u"CONTEXT_BUTTON"},
    {views::style::TextContext::CONTEXT_BUTTON_MD, u"CONTEXT_BUTTON_MD"},
    {views::style::TextContext::CONTEXT_DIALOG_TITLE, u"CONTEXT_DIALOG_TITLE"},
    {views::style::TextContext::CONTEXT_LABEL, u"CONTEXT_LABEL"},
    {views::style::TextContext::CONTEXT_DIALOG_BODY_TEXT,
     u"CONTEXT_DIALOG_BODY_TEXT"},
    {views::style::TextContext::CONTEXT_TABLE_ROW, u"CONTEXT_TABLE_ROW"},
    {views::style::TextContext::CONTEXT_TEXTFIELD, u"CONTEXT_TEXTFIELD"},
    {views::style::TextContext::CONTEXT_TEXTFIELD_PLACEHOLDER,
     u"CONTEXT_TEXTFIELD_PLACEHOLDER"},
    {views::style::TextContext::CONTEXT_TEXTFIELD_SUPPORTING_TEXT,
     u"CONTEXT_TEXTFIELD_SUPPORTING_TEXT"},
    {views::style::TextContext::CONTEXT_MENU, u"CONTEXT_MENU"},
    {views::style::TextContext::CONTEXT_TOUCH_MENU, u"CONTEXT_TOUCH_MENU"})

DEFINE_ENUM_CONVERTERS(
    views::style::TextStyle,
    {views::style::TextStyle::STYLE_PRIMARY, u"STYLE_PRIMARY"},
    {views::style::TextStyle::STYLE_SECONDARY, u"STYLE_SECONDARY"},
    {views::style::TextStyle::STYLE_HINT, u"STYLE_HINT"},
    {views::style::TextStyle::STYLE_SELECTED, u"STYLE_SELECTED"},
    {views::style::TextStyle::STYLE_HIGHLIGHTED, u"STYLE_HIGHLIGHTED"},
    {views::style::TextStyle::STYLE_DIALOG_BUTTON_DEFAULT,
     u"STYLE_DIALOG_BUTTON_DEFAULT"},
    {views::style::TextStyle::STYLE_DIALOG_BUTTON_TONAL,
     u"STYLE_DIALOG_BUTTON_TONAL"},
    {views::style::TextStyle::STYLE_DISABLED, u"STYLE_DISABLED"},
    {views::style::TextStyle::STYLE_EMPHASIZED, u"STYLE_EMPHASIZED"},
    {views::style::TextStyle::STYLE_EMPHASIZED_SECONDARY,
     u"STYLE_EMPHASIZED_SECONDARY"},
    {views::style::TextStyle::STYLE_INVALID, u"STYLE_INVALID"},
    {views::style::TextStyle::STYLE_LINK, u"STYLE_LINK"},
    {views::style::TextStyle::STYLE_TAB_ACTIVE, u"STYLE_TAB_ACTIVE"},
    {views::style::TextStyle::STYLE_PRIMARY_MONOSPACED,
     u"STYLE_PRIMARY_MONOSPACED"},
    {views::style::TextStyle::STYLE_SECONDARY_MONOSPACED,
     u"STYLE_SECONDARY_MONOSPACED"},
    {views::style::TextStyle::STYLE_HEADLINE_1, u"STYLE_HEADLINE_1"},
    {views::style::TextStyle::STYLE_HEADLINE_2, u"STYLE_HEADLINE_2"},
    {views::style::TextStyle::STYLE_HEADLINE_3, u"STYLE_HEADLINE_3"},
    {views::style::TextStyle::STYLE_HEADLINE_4, u"STYLE_HEADLINE_4"},
    {views::style::TextStyle::STYLE_HEADLINE_4_BOLD, u"STYLE_HEADLINE_4_BOLD"},
    {views::style::TextStyle::STYLE_HEADLINE_5, u"STYLE_HEADLINE_5"},
    {views::style::TextStyle::STYLE_BODY_1, u"STYLE_BODY_1"},
    {views::style::TextStyle::STYLE_BODY_1_EMPHASIS, u"STYLE_BODY_1_EMPHASIS"},
    {views::style::TextStyle::STYLE_BODY_1_BOLD, u"STYLE_BODY_1_BOLD"},
    {views::style::TextStyle::STYLE_BODY_2, u"STYLE_BODY_2"},
    {views::style::TextStyle::STYLE_BODY_2_EMPHASIS, u"STYLE_BODY_2_EMPHASIS"},
    {views::style::TextStyle::STYLE_BODY_2_BOLD, u"STYLE_BODY_2_BOLD"},
    {views::style::TextStyle::STYLE_BODY_3, u"STYLE_BODY_3"},
    {views::style::TextStyle::STYLE_BODY_3_EMPHASIS, u"STYLE_BODY_3_EMPHASIS"},
    {views::style::TextStyle::STYLE_BODY_3_BOLD, u"STYLE_BODY_3_BOLD"},
    {views::style::TextStyle::STYLE_BODY_4, u"STYLE_BODY_4"},
    {views::style::TextStyle::STYLE_BODY_4_EMPHASIS, u"STYLE_BODY_4_EMPHASIS"},
    {views::style::TextStyle::STYLE_BODY_4_BOLD, u"STYLE_BODY_4_BOLD"},
    {views::style::TextStyle::STYLE_BODY_5, u"STYLE_BODY_5"},
    {views::style::TextStyle::STYLE_BODY_5_EMPHASIS, u"STYLE_BODY_5_EMPHASIS"},
    {views::style::TextStyle::STYLE_BODY_5_BOLD, u"STYLE_BODY_5_BOLD"},
    {views::style::TextStyle::STYLE_CAPTION, u"STYLE_CAPTION"},
    {views::style::TextStyle::STYLE_CAPTION_EMPHASIS,
     u"STYLE_CAPTION_EMPHASIS"},
    {views::style::TextStyle::STYLE_CAPTION_BOLD, u"STYLE_CAPTION_BOLD"},
    {views::style::TextStyle::STYLE_LINK_2, u"STYLE_LINK_2"},
    {views::style::TextStyle::STYLE_LINK_3, u"STYLE_LINK_3"},
    {views::style::TextStyle::STYLE_LINK_4, u"STYLE_LINK_4"},
    {views::style::TextStyle::STYLE_LINK_5, u"STYLE_LINK_5"})

namespace views::examples {

namespace {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(JsonTableModel, kJsonTableModelKey)

std::optional<SkColor> ParseColor(const std::u16string& str,
                                  const views::View* view) {
  std::string str_utf8 = base::UTF16ToUTF8(str);
  if (base::StartsWith(str_utf8,
                       "ColorId:", base::CompareCase::INSENSITIVE_ASCII)) {
    std::string color_name = str_utf8.substr(8);
    std::optional<ui::ColorId> color_id = ui::NameToColorId(color_name);
    if (color_id) {
      const ui::ColorProvider* provider =
          (view && view->GetWidget())
              ? view->GetColorProvider()
              : ui::ColorProviderManager::Get().GetColorProviderFor(
                    ui::ColorProviderKey());
      if (provider) {
        return provider->GetColor(*color_id);
      }
    }
    return std::nullopt;
  }

  if (str == u"red") {
    return SK_ColorRED;
  }
  if (str == u"green") {
    return SK_ColorGREEN;
  }
  if (str == u"blue") {
    return SK_ColorBLUE;
  }
  if (str == u"white") {
    return SK_ColorWHITE;
  }
  if (str == u"black") {
    return SK_ColorBLACK;
  }
  if (str == u"gray") {
    return SK_ColorGRAY;
  }
  if (str == u"lightgray") {
    return SK_ColorLTGRAY;
  }
  if (str == u"darkgray") {
    return SK_ColorDKGRAY;
  }
  if (str == u"cyan") {
    return SK_ColorCYAN;
  }
  if (str == u"magenta") {
    return SK_ColorMAGENTA;
  }
  if (str == u"yellow") {
    return SK_ColorYELLOW;
  }
  if (str == u"transparent") {
    return SK_ColorTRANSPARENT;
  }
  return ui::metadata::SkColorConverter::FromString(str);
}

std::u16string MapTextStyleOrContext(const std::u16string& str) {
  if (auto style =
          ui::metadata::TypeConverter<views::style::TextStyle>::FromString(
              str)) {
    return base::ASCIIToUTF16(base::NumberToString(static_cast<int>(*style)));
  }
  if (auto context =
          ui::metadata::TypeConverter<views::style::TextContext>::FromString(
              str)) {
    return base::ASCIIToUTF16(base::NumberToString(static_cast<int>(*context)));
  }
  return str;
}

std::u16string JsonValueToU16String(const base::Value& val) {
  if (val.is_string()) {
    return base::UTF8ToUTF16(val.GetString());
  }
  if (val.is_bool()) {
    return val.GetBool() ? u"true" : u"false";
  }
  if (val.is_int()) {
    return base::ASCIIToUTF16(base::NumberToString(val.GetInt()));
  }
  if (val.is_double()) {
    return base::ASCIIToUTF16(base::NumberToString(val.GetDouble()));
  }
  return u"";
}

// Case-insensitive lookup of properties in ClassMetaData.
ui::metadata::MemberMetaDataBase* FindMemberDataCaseInsensitive(
    ui::metadata::ClassMetaData* class_meta,
    const std::string& property_name) {
  std::string lower_name = base::ToLowerASCII(property_name);
  for (ui::metadata::ClassMetaData* current = class_meta; current;
       current = current->parent_class_meta_data()) {
    for (ui::metadata::MemberMetaDataBase* member : current->members()) {
      if (base::EqualsCaseInsensitiveASCII(member->member_name(), lower_name)) {
        return member;
      }
    }
  }
  return nullptr;
}

enum class PropertyType {
  kUnknown,
  kColor,
  kInsets,
  kInt,
  kString,
  kCompound,
};

class DynamicProperty;

bool ResolveValue(views::View* view,
                  std::string_view key,
                  PropertyType target_type,
                  const std::u16string& value_str,
                  std::u16string* resolved_value,
                  std::string* error_msg);

// Abstract base class representing dynamic property accessor.
class DynamicProperty {
 public:
  virtual ~DynamicProperty() = default;
  virtual const std::string_view name() const = 0;
  virtual PropertyType GetType() const = 0;
  virtual bool SetValue(views::View* view,
                        const std::u16string& value_str,
                        std::string* error_msg) = 0;
};

// Custom Background property wrapper.
class BackgroundDynamicProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "background";
    return name;
  }

  PropertyType GetType() const override { return PropertyType::kCompound; }

  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    std::string value_utf8 = base::UTF16ToUTF8(value_str);
    std::vector<std::string> parts = base::SplitString(
        value_utf8, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.empty()) {
      *error_msg = "Invalid background format. Expected 'solid,color'";
      return false;
    }

    if (parts[0] == "solid") {
      if (parts.size() < 2) {
        *error_msg = "Solid background requires a color";
        return false;
      }
      std::u16string color_resolved;
      if (!ResolveValue(view, "background.color", PropertyType::kColor,
                        base::UTF8ToUTF16(parts[1]), &color_resolved,
                        error_msg)) {
        return false;
      }
      std::optional<SkColor> color = ParseColor(color_resolved, view);
      if (!color) {
        *error_msg = "Failed to parse color: " + parts[1];
        return false;
      }
      view->SetBackground(views::CreateSolidBackground(*color));
      return true;
    }

    *error_msg = "Unsupported background type: " + parts[0];
    return false;
  }
};

// Custom Border property wrapper.
class BorderDynamicProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "border";
    return name;
  }

  PropertyType GetType() const override { return PropertyType::kCompound; }

  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    std::string value_utf8 = base::UTF16ToUTF8(value_str);
    std::vector<std::string> parts = base::SplitString(
        value_utf8, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.empty()) {
      *error_msg = "Invalid border format. Expected 'solid,...' or 'empty,...'";
      return false;
    }

    if (parts[0] == "solid") {
      if (parts.size() < 3) {
        *error_msg =
            "Solid border requires thickness and color: solid,thickness,color";
        return false;
      }
      std::u16string thickness_resolved;
      if (!ResolveValue(view, "border.thickness", PropertyType::kInt,
                        base::UTF8ToUTF16(parts[1]), &thickness_resolved,
                        error_msg)) {
        return false;
      }
      int thickness = 0;
      if (!base::StringToInt(thickness_resolved, &thickness)) {
        *error_msg = "Invalid thickness: " + parts[1];
        return false;
      }
      std::u16string color_resolved;
      if (!ResolveValue(view, "border.color", PropertyType::kColor,
                        base::UTF8ToUTF16(parts[2]), &color_resolved,
                        error_msg)) {
        return false;
      }
      std::optional<SkColor> color = ParseColor(color_resolved, view);
      if (!color) {
        *error_msg = "Failed to parse color: " + parts[2];
        return false;
      }
      view->SetBorder(views::CreateSolidBorder(thickness, *color));
      return true;
    } else if (parts[0] == "empty") {
      if (parts.size() == 2) {
        std::u16string resolved;
        if (base::StartsWith(parts[1], "InsetsMetric:",
                             base::CompareCase::INSENSITIVE_ASCII)) {
          if (!ResolveValue(view, "border.insets", PropertyType::kInsets,
                            base::UTF8ToUTF16(parts[1]), &resolved,
                            error_msg)) {
            return false;
          }
          std::vector<std::string> inset_parts = base::SplitString(
              base::UTF16ToUTF8(resolved), ",", base::TRIM_WHITESPACE,
              base::SPLIT_WANT_NONEMPTY);
          if (inset_parts.size() < 4) {
            *error_msg =
                "Failed to resolve InsetsMetric for border: " + parts[1];
            return false;
          }
          int top, left, bottom, right;
          base::StringToInt(inset_parts[0], &top);
          base::StringToInt(inset_parts[1], &left);
          base::StringToInt(inset_parts[2], &bottom);
          base::StringToInt(inset_parts[3], &right);
          view->SetBorder(views::CreateEmptyBorder(
              gfx::Insets::TLBR(top, left, bottom, right)));
          return true;
        } else {
          if (!ResolveValue(view, "border.thickness", PropertyType::kInt,
                            base::UTF8ToUTF16(parts[1]), &resolved,
                            error_msg)) {
            return false;
          }
          int thickness = 0;
          if (!base::StringToInt(resolved, &thickness)) {
            *error_msg = "Invalid thickness: " + parts[1];
            return false;
          }
          view->SetBorder(views::CreateEmptyBorder(thickness));
          return true;
        }
      } else if (parts.size() >= 5) {
        std::array<int, 4> vals;
        for (size_t i = 0; i < 4; i++) {
          std::u16string resolved;
          if (!ResolveValue(view, "border.insets.part", PropertyType::kInt,
                            base::UTF8ToUTF16(parts[i + 1]), &resolved,
                            error_msg)) {
            return false;
          }
          if (!base::StringToInt(resolved, &vals[i])) {
            *error_msg = "Invalid insets part: " + parts[i + 1];
            return false;
          }
        }
        view->SetBorder(views::CreateEmptyBorder(
            gfx::Insets::TLBR(vals[0], vals[1], vals[2], vals[3])));
        return true;
      } else {
        *error_msg =
            "Empty border requires 1 or 4 values: empty,thickness or "
            "empty,top,left,bottom,right";
        return false;
      }
    }

    *error_msg = "Unsupported border type: " + parts[0];
    return false;
  }
};

// Custom layout_flex property wrapper.
class LayoutFlexDynamicProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "layout_flex";
    return name;
  }

  PropertyType GetType() const override { return PropertyType::kInt; }

  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    int weight = 0;
    if (!base::StringToInt(value_str, &weight)) {
      *error_msg = "Invalid flex weight: " + base::UTF16ToUTF8(value_str);
      return false;
    }

    views::View* parent = view->parent();
    if (!parent) {
      *error_msg = "Cannot set layout_flex on a view with no parent";
      return false;
    }

    if (views::IsViewClass<views::BoxLayoutView>(parent)) {
      views::AsViewClass<views::BoxLayoutView>(parent)->SetFlexForView(view,
                                                                       weight);
      return true;
    } else if (views::IsViewClass<views::FlexLayoutView>(parent)) {
      view->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded)
              .WithWeight(weight));
      return true;
    }

    *error_msg = "Parent layout does not support flex weight: " +
                 std::string(parent->GetClassName());
    return false;
  }
};

// Custom AccessibleName property wrapper.
class AccessibleNameDynamicProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "accessiblename";
    return name;
  }

  PropertyType GetType() const override { return PropertyType::kString; }

  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    view->GetViewAccessibility().SetName(value_str);
    return true;
  }
};

// Metadata property wrapper.
class MetadataDynamicProperty : public DynamicProperty {
 public:
  explicit MetadataDynamicProperty(
      ui::metadata::MemberMetaDataBase* member_meta)
      : member_meta_(member_meta), name_(member_meta->member_name()) {}

  const std::string_view name() const override { return name_; }

  PropertyType GetType() const override {
    std::string type = member_meta_->member_type();
    if (type == "SkColor") {
      return PropertyType::kColor;
    }
    if (type == "gfx::Insets") {
      return PropertyType::kInsets;
    }
    if (type == "int" || type == "bool") {
      return PropertyType::kInt;
    }
    if (type == "std::u16string" || type == "std::string") {
      return PropertyType::kString;
    }
    return PropertyType::kUnknown;
  }

  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    if ((member_meta_->GetPropertyFlags() &
         ui::metadata::PropertyFlags::kReadOnly) !=
        ui::metadata::PropertyFlags::kEmpty) {
      *error_msg = "Property is read-only: " + name_;
      return false;
    }
    member_meta_->SetValueAsString(view, value_str);
    return true;
  }

 private:
  raw_ptr<ui::metadata::MemberMetaDataBase> member_meta_;
  std::string name_;
};

// Custom layout_manager property wrapper.
class LayoutManagerDynamicProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "layout_manager";
    return name;
  }

  PropertyType GetType() const override { return PropertyType::kCompound; }

  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    std::string value_utf8 = base::UTF16ToUTF8(value_str);
    if (value_utf8 == "FlexLayout") {
      view->SetLayoutManager(std::make_unique<views::FlexLayout>());
      return true;
    } else if (value_utf8 == "TableLayout") {
      view->SetLayoutManager(std::make_unique<views::TableLayout>());
      return true;
    } else if (value_utf8 == "BoxLayout") {
      view->SetLayoutManager(std::make_unique<views::BoxLayout>());
      return true;
    }
    *error_msg = "Unsupported layout manager type: " + value_utf8;
    return false;
  }
};

class TokenResolver {
 public:
  virtual ~TokenResolver() = default;

  virtual bool Matches(std::string_view value) const = 0;
  virtual PropertyType GetSupportedType() const = 0;

  virtual bool Resolve(views::View* view,
                       std::string_view value,
                       std::u16string* resolved_value,
                       std::string* error_msg) const = 0;
};

class ColorIdResolver : public TokenResolver {
 public:
  bool Matches(std::string_view value) const override {
    return base::StartsWith(value,
                            "ColorId:", base::CompareCase::INSENSITIVE_ASCII);
  }

  PropertyType GetSupportedType() const override {
    return PropertyType::kColor;
  }

  bool Resolve(views::View* view,
               std::string_view value,
               std::u16string* resolved_value,
               std::string* error_msg) const override {
    std::u16string value_u16 = base::UTF8ToUTF16(value);
    std::optional<SkColor> color = ParseColor(value_u16, view);
    if (!color) {
      *error_msg = "Failed to resolve ColorId: " + std::string(value);
      return false;
    }
    *resolved_value = base::ASCIIToUTF16(base::StringPrintf("0x%08X", *color));
    return true;
  }
};

class InsetsMetricResolver : public TokenResolver {
 public:
  bool Matches(std::string_view value) const override {
    return base::StartsWith(
        value, "InsetsMetric:", base::CompareCase::INSENSITIVE_ASCII);
  }

  PropertyType GetSupportedType() const override {
    return PropertyType::kInsets;
  }

  bool Resolve(views::View* view,
               std::string_view value,
               std::u16string* resolved_value,
               std::string* error_msg) const override {
    std::optional<views::InsetsMetric> metric =
        ui::metadata::TypeConverter<views::InsetsMetric>::FromString(
            base::UTF8ToUTF16(value.substr(13)));
    if (!metric) {
      *error_msg = "Failed to resolve InsetsMetric: " + std::string(value);
      return false;
    }
    gfx::Insets insets = views::LayoutProvider::Get()->GetInsetsMetric(*metric);
    *resolved_value = base::ASCIIToUTF16(
        base::StringPrintf("%d,%d,%d,%d", insets.top(), insets.left(),
                           insets.bottom(), insets.right()));
    return true;
  }
};

class DistanceMetricResolver : public TokenResolver {
 public:
  bool Matches(std::string_view value) const override {
    return base::StartsWith(
        value, "DistanceMetric:", base::CompareCase::INSENSITIVE_ASCII);
  }

  PropertyType GetSupportedType() const override { return PropertyType::kInt; }

  bool Resolve(views::View* view,
               std::string_view value,
               std::u16string* resolved_value,
               std::string* error_msg) const override {
    std::optional<views::DistanceMetric> metric =
        ui::metadata::TypeConverter<views::DistanceMetric>::FromString(
            base::UTF8ToUTF16(value.substr(15)));
    if (!metric) {
      *error_msg = "Failed to resolve DistanceMetric: " + std::string(value);
      return false;
    }
    int distance = views::LayoutProvider::Get()->GetDistanceMetric(*metric);
    *resolved_value = base::ASCIIToUTF16(base::NumberToString(distance));
    return true;
  }
};

class TextStyleResolver : public TokenResolver {
 public:
  bool Matches(std::string_view value) const override {
    return base::StartsWith(value, "STYLE_",
                            base::CompareCase::INSENSITIVE_ASCII) ||
           base::StartsWith(value, "CONTEXT_",
                            base::CompareCase::INSENSITIVE_ASCII) ||
           base::StartsWith(
               value, "TextStyle:", base::CompareCase::INSENSITIVE_ASCII) ||
           base::StartsWith(
               value, "TextContext:", base::CompareCase::INSENSITIVE_ASCII);
  }

  PropertyType GetSupportedType() const override { return PropertyType::kInt; }

  bool Resolve(views::View* view,
               std::string_view value,
               std::u16string* resolved_value,
               std::string* error_msg) const override {
    std::string_view token = value;
    if (base::StartsWith(token,
                         "TextStyle:", base::CompareCase::INSENSITIVE_ASCII)) {
      token = token.substr(10);
    } else if (base::StartsWith(token, "TextContext:",
                                base::CompareCase::INSENSITIVE_ASCII)) {
      token = token.substr(12);
    }
    std::u16string mapped = MapTextStyleOrContext(base::UTF8ToUTF16(token));
    if (mapped == base::UTF8ToUTF16(token)) {
      *error_msg = "Failed to resolve typography token: " + std::string(value);
      return false;
    }
    *resolved_value = mapped;
    return true;
  }
};

std::vector<std::unique_ptr<TokenResolver>> GetTokenResolvers() {
  std::vector<std::unique_ptr<TokenResolver>> resolvers;
  resolvers.push_back(std::make_unique<ColorIdResolver>());
  resolvers.push_back(std::make_unique<InsetsMetricResolver>());
  resolvers.push_back(std::make_unique<DistanceMetricResolver>());
  resolvers.push_back(std::make_unique<TextStyleResolver>());
  return resolvers;
}

bool ResolveValue(views::View* view,
                  std::string_view key,
                  PropertyType target_type,
                  const std::u16string& value_str,
                  std::u16string* resolved_value,
                  std::string* error_msg) {
  std::string value_utf8 = base::UTF16ToUTF8(value_str);

  if (target_type == PropertyType::kCompound) {
    *resolved_value = value_str;
    return true;
  }

  const auto resolvers = GetTokenResolvers();
  for (const auto& resolver : resolvers) {
    if (resolver->Matches(value_utf8)) {
      if (resolver->GetSupportedType() != target_type) {
        *error_msg = base::StringPrintf(
            "Property '%s' expects type %d, but received incompatible token "
            "'%s'",
            std::string(key).c_str(), static_cast<int>(target_type),
            value_utf8.c_str());
        return false;
      }
      return resolver->Resolve(view, value_utf8, resolved_value, error_msg);
    }
  }

  *resolved_value = value_str;
  return true;
}

struct TableColumnParams {
  bool is_padding = false;
  views::LayoutAlignment h_align = views::LayoutAlignment::kStretch;
  views::LayoutAlignment v_align = views::LayoutAlignment::kStretch;
  float horizontal_resize = views::TableLayout::kFixedSize;
  views::TableLayout::ColumnSize size_type =
      views::TableLayout::ColumnSize::kUsePreferred;
  int fixed_width = 0;
  int min_width = 0;
  int width = 0;
};

using ColumnPropertyHandler = void (*)(TableColumnParams&, const base::Value&);

struct TableRowParams {
  bool is_padding = false;
  float vertical_resize = views::TableLayout::kFixedSize;
  int height = 0;
};

using RowPropertyHandler = void (*)(TableRowParams&, const base::Value&);

views::LayoutAlignment ParseLayoutAlignment(const base::Value& val) {
  if (val.is_string()) {
    const std::string& s = val.GetString();
    if (s == "kStart") {
      return views::LayoutAlignment::kStart;
    }
    if (s == "kCenter") {
      return views::LayoutAlignment::kCenter;
    }
    if (s == "kEnd") {
      return views::LayoutAlignment::kEnd;
    }
  }
  return views::LayoutAlignment::kStretch;
}

// Column handlers
void HandleColPadding(TableColumnParams& p, const base::Value& v) {
  p.is_padding = v.GetIfBool().value_or(false);
}
void HandleColHAlign(TableColumnParams& p, const base::Value& v) {
  p.h_align = ParseLayoutAlignment(v);
}
void HandleColVAlign(TableColumnParams& p, const base::Value& v) {
  p.v_align = ParseLayoutAlignment(v);
}
void HandleColHResize(TableColumnParams& p, const base::Value& v) {
  p.horizontal_resize =
      v.GetIfDouble().value_or(views::TableLayout::kFixedSize);
}
void HandleColSizeType(TableColumnParams& p, const base::Value& v) {
  if (v.is_string() && v.GetString() == "kFixed") {
    p.size_type = views::TableLayout::ColumnSize::kFixed;
  }
}
void HandleColFixedWidth(TableColumnParams& p, const base::Value& v) {
  p.fixed_width = v.GetIfInt().value_or(0);
}
void HandleColMinWidth(TableColumnParams& p, const base::Value& v) {
  p.min_width = v.GetIfInt().value_or(0);
}
void HandleColWidth(TableColumnParams& p, const base::Value& v) {
  p.width = v.GetIfInt().value_or(0);
}

// Row handlers
void HandleRowPadding(TableRowParams& p, const base::Value& v) {
  p.is_padding = v.GetIfBool().value_or(false);
}
void HandleRowVResize(TableRowParams& p, const base::Value& v) {
  p.vertical_resize = v.GetIfDouble().value_or(views::TableLayout::kFixedSize);
}
void HandleRowHeight(TableRowParams& p, const base::Value& v) {
  p.height = v.GetIfInt().value_or(0);
}

void ProcessTableColumn(views::TableLayout* layout,
                        const base::DictValue& col_dict) {
  static const auto kColumnHandlers =
      base::MakeFixedFlatMap<std::string_view, ColumnPropertyHandler>({
          {"is_padding", &HandleColPadding},
          {"h_align", &HandleColHAlign},
          {"v_align", &HandleColVAlign},
          {"horizontal_resize", &HandleColHResize},
          {"size_type", &HandleColSizeType},
          {"fixed_width", &HandleColFixedWidth},
          {"min_width", &HandleColMinWidth},
          {"width", &HandleColWidth},
      });

  TableColumnParams params;
  for (auto&& [key, val] : col_dict) {
    auto it = kColumnHandlers.find(key);
    if (it != kColumnHandlers.end()) {
      it->second(params, val);
    }
  }

  if (params.is_padding) {
    layout->AddPaddingColumn(params.horizontal_resize, params.width);
  } else {
    layout->AddColumn(params.h_align, params.v_align, params.horizontal_resize,
                      params.size_type, params.fixed_width, params.min_width);
  }
}

void ProcessTableRow(views::TableLayout* layout,
                     const base::DictValue& row_dict) {
  static const auto kRowHandlers =
      base::MakeFixedFlatMap<std::string_view, RowPropertyHandler>({
          {"is_padding", &HandleRowPadding},
          {"vertical_resize", &HandleRowVResize},
          {"height", &HandleRowHeight},
      });

  TableRowParams params;
  for (auto&& [key, val] : row_dict) {
    auto it = kRowHandlers.find(key);
    if (it != kRowHandlers.end()) {
      it->second(params, val);
    }
  }

  if (params.is_padding) {
    layout->AddPaddingRow(params.vertical_resize, params.height);
  } else {
    layout->AddRows(1, params.vertical_resize, params.height);
  }
}

using TableLayoutListHandler = void (*)(views::TableLayout*,
                                        const base::ListValue&);
void HandleTableColumns(views::TableLayout* layout,
                        const base::ListValue& columns) {
  for (const auto& col_val : columns) {
    if (col_val.is_dict()) {
      ProcessTableColumn(layout, col_val.GetDict());
    }
  }
}
void HandleTableRows(views::TableLayout* layout, const base::ListValue& rows) {
  for (const auto& row_val : rows) {
    if (row_val.is_dict()) {
      ProcessTableRow(layout, row_val.GetDict());
    }
  }
}

using LayoutManagerFactory = void (*)(views::View*, const base::DictValue&);

void BuildFlexLayout(views::View* view, const base::DictValue& val) {
  view->SetLayoutManager(std::make_unique<views::FlexLayout>());
}

void BuildBoxLayout(views::View* view, const base::DictValue& val) {
  view->SetLayoutManager(std::make_unique<views::BoxLayout>());
}

void BuildTableLayout(views::View* view, const base::DictValue& val) {
  auto layout = std::make_unique<views::TableLayout>();
  static const auto kTableLayoutHandlers =
      base::MakeFixedFlatMap<std::string_view, TableLayoutListHandler>({
          {"columns", &HandleTableColumns},
          {"rows", &HandleTableRows},
      });

  for (const auto& [handler_key, list_handler] : kTableLayoutHandlers) {
    const base::ListValue* list = val.FindList(handler_key);
    if (list) {
      list_handler(layout.get(), *list);
    }
  }
  view->SetLayoutManager(std::move(layout));
}

bool ApplyLayoutManager(views::View* view,
                        const base::DictValue& val,
                        std::string* error_msg) {
  const std::string* type_ptr = val.FindString("type");
  std::string type = type_ptr ? *type_ptr : "";

  static const auto kLayoutManagerFactories =
      base::MakeFixedFlatMap<std::string_view, LayoutManagerFactory>({
          {"FlexLayout", &BuildFlexLayout},
          {"BoxLayout", &BuildBoxLayout},
          {"TableLayout", &BuildTableLayout},
      });

  auto it = kLayoutManagerFactories.find(type);
  if (it != kLayoutManagerFactories.end()) {
    it->second(view, val);
  } else {
    *error_msg = "Unknown layout manager type: " + type;
    return false;
  }

  ui::metadata::ClassMetaData* layout_meta =
      view->GetLayoutManager()->GetClassMetaData();
  if (layout_meta) {
    for (auto&& [layout_key, layout_val] : val) {
      if (layout_key == "type" || layout_key == "columns" ||
          layout_key == "rows") {
        continue;
      }
      ui::metadata::MemberMetaDataBase* member =
          FindMemberDataCaseInsensitive(layout_meta, layout_key);
      if (!member) {
        *error_msg = "Unknown layout property: " + layout_key;
        return false;
      }
      std::u16string val_str = JsonValueToU16String(layout_val);
      std::string type_str = member->member_type();
      PropertyType ptype = PropertyType::kUnknown;
      if (type_str == "SkColor") {
        ptype = PropertyType::kColor;
      } else if (type_str == "gfx::Insets") {
        ptype = PropertyType::kInsets;
      } else if (type_str == "int" || type_str == "bool") {
        ptype = PropertyType::kInt;
      } else if (type_str == "std::u16string" || type_str == "std::string") {
        ptype = PropertyType::kString;
      }
      std::u16string resolved;
      if (!ResolveValue(view, layout_key, ptype, val_str, &resolved,
                        error_msg)) {
        return false;
      }
      member->SetValueAsString(view->GetLayoutManager(), resolved);
    }
  }
  return true;
}

// Base class for component instantiation, child hierarchy, and property
// handling.
class ViewHandlerBase {
 public:
  virtual ~ViewHandlerBase() = default;

  // Constructs a new view instance from the JSON dictionary.
  virtual std::unique_ptr<views::View> Instantiate(const base::DictValue& dict,
                                                   std::string* error_msg) = 0;

  // Constructs and adds children to the view.
  virtual bool AddChildren(views::View* view,
                           const base::ListValue& children,
                           std::string* error_msg) {
    for (const auto& child_val : children) {
      if (!child_val.is_dict()) {
        *error_msg = "Child element must be a dictionary";
        return false;
      }
      auto child_view =
          JsonViewBuilder::BuildView(child_val.GetDict(), error_msg);
      if (!child_view) {
        return false;
      }
      view->AddChildView(std::move(child_view));
    }
    return true;
  }

  // Returns the child view at index for hierarchical property application.
  virtual views::View* GetChildAt(views::View* view, size_t index) {
    if (index < view->children().size()) {
      return view->children()[index];
    }
    return nullptr;
  }

  // Applies properties to the view instance.
  virtual bool ApplyProperties(views::View* view,
                               const base::DictValue& dict,
                               std::string* error_msg) {
    return ApplyStandardProperties(view, dict, error_msg);
  }

  // Resolves component-specific dynamic properties.
  virtual std::unique_ptr<DynamicProperty> GetCustomProperty(
      views::View* view,
      const std::string& name) {
    return nullptr;
  }

 protected:
  // Universal dynamic properties applicable to all views.
  std::unique_ptr<DynamicProperty> GetUniversalProperty(
      views::View* view,
      const std::string& name) {
    std::string lower_name = base::ToLowerASCII(name);
    if (lower_name == "background") {
      return std::make_unique<BackgroundDynamicProperty>();
    } else if (lower_name == "border") {
      return std::make_unique<BorderDynamicProperty>();
    } else if (lower_name == "layout_flex") {
      return std::make_unique<LayoutFlexDynamicProperty>();
    } else if (lower_name == "layout_manager") {
      return std::make_unique<LayoutManagerDynamicProperty>();
    } else if (lower_name == "accessiblename") {
      return std::make_unique<AccessibleNameDynamicProperty>();
    }
    return nullptr;
  }

  // Resolves a property by checking custom, universal, and metadata accessors.
  std::unique_ptr<DynamicProperty> GetProperty(views::View* view,
                                               const std::string& name) {
    std::unique_ptr<DynamicProperty> custom_prop =
        GetCustomProperty(view, name);
    if (custom_prop) {
      return custom_prop;
    }

    std::unique_ptr<DynamicProperty> universal_prop =
        GetUniversalProperty(view, name);
    if (universal_prop) {
      return universal_prop;
    }

    ui::metadata::ClassMetaData* class_meta = view->GetClassMetaData();
    ui::metadata::MemberMetaDataBase* member_meta =
        FindMemberDataCaseInsensitive(class_meta, name);
    if (member_meta) {
      return std::make_unique<MetadataDynamicProperty>(member_meta);
    }

    return nullptr;
  }

  // Shared property application logic using metadata and dynamic properties.
  bool ApplyStandardProperties(
      views::View* view,
      const base::DictValue& dict,
      std::string* error_msg,
      const base::flat_set<std::string_view>& skipped_properties = {}) {
    const base::DictValue* properties = dict.FindDict("properties");
    if (!properties) {
      return true;
    }

    for (auto&& [key, val] : *properties) {
      if (skipped_properties.contains(key)) {
        continue;
      }
      if (key == "layout_manager" && val.is_dict()) {
        if (!ApplyLayoutManager(view, val.GetDict(), error_msg)) {
          return false;
        }
        continue;
      }

      std::u16string val_str = JsonValueToU16String(val);

      std::unique_ptr<DynamicProperty> prop = GetProperty(view, key);
      if (!prop) {
        *error_msg = "Property not found: " + key + " on class " +
                     std::string(view->GetClassName());
        return false;
      }

      std::u16string resolved;
      if (!ResolveValue(view, key, prop->GetType(), val_str, &resolved,
                        error_msg)) {
        return false;
      }

      if (!prop->SetValue(view, resolved, error_msg)) {
        return false;
      }
    }
    return true;
  }
};

// Primary component handler template.
template <typename T>
class ComponentHandler : public ViewHandlerBase {
 public:
  static ComponentHandler<T>* GetInstance() {
    static base::NoDestructor<ComponentHandler<T>> instance;
    return instance.get();
  }

  static ViewHandlerBase* GetBaseInstance() { return GetInstance(); }

  std::unique_ptr<views::View> Instantiate(const base::DictValue& dict,
                                           std::string* error_msg) override {
    return std::make_unique<T>();
  }
};

// Specialization: RadioButton
template <>
class ComponentHandler<views::RadioButton> : public ViewHandlerBase {
 public:
  static ComponentHandler<views::RadioButton>* GetInstance() {
    static base::NoDestructor<ComponentHandler<views::RadioButton>> instance;
    return instance.get();
  }

  static ViewHandlerBase* GetBaseInstance() { return GetInstance(); }

  std::unique_ptr<views::View> Instantiate(const base::DictValue& dict,
                                           std::string* error_msg) override {
    return std::make_unique<views::RadioButton>(u"", 0);
  }
};

// Specialization: ScrollView
template <>
class ComponentHandler<views::ScrollView> : public ViewHandlerBase {
 public:
  static ComponentHandler<views::ScrollView>* GetInstance() {
    static base::NoDestructor<ComponentHandler<views::ScrollView>> instance;
    return instance.get();
  }

  static ViewHandlerBase* GetBaseInstance() { return GetInstance(); }

  std::unique_ptr<views::View> Instantiate(const base::DictValue& dict,
                                           std::string* error_msg) override {
    return std::make_unique<views::ScrollView>();
  }

  bool AddChildren(views::View* view,
                   const base::ListValue& children,
                   std::string* error_msg) override {
    auto* scroll_view = views::AsViewClass<views::ScrollView>(view);
    if (!scroll_view) {
      *error_msg = "View is not a ScrollView";
      return false;
    }
    if (children.size() > 1) {
      *error_msg = "ScrollView can only have a single child view in JSON";
      return false;
    }
    for (const auto& child_val : children) {
      if (!child_val.is_dict()) {
        *error_msg = "Child element must be a dictionary";
        return false;
      }
      auto child_view =
          JsonViewBuilder::BuildView(child_val.GetDict(), error_msg);
      if (!child_view) {
        return false;
      }
      scroll_view->SetContents(std::move(child_view));
    }
    return true;
  }

  views::View* GetChildAt(views::View* view, size_t index) override {
    if (index == 0) {
      if (auto* scroll_view = views::AsViewClass<views::ScrollView>(view)) {
        return scroll_view->contents();
      }
    }
    return nullptr;
  }
};

// Specialization: TabbedPane
class TabbedPaneSelectedTabIndexProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "selectedtabindex";
    return name;
  }
  PropertyType GetType() const override { return PropertyType::kInt; }
  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    auto* tabbed_pane = views::AsViewClass<views::TabbedPane>(view);
    if (!tabbed_pane) {
      *error_msg = "View is not a TabbedPane";
      return false;
    }
    int index = 0;
    if (!base::StringToInt(value_str, &index) || index < 0 ||
        static_cast<size_t>(index) >= tabbed_pane->GetTabCount()) {
      *error_msg = "Invalid tab index: " + base::UTF16ToUTF8(value_str);
      return false;
    }
    tabbed_pane->SelectTabAt(static_cast<size_t>(index), /*animate=*/false);
    return true;
  }
};

class TabbedPaneDrawTabDividerProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "drawtabdivider";
    return name;
  }
  PropertyType GetType() const override { return PropertyType::kInt; }
  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    auto* tabbed_pane = views::AsViewClass<views::TabbedPane>(view);
    if (!tabbed_pane) {
      *error_msg = "View is not a TabbedPane";
      return false;
    }
    std::string str_utf8 = base::UTF16ToUTF8(value_str);
    tabbed_pane->SetDrawTabDivider(str_utf8 == "true" || str_utf8 == "1");
    return true;
  }
};

template <>
class ComponentHandler<views::TabbedPane> : public ViewHandlerBase {
 public:
  static ComponentHandler<views::TabbedPane>* GetInstance() {
    static base::NoDestructor<ComponentHandler<views::TabbedPane>> instance;
    return instance.get();
  }

  static ViewHandlerBase* GetBaseInstance() { return GetInstance(); }

  std::unique_ptr<views::View> Instantiate(const base::DictValue& dict,
                                           std::string* error_msg) override {
    return std::make_unique<views::TabbedPane>();
  }

  bool AddChildren(views::View* view,
                   const base::ListValue& children,
                   std::string* error_msg) override {
    auto* tabbed_pane = views::AsViewClass<views::TabbedPane>(view);
    if (!tabbed_pane) {
      *error_msg = "View is not a TabbedPane";
      return false;
    }
    for (size_t i = 0; i < children.size(); ++i) {
      const auto& child_val = children[i];
      if (!child_val.is_dict()) {
        *error_msg = "Child element must be a dictionary";
        return false;
      }
      const base::DictValue& child_dict = child_val.GetDict();
      const std::string* title_str = child_dict.FindString("title");
      if (!title_str) {
        title_str = child_dict.FindString("TabTitle");
      }
      std::u16string title =
          title_str ? base::UTF8ToUTF16(*title_str)
                    : base::UTF8ToUTF16(base::StringPrintf("Tab %zu", i + 1));
      auto child_view = JsonViewBuilder::BuildView(child_dict, error_msg);
      if (!child_view) {
        return false;
      }
      tabbed_pane->AddTab(title, std::move(child_view));
    }
    return true;
  }

  views::View* GetChildAt(views::View* view, size_t index) override {
    if (auto* tabbed_pane = views::AsViewClass<views::TabbedPane>(view)) {
      if (index < tabbed_pane->GetTabCount()) {
        return const_cast<views::View*>(tabbed_pane->GetTabContents(index));
      }
    }
    return nullptr;
  }

  std::unique_ptr<DynamicProperty> GetCustomProperty(
      views::View* view,
      const std::string& name) override {
    std::string lower_name = base::ToLowerASCII(name);
    if (lower_name == "selectedtabindex") {
      return std::make_unique<TabbedPaneSelectedTabIndexProperty>();
    } else if (lower_name == "drawtabdivider") {
      return std::make_unique<TabbedPaneDrawTabDividerProperty>();
    }
    return nullptr;
  }
};

// Specialization: TableView
template <>
class ComponentHandler<views::TableView> : public ViewHandlerBase {
 public:
  static ComponentHandler<views::TableView>* GetInstance() {
    static base::NoDestructor<ComponentHandler<views::TableView>> instance;
    return instance.get();
  }

  static ViewHandlerBase* GetBaseInstance() { return GetInstance(); }

  std::unique_ptr<views::View> Instantiate(const base::DictValue& dict,
                                           std::string* error_msg) override {
    return std::make_unique<views::TableView>();
  }

  bool ApplyProperties(views::View* view,
                       const base::DictValue& dict,
                       std::string* error_msg) override {
    auto* table_view = views::AsViewClass<views::TableView>(view);
    if (!table_view) {
      *error_msg = "View is not a TableView";
      return false;
    }

    const base::DictValue* properties = dict.FindDict("properties");
    if (properties) {
      const base::ListValue* columns_list = properties->FindList("columns");
      const base::ListValue* rows_list = properties->FindList("rows");
      if (!rows_list) {
        rows_list = properties->FindList("data");
      }
      if (columns_list || rows_list) {
        std::vector<ui::TableColumn> columns;
        if (columns_list) {
          int default_id = 0;
          for (const auto& col_val : *columns_list) {
            if (col_val.is_dict()) {
              columns.push_back(
                  ParseTableColumn(col_val.GetDict(), default_id++));
            }
          }
        }
        std::vector<std::vector<std::u16string>> rows;
        if (rows_list) {
          rows = ParseTableRows(*rows_list);
        }
        if (columns.empty() && !rows.empty()) {
          size_t max_cols = 0;
          for (const auto& r : rows) {
            max_cols = std::max(max_cols, r.size());
          }
          for (size_t c = 0; c < max_cols; ++c) {
            ui::TableColumn col;
            col.id = static_cast<int>(c);
            col.title = base::UTF8ToUTF16(base::StringPrintf("Column %zu", c));
            col.sortable = true;
            columns.push_back(col);
          }
        }
        auto table_model =
            std::make_unique<JsonTableModel>(columns, std::move(rows));
        table_view->SetProperty(kJsonTableModelKey, std::move(table_model));
        table_view->SetColumns(columns);
        table_view->SetModel(table_view->GetProperty(kJsonTableModelKey));
      }
    }

    return ApplyStandardProperties(view, dict, error_msg,
                                   {"columns", "rows", "data"});
  }

 private:
  static ui::TableColumn ParseTableColumn(const base::DictValue& dict,
                                          int default_id) {
    ui::TableColumn col;
    col.id = dict.FindInt("id").value_or(default_id);
    const std::string* title_str = dict.FindString("title");
    if (title_str) {
      col.title = base::UTF8ToUTF16(*title_str);
    }
    const std::string* align_str = dict.FindString("alignment");
    if (align_str) {
      if (base::EqualsCaseInsensitiveASCII(*align_str, "RIGHT")) {
        col.alignment = ui::TableColumn::RIGHT;
      } else if (base::EqualsCaseInsensitiveASCII(*align_str, "CENTER")) {
        col.alignment = ui::TableColumn::CENTER;
      } else {
        col.alignment = ui::TableColumn::LEFT;
      }
    }
    col.width = dict.FindInt("width").value_or(-1);
    if (std::optional<double> pct = dict.FindDouble("percent")) {
      col.percent = static_cast<float>(*pct);
    }
    if (std::optional<int> min_w = dict.FindInt("min_width")) {
      col.min_visible_width = *min_w;
    }
    col.sortable = dict.FindBool("sortable").value_or(true);
    return col;
  }

  static std::vector<std::vector<std::u16string>> ParseTableRows(
      const base::ListValue& list) {
    std::vector<std::vector<std::u16string>> rows;
    for (const auto& row_val : list) {
      std::vector<std::u16string> row_cells;
      if (row_val.is_list()) {
        for (const auto& cell_val : row_val.GetList()) {
          row_cells.push_back(JsonValueToU16String(cell_val));
        }
      }
      rows.push_back(std::move(row_cells));
    }
    return rows;
  }
};

// Specialization: StyledLabel
class StyledLabelHorizontalAlignmentProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "horizontalalignment";
    return name;
  }
  PropertyType GetType() const override { return PropertyType::kCompound; }
  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    auto* styled_label = views::AsViewClass<views::StyledLabel>(view);
    if (!styled_label) {
      *error_msg = "View is not a StyledLabel";
      return false;
    }
    std::optional<gfx::HorizontalAlignment> align =
        ui::metadata::TypeConverter<gfx::HorizontalAlignment>::FromString(
            value_str);
    if (!align) {
      std::string s = base::ToUpperASCII(base::UTF16ToUTF8(value_str));
      if (s == "LEFT" || s == "KLEFT" || s == "ALIGN_LEFT") {
        align = gfx::ALIGN_LEFT;
      } else if (s == "CENTER" || s == "KCENTER" || s == "ALIGN_CENTER") {
        align = gfx::ALIGN_CENTER;
      } else if (s == "RIGHT" || s == "KRIGHT" || s == "ALIGN_RIGHT") {
        align = gfx::ALIGN_RIGHT;
      } else if (s == "ALIGN_TO_HEAD") {
        align = gfx::ALIGN_TO_HEAD;
      }
    }
    if (!align) {
      *error_msg =
          "Invalid horizontal alignment: " + base::UTF16ToUTF8(value_str);
      return false;
    }
    styled_label->SetHorizontalAlignment(*align);
    return true;
  }
};

template <>
class ComponentHandler<views::StyledLabel> : public ViewHandlerBase {
 public:
  static ComponentHandler<views::StyledLabel>* GetInstance() {
    static base::NoDestructor<ComponentHandler<views::StyledLabel>> instance;
    return instance.get();
  }

  static ViewHandlerBase* GetBaseInstance() { return GetInstance(); }

  std::unique_ptr<views::View> Instantiate(const base::DictValue& dict,
                                           std::string* error_msg) override {
    return std::make_unique<views::StyledLabel>();
  }

  std::unique_ptr<DynamicProperty> GetCustomProperty(
      views::View* view,
      const std::string& name) override {
    std::string lower_name = base::ToLowerASCII(name);
    if (lower_name == "horizontalalignment") {
      return std::make_unique<StyledLabelHorizontalAlignmentProperty>();
    }
    return nullptr;
  }

  bool ApplyProperties(views::View* view,
                       const base::DictValue& dict,
                       std::string* error_msg) override {
    if (!ApplyStandardProperties(view, dict, error_msg,
                                 {"ranges", "style_ranges"})) {
      return false;
    }

    auto* styled_label = views::AsViewClass<views::StyledLabel>(view);
    if (!styled_label) {
      *error_msg = "View is not a StyledLabel";
      return false;
    }

    const base::DictValue* properties = dict.FindDict("properties");
    if (properties) {
      const base::ListValue* ranges = properties->FindList("ranges");
      if (!ranges) {
        ranges = properties->FindList("style_ranges");
      }
      if (ranges) {
        ApplyStyleRanges(styled_label, *ranges);
      }
    }
    return true;
  }

 private:
  static void ApplyStyleRanges(views::StyledLabel* styled_label,
                               const base::ListValue& ranges) {
    for (const auto& range_val : ranges) {
      if (!range_val.is_dict()) {
        continue;
      }
      const base::DictValue& range_dict = range_val.GetDict();
      int start = range_dict.FindInt("start").value_or(0);
      int end = range_dict.FindInt("end").value_or(0);
      if (end <= start && range_dict.FindInt("length").has_value()) {
        end = start + range_dict.FindInt("length").value();
      }
      if (end > start) {
        views::StyledLabel::RangeStyleInfo style_info;
        if (const std::string* style_str = range_dict.FindString("style")) {
          std::u16string mapped =
              MapTextStyleOrContext(base::UTF8ToUTF16(*style_str));
          int style_int = 0;
          if (base::StringToInt(mapped, &style_int)) {
            style_info.text_style = style_int;
          }
        }
        if (const std::string* color_str = range_dict.FindString("color")) {
          std::u16string color_u16 = base::UTF8ToUTF16(*color_str);
          if (base::StartsWith(*color_str, "ColorId:",
                               base::CompareCase::INSENSITIVE_ASCII)) {
            std::string color_name = color_str->substr(8);
            std::optional<ui::ColorId> color_id = ui::NameToColorId(color_name);
            if (color_id) {
              style_info.override_color_id = *color_id;
            }
          } else {
            std::optional<SkColor> color = ParseColor(color_u16, styled_label);
            if (color) {
              style_info.override_color = *color;
            }
          }
        }
        if (const std::string* tooltip = range_dict.FindString("tooltip")) {
          style_info.tooltip = base::UTF8ToUTF16(*tooltip);
        }
        if (const std::string* acc_name =
                range_dict.FindString("accessible_name")) {
          style_info.accessible_name = base::UTF8ToUTF16(*acc_name);
        }
        styled_label->AddStyleRange(gfx::Range(start, end), style_info);
      }
    }
  }
};

// Specialization: ImageView
const gfx::VectorIcon* FindVectorIconByName(std::string_view name) {
  static const base::NoDestructor<
      base::flat_map<std::string_view, const gfx::VectorIcon*>>
      kIcons({
          {"account_box", &views::kAccountBoxIcon},
          {"arrow_drop_down", &views::kArrowDropDownIcon},
          {"arrow_drop_up", &views::kArrowDropUpIcon},
          {"arrow_outward", &views::kArrowOutwardIcon},
          {"cancel", &views::kCancelIcon},
          {"check", &views::kCheckIcon},
          {"checkbox_filled", &views::kCheckBoxFilledIcon},
          {"checkbox_outline_blank", &views::kCheckBoxOutlineBlankIcon},
          {"circle", &views::kCircleIcon},
          {"close", &views::kCloseIcon},
          {"delete", &views::kDeleteIcon},
          {"info", &views::kInfoIcon},
          {"launch", &views::kLaunchOldIcon},
          {"menu_open", &views::kMenuOpenIcon},
          {"more_horiz", &views::kMoreHorizIcon},
          {"new_window", &views::kNewWindowIcon},
          {"open_in_new", &views::kOpenInNewIcon},
          {"options", &views::kOptionsOldIcon},
          {"visibility", &views::kVisibilityIcon},
          {"visibility_filled", &views::kVisibilityFilledIcon},
          {"visibility_off", &views::kVisibilityOffIcon},
          {"visibility_off_filled", &views::kVisibilityOffFilledIcon},
      });
  auto it = kIcons->find(base::ToLowerASCII(name));
  return it != kIcons->end() ? it->second : nullptr;
}

class ImageDynamicProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "image";
    return name;
  }

  PropertyType GetType() const override { return PropertyType::kCompound; }

  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    auto* image_view = views::AsViewClass<views::ImageView>(view);
    if (!image_view) {
      *error_msg = "View is not an ImageView";
      return false;
    }

    std::string value_utf8 = base::UTF16ToUTF8(value_str);
    std::vector<std::string> parts = base::SplitString(
        value_utf8, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.empty()) {
      *error_msg = "Empty image specification";
      return false;
    }

    if (parts[0] == "solid") {
      if (parts.size() < 2) {
        *error_msg =
            "Solid image requires at least a color: solid,color[,width,height]";
        return false;
      }
      std::u16string color_resolved;
      if (!ResolveValue(view, "image.color", PropertyType::kColor,
                        base::UTF8ToUTF16(parts[1]), &color_resolved,
                        error_msg)) {
        return false;
      }
      std::optional<SkColor> color = ParseColor(color_resolved, view);
      if (!color) {
        *error_msg = "Failed to parse color: " + parts[1];
        return false;
      }
      int width = 16;
      int height = 16;
      if (parts.size() >= 3) {
        base::StringToInt(parts[2], &width);
      }
      if (parts.size() >= 4) {
        base::StringToInt(parts[3], &height);
      } else if (parts.size() == 3) {
        height = width;
      }
      SkBitmap bitmap;
      bitmap.allocN32Pixels(width, height);
      bitmap.eraseColor(*color);
      image_view->SetImage(ui::ImageModel::FromImageSkia(
          gfx::ImageSkia::CreateFrom1xBitmap(bitmap)));
      return true;
    }

    std::string icon_token = parts[0];
    if (base::StartsWith(
            icon_token, "vector_icon:", base::CompareCase::INSENSITIVE_ASCII)) {
      icon_token = icon_token.substr(12);
    }
    const gfx::VectorIcon* icon = FindVectorIconByName(icon_token);
    if (icon) {
      int size = 16;
      if (parts.size() >= 2) {
        base::StringToInt(parts[1], &size);
      }
      if (parts.size() >= 3) {
        std::string color_part = parts[2];
        if (base::StartsWith(
                color_part, "ColorId:", base::CompareCase::INSENSITIVE_ASCII)) {
          std::string color_name = color_part.substr(8);
          std::optional<ui::ColorId> color_id = ui::NameToColorId(color_name);
          if (color_id) {
            image_view->SetImage(
                ui::ImageModel::FromVectorIcon(*icon, *color_id, size));
            return true;
          }
        }
        std::u16string color_resolved;
        if (ResolveValue(view, "image.color", PropertyType::kColor,
                         base::UTF8ToUTF16(color_part), &color_resolved,
                         error_msg)) {
          std::optional<SkColor> color = ParseColor(color_resolved, view);
          if (color) {
            image_view->SetImage(
                ui::ImageModel::FromVectorIcon(*icon, *color, size));
            return true;
          }
        }
      }
      image_view->SetImage(
          ui::ImageModel::FromVectorIcon(*icon, ui::kColorIcon, size));
      return true;
    }

    *error_msg = "Unsupported image format or unknown icon: " + parts[0];
    return false;
  }
};

class ImageViewCornerRadiusProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "cornerradius";
    return name;
  }
  PropertyType GetType() const override { return PropertyType::kInt; }
  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    auto* image_view = views::AsViewClass<views::ImageView>(view);
    if (!image_view) {
      *error_msg = "View is not an ImageView";
      return false;
    }
    int radius = 0;
    if (!base::StringToInt(value_str, &radius)) {
      *error_msg = "Invalid corner radius: " + base::UTF16ToUTF8(value_str);
      return false;
    }
    image_view->SetCornerRadius(radius);
    return true;
  }
};

class ImageViewTooltipTextProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "tooltiptext";
    return name;
  }
  PropertyType GetType() const override { return PropertyType::kString; }
  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    auto* image_view = views::AsViewClass<views::ImageView>(view);
    if (!image_view) {
      *error_msg = "View is not an ImageView";
      return false;
    }
    image_view->SetTooltipText(value_str);
    return true;
  }
};

class ImageViewImageSizeProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "imagesize";
    return name;
  }
  PropertyType GetType() const override { return PropertyType::kCompound; }
  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    auto* image_view = views::AsViewClass<views::ImageViewBase>(view);
    if (!image_view) {
      *error_msg = "View is not an ImageView";
      return false;
    }
    std::optional<gfx::Size> size =
        ui::metadata::TypeConverter<gfx::Size>::FromString(value_str);
    if (!size) {
      std::string s = base::UTF16ToUTF8(value_str);
      std::vector<std::string> parts = base::SplitString(
          s, ", x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      int w = 0, h = 0;
      if (parts.size() == 2 && base::StringToInt(parts[0], &w) &&
          base::StringToInt(parts[1], &h)) {
        size = gfx::Size(w, h);
      } else if (parts.size() == 1 && base::StringToInt(parts[0], &w)) {
        size = gfx::Size(w, w);
      }
    }
    if (!size) {
      *error_msg = "Invalid image size: " + base::UTF16ToUTF8(value_str);
      return false;
    }
    image_view->SetImageSize(*size);
    return true;
  }
};

template <>
class ComponentHandler<views::ImageView> : public ViewHandlerBase {
 public:
  static ComponentHandler<views::ImageView>* GetInstance() {
    static base::NoDestructor<ComponentHandler<views::ImageView>> instance;
    return instance.get();
  }

  static ViewHandlerBase* GetBaseInstance() { return GetInstance(); }

  std::unique_ptr<views::View> Instantiate(const base::DictValue& dict,
                                           std::string* error_msg) override {
    return std::make_unique<views::ImageView>();
  }

  std::unique_ptr<DynamicProperty> GetCustomProperty(
      views::View* view,
      const std::string& name) override {
    std::string lower_name = base::ToLowerASCII(name);
    if (lower_name == "image" || lower_name == "vector_icon" ||
        lower_name == "vectoricon") {
      return std::make_unique<ImageDynamicProperty>();
    } else if (lower_name == "imagesize") {
      return std::make_unique<ImageViewImageSizeProperty>();
    } else if (lower_name == "cornerradius") {
      return std::make_unique<ImageViewCornerRadiusProperty>();
    } else if (lower_name == "tooltiptext") {
      return std::make_unique<ImageViewTooltipTextProperty>();
    }
    return nullptr;
  }
};

// Specialization: Slider
class SliderStyleDynamicProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "style";
    return name;
  }
  PropertyType GetType() const override { return PropertyType::kCompound; }
  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    auto* slider = views::AsViewClass<views::Slider>(view);
    if (!slider) {
      *error_msg = "View is not a Slider";
      return false;
    }
    std::string str_utf8 = base::UTF16ToUTF8(value_str);
    if (base::EqualsCaseInsensitiveASCII(str_utf8, "kMinimalStyle") ||
        base::EqualsCaseInsensitiveASCII(str_utf8, "minimal")) {
      slider->SetRenderingStyle(views::Slider::RenderingStyle::kMinimalStyle);
      return true;
    } else if (base::EqualsCaseInsensitiveASCII(str_utf8, "kDefaultStyle") ||
               base::EqualsCaseInsensitiveASCII(str_utf8, "default")) {
      slider->SetRenderingStyle(views::Slider::RenderingStyle::kDefaultStyle);
      return true;
    }
    *error_msg = "Unknown slider style: " + str_utf8;
    return false;
  }
};

template <>
class ComponentHandler<views::Slider> : public ViewHandlerBase {
 public:
  static ComponentHandler<views::Slider>* GetInstance() {
    static base::NoDestructor<ComponentHandler<views::Slider>> instance;
    return instance.get();
  }

  static ViewHandlerBase* GetBaseInstance() { return GetInstance(); }

  std::unique_ptr<views::View> Instantiate(const base::DictValue& dict,
                                           std::string* error_msg) override {
    return std::make_unique<views::Slider>();
  }

  std::unique_ptr<DynamicProperty> GetCustomProperty(
      views::View* view,
      const std::string& name) override {
    std::string lower_name = base::ToLowerASCII(name);
    if (lower_name == "renderingstyle" || lower_name == "style") {
      return std::make_unique<SliderStyleDynamicProperty>();
    }
    return nullptr;
  }
};

// Specialization: Throbber
class ThrobberRunningDynamicProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "running";
    return name;
  }
  PropertyType GetType() const override { return PropertyType::kInt; }
  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    auto* throbber = views::AsViewClass<views::Throbber>(view);
    if (!throbber) {
      *error_msg = "View is not a Throbber";
      return false;
    }
    std::string str_utf8 = base::UTF16ToUTF8(value_str);
    if (str_utf8 == "true" || str_utf8 == "1") {
      throbber->Start();
    } else {
      throbber->Stop();
    }
    return true;
  }
};

template <>
class ComponentHandler<views::Throbber> : public ViewHandlerBase {
 public:
  static ComponentHandler<views::Throbber>* GetInstance() {
    static base::NoDestructor<ComponentHandler<views::Throbber>> instance;
    return instance.get();
  }

  static ViewHandlerBase* GetBaseInstance() { return GetInstance(); }

  std::unique_ptr<views::View> Instantiate(const base::DictValue& dict,
                                           std::string* error_msg) override {
    return std::make_unique<views::Throbber>();
  }

  std::unique_ptr<DynamicProperty> GetCustomProperty(
      views::View* view,
      const std::string& name) override {
    std::string lower_name = base::ToLowerASCII(name);
    if (lower_name == "running" || lower_name == "isrunning") {
      return std::make_unique<ThrobberRunningDynamicProperty>();
    }
    return nullptr;
  }
};

// Specialization: SmoothedThrobber
class SmoothedThrobberStartDelayMsProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "startdelayms";
    return name;
  }
  PropertyType GetType() const override { return PropertyType::kInt; }
  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    auto* throbber = views::AsViewClass<views::SmoothedThrobber>(view);
    if (!throbber) {
      *error_msg = "View is not a SmoothedThrobber";
      return false;
    }
    int ms = 0;
    if (!base::StringToInt(value_str, &ms)) {
      *error_msg = "Invalid start delay: " + base::UTF16ToUTF8(value_str);
      return false;
    }
    throbber->SetStartDelay(base::Milliseconds(ms));
    return true;
  }
};

class SmoothedThrobberStopDelayMsProperty : public DynamicProperty {
 public:
  const std::string_view name() const override {
    static const char name[] = "stopdelayms";
    return name;
  }
  PropertyType GetType() const override { return PropertyType::kInt; }
  bool SetValue(views::View* view,
                const std::u16string& value_str,
                std::string* error_msg) override {
    auto* throbber = views::AsViewClass<views::SmoothedThrobber>(view);
    if (!throbber) {
      *error_msg = "View is not a SmoothedThrobber";
      return false;
    }
    int ms = 0;
    if (!base::StringToInt(value_str, &ms)) {
      *error_msg = "Invalid stop delay: " + base::UTF16ToUTF8(value_str);
      return false;
    }
    throbber->SetStopDelay(base::Milliseconds(ms));
    return true;
  }
};

template <>
class ComponentHandler<views::SmoothedThrobber> : public ViewHandlerBase {
 public:
  static ComponentHandler<views::SmoothedThrobber>* GetInstance() {
    static base::NoDestructor<ComponentHandler<views::SmoothedThrobber>>
        instance;
    return instance.get();
  }

  static ViewHandlerBase* GetBaseInstance() { return GetInstance(); }

  std::unique_ptr<views::View> Instantiate(const base::DictValue& dict,
                                           std::string* error_msg) override {
    return std::make_unique<views::SmoothedThrobber>();
  }

  std::unique_ptr<DynamicProperty> GetCustomProperty(
      views::View* view,
      const std::string& name) override {
    std::string lower_name = base::ToLowerASCII(name);
    if (lower_name == "startdelayms") {
      return std::make_unique<SmoothedThrobberStartDelayMsProperty>();
    } else if (lower_name == "stopdelayms") {
      return std::make_unique<SmoothedThrobberStopDelayMsProperty>();
    } else if (lower_name == "running" || lower_name == "isrunning") {
      return std::make_unique<ThrobberRunningDynamicProperty>();
    }
    return nullptr;
  }
};

using HandlerGetterFn = ViewHandlerBase* (*)();

static constexpr auto kViewRegistry = base::MakeFixedFlatMap<std::string_view,
                                                             HandlerGetterFn>({
    {"BoxLayoutView", &ComponentHandler<views::BoxLayoutView>::GetBaseInstance},
    {"Checkbox", &ComponentHandler<views::Checkbox>::GetBaseInstance},
    {"FlexLayoutView",
     &ComponentHandler<views::FlexLayoutView>::GetBaseInstance},
    {"ImageView", &ComponentHandler<views::ImageView>::GetBaseInstance},
    {"Label", &ComponentHandler<views::Label>::GetBaseInstance},
    {"Link", &ComponentHandler<views::Link>::GetBaseInstance},
    {"MdTextButton", &ComponentHandler<views::MdTextButton>::GetBaseInstance},
    {"RadioButton", &ComponentHandler<views::RadioButton>::GetBaseInstance},
    {"ScrollView", &ComponentHandler<views::ScrollView>::GetBaseInstance},
    {"Slider", &ComponentHandler<views::Slider>::GetBaseInstance},
    {"SmoothedThrobber",
     &ComponentHandler<views::SmoothedThrobber>::GetBaseInstance},
    {"StyledLabel", &ComponentHandler<views::StyledLabel>::GetBaseInstance},
    {"TabbedPane", &ComponentHandler<views::TabbedPane>::GetBaseInstance},
    {"TableLayoutView",
     &ComponentHandler<views::TableLayoutView>::GetBaseInstance},
    {"TableView", &ComponentHandler<views::TableView>::GetBaseInstance},
    {"Textarea", &ComponentHandler<views::Textarea>::GetBaseInstance},
    {"Textfield", &ComponentHandler<views::Textfield>::GetBaseInstance},
    {"Throbber", &ComponentHandler<views::Throbber>::GetBaseInstance},
    {"ToggleButton", &ComponentHandler<views::ToggleButton>::GetBaseInstance},
    {"View", &ComponentHandler<views::View>::GetBaseInstance},
});

ViewHandlerBase* GetHandlerForType(std::string_view type) {
  auto it = kViewRegistry.find(type);
  if (it != kViewRegistry.end()) {
    return (it->second)();
  }
  return nullptr;
}

}  // namespace

std::unique_ptr<views::View> JsonViewBuilder::BuildView(
    const base::DictValue& dict,
    std::string* error_msg) {
  const std::string* type_ptr = dict.FindString("type");
  std::string_view type =
      type_ptr ? std::string_view(*type_ptr) : std::string_view("View");

  ViewHandlerBase* handler = GetHandlerForType(type);
  if (!handler) {
    *error_msg = "Unknown view type: " + std::string(type);
    return nullptr;
  }

  std::unique_ptr<views::View> view = handler->Instantiate(dict, error_msg);
  if (!view) {
    return nullptr;
  }

  const base::ListValue* children = dict.FindList("children");
  if (children) {
    if (!handler->AddChildren(view.get(), *children, error_msg)) {
      return nullptr;
    }
  }

  return view;
}

bool JsonViewBuilder::ApplyPropertiesRecursive(views::View* view,
                                               const base::DictValue& dict,
                                               std::string* error_msg) {
  const std::string* type_ptr = dict.FindString("type");
  std::string_view type =
      type_ptr ? std::string_view(*type_ptr) : std::string_view("View");

  ViewHandlerBase* handler = GetHandlerForType(type);
  if (!handler) {
    handler = ComponentHandler<views::View>::GetInstance();
  }

  if (!handler->ApplyProperties(view, dict, error_msg)) {
    return false;
  }

  const base::ListValue* children = dict.FindList("children");
  if (children) {
    size_t idx = 0;
    for (const auto& child_val : *children) {
      if (!child_val.is_dict()) {
        *error_msg = "Child element must be a dictionary";
        return false;
      }
      views::View* target_child = handler->GetChildAt(view, idx);
      if (!target_child) {
        *error_msg = "Hierarchy mismatch during property application";
        return false;
      }
      if (!ApplyPropertiesRecursive(target_child, child_val.GetDict(),
                                    error_msg)) {
        return false;
      }
      idx++;
    }
  }

  return true;
}

}  // namespace views::examples
