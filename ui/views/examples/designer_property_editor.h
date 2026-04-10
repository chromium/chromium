// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_DESIGNER_PROPERTY_EDITOR_H_
#define UI_VIEWS_EXAMPLES_DESIGNER_PROPERTY_EDITOR_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_types.h"

namespace views {
class View;
}

namespace views::examples {

// DesignerPropertyEditor encapsulates the UI interaction for a given property
// on a view. It serves as an abstraction layer between the Inspector UI and
// the underlying ui::metadata property system.
class DesignerPropertyEditor {
 public:
  enum class EditorType {
    kTextField,    // Standard text input
    kCombobox,     // Drop-down list
    kCheckbox,     // Boolean toggle
    kCustomDialog  // Ellipsis button [...]
  };

  using PropertyChangedCallback = base::RepeatingClosure;

  DesignerPropertyEditor();
  DesignerPropertyEditor(const DesignerPropertyEditor&) = delete;
  DesignerPropertyEditor& operator=(const DesignerPropertyEditor&) = delete;
  virtual ~DesignerPropertyEditor();

  // --------------------------------------------------------------------------
  // Core Property Information
  // --------------------------------------------------------------------------
  virtual std::u16string GetPropertyName() const = 0;
  virtual std::u16string GetValueAsString() const = 0;

  // --------------------------------------------------------------------------
  // UI Hints & State
  // --------------------------------------------------------------------------
  virtual EditorType GetEditorType() const = 0;
  virtual bool IsReadOnly() const = 0;

  // --------------------------------------------------------------------------
  // Editing Operations
  // --------------------------------------------------------------------------

  // For kTextField / kCombobox. Returns true if the value was applied.
  virtual bool SetValueFromString(const std::u16string& value) = 0;

  // For kCombobox. Returns the list of valid strings (e.g., enum names).
  virtual std::vector<std::u16string> GetComboboxValues() const;

  // For kCustomDialog. Launches a specialized editor relative to |anchor_view|.
  virtual void ShowCustomDialog(views::View* anchor_view);

  // --------------------------------------------------------------------------
  // Hierarchy & Expansion
  // --------------------------------------------------------------------------
  virtual bool IsExpandable() const;
  virtual bool IsExpanded() const;
  virtual void SetExpanded(bool expanded);
  virtual size_t GetLevel() const;
  virtual std::vector<DesignerPropertyEditor*> GetSubEditors();

  void SetPropertyChangedCallback(PropertyChangedCallback callback) {
    callback_ = std::move(callback);
  }

 protected:
  void NotifyPropertyChanged() {
    if (callback_) {
      callback_.Run();
    }
  }

 private:
  PropertyChangedCallback callback_;
};

using PropertyEditorFactory = base::RepeatingCallback<std::unique_ptr<
    DesignerPropertyEditor>(View*, ui::metadata::MemberMetaDataBase*)>;

// Registers a new property editor. Empty strings act as wildcards.
// Registrations are automatically sorted from most-specific to least-specific.
void RegisterPropertyEditor(PropertyEditorFactory factory,
                            std::string_view property_type = {},
                            std::string_view property_name = {},
                            std::string_view class_name = {});

// Helper template to register a property editor cleanly.
template <typename EditorClass>
void RegisterPropertyEditor(std::string_view property_type = {},
                            std::string_view property_name = {},
                            std::string_view class_name = {}) {
  RegisterPropertyEditor(
      base::BindRepeating([](View* view, ui::metadata::MemberMetaDataBase* meta)
                              -> std::unique_ptr<DesignerPropertyEditor> {
        return std::make_unique<EditorClass>(view, meta);
      }),
      property_type, property_name, class_name);
}

// Factory function to create the appropriate DesignerPropertyEditor subclass
// based on the registered rules and metadata type.
std::unique_ptr<DesignerPropertyEditor> CreatePropertyEditor(
    View* view,
    ui::metadata::MemberMetaDataBase* meta_data);

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_DESIGNER_PROPERTY_EDITOR_H_
