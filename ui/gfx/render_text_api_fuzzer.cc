// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_util.h"
#include "ui/gfx/render_text.h"

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

    // Some platforms require discardable memory to use bitmap fonts.
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator);

    CHECK(base::i18n::InitializeICU());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    test_fonts::SetUpFontconfig();
#endif
    gfx::InitializeFonts();
    gfx::FontList::SetDefaultFontDescription(kFontDescription);
  }

  base::TestDiscardableMemoryAllocator discardable_memory_allocator;
  base::AtExitManager at_exit_manager;
  base::test::TaskEnvironment task_environment;
};

// Commands recognized to drive the API calls on RenderText.
enum class RenderTextAPI {
  kDraw,
  kSetText,
  kAppendText,
  kSetHorizontalAlignment,
  kSetVerticalAlignment,
  kSetCursorEnabled,
  kSetSelectionColor,
  kSetSelectionBackgroundFocusedColor,
  kSetSymmetricSelectionVisualBounds,
  kSetFocused,
  kSetClipToDisplayRect,
  kSetObscured,
  kSetObscuredRevealIndex,
  kSetMultiline,
  kSetMaxLines,
  kSetWordWrapBehavior,
  kSetWhitespaceElision,
  kSetSubpixelRenderingSuppressed,
  kSetColor,
  kApplyColor,
  kSetStyle,
  kApplyStyle,
  kSetWeight,
  kApplyWeight,
  kSetDirectionalityMode,
  kSetElideBehavior,
  kIsGraphemeBoundary,
  kIndexOfAdjacentGrapheme,
  kSetObscuredGlyphSpacing,
  kSetDisplayRect,
  kGetSubstringBounds,
  kGetCursorSpan,
  kSetTruncateLength,
  kSetFillStyle,
  kSetStrokeWidth,
  kMaxValue = kSetStrokeWidth
};

gfx::DirectionalityMode ConsumeDirectionalityMode(FuzzedDataProvider* fdp) {
  if (fdp->ConsumeBool())
    return gfx::DIRECTIONALITY_FORCE_RTL;
  return gfx::DIRECTIONALITY_FORCE_LTR;
}

gfx::HorizontalAlignment ConsumeHorizontalAlignment(FuzzedDataProvider* fdp) {
  switch (fdp->ConsumeIntegralInRange(0, 4)) {
    case 0:
      return gfx::ALIGN_LEFT;
    case 1:
      return gfx::ALIGN_CENTER;
    case 2:
      return gfx::ALIGN_RIGHT;
    case 3:
      return gfx::ALIGN_TO_HEAD;
    default:
      return gfx::ALIGN_LEFT;
  }
}

gfx::VerticalAlignment ConsumeVerticalAlignment(FuzzedDataProvider* fdp) {
  switch (fdp->ConsumeIntegralInRange(0, 3)) {
    case 0:
      return gfx::ALIGN_TOP;
    case 1:
      return gfx::ALIGN_MIDDLE;
    case 2:
      return gfx::ALIGN_BOTTOM;
    default:
      return gfx::ALIGN_TOP;
  }
}

gfx::TextStyle ConsumeStyle(FuzzedDataProvider* fdp) {
  switch (fdp->ConsumeIntegralInRange(0, 4)) {
    case 0:
      return gfx::TEXT_STYLE_ITALIC;
    case 1:
      return gfx::TEXT_STYLE_STRIKE;
    case 2:
      return gfx::TEXT_STYLE_UNDERLINE;
    case 3:
      return gfx::TEXT_STYLE_HEAVY_UNDERLINE;
    default:
      return gfx::TEXT_STYLE_ITALIC;
  }
}

gfx::WordWrapBehavior ConsumeWordWrap(FuzzedDataProvider* fdp) {
  // TODO(crbug.com/40157791): ELIDE_LONG_WORDS is not supported.
  switch (fdp->ConsumeIntegralInRange(0, 3)) {
    case 0:
      return gfx::IGNORE_LONG_WORDS;
    case 1:
      return gfx::TRUNCATE_LONG_WORDS;
    case 2:
      return gfx::WRAP_LONG_WORDS;
    default:
      return gfx::IGNORE_LONG_WORDS;
  }
}

gfx::ElideBehavior ConsumeElideBehavior(FuzzedDataProvider* fdp,
                                        bool generate_only_homogeneous_styles) {
  if (generate_only_homogeneous_styles) {
    // The styles are guaranteed to be homogenous and it is safe to generate
    // any eliding behavior.
    switch (fdp->ConsumeIntegralInRange(0, 7)) {
      case 0:
        return gfx::NO_ELIDE;
      case 1:
        return gfx::TRUNCATE;
      case 2:
        return gfx::ELIDE_HEAD;
      case 3:
        return gfx::ELIDE_MIDDLE;
      case 4:
        return gfx::ELIDE_TAIL;
      case 5:
        return gfx::ELIDE_EMAIL;
      case 6:
        return gfx::FADE_TAIL;
      default:
        return gfx::NO_ELIDE;
    }
  } else {
    // Only generate eliding behaviors that are compatible with non homogeneous
    // text. Remove this when http://crbug.com/1085014 is fixed.
    switch (fdp->ConsumeIntegralInRange(0, 4)) {
      case 0:
        return gfx::NO_ELIDE;
      case 1:
        return gfx::TRUNCATE;
      case 2:
        return gfx::ELIDE_TAIL;
      case 3:
        return gfx::FADE_TAIL;
      default:
        return gfx::NO_ELIDE;
    }
  }
}

