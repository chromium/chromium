// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/test/test_timeouts.h"
#include "ui/base/resource/lottie_resource.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_main_proc.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace {

class LottieExample : public views::examples::ExampleBase {
 public:
  explicit LottieExample(const gfx::ImageSkia& image)
      : ExampleBase("Lottie Example"), image_(image) {}

  LottieExample(const LottieExample&) = delete;
  LottieExample& operator=(const LottieExample&) = delete;

  ~LottieExample() override = default;

  // views::examples::ExampleBase:
  void CreateExampleView(views::View* parent) override {
    parent->SetLayoutManager(std::make_unique<views::FillLayout>());
    auto* image_view =
        parent->AddChildView(std::make_unique<views::ImageView>());
    image_view->SetImage(ui::ImageModel::FromImageSkia(image_));
  }

 private:
  gfx::ImageSkia image_;
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  TestTimeouts::Initialize();
  auto* command_line = base::CommandLine::ForCurrentProcess();
  auto args = command_line->GetArgs();
  if (args.size() != 1) {
    std::cerr << "Usage: lottie_viewer <input.json>\n";
    return 1;
  }

  std::string file_content;
  if (!base::ReadFileToString(base::FilePath(args[0]), &file_content)) {
    std::cerr << "Cannot read input file " << args[0] << "\n";
    return 1;
  }

  std::vector<uint8_t> data(file_content.begin(), file_content.end());
  gfx::ImageSkia image_skia = ui::ParseLottieAsStillImage(std::move(data));

  if (image_skia.isNull()) {
    std::cerr << "Failed to parse Lottie file to ImageSkia\n";
    return 1;
  }

  views::examples::ExampleVector examples;
  examples.push_back(std::make_unique<LottieExample>(image_skia));

  return static_cast<int>(
      views::examples::ExamplesMainProc(false, std::move(examples)));
}
