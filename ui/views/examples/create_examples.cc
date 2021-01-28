// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/create_examples.h"

#include <utility>

#include "ui/views/examples/ax_example.h"
#include "ui/views/examples/box_layout_example.h"
#include "ui/views/examples/bubble_example.h"
#include "ui/views/examples/button_example.h"
#include "ui/views/examples/button_sticker_sheet.h"
#include "ui/views/examples/checkbox_example.h"
#include "ui/views/examples/colored_dialog_example.h"
#include "ui/views/examples/combobox_example.h"
#include "ui/views/examples/dialog_example.h"
#include "ui/views/examples/flex_layout_example.h"
#include "ui/views/examples/label_example.h"
#include "ui/views/examples/link_example.h"
#include "ui/views/examples/login_bubble_dialog_example.h"
#include "ui/views/examples/menu_example.h"
#include "ui/views/examples/message_box_example.h"
#include "ui/views/examples/multiline_example.h"
#include "ui/views/examples/native_theme_example.h"
#include "ui/views/examples/progress_bar_example.h"
#include "ui/views/examples/radio_button_example.h"
#include "ui/views/examples/scroll_view_example.h"
#include "ui/views/examples/slider_example.h"
#include "ui/views/examples/tabbed_pane_example.h"
#include "ui/views/examples/table_example.h"
#include "ui/views/examples/text_example.h"
#include "ui/views/examples/textarea_example.h"
#include "ui/views/examples/textfield_example.h"
#include "ui/views/examples/throbber_example.h"
#include "ui/views/examples/toggle_button_example.h"
#include "ui/views/examples/tree_view_example.h"
#include "ui/views/examples/vector_example.h"
#include "ui/views/examples/widget_example.h"

namespace views {
namespace examples {

// Creates the default set of examples.
ExampleVector CreateExamples(ExampleVector extra_examples) {
  ExampleVector examples = std::move(extra_examples);
  examples.push_back(std::make_unique<AxExample>());
  examples.push_back(std::make_unique<BoxLayoutExample>());
  examples.push_back(std::make_unique<BubbleExample>());
  examples.push_back(std::make_unique<ButtonExample>());
  examples.push_back(std::make_unique<ButtonStickerSheet>());
  examples.push_back(std::make_unique<CheckboxExample>());
  examples.push_back(std::make_unique<ColoredDialogExample>());
  examples.push_back(std::make_unique<ComboboxExample>());
  examples.push_back(std::make_unique<DialogExample>());
  examples.push_back(std::make_unique<FlexLayoutExample>());
  examples.push_back(std::make_unique<LabelExample>());
  examples.push_back(std::make_unique<LinkExample>());
  examples.push_back(std::make_unique<LoginBubbleDialogExample>());
  examples.push_back(std::make_unique<MenuExample>());
  examples.push_back(std::make_unique<MessageBoxExample>());
  examples.push_back(std::make_unique<MultilineExample>());
  examples.push_back(std::make_unique<NativeThemeExample>());
  examples.push_back(std::make_unique<ProgressBarExample>());
  examples.push_back(std::make_unique<RadioButtonExample>());
  examples.push_back(std::make_unique<ScrollViewExample>());
  examples.push_back(std::make_unique<SliderExample>());
  examples.push_back(std::make_unique<TabbedPaneExample>());
  examples.push_back(std::make_unique<TableExample>());
  examples.push_back(std::make_unique<TextExample>());
  examples.push_back(std::make_unique<TextareaExample>());
  examples.push_back(std::make_unique<TextfieldExample>());
  examples.push_back(std::make_unique<ToggleButtonExample>());
  examples.push_back(std::make_unique<ThrobberExample>());
  examples.push_back(std::make_unique<TreeViewExample>());
  examples.push_back(std::make_unique<VectorExample>());
  examples.push_back(std::make_unique<WidgetExample>());
  return examples;
}

}  // namespace examples
}  // namespace views
