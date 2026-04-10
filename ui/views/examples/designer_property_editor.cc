// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/designer_property_editor.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace views::examples {

DesignerPropertyEditor::DesignerPropertyEditor() = default;
DesignerPropertyEditor::~DesignerPropertyEditor() = default;

std::vector<std::u16string> DesignerPropertyEditor::GetComboboxValues() const {
  return {};
}

void DesignerPropertyEditor::ShowCustomDialog(views::View* anchor_view) {}

bool DesignerPropertyEditor::IsExpandable() const {
  return false;
}

bool DesignerPropertyEditor::IsExpanded() const {
  return false;
}

void DesignerPropertyEditor::SetExpanded(bool expanded) {}

size_t DesignerPropertyEditor::GetLevel() const {
  return 0;
}

std::vector<DesignerPropertyEditor*> DesignerPropertyEditor::GetSubEditors() {
  return {};
}

namespace {

bool IsAssignableTo(const ui::metadata::ClassMetaData* meta,
                    std::string_view class_name) {
  while (meta) {
    if (meta->type_name() == class_name) {
      return true;
    }
    meta = meta->parent_class_meta_data();
  }
  return false;
}

class BaseMetadataPropertyEditor : public DesignerPropertyEditor {
 public:
  BaseMetadataPropertyEditor(View* view,
                             ui::metadata::MemberMetaDataBase* meta_data)
      : view_(view), meta_data_(meta_data) {}

  ~BaseMetadataPropertyEditor() override = default;

  std::u16string GetPropertyName() const override {
    return base::ASCIIToUTF16(meta_data_->member_name());
  }

  std::u16string GetValueAsString() const override {
    return meta_data_->GetValueAsString(view_);
  }

  bool IsReadOnly() const override {
    return (meta_data_->GetPropertyFlags() &
            ui::metadata::PropertyFlags::kReadOnly) !=
           ui::metadata::PropertyFlags::kEmpty;
  }

  bool SetValueFromString(const std::u16string& value) override {
    if (IsReadOnly()) {
      return false;
    }
    meta_data_->SetValueAsString(view_, value);
    NotifyPropertyChanged();
    return true;
  }

 protected:
  View* view() const { return view_; }
  ui::metadata::MemberMetaDataBase* meta_data() const { return meta_data_; }

 private:
  raw_ptr<View> view_;
  raw_ptr<ui::metadata::MemberMetaDataBase> meta_data_;
};

template <typename T, typename V>
class SubComponentPropertyEditor : public DesignerPropertyEditor {
 public:
  using Getter = base::RepeatingCallback<V(const T&)>;
  using Setter = base::RepeatingCallback<void(T&, V)>;

  SubComponentPropertyEditor(DesignerPropertyEditor* parent,
                             std::u16string name,
                             Getter getter,
                             Setter setter)
      : parent_(parent),
        name_(std::move(name)),
        getter_(std::move(getter)),
        setter_(std::move(setter)) {}

  ~SubComponentPropertyEditor() override = default;

  std::u16string GetPropertyName() const override { return name_; }

  std::u16string GetValueAsString() const override {
    std::u16string full_value = parent_->GetValueAsString();
    if (auto val = ui::metadata::TypeConverter<T>::FromString(full_value)) {
      return ui::metadata::TypeConverter<V>::ToString(getter_.Run(val.value()));
    }
    return u"";
  }

  bool SetValueFromString(const std::u16string& value) override {
    if (parent_->IsReadOnly()) {
      return false;
    }
    std::u16string full_value = parent_->GetValueAsString();
    if (auto val = ui::metadata::TypeConverter<T>::FromString(full_value)) {
      if (auto component_val =
              ui::metadata::TypeConverter<V>::FromString(value)) {
        setter_.Run(val.value(), component_val.value());
        parent_->SetValueFromString(
            ui::metadata::TypeConverter<T>::ToString(val.value()));
        return true;
      }
    }
    return false;
  }

  EditorType GetEditorType() const override {
    if constexpr (std::is_same_v<V, bool>) {
      return EditorType::kCheckbox;
    }
    return EditorType::kTextField;
  }

  bool IsReadOnly() const override { return parent_->IsReadOnly(); }

  size_t GetLevel() const override { return parent_->GetLevel() + 1; }

