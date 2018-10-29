// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/animated_image_view_example.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

namespace {

// This class can load a skottie(and lottie) animation file from disk and play
// it in a view as AnimatedImageView.
// See https://skia.org/user/modules/skottie for more info on skottie.
class AnimationGallery : public View,
                         public TextfieldController,
                         public ButtonListener {
 public:
  AnimationGallery()
      : animated_image_view_(new AnimatedImageView()),
        image_view_container_(new views::View()),
        size_input_(new Textfield()),
        file_chooser_(new Textfield()),
        file_go_button_(
            MdTextButton::Create(this, base::ASCIIToUTF16("Render"))) {
    AddChildView(size_input_);

    image_view_container_->AddChildView(animated_image_view_);
    image_view_container_->SetLayoutManager(std::make_unique<FillLayout>());
    animated_image_view_->SetBorder(
        CreateSolidSidedBorder(1, 1, 1, 1, SK_ColorBLACK));
    AddChildView(image_view_container_);

    BoxLayout* box = SetLayoutManager(
        std::make_unique<BoxLayout>(BoxLayout::kVertical, gfx::Insets(10), 10));
    box->SetFlexForView(image_view_container_, 1);

    file_chooser_->set_placeholder_text(
        base::ASCIIToUTF16("Enter path to lottie JSON file"));
    View* file_container = new View();
    BoxLayout* file_box =
        file_container->SetLayoutManager(std::make_unique<BoxLayout>(
            BoxLayout::kHorizontal, gfx::Insets(10), 10));
    file_container->AddChildView(file_chooser_);
    file_container->AddChildView(file_go_button_);
    file_box->SetFlexForView(file_chooser_, 1);
    AddChildView(file_container);

    size_input_->set_placeholder_text(
        base::ASCIIToUTF16("Size in dip (Empty for default)"));
    size_input_->set_controller(this);
  }

  ~AnimationGallery() override = default;

  // TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const base::string16& new_contents) override {
    if (sender == size_input_) {
      if (!base::StringToInt(new_contents, &size_) && (size_ > 0)) {
        size_ = 0;
        size_input_->SetText(base::string16());
      }
      Update();
    }
  }

  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override {
    DCHECK_EQ(file_go_button_, sender);
    std::string json;
    base::ScopedAllowBlockingForTesting allow_blocking;
#if defined(OS_POSIX)
    base::FilePath path(base::UTF16ToUTF8(file_chooser_->text()));
#else
    base::FilePath path(file_chooser_->text());
#endif  // defined(OS_POSIX)
    base::ReadFileToString(path, &json);

    auto skottie = base::MakeRefCounted<cc::SkottieWrapper>(
        base::RefCountedString::TakeString(&json));
    animated_image_view_->SetAnimatedImage(
        std::make_unique<gfx::SkiaVectorAnimation>(skottie));
    animated_image_view_->Play();
    Update();
  }

 private:
  void Update() {
    if (size_ > 24)
      animated_image_view_->SetImageSize(gfx::Size(size_, size_));
    else
      animated_image_view_->ResetImageSize();
    Layout();
  }

  AnimatedImageView* animated_image_view_;
  View* image_view_container_;
  Textfield* size_input_;
  Textfield* file_chooser_;
  Button* file_go_button_;

  int size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AnimationGallery);
};

}  // namespace

AnimatedImageViewExample::AnimatedImageViewExample()
    : ExampleBase("Animated Image View") {}

AnimatedImageViewExample::~AnimatedImageViewExample() {}

void AnimatedImageViewExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(new AnimationGallery());
}

}  // namespace examples
}  // namespace views
