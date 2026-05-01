// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/test/test_timeouts.h"
// To use CreateVectorIconFromSource.
#define GFX_VECTOR_ICONS_UNSAFE
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_main_proc.h"
#include "ui/views/examples/vector_example.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace {

class VectorIconViewerExample : public views::examples::ExampleBase {
 public:
  explicit VectorIconViewerExample(const std::string& icon_source)
      : ExampleBase("Vector Icon Viewer"), icon_source_(icon_source) {}

  VectorIconViewerExample(const VectorIconViewerExample&) = delete;
  VectorIconViewerExample& operator=(const VectorIconViewerExample&) = delete;

  ~VectorIconViewerExample() override = default;

  void CreateExampleView(views::View* container) override {
    container->SetLayoutManager(std::make_unique<views::FillLayout>());
    auto image_view = std::make_unique<views::ImageView>();
    image_view->SetImage(
        ui::ImageModel::FromImageSkia(gfx::CreateVectorIconFromSource(
            views::examples::CleanUpContents(icon_source_), 128,
            SK_ColorBLACK)));
    container->AddChildView(std::move(image_view));
  }

 private:
  std::string icon_source_;
};

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  TestTimeouts::Initialize();
  base::AtExitManager at_exit;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = command_line->GetArgs();

  if (args.empty()) {
    LOG(ERROR) << "Usage: vector_icon_viewer <path_to_icon_file>";
    return 1;
  }

  base::FilePath file_path(args[0]);
  std::string file_content;
  if (!base::ReadFileToString(file_path, &file_content)) {
    LOG(ERROR) << "Failed to read file: " << file_path;
    return 1;
  }

  views::examples::ExampleVector examples;
  examples.push_back(std::make_unique<VectorIconViewerExample>(file_content));

  return static_cast<int>(
      views::examples::ExamplesMainProc(false, std::move(examples)));
}