gfx::LogicalCursorDirection ConsumeLogicalCursorDirection(
    FuzzedDataProvider* fdp) {
  switch (fdp->ConsumeIntegralInRange(0, 1)) {
    case 0:
      return gfx::CURSOR_BACKWARD;
    default:
      return gfx::CURSOR_FORWARD;
  }
}

gfx::Font::Weight ConsumeWeight(FuzzedDataProvider* fdp) {
  if (fdp->ConsumeBool())
    return gfx::Font::Weight::BOLD;
  return gfx::Font::Weight::NORMAL;
}

SkColor ConsumeSkColor(FuzzedDataProvider* fdp) {
  return static_cast<SkColor>(fdp->ConsumeIntegral<uint32_t>());
}

gfx::Range ConsumeRange(FuzzedDataProvider* fdp, size_t max) {
  size_t start = fdp->ConsumeIntegralInRange<size_t>(0, max);
  size_t end = fdp->ConsumeIntegralInRange<size_t>(start, max);
  return gfx::Range(start, end);
}

cc::PaintFlags::Style ConsumeFillStyle(FuzzedDataProvider* fdp) {
  switch (fdp->ConsumeIntegralInRange(0, 2)) {
    case 0:
      return cc::PaintFlags::kFill_Style;
    case 1:
      return cc::PaintFlags::kStroke_Style;
    default:
      return cc::PaintFlags::kFill_Style;
  }
}

// Eliding behaviors are not all fully supported by RenderText. Ignore
// unsupported cases. This is causing clusterfuzz to fail with invalid
// tests (http://crbug.com/1185542). Remove when https://crbug.com/1085014 is
// fixed.
bool DoesDisplayRangeSupportElideBehavior(const gfx::RenderText* render_text) {
  const gfx::ElideBehavior behavior = render_text->elide_behavior();
  return behavior != gfx::ELIDE_HEAD && behavior != gfx::ELIDE_MIDDLE &&
         behavior != gfx::ELIDE_EMAIL;
}

