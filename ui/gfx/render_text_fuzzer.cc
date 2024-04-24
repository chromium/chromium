// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text.h"

#include <string_view>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_util.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "third_party/test_fonts/fontconfig/fontconfig_util_linux.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)
const char kFontDescription[] = "Segoe UI, 13px";
#elif BUILDFLAG(IS_ANDROID)
const char kFontDescription[] = "serif, 13px";
#else
const char kFontDescription[] = "sans, 13px";
#endif

struct Environment {
  Environment()
      : task_environment((base::CommandLine::Init(0, nullptr),
                          TestTimeouts::Initialize(),
                          base::test::TaskEnvironment::MainThreadType::UI)) {
    logging::SetMinLogLevel(logging::LOGGING_FATAL);

    CHECK(base::i18n::InitializeICU());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    test_fonts::SetUpFontconfig();
#endif
    gfx::InitializeFonts();
    gfx::FontList::SetDefaultFontDescription(kFontDescription);
  }

  base::AtExitManager at_exit_manager;
  base::test::TaskEnvironment task_environment;
};

}  // anonymous namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  std::unique_ptr<gfx::RenderText> render_text =
      gfx::RenderText::CreateRenderText();
  gfx::Canvas canvas;
  render_text->SetText(base::UTF8ToUTF16(
      std::string_view(reinterpret_cast<const char*>(data), size)));
  render_text->Draw(&canvas);
  return 0;
}
