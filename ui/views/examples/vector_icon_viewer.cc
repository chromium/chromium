// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
// To use CreateVectorIconFromSource.
#define GFX_VECTOR_ICONS_UNSAFE
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_main_proc.h"
#include "ui/views/examples/vector_example.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace {

std::optional<SkColor> ParseHexColor(const std::string& color_str) {
  std::string lower_color_str = base::ToLowerASCII(color_str);
  static constexpr auto kColorMap =
      base::MakeFixedFlatMap<std::string_view, SkColor>({
          {"black", SK_ColorBLACK},
          {"blue", SK_ColorBLUE},
          {"cyan", SK_ColorCYAN},
          {"darkgray", SK_ColorDKGRAY},
          {"gray", SK_ColorGRAY},
          {"green", SK_ColorGREEN},
          {"lightgray", SK_ColorLTGRAY},
          {"magenta", SK_ColorMAGENTA},
          {"red", SK_ColorRED},
          {"transparent", SK_ColorTRANSPARENT},
          {"white", SK_ColorWHITE},
          {"yellow", SK_ColorYELLOW},
      });

  auto it = kColorMap.find(lower_color_str);
  if (it != kColorMap.end()) {
    return it->second;
  }

  std::string hex = color_str;
  if (hex.starts_with("#")) {
    hex = hex.substr(1);
  } else if (hex.starts_with("0x") || hex.starts_with("0X")) {
    hex = hex.substr(2);
  }
  uint32_t value = 0;
  if (!base::HexStringToUInt(hex, &value)) {
    return std::nullopt;
  }
  if (hex.length() <= 6) {
    return SkColorSetA(value, 0xFF);
  }
  return value;
}

class DynamicVectorIcon {
 public:
  explicit DynamicVectorIcon(const std::string& icon_source) {
    gfx::ParsePathElements(icon_source, path_elements_);
    for (const auto& path : path_elements_) {
      reps_.push_back({path});
    }
    icon_ = std::make_unique<gfx::VectorIcon>(reps_.data(), reps_.size(),
                                              "DynamicVectorIcon");
  }

  DynamicVectorIcon(const DynamicVectorIcon&) = delete;
  DynamicVectorIcon& operator=(const DynamicVectorIcon&) = delete;

  const gfx::VectorIcon& GetVectorIcon() const { return *icon_; }

 private:
  std::vector<std::vector<gfx::PathElement>> path_elements_;
  std::vector<gfx::VectorIconRep> reps_;
  std::unique_ptr<gfx::VectorIcon> icon_;
};
class VectorIconViewerExample : public views::examples::ExampleBase {
 public:
  VectorIconViewerExample(const std::string& icon_source,
                          std::optional<int> custom_size,
                          std::optional<SkColor> icon_color,
                          std::optional<SkColor> bg_color)
      : ExampleBase("Vector Icon Viewer"),
        dynamic_icon_(views::examples::CleanUpContents(icon_source)),
        custom_size_(std::move(custom_size)),
        icon_color_(icon_color),
        bg_color_(bg_color) {}

  VectorIconViewerExample(const VectorIconViewerExample&) = delete;
  VectorIconViewerExample& operator=(const VectorIconViewerExample&) = delete;

  ~VectorIconViewerExample() override = default;

  void CreateExampleView(views::View* container) override {
    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal);
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    container->SetLayoutManager(std::move(layout));
    container->SetBackground(views::CreateSolidBackground(SK_ColorGRAY));
    auto image_view = std::make_unique<views::ImageView>();
    image_view->SetBackground(
        views::CreateSolidBackground(bg_color_.value_or(SK_ColorWHITE)));

    if (custom_size_) {
      image_view->SetPreferredSize(gfx::Size(*custom_size_, *custom_size_));
    }

    int size = custom_size_.value_or(
        gfx::GetDefaultSizeOfVectorIcon(dynamic_icon_.GetVectorIcon()));
    if (size <= 0) {
      size = 128;
    }

    image_view->SetImage(ui::ImageModel::FromImageSkia(
        gfx::CreateVectorIcon(dynamic_icon_.GetVectorIcon(), size,
                              icon_color_.value_or(SK_ColorBLACK))));
    container->AddChildView(std::move(image_view));
  }

 private:
  DynamicVectorIcon dynamic_icon_;
  std::optional<int> custom_size_;
  std::optional<SkColor> icon_color_;
  std::optional<SkColor> bg_color_;
};
}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  TestTimeouts::Initialize();
  base::AtExitManager at_exit;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = command_line->GetArgs();

  std::optional<int> custom_size;
  if (command_line->HasSwitch("size")) {
    std::string size_str = command_line->GetSwitchValueASCII("size");
    if (size_str.empty() && !args.empty()) {
#if BUILDFLAG(IS_WIN)
      size_str = base::WideToUTF8(args.front());
#else
      size_str = args.front();
#endif
      args.erase(args.begin());
    }
    int parsed_size = 0;
    if (base::StringToInt(size_str, &parsed_size)) {
      custom_size = parsed_size;
    } else {
      LOG(ERROR) << "Invalid size: " << size_str;
      return 1;
    }
  }

  std::optional<SkColor> icon_color;
  if (command_line->HasSwitch("icon-color")) {
    std::string color_str = command_line->GetSwitchValueASCII("icon-color");
    if (color_str.empty() && !args.empty()) {
#if BUILDFLAG(IS_WIN)
      color_str = base::WideToUTF8(args.front());
#else
      color_str = args.front();
#endif
      args.erase(args.begin());
    }
    icon_color = ParseHexColor(color_str);
    if (!icon_color) {
      LOG(ERROR) << "Invalid icon color: " << color_str;
      return 1;
    }
  }

  std::optional<SkColor> bg_color;
  if (command_line->HasSwitch("background-color")) {
    std::string color_str =
        command_line->GetSwitchValueASCII("background-color");
    if (color_str.empty() && !args.empty()) {
#if BUILDFLAG(IS_WIN)
      color_str = base::WideToUTF8(args.front());
#else
      color_str = args.front();
#endif
      args.erase(args.begin());
    }
    bg_color = ParseHexColor(color_str);
    if (!bg_color) {
      LOG(ERROR) << "Invalid background color: " << color_str;
      return 1;
    }
  }

  if (args.empty()) {
    LOG(ERROR) << "Usage: vector_icon_viewer [--size size] [--icon-color "
                  "color] [--background-color color] <path_to_icon_file>";
    return 1;
  }

  base::FilePath file_path(args[0]);
  std::string file_content;
  if (!base::ReadFileToString(file_path, &file_content)) {
    LOG(ERROR) << "Failed to read file: " << file_path;
    return 1;
  }

  views::examples::ExampleVector examples;
  examples.push_back(std::make_unique<VectorIconViewerExample>(
      file_content, custom_size, icon_color, bg_color));

  return static_cast<int>(
      views::examples::ExamplesMainProc(false, std::move(examples)));
}
