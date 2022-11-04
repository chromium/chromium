// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/vector_example.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views::examples {

namespace {

class VectorIconGallery : public View, public TextfieldController {
 public:
  VectorIconGallery() {
    size_input_ = AddChildView(std::make_unique<Textfield>());
    color_input_ = AddChildView(std::make_unique<Textfield>());

    auto image_view_container = std::make_unique<views::View>();
    image_view_ =
        image_view_container->AddChildView(std::make_unique<ImageView>());
    auto image_layout =
        std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
    image_layout->set_cross_axis_alignment(
        BoxLayout::CrossAxisAlignment::kCenter);
    image_layout->set_main_axis_alignment(
        BoxLayout::MainAxisAlignment::kCenter);
    image_view_container->SetLayoutManager(std::move(image_layout));
    image_view_->SetBorder(CreateThemedSolidBorder(
        1, ExamplesColorIds::kColorVectorExampleImageBorder));
    image_view_container_ = AddChildView(std::move(image_view_container));

    BoxLayout* box = SetLayoutManager(std::make_unique<BoxLayout>(
        BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));
    box->SetFlexForView(image_view_container_, 1);

    auto file_chooser = std::make_unique<Textfield>();
    file_chooser->SetPlaceholderText(
        GetStringUTF16(IDS_VECTOR_FILE_SELECT_LABEL));
    auto file_container = std::make_unique<View>();
    BoxLayout* file_box =
        file_container->SetLayoutManager(std::make_unique<BoxLayout>(
            BoxLayout::Orientation::kHorizontal, gfx::Insets(10), 10));
    file_chooser_ = file_container->AddChildView(std::move(file_chooser));
    file_go_button_ =
        file_container->AddChildView(std::make_unique<MdTextButton>(
            base::BindRepeating(&VectorIconGallery::FileGoButtonPressed,
                                base::Unretained(this)),
            GetStringUTF16(IDS_VECTOR_RENDER_LABEL)));
    file_box->SetFlexForView(file_chooser_, 1);
    AddChildView(std::move(file_container));

    size_input_->SetPlaceholderText(
        GetStringUTF16(IDS_VECTOR_DIP_SIZE_DESC_LABEL));
    size_input_->set_controller(this);
    color_input_->SetPlaceholderText(
        GetStringUTF16(IDS_VECTOR_COLOR_DESC_LABEL));
    color_input_->set_controller(this);
  }

  VectorIconGallery(const VectorIconGallery&) = delete;
  VectorIconGallery& operator=(const VectorIconGallery&) = delete;

  ~VectorIconGallery() override = default;

  // TextfieldController implementation.
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override {
    if (sender == size_input_) {
      if (base::StringToInt(new_contents, &size_) && (size_ > 0))
        Update();
      else
        size_input_->SetText(std::u16string());

      return;
    }

    DCHECK_EQ(color_input_, sender);
    if (new_contents.size() != 8u)
      return;
    unsigned new_color =
        strtoul(base::UTF16ToASCII(new_contents).c_str(), nullptr, 16);
    if (new_color <= 0xffffffff) {
      color_ = new_color;
      Update();
    }
  }

 private:
  void FileGoButtonPressed() {
    base::ScopedAllowBlockingForTesting allow_blocking;
#if BUILDFLAG(IS_WIN)
    base::FilePath path(base::UTF16ToWide(file_chooser_->GetText()));
#else
    base::FilePath path(base::UTF16ToUTF8(file_chooser_->GetText()));
#endif
    base::ReadFileToString(path, &contents_);
    // Skip over comments.
    for (size_t slashes = contents_.find("//"); slashes != std::string::npos;
         slashes = contents_.find("//")) {
      size_t eol = contents_.find("\n", slashes);
      contents_.erase(slashes, eol - slashes);
    }
    Update();
  }

  void Update() {
    if (!contents_.empty()) {
      image_view_->SetImage(
          gfx::CreateVectorIconFromSource(contents_, size_, color_));
    }
    InvalidateLayout();
  }

  // 36dp is one of the natural sizes for MD icons, and corresponds roughly to a
  // 32dp usable area.
  int size_ = 36;
  SkColor color_ = gfx::kPlaceholderColor;

  raw_ptr<ImageView> image_view_;
  raw_ptr<View> image_view_container_;
  raw_ptr<Textfield> size_input_;
  raw_ptr<Textfield> color_input_;
  raw_ptr<Textfield> file_chooser_;
  raw_ptr<Button> file_go_button_;
  std::string contents_;
};

}  // namespace

VectorExample::VectorExample()
    : ExampleBase(GetStringUTF8(IDS_VECTOR_SELECT_LABEL).c_str()) {}

VectorExample::~VectorExample() = default;

void VectorExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(std::make_unique<VectorIconGallery>());
}

}  // namespace views::examples
