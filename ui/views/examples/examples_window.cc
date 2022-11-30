// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_window.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/cxx20_erase.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/ui_base_paths.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/examples/create_examples.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views::examples {

const char kExamplesWidgetName[] = "ExamplesWidget";
static const char kEnableExamples[] = "enable-examples";
static const char kEnableSidePanel[] = "side-panel";

bool CheckCommandLineUsage() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("help")) {
    // Print the program usage.
    std::cout << "Usage: " << command_line->GetProgram() << " [--"
              << kEnableExamples << "=<example1,[example2...]>]\n";
    return true;
  }
  return false;
}

namespace {

ExampleVector GetExamplesToShow(ExampleVector examples) {
  using StringVector = std::vector<std::string>;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::sort(examples.begin(), examples.end(), [](const auto& a, const auto& b) {
    return a->example_title() < b->example_title();
  });

  std::string enable_examples =
      command_line->GetSwitchValueASCII(kEnableExamples);

  if (!enable_examples.empty()) {
    // Filter examples to show based on the command line switch.
    StringVector enabled =
        base::SplitString(enable_examples, ";,", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    // Transform list of examples to just the list of names.
    StringVector example_names;
    std::transform(
        examples.begin(), examples.end(), std::back_inserter(example_names),
        [](const auto& example) { return example->example_title(); });

    std::sort(enabled.begin(), enabled.end());

    // Get an intersection of list of titles between the full list and the list
    // from the command-line.
    StringVector valid_examples =
        base::STLSetIntersection<StringVector>(enabled, example_names);

    // If there are still example names in the list, only include the examples
    // from the list.
    if (!valid_examples.empty()) {
      base::EraseIf(examples, [valid_examples](auto& example) {
        return std::find(valid_examples.begin(), valid_examples.end(),
                         example->example_title()) == valid_examples.end();
      });
    }
  } else if (command_line->HasSwitch(kEnableExamples)) {
    std::string titles;
    for (auto& example : examples) {
      titles += "\n\t";
      titles += example->example_title();
    }
    titles += "\n";
    std::cout << "By default, all examples will be shown.";
    std::cout << "You may want to specify the example(s) you want to run:"
              << titles;
  }
  return examples;
}

}  // namespace

// Model for the examples that are being added via AddExample().
class ComboboxModelExampleList : public ui::ComboboxModel {
 public:
  ComboboxModelExampleList() = default;

  ComboboxModelExampleList(const ComboboxModelExampleList&) = delete;
  ComboboxModelExampleList& operator=(const ComboboxModelExampleList&) = delete;

  ~ComboboxModelExampleList() override = default;

  void SetExamples(ExampleVector examples) {
    example_list_ = std::move(examples);
  }

  // ui::ComboboxModel:
  size_t GetItemCount() const override { return example_list_.size(); }
  std::u16string GetItemAt(size_t index) const override {
    return base::UTF8ToUTF16(example_list_[index]->example_title());
  }

  View* GetItemViewAt(size_t index) {
    return example_list_[index]->example_view();
  }

 private:
  ExampleVector example_list_;
};

class ExamplesWindowContents : public WidgetDelegateView {
 public:
  ExamplesWindowContents(base::OnceClosure on_close, ExampleVector examples)
      : on_close_(std::move(on_close)) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

    SetHasWindowSizeControls(true);
    SetBackground(CreateThemedSolidBackground(ui::kColorDialogBackground));

    if (command_line->HasSwitch(kEnableSidePanel)) {
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          BoxLayout::Orientation::kHorizontal, gfx::Insets(0)));

      auto tabbed_pane =
          std::make_unique<TabbedPane>(TabbedPane::Orientation::kVertical,
                                       TabbedPane::TabStripStyle::kBorder);

