// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/designer_property_editor.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/views/view.h"

namespace views::examples {

DesignerPropertyEditor::DesignerPropertyEditor() = default;
DesignerPropertyEditor::~DesignerPropertyEditor() = default;

std::vector<std::u16string> DesignerPropertyEditor::GetComboboxValues() const {
  return {};
}

void DesignerPropertyEditor::ShowCustomDialog(views::View* anchor_view) {}

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
