// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_timeouts.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/lottie_resource.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/lottie/animation.h"
#include "ui/views/background.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_main_proc.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

class LottieImageSource : public gfx::CanvasImageSource {
 public:
  LottieImageSource(lottie::Animation* animation, float t)
      : gfx::CanvasImageSource(animation->GetOriginalSize()),
        animation_(animation),
        t_(t) {}
  LottieImageSource(const LottieImageSource&) = delete;
  LottieImageSource& operator=(const LottieImageSource&) = delete;
  ~LottieImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    animation_->PaintFrame(canvas, t_, size());
  }

 private:
  const raw_ptr<lottie::Animation> animation_;
  const float t_;
};

class LottieExample : public views::examples::ExampleBase {
 public:
  explicit LottieExample(const gfx::ImageSkia& image)
      : ExampleBase("Lottie Example"), image_(image) {}

  explicit LottieExample(std::unique_ptr<lottie::Animation> animation)
      : ExampleBase("Lottie Example"), animation_(std::move(animation)) {}

  LottieExample(const LottieExample&) = delete;
  LottieExample& operator=(const LottieExample&) = delete;

  ~LottieExample() override = default;

  // views::examples::ExampleBase:
  void CreateExampleView(views::View* parent) override {
    auto* layout = parent->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    parent->SetBackground(views::CreateSolidBackground(SK_ColorGRAY));

    if (animation_) {
      auto* animated_image_view =
          parent->AddChildView(std::make_unique<views::AnimatedImageView>());
      animated_image_view->SetAnimatedImage(std::move(animation_));
      animated_image_view->Play();
      animated_image_view->SetBackground(
          views::CreateSolidBackground(SK_ColorWHITE));
    } else {
      auto* image_view =
          parent->AddChildView(std::make_unique<views::ImageView>());
      image_view->SetImage(ui::ImageModel::FromImageSkia(image_));
      image_view->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
    }
  }

 private:
  gfx::ImageSkia image_;
  std::unique_ptr<lottie::Animation> animation_;
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  TestTimeouts::Initialize();
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool static_mode = command_line->HasSwitch("static");
  auto args = command_line->GetArgs();
  if (args.size() != 1) {
    std::cerr
        << "Usage: lottie_viewer [--static[=frame_number]] <input.json>\n";
    return 1;
  }

  std::string file_content;
  if (!base::ReadFileToString(base::FilePath(args[0]), &file_content)) {
    std::cerr << "Cannot read input file " << args[0] << "\n";
    return 1;
  }

  std::vector<uint8_t> data_vector(file_content.begin(), file_content.end());
  auto skottie = cc::SkottieWrapper::UnsafeCreateSerializable(std::move(data_vector));
  if (!skottie || !skottie->is_valid()) {
    std::cerr << "Failed to parse Lottie file\n";
    return 1;
  }

  auto animation = std::make_unique<lottie::Animation>(skottie);
  bool use_animated_mode =
      !static_mode && !animation->GetAnimationDuration().is_zero();

  std::cout << "Lottie info:\n"
            << "  Duration: " << animation->GetAnimationDuration() << "\n"
            << "  Mode: " << (use_animated_mode ? "animated" : "static")
            << "\n";

  views::examples::ExampleVector examples;
  if (use_animated_mode) {
    examples.push_back(std::make_unique<LottieExample>(std::move(animation)));
  } else {
    int frame = 0;
    std::string static_value = command_line->GetSwitchValueASCII("static");
    if (!static_value.empty()) {
      if (!base::StringToInt(static_value, &frame)) {
        std::cerr << "Invalid frame number: " << static_value << "\n";
        return 1;
      }
    }

    float t = skottie->GetNormalizedTimeForFrame(frame);

    std::cout << "  Frame: " << frame << " (t=" << t << ")\n";

    gfx::ImageSkia image_skia =
        gfx::CanvasImageSource::MakeImageSkia<LottieImageSource>(
            animation.get(), t);
    if (image_skia.isNull()) {
      std::cerr << "Failed to render Lottie frame to ImageSkia\n";
      return 1;
    }
    examples.push_back(std::make_unique<LottieExample>(image_skia));
  }

  return static_cast<int>(
      views::examples::ExamplesMainProc(false, std::move(examples)));
}