 private:
  raw_ptr<DesignerPropertyEditor> parent_;
  std::u16string name_;
  Getter getter_;
  Setter setter_;
};

template <typename T>
class CompoundPropertyEditor : public BaseMetadataPropertyEditor {
 public:
  using BaseMetadataPropertyEditor::BaseMetadataPropertyEditor;

  bool IsExpandable() const override { return true; }
  bool IsExpanded() const override { return expanded_; }
  void SetExpanded(bool expanded) override { expanded_ = expanded; }

  std::vector<DesignerPropertyEditor*> GetSubEditors() override {
    if (sub_editors_.empty()) {
      CreateSubEditors();
    }
    std::vector<DesignerPropertyEditor*> result;
    for (auto& editor : sub_editors_) {
      result.push_back(editor.get());
    }
    return result;
  }

  EditorType GetEditorType() const override { return EditorType::kTextField; }

 protected:
  template <typename V>
  void AddSubEditor(std::u16string name,
                    typename SubComponentPropertyEditor<T, V>::Getter getter,
                    typename SubComponentPropertyEditor<T, V>::Setter setter) {
    sub_editors_.push_back(std::make_unique<SubComponentPropertyEditor<T, V>>(
        this, std::move(name), std::move(getter), std::move(setter)));
  }

  virtual void CreateSubEditors() = 0;

 private:
  bool expanded_ = false;
  std::vector<std::unique_ptr<DesignerPropertyEditor>> sub_editors_;
};

class RectPropertyEditor : public CompoundPropertyEditor<gfx::Rect> {
 public:
  using CompoundPropertyEditor<gfx::Rect>::CompoundPropertyEditor;

 protected:
  void CreateSubEditors() override {
    AddSubEditor<int>(
        u"X", base::BindRepeating([](const gfx::Rect& r) { return r.x(); }),
        base::BindRepeating([](gfx::Rect& r, int v) { r.set_x(v); }));
    AddSubEditor<int>(
        u"Y", base::BindRepeating([](const gfx::Rect& r) { return r.y(); }),
        base::BindRepeating([](gfx::Rect& r, int v) { r.set_y(v); }));
    AddSubEditor<int>(
        u"Width",
        base::BindRepeating([](const gfx::Rect& r) { return r.width(); }),
        base::BindRepeating([](gfx::Rect& r, int v) { r.set_width(v); }));
    AddSubEditor<int>(
        u"Height",
        base::BindRepeating([](const gfx::Rect& r) { return r.height(); }),
        base::BindRepeating([](gfx::Rect& r, int v) { r.set_height(v); }));
  }
};

class InsetsPropertyEditor : public CompoundPropertyEditor<gfx::Insets> {
 public:
  using CompoundPropertyEditor<gfx::Insets>::CompoundPropertyEditor;

 protected:
  void CreateSubEditors() override {
    AddSubEditor<int>(
        u"Top",
        base::BindRepeating([](const gfx::Insets& i) { return i.top(); }),
        base::BindRepeating([](gfx::Insets& i, int v) { i.set_top(v); }));
    AddSubEditor<int>(
        u"Left",
        base::BindRepeating([](const gfx::Insets& i) { return i.left(); }),
        base::BindRepeating([](gfx::Insets& i, int v) { i.set_left(v); }));
    AddSubEditor<int>(
        u"Bottom",
        base::BindRepeating([](const gfx::Insets& i) { return i.bottom(); }),
        base::BindRepeating([](gfx::Insets& i, int v) { i.set_bottom(v); }));
    AddSubEditor<int>(
        u"Right",
        base::BindRepeating([](const gfx::Insets& i) { return i.right(); }),
        base::BindRepeating([](gfx::Insets& i, int v) { i.set_right(v); }));
  }
};

class PointPropertyEditor : public CompoundPropertyEditor<gfx::Point> {
 public:
  using CompoundPropertyEditor<gfx::Point>::CompoundPropertyEditor;

 protected:
  void CreateSubEditors() override {
    AddSubEditor<int>(
        u"X", base::BindRepeating([](const gfx::Point& p) { return p.x(); }),
        base::BindRepeating([](gfx::Point& p, int v) { p.set_x(v); }));
    AddSubEditor<int>(
        u"Y", base::BindRepeating([](const gfx::Point& p) { return p.y(); }),
        base::BindRepeating([](gfx::Point& p, int v) { p.set_y(v); }));
  }
};

class SizePropertyEditor : public CompoundPropertyEditor<gfx::Size> {
 public:
  using CompoundPropertyEditor<gfx::Size>::CompoundPropertyEditor;