      tabbed_pane_ = AddChildView(std::move(tabbed_pane));
      CreateSidePanel(std::move(examples));
    } else {
      for (auto& example : examples)
        example->CreateExampleView(example->example_view());

      auto combobox_model = std::make_unique<ComboboxModelExampleList>();
      combobox_model_ = combobox_model.get();
      combobox_model_->SetExamples(std::move(examples));
      auto combobox = std::make_unique<Combobox>(std::move(combobox_model));

      combobox->SetCallback(base::BindRepeating(
          &ExamplesWindowContents::ComboboxChanged, base::Unretained(this)));
      combobox->SetAccessibleName(
          l10n_util::GetStringUTF16(IDS_EXAMPLES_COMBOBOX_AX_LABEL));

      auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
          BoxLayout::Orientation::kVertical, gfx::Insets(5)));

      combobox_ = AddChildView(std::move(combobox));

      auto item_count = combobox_model_->GetItemCount();
      if (item_count > 0) {
        combobox_->SetVisible(item_count > 1);
        example_shown_ = AddChildView(std::make_unique<View>());
        example_shown_->SetLayoutManager(std::make_unique<FillLayout>());
        example_shown_->AddChildView(combobox_model_->GetItemViewAt(0));
        layout->SetFlexForView(example_shown_, 1);
      }
    }
    status_label_ = AddChildView(std::make_unique<Label>());
    instance_ = this;
  }

  ExamplesWindowContents(const ExamplesWindowContents&) = delete;
  ExamplesWindowContents& operator=(const ExamplesWindowContents&) = delete;

  ~ExamplesWindowContents() override = default;

  // Sets the status area (at the bottom of the window) to |status|.
  void SetStatus(const std::string& status) {
    status_label_->SetText(base::UTF8ToUTF16(status));
  }

  static ExamplesWindowContents* instance() { return instance_; }

 private:
  // WidgetDelegateView:
  std::u16string GetWindowTitle() const override { return u"Views Examples"; }
  void WindowClosing() override {
    instance_ = nullptr;
    if (on_close_)
      std::move(on_close_).Run();
  }
  gfx::Size CalculatePreferredSize() const override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    gfx::Size size(800, 300);
    if (command_line->HasSwitch(kEnableSidePanel)) {
      for (size_t i = 0; i < tabbed_pane_->GetTabCount(); i++) {
        size.set_height(std::max(
            size.height(),
            tabbed_pane_->GetTabAt(i)->contents()->GetHeightForWidth(800)));
      }
    } else {
      for (size_t i = 0; i < combobox_model_->GetItemCount(); i++) {
        size.set_height(std::max(
            size.height(),
            combobox_model_->GetItemViewAt(i)->GetHeightForWidth(800)));
      }
    }
    return size;
  }
  gfx::Size GetMinimumSize() const override { return gfx::Size(50, 50); }

  void ComboboxChanged() {
    size_t index = combobox_->GetSelectedIndex().value();
    DCHECK_LT(index, combobox_model_->GetItemCount());
    example_shown_->RemoveAllChildViewsWithoutDeleting();
    example_shown_->AddChildView(combobox_model_->GetItemViewAt(index));
    example_shown_->RequestFocus();
    SetStatus(std::string());
    InvalidateLayout();
  }

  void CreateSidePanel(ExampleVector examples) {
    for (auto& example : examples) {
      auto tab_contents = std::make_unique<View>();
      tabbed_pane_->AddTab(base::UTF8ToUTF16(example->example_title()),
                           std::move(tab_contents));
    }
    // Currently only create a few examples as some will crash the application
    // on run.
    for (int i = 0; i < 11; i++) {
      examples[i]->CreateExampleView(tabbed_pane_->GetTabAt(i)->contents());
    }
  }

  static ExamplesWindowContents* instance_;
  raw_ptr<View> example_shown_ = nullptr;
  raw_ptr<Label> status_label_ = nullptr;
  base::OnceClosure on_close_;
  raw_ptr<TabbedPane> tabbed_pane_ = nullptr;
  raw_ptr<Combobox> combobox_ = nullptr;
  // Owned by |combobox_|.
  raw_ptr<ComboboxModelExampleList> combobox_model_ = nullptr;
};

// static
ExamplesWindowContents* ExamplesWindowContents::instance_ = nullptr;

Widget* GetExamplesWidget() {
  return ExamplesWindowContents::instance()
             ? ExamplesWindowContents::instance()->GetWidget()
             : nullptr;
}

void ShowExamplesWindow(base::OnceClosure on_close,
                        ExampleVector examples,
                        gfx::NativeWindow window_context) {
  if (ExamplesWindowContents::instance()) {
    ExamplesWindowContents::instance()->GetWidget()->Activate();
  } else {
    examples = GetExamplesToShow(std::move(examples));
    Widget* widget = new Widget;
    Widget::InitParams params;
    params.delegate =
        new ExamplesWindowContents(std::move(on_close), std::move(examples));
    params.context = window_context;
    params.name = kExamplesWidgetName;
    widget->Init(std::move(params));
    widget->Show();
  }
}

void LogStatus(const std::string& string) {
  if (ExamplesWindowContents::instance())
    ExamplesWindowContents::instance()->SetStatus(string);
}

}  // namespace views::examples
