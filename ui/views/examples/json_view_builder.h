// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_JSON_VIEW_BUILDER_H_
#define UI_VIEWS_EXAMPLES_JSON_VIEW_BUILDER_H_

#include <memory>
#include <string>

#include "ui/gfx/native_ui_types.h"
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
class Widget;
}

namespace views::examples {

// JsonViewBuilder provides declarative runtime instantiation and property
// application for Views UI components, Dialogs, Bubbles, and DialogModels from
// JSON specifications without requiring recompilation.
//
// The complete JSON Schema specification describing all supported component
// types, layout managers, properties, and dynamic token resolvers is documented
// in:
//   ui/views/examples/json_view_builder_schema.md
//
// Whenever new components, properties, or converters are added or modified in
// JsonViewBuilder, the json_view_builder_schema.md file MUST also be updated to
// maintain congruency.
class VIEWS_EXAMPLES_EXPORT JsonViewBuilder {
 public:
  // Tree Construction.
  static std::unique_ptr<views::View> BuildView(const base::DictValue& dict,
                                                std::string* error_msg);

  // Property Application.
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

  struct VIEWS_EXAMPLES_EXPORT DialogWidgetResult {
    DialogWidgetResult();
    DialogWidgetResult(std::unique_ptr<views::Widget> widget,
                       std::unique_ptr<views::DialogDelegate> delegate);
    ~DialogWidgetResult();
    DialogWidgetResult(DialogWidgetResult&&);
    DialogWidgetResult& operator=(DialogWidgetResult&&);

    // Member destruction order is reverse-declaration order. Declaring
    // `delegate` before `widget` (along with explicit reset in the destructor)
    // guarantees that `widget` is destroyed before `delegate`.
    std::unique_ptr<views::DialogDelegate> delegate;
    std::unique_ptr<views::Widget> widget;
  };

  // Helper to instantiate, configure, and show a Widget using
  // CLIENT_OWNS_WIDGET ownership for any supported JSON spec (DialogModel,
  // Dialog, Bubble). The caller retains ownership of both the returned Widget
  // and DialogDelegate.
  static DialogWidgetResult ShowDialogOrBubble(const base::DictValue& dict,
                                               views::View* anchor_view,
                                               gfx::NativeWindow parent_window,
                                               std::string* error_msg);
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_JSON_VIEW_BUILDER_H_