 protected:
  void CreateSubEditors() override {
    AddSubEditor<int>(
        u"Width",
        base::BindRepeating([](const gfx::Size& s) { return s.width(); }),
        base::BindRepeating([](gfx::Size& s, int v) { s.set_width(v); }));
    AddSubEditor<int>(
        u"Height",
        base::BindRepeating([](const gfx::Size& s) { return s.height(); }),
        base::BindRepeating([](gfx::Size& s, int v) { s.set_height(v); }));
  }
};

class StringPropertyEditor : public BaseMetadataPropertyEditor {
 public:
  using BaseMetadataPropertyEditor::BaseMetadataPropertyEditor;
  EditorType GetEditorType() const override { return EditorType::kTextField; }
};

class BoolPropertyEditor : public BaseMetadataPropertyEditor {
 public:
  using BaseMetadataPropertyEditor::BaseMetadataPropertyEditor;
  EditorType GetEditorType() const override { return EditorType::kCheckbox; }
};

class EnumPropertyEditor : public BaseMetadataPropertyEditor {
 public:
  using BaseMetadataPropertyEditor::BaseMetadataPropertyEditor;
  EditorType GetEditorType() const override { return EditorType::kCombobox; }
  std::vector<std::u16string> GetComboboxValues() const override {
    return meta_data()->GetValidValues();
  }
};

struct PropertyEditorRegistration {
  PropertyEditorFactory factory;
  std::string property_type;
  std::string property_name;
  std::string class_name;

  int GetSpecificity() const {
    int score = 0;
    if (!property_name.empty()) {
      score += 4;
    }
    if (!class_name.empty()) {
      score += 2;
    }
    if (!property_type.empty()) {
      score += 1;
    }
    return score;
  }
};

std::vector<PropertyEditorRegistration>& GetRegistry() {
  static base::NoDestructor<std::vector<PropertyEditorRegistration>> registry;
  return *registry;
}

bool g_defaults_registered = false;

void RegisterDefaults() {
  if (g_defaults_registered) {
    return;
  }
  g_defaults_registered = true;

  // Type-specific defaults
  RegisterPropertyEditor<BoolPropertyEditor>("bool");
  RegisterPropertyEditor<RectPropertyEditor>("gfx::Rect");
  RegisterPropertyEditor<InsetsPropertyEditor>("gfx::Insets");
  RegisterPropertyEditor<PointPropertyEditor>("gfx::Point");
  RegisterPropertyEditor<SizePropertyEditor>("gfx::Size");
}

}  // namespace

void RegisterPropertyEditor(PropertyEditorFactory factory,
                            std::string_view property_type,
                            std::string_view property_name,
                            std::string_view class_name) {
  GetRegistry().push_back({std::move(factory), std::string(property_type),
                           std::string(property_name),
                           std::string(class_name)});

  // Keep registry sorted by specificity descending.
  std::stable_sort(GetRegistry().begin(), GetRegistry().end(),
                   [](const auto& a, const auto& b) {
                     return a.GetSpecificity() > b.GetSpecificity();
                   });
}

std::unique_ptr<DesignerPropertyEditor> CreatePropertyEditor(
    View* view,
    ui::metadata::MemberMetaDataBase* meta_data) {
  RegisterDefaults();

  for (const auto& reg : GetRegistry()) {
    if (!reg.class_name.empty() &&
        !IsAssignableTo(view->GetClassMetaData(), reg.class_name)) {
      continue;
    }
    if (!reg.property_name.empty() &&
        reg.property_name != meta_data->member_name()) {
      continue;
    }
    if (!reg.property_type.empty() &&
        reg.property_type != meta_data->member_type()) {
      continue;
    }

    return reg.factory.Run(view, meta_data);
  }

  // Fallback 1: Enums
  if (!meta_data->GetValidValues().empty()) {
    return std::make_unique<EnumPropertyEditor>(view, meta_data);
  }

  // Fallback 2: Default string editor
  return std::make_unique<StringPropertyEditor>(view, meta_data);
}

}  // namespace views::examples
