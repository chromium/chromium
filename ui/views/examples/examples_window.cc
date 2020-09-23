// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_window.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/background.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/examples/create_examples.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
namespace examples {

const char kExamplesWidgetName[] = "ExamplesWidget";

namespace {

const char kEnableExamples[] = "enable-examples";

ExampleVector GetExamplesToShow(ExampleVector examples) {
  using StringVector = std::vector<std::string>;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::sort(examples.begin(), examples.end(), [](const auto& a, const auto& b) {
    return a->example_title() < b->example_title();
  });

  std::string enable_examples =
      command_line->GetSwitchValueASCII(kEnableExamples);
  if (!enable_examples.empty()) {
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
  }

  for (auto& example : examples)
    example->CreateExampleView(example->example_view());
  return examples;
}

}  // namespace

// Model for the examples that are being added via AddExample().
class ComboboxModelExampleList : public ui::ComboboxModel {
 public:
  ComboboxModelExampleList() = default;
  ~ComboboxModelExampleList() override = default;

  void SetExamples(ExampleVector examples) {
    example_list_ = std::move(examples);
  }

  // ui::ComboboxModel:
  int GetItemCount() const override { return example_list_.size(); }
  base::string16 GetItemAt(int index) const override {
    return base::UTF8ToUTF16(example_list_[index]->example_title());
  }

  View* GetItemViewAt(int index) {
    return example_list_[index]->example_view();
  }

 private:
  ExampleVector example_list_;

  DISALLOW_COPY_AND_ASSIGN(ComboboxModelExampleList);
};

class ExamplesWindowContents : public WidgetDelegateView {
 public:
  ExamplesWindowContents(base::OnceClosure on_close, ExampleVector examples)
      : on_close_(std::move(on_close)) {
    SetHasWindowSizeControls(true);

    auto combobox_model = std::make_unique<ComboboxModelExampleList>();
    combobox_model_ = combobox_model.get();
    combobox_model_->SetExamples(std::move(examples));
    auto combobox = std::make_unique<Combobox>(std::move(combobox_model));

    instance_ = this;
    combobox->set_callback(base::BindRepeating(
        &ExamplesWindowContents::ComboboxChanged, base::Unretained(this)));

    SetBackground(CreateThemedSolidBackground(
        this, ui::NativeTheme::kColorId_DialogBackground));
    GridLayout* layout =
        SetLayoutManager(std::make_unique<views::GridLayout>());
    ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddPaddingColumn(0, 5);
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                          GridLayout::ColumnSize::kUsePreferred, 0, 0);
    column_set->AddPaddingColumn(0, 5);
    layout->AddPaddingRow(0, 5);
    layout->StartRow(0 /* no expand */, 0);
    combobox_ = layout->AddView(std::move(combobox));

    auto item_count = combobox_model_->GetItemCount();
    if (item_count > 0) {
      combobox_->SetVisible(item_count > 1);
      layout->StartRow(1, 0);
      auto example_shown = std::make_unique<View>();
      example_shown->SetLayoutManager(std::make_unique<FillLayout>());
      example_shown->AddChildView(combobox_model_->GetItemViewAt(0));
      example_shown_ = layout->AddView(std::move(example_shown));
    }

    layout->StartRow(0 /* no expand */, 0);
    status_label_ = layout->AddView(std::make_unique<Label>());
    layout->AddPaddingRow(0, 5);
  }

  ~ExamplesWindowContents() override = default;

  // Sets the status area (at the bottom of the window) to |status|.
  void SetStatus(const std::string& status) {
    status_label_->SetText(base::UTF8ToUTF16(status));
  }

  static ExamplesWindowContents* instance() { return instance_; }

 private:
  // WidgetDelegateView:
  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("Views Examples");
  }
  void WindowClosing() override {
    instance_ = nullptr;
    if (on_close_)
      std::move(on_close_).Run();
  }
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size(800, 300);
    for (int i = 0; i < combobox_model_->GetItemCount(); i++) {
      size.set_height(
          std::max(size.height(),
                   combobox_model_->GetItemViewAt(i)->GetHeightForWidth(800)));
    }
    return size;
  }

  void ComboboxChanged() {
    int index = combobox_->GetSelectedIndex();
    DCHECK_LT(index, combobox_model_->GetItemCount());
    example_shown_->RemoveAllChildViews(false);
    example_shown_->AddChildView(combobox_model_->GetItemViewAt(index));
    example_shown_->RequestFocus();
    SetStatus(std::string());
    InvalidateLayout();
  }

  static ExamplesWindowContents* instance_;
  View* example_shown_ = nullptr;
  Label* status_label_ = nullptr;
  base::OnceClosure on_close_;
  Combobox* combobox_ = nullptr;
  // Owned by |combobox_|.
  ComboboxModelExampleList* combobox_model_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExamplesWindowContents);
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

}  // namespace examples
}  // namespace views
