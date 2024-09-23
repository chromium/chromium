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
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_paths.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/examples/create_examples.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views::examples {

const char kExamplesWidgetName[] = "ExamplesWidget";
static const char kEnableExamples[] = "enable-examples";

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
    base::ranges::transform(examples, std::back_inserter(example_names),
                            &ExampleBase::example_title);

    base::ranges::sort(enabled);

    // Get an intersection of list of titles between the full list and the list
    // from the command-line.
    StringVector valid_examples =
        base::STLSetIntersection<StringVector>(enabled, example_names);

    // If there are still example names in the list, only include the examples
    // from the list.
    if (!valid_examples.empty()) {
      std::erase_if(examples, [valid_examples](auto& example) {
        return !base::Contains(valid_examples, example->example_title());
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

class ExamplesWindowContents : public WidgetDelegateView,
                               public TabbedPaneListener {
 public:
  ExamplesWindowContents(base::OnceClosure on_close, ExampleVector examples)
      : on_close_(std::move(on_close)) {
    SetHasWindowSizeControls(true);
    SetBackground(CreateThemedSolidBackground(ui::kColorDialogBackground));

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        BoxLayout::Orientation::kVertical, gfx::Insets(0)));

    auto tabbed_pane =
        std::make_unique<TabbedPane>(TabbedPane::Orientation::kVertical,
                                     TabbedPane::TabStripStyle::kBorder, true);

    tabbed_pane_ = AddChildView(std::move(tabbed_pane));
    layout->SetFlexForView(tabbed_pane_, 1);
    CreateSidePanel(std::move(examples));

    status_label_ = AddChildView(std::make_unique<Label>());
    status_label_->SetVisible(false);
    tabbed_pane_->set_listener(this);
    instance_ = this;
  }

  ExamplesWindowContents(const ExamplesWindowContents&) = delete;
  ExamplesWindowContents& operator=(const ExamplesWindowContents&) = delete;

  ~ExamplesWindowContents() override = default;

  // Sets the status area (at the bottom of the window) to |status|.
  void SetStatus(const std::string& status) {
    status_label_->SetText(base::UTF8ToUTF16(status));
    status_label_->SetVisible(!status.empty());
  }

  void TabSelectedAt(int index) override { status_label_->SetVisible(false); }

  static ExamplesWindowContents* instance() { return instance_; }

 private:
  // WidgetDelegateView:
  std::u16string GetWindowTitle() const override { return u"Views Examples"; }
  void WindowClosing() override {
    instance_ = nullptr;
    if (on_close_)
      std::move(on_close_).Run();
  }
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override {
    gfx::Size size(800, 300);
    for (size_t i = 0; i < tabbed_pane_->GetTabCount(); i++) {
      size.set_height(std::max(
          size.height(),
          tabbed_pane_->GetTabAt(i)->contents()->GetHeightForWidth(800)));
    }
    return size;
  }
  gfx::Size GetMinimumSize() const override { return gfx::Size(50, 50); }

  void CreateSidePanel(ExampleVector examples) {
    for (auto& example : examples) {
      auto tab_contents = std::make_unique<View>();
      example->CreateExampleView(tab_contents.get());
      example->SetContainer(
          tabbed_pane_->AddTab(base::UTF8ToUTF16(example->example_title()),
                               std::move(tab_contents)));
    }
    examples_ = std::move(examples);
  }

  static ExamplesWindowContents* instance_;
  raw_ptr<Label> status_label_ = nullptr;
  base::OnceClosure on_close_;
  raw_ptr<TabbedPane> tabbed_pane_ = nullptr;
  ExampleVector examples_;
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
    Widget::InitParams params(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                              Widget::InitParams::TYPE_WINDOW);
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
