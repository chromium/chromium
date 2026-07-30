// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/json_view_builder.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/views_canvas_example.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

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

namespace views::examples {

namespace {

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
  if (str == u"STYLE_PRIMARY") {
    return base::ASCIIToUTF16(
        base::NumberToString(views::style::STYLE_PRIMARY));
  }
  if (str == u"STYLE_SECONDARY") {
    return base::ASCIIToUTF16(
        base::NumberToString(views::style::STYLE_SECONDARY));
  }
  if (str == u"STYLE_HINT") {
    return base::ASCIIToUTF16(base::NumberToString(views::style::STYLE_HINT));
  }
  if (str == u"STYLE_DISABLED") {
    return base::ASCIIToUTF16(
        base::NumberToString(views::style::STYLE_DISABLED));
  }
  if (str == u"STYLE_EMPHASIZED") {
    return base::ASCIIToUTF16(
        base::NumberToString(views::style::STYLE_EMPHASIZED));
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
      if (base::ToLowerASCII(member->member_name()) == lower_name) {
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

// Resolves a property by name (checks custom first, then metadata).
std::unique_ptr<DynamicProperty> GetProperty(views::View* view,
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

  // Look up in class metadata
  ui::metadata::ClassMetaData* class_meta = view->GetClassMetaData();
  ui::metadata::MemberMetaDataBase* member_meta =
      FindMemberDataCaseInsensitive(class_meta, name);
  if (member_meta) {
    return std::make_unique<MetadataDynamicProperty>(member_meta);
  }

  return nullptr;
}

template <typename T>
std::unique_ptr<views::View> CreateViewInstance() {
  return std::make_unique<T>();
}

template <>
std::unique_ptr<views::View> CreateViewInstance<views::RadioButton>() {
  return std::make_unique<views::RadioButton>(u"", 0);
}

using ViewFactoryFn = std::unique_ptr<views::View> (*)();

static constexpr auto kViewRegistry =
    base::MakeFixedFlatMap<std::string_view, ViewFactoryFn>({
        {"View", &CreateViewInstance<views::View>},
        {"Label", &CreateViewInstance<views::Label>},
        {"MdTextButton", &CreateViewInstance<views::MdTextButton>},
        {"Textfield", &CreateViewInstance<views::Textfield>},
        {"Textarea", &CreateViewInstance<views::Textarea>},
        {"Checkbox", &CreateViewInstance<views::Checkbox>},
        {"RadioButton", &CreateViewInstance<views::RadioButton>},
        {"ToggleButton", &CreateViewInstance<views::ToggleButton>},
        {"BoxLayoutView", &CreateViewInstance<views::BoxLayoutView>},
        {"FlexLayoutView", &CreateViewInstance<views::FlexLayoutView>},
        {"TableLayoutView", &CreateViewInstance<views::TableLayoutView>},
        {"ScrollView", &CreateViewInstance<views::ScrollView>},
    });

// Instantiates a view based on class name.
std::unique_ptr<views::View> InstantiateView(const std::string& type,
                                             std::string* error_msg) {
  auto it = kViewRegistry.find(type);
  if (it != kViewRegistry.end()) {
    return (it->second)();
  }
  *error_msg = "Unknown view type: " + type;
  return nullptr;
}

// Pass 1: Tree Construction.
std::unique_ptr<views::View> JsonViewBuilderBuildView(
    const base::DictValue& dict,
    std::string* error_msg) {
  const std::string* type_ptr = dict.FindString("type");
  std::string type = type_ptr ? *type_ptr : "View";

  std::unique_ptr<views::View> view = InstantiateView(type, error_msg);
  if (!view) {
    return nullptr;
  }

  const base::ListValue* children = dict.FindList("children");
  if (children) {
    if (auto* scroll_view = views::AsViewClass<views::ScrollView>(view.get())) {
      if (children->size() > 1) {
        *error_msg = "ScrollView can only have a single child view in JSON";
        return nullptr;
      }
      for (const auto& child_val : *children) {
        if (!child_val.is_dict()) {
          *error_msg = "Child element must be a dictionary";
          return nullptr;
        }
        auto child_view =
            JsonViewBuilderBuildView(child_val.GetDict(), error_msg);
        if (!child_view) {
          return nullptr;
        }
        scroll_view->SetContents(std::move(child_view));
      }
    } else {
      for (const auto& child_val : *children) {
        if (!child_val.is_dict()) {
          *error_msg = "Child element must be a dictionary";
          return nullptr;
        }
        auto child_view =
            JsonViewBuilderBuildView(child_val.GetDict(), error_msg);
        if (!child_view) {
          return nullptr;
        }
        view->AddChildView(std::move(child_view));
      }
    }
  }

  return view;
}

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
                            base::CompareCase::INSENSITIVE_ASCII);
  }

  PropertyType GetSupportedType() const override { return PropertyType::kInt; }

  bool Resolve(views::View* view,
               std::string_view value,
               std::u16string* resolved_value,
               std::string* error_msg) const override {
    *resolved_value = MapTextStyleOrContext(base::UTF8ToUTF16(value));
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

// Pass 2: Property Application.
bool JsonViewBuilderApplyPropertiesRecursive(views::View* view,
                                             const base::DictValue& dict,
                                             std::string* error_msg) {
  const base::DictValue* properties = dict.FindDict("properties");
  if (properties) {
    for (auto&& [key, val] : *properties) {
      if (key == "layout_manager" && val.is_dict()) {
        const std::string* type_ptr = val.GetDict().FindString("type");
        std::string type = type_ptr ? *type_ptr : "";

        static const auto kLayoutManagerFactories =
            base::MakeFixedFlatMap<std::string_view, LayoutManagerFactory>({
                {"FlexLayout", &BuildFlexLayout},
                {"BoxLayout", &BuildBoxLayout},
                {"TableLayout", &BuildTableLayout},
            });

        auto it = kLayoutManagerFactories.find(type);
        if (it != kLayoutManagerFactories.end()) {
          it->second(view, val.GetDict());
        } else {
          *error_msg = "Unknown layout manager type: " + type;
          return false;
        }

        ui::metadata::ClassMetaData* layout_meta =
            view->GetLayoutManager()->GetClassMetaData();
        if (layout_meta) {
          for (auto&& [layout_key, layout_val] : val.GetDict()) {
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
            } else if (type_str == "std::u16string" ||
                       type_str == "std::string") {
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
  }

  const base::ListValue* children = dict.FindList("children");
  if (children) {
    size_t idx = 0;
    for (const auto& child_val : *children) {
      views::View* target_child = nullptr;
      if (auto* scroll_view = views::AsViewClass<views::ScrollView>(view)) {
        if (idx == 0) {
          target_child = scroll_view->contents();
        }
      } else if (idx < view->children().size()) {
        target_child = view->children()[idx];
      }

      if (!target_child) {
        *error_msg = "Hierarchy mismatch during property application";
        return false;
      }
      if (!JsonViewBuilderApplyPropertiesRecursive(
              target_child, child_val.GetDict(), error_msg)) {
        return false;
      }
      idx++;
    }
  }

  return true;
}

}  // namespace

std::unique_ptr<views::View> JsonViewBuilder::BuildView(
    const base::DictValue& dict,
    std::string* error_msg) {
  return JsonViewBuilderBuildView(dict, error_msg);
}

bool JsonViewBuilder::ApplyPropertiesRecursive(views::View* view,
                                               const base::DictValue& dict,
                                               std::string* error_msg) {
  return JsonViewBuilderApplyPropertiesRecursive(view, dict, error_msg);
}

}  // namespace views::examples
