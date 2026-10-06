// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_JSON_VIEW_BUILDER_H_
#define UI_VIEWS_EXAMPLES_JSON_VIEW_BUILDER_H_

#include <memory>
#include <string>

#include "ui/views/examples/views_examples_export.h"

namespace base {
class DictValue;
}

namespace ui {
class DialogModel;
}

namespace views {
class DialogDelegate;
class View;
}

namespace views::examples {

// JsonViewBuilder provides declarative runtime instantiation and property
// application for Views UI components, Dialogs, Bubbles, and DialogModels from
// JSON specifications without requiring recompilation.
//
// The complete JSON Schema specification describing all supported component
// types, layout managers, properties, and dynamic token resolvers is documented
// in json_view_builder_schema.md.
class VIEWS_EXAMPLES_EXPORT JsonViewBuilder {
 public:
  // Instantiates a new views::View subclass instance corresponding to the
  // "type" property in `dict`. Returns nullptr on failure and populates
  // `error_msg` if provided.
  static std::unique_ptr<views::View> BuildView(const base::DictValue& dict,
                                                std::string* error_msg);

  // Recursively applies property values from `dict` onto `view` and its
  // children using Views metadata reflection and property-specific handlers.
  // Returns false if validation or type conversion fails.
  static bool ApplyPropertiesRecursive(views::View* view,
                                       const base::DictValue& dict,
                                       std::string* error_msg);

  // Dialog & Bubble Support.
  // Returns true if `dict` represents a top-level Dialog, Bubble, or
  // DialogModel.
  static bool IsDialogOrBubbleSpec(const base::DictValue& dict);

  // Returns true if `dict` represents a DialogModel specification.
  static bool IsDialogModelSpec(const base::DictValue& dict);

  // Builds a ui::DialogModel from a JSON specification ("type": "DialogModel").
  static std::unique_ptr<ui::DialogModel> BuildDialogModel(
      const base::DictValue& dict,
      std::string* error_msg);

  // Creates a DialogDelegate or BubbleDialogDelegate from an explicit Dialog or
  // Bubble JSON specification ("type": "Dialog" or "type": "Bubble").
  static std::unique_ptr<views::DialogDelegate> BuildDialogDelegate(
      const base::DictValue& dict,
      std::string* error_msg);
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_JSON_VIEW_BUILDER_H_