const int kMaxStringLength = 128;

}  // anonymous namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  std::unique_ptr<gfx::RenderText> render_text =
      gfx::RenderText::CreateRenderText();
  gfx::Canvas canvas;

  FuzzedDataProvider fdp(data, size);
  if (size == 0)
    return 0;

  // Eliding and Styles are not well supported by RenderText. DCHECKs are
  // present in RenderText code to avoid any incorrect uses but the fuzzer
  // should not generate them until full support (http://crbug.com/1283159).
  const bool generate_only_homogeneous_styles = fdp.ConsumeBool();

  while (fdp.remaining_bytes() != 0) {
    const RenderTextAPI command = fdp.ConsumeEnum<RenderTextAPI>();
    switch (command) {
      case RenderTextAPI::kDraw:
        render_text->Draw(&canvas);
        break;

      case RenderTextAPI::kSetText:
        render_text->SetText(
            base::UTF8ToUTF16(fdp.ConsumeRandomLengthString(kMaxStringLength)));
        break;

      case RenderTextAPI::kAppendText:
        render_text->AppendText(
            base::UTF8ToUTF16(fdp.ConsumeRandomLengthString(kMaxStringLength)));
        break;

      case RenderTextAPI::kSetHorizontalAlignment:
        render_text->SetHorizontalAlignment(ConsumeHorizontalAlignment(&fdp));
        break;

      case RenderTextAPI::kSetVerticalAlignment:
        render_text->SetVerticalAlignment(ConsumeVerticalAlignment(&fdp));
        break;

      case RenderTextAPI::kSetCursorEnabled:
        render_text->SetCursorEnabled(fdp.ConsumeBool());
        break;

      case RenderTextAPI::kSetSelectionColor:
        render_text->set_selection_color(ConsumeSkColor(&fdp));
        break;

      case RenderTextAPI::kSetSelectionBackgroundFocusedColor:
        render_text->set_selection_background_focused_color(
            ConsumeSkColor(&fdp));
        break;

      case RenderTextAPI::kSetSymmetricSelectionVisualBounds:
        render_text->set_symmetric_selection_visual_bounds(fdp.ConsumeBool());
        break;

      case RenderTextAPI::kSetFocused:
        render_text->set_focused(fdp.ConsumeBool());
        break;

      case RenderTextAPI::kSetClipToDisplayRect:
        render_text->set_clip_to_display_rect(fdp.ConsumeBool());
        break;

      case RenderTextAPI::kSetObscured:
        render_text->SetObscured(fdp.ConsumeBool());
        break;

      case RenderTextAPI::kSetObscuredRevealIndex:
        render_text->SetObscuredRevealIndex(fdp.ConsumeIntegralInRange<size_t>(
            0, render_text->text().length()));
        break;

      case RenderTextAPI::kSetMultiline:
        if (generate_only_homogeneous_styles) {
          render_text->SetMultiline(fdp.ConsumeBool());
        }
        break;

      case RenderTextAPI::kSetMaxLines:
        render_text->SetMaxLines(fdp.ConsumeIntegralInRange<size_t>(0, 5));
        break;

      case RenderTextAPI::kSetWordWrapBehavior:
        render_text->SetWordWrapBehavior(ConsumeWordWrap(&fdp));
        break;

      case RenderTextAPI::kSetWhitespaceElision:
        render_text->SetWhitespaceElision(fdp.ConsumeBool());
        break;

      case RenderTextAPI::kSetSubpixelRenderingSuppressed:
        render_text->set_subpixel_rendering_suppressed(fdp.ConsumeBool());
        break;

      case RenderTextAPI::kSetColor:
        render_text->SetColor(ConsumeSkColor(&fdp));
        break;

      case RenderTextAPI::kApplyColor:
        if (!generate_only_homogeneous_styles) {
          render_text->ApplyColor(
              ConsumeSkColor(&fdp),
              ConsumeRange(&fdp, render_text->text().length()));
        }
        break;

      case RenderTextAPI::kSetStyle:
        render_text->SetStyle(ConsumeStyle(&fdp), fdp.ConsumeBool());
        break;

      case RenderTextAPI::kApplyStyle:
        if (!generate_only_homogeneous_styles) {
          render_text->ApplyStyle(
              ConsumeStyle(&fdp), fdp.ConsumeBool(),
              ConsumeRange(&fdp, render_text->text().length()));
        }
        break;

      case RenderTextAPI::kSetWeight:
        render_text->SetWeight(ConsumeWeight(&fdp));
        break;

      case RenderTextAPI::kApplyWeight:
        if (!generate_only_homogeneous_styles) {
          render_text->ApplyWeight(
              ConsumeWeight(&fdp),
              ConsumeRange(&fdp, render_text->text().length()));
        }
        break;

      case RenderTextAPI::kSetDirectionalityMode:
        render_text->SetDirectionalityMode(ConsumeDirectionalityMode(&fdp));
        break;

      case RenderTextAPI::kSetElideBehavior:
        render_text->SetElideBehavior(
            ConsumeElideBehavior(&fdp, generate_only_homogeneous_styles));
        break;

      case RenderTextAPI::kIsGraphemeBoundary:
        render_text->IsGraphemeBoundary(fdp.ConsumeIntegralInRange<size_t>(
            0, render_text->text().length()));
        break;

      case RenderTextAPI::kIndexOfAdjacentGrapheme: {
        size_t index = render_text->IndexOfAdjacentGrapheme(
            fdp.ConsumeIntegralInRange<size_t>(0, render_text->text().length()),
            ConsumeLogicalCursorDirection(&fdp));
        bool is_grapheme = render_text->IsGraphemeBoundary(index);
        DCHECK(is_grapheme);
        break;
      }
      case RenderTextAPI::kSetObscuredGlyphSpacing:
        render_text->SetObscuredGlyphSpacing(
            fdp.ConsumeIntegralInRange<size_t>(0, 10));
        break;
      case RenderTextAPI::kSetDisplayRect:
        render_text->SetDisplayRect(
            gfx::Rect(fdp.ConsumeIntegralInRange<int>(-30, 30),
                      fdp.ConsumeIntegralInRange<int>(-30, 30),
                      fdp.ConsumeIntegralInRange<int>(0, 200),
                      fdp.ConsumeIntegralInRange<int>(0, 30)));
        break;
      case RenderTextAPI::kGetSubstringBounds:
        // RenderText doesn't support that case (https://crbug.com/1085014).
        if (!DoesDisplayRangeSupportElideBehavior(render_text.get()))
          break;

        render_text->GetSubstringBounds(
            ConsumeRange(&fdp, render_text->text().length()));
        break;
      case RenderTextAPI::kGetCursorSpan:
        // RenderText doesn't support that case (https://crbug.com/1085014).
        if (!DoesDisplayRangeSupportElideBehavior(render_text.get()))
          break;

        render_text->GetCursorSpan(
            ConsumeRange(&fdp, render_text->text().length()));
        break;
      case RenderTextAPI::kSetTruncateLength:
        render_text->set_truncate_length(fdp.ConsumeIntegral<uint32_t>());
        break;
      case RenderTextAPI::kSetFillStyle:
        render_text->SetFillStyle(ConsumeFillStyle(&fdp));
        break;
      case RenderTextAPI::kSetStrokeWidth:
        render_text->SetStrokeWidth(
            fdp.ConsumeFloatingPointInRange(0.0f, 5.0f));
        break;
    }
  }

  return 0;
}
