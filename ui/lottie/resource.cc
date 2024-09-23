// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/lottie/resource.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/record_paint_canvas.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/lottie/animation.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/base/models/image_model.h"  // nogncheck
#include "ui/color/color_id.h"           // nogncheck
#include "ui/color/color_provider.h"     // nogncheck
#endif

namespace lottie {

namespace {

// A descendant of |gfx::ImageSkiaSource| that simply uses one
// |gfx::ImageSkiaRep| for all scales.
class LottieImageSource : public gfx::ImageSkiaSource {
 public:
  explicit LottieImageSource(const gfx::ImageSkiaRep& rep) : rep_(rep) {}
  LottieImageSource(const LottieImageSource&) = delete;
  LottieImageSource& operator=(const LottieImageSource&) = delete;
  ~LottieImageSource() override = default;

  // gfx::ImageSkiaSource overrides:
  gfx::ImageSkiaRep GetImageForScale(float scale) override { return rep_; }
  bool HasRepresentationAtAllScales() const override { return true; }

 private:
  gfx::ImageSkiaRep rep_;
};

// Uses |LottieImageSource| to create a |gfx::ImageSkia| from an |Animation|.
gfx::ImageSkia CreateImageSkia(Animation* content) {
  const gfx::Size size = content->GetOriginalSize();

  cc::InspectableRecordPaintCanvas record_canvas(size);
  gfx::Canvas canvas(&record_canvas, 1.0);
#if DCHECK_IS_ON()
  gfx::Rect clip_rect;
  DCHECK(canvas.GetClipBounds(&clip_rect));
  DCHECK(clip_rect.Contains(gfx::Rect(size)));
#endif
  content->PaintFrame(&canvas, 0.f, size);

  const gfx::ImageSkiaRep rep(record_canvas.ReleaseAsRecord(), size, 0.f);
  return gfx::ImageSkia(std::make_unique<LottieImageSource>(rep),
                        rep.pixel_size());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Creates a |cc::SkottieColorMap| with theme colors from a |ui::ColorProvider|.
cc::SkottieColorMap CreateColorMap(const ui::ColorProvider* color_provider) {
  return {
      cc::SkottieMapColor("cros.sys.illo.color1",
                          color_provider->GetColor(ui::kColorNativeColor1)),
      cc::SkottieMapColor(
          "cros.sys.illo.color1.1",
          color_provider->GetColor(ui::kColorNativeColor1Shade1)),
      cc::SkottieMapColor(
          "cros.sys.illo.color1.2",
          color_provider->GetColor(ui::kColorNativeColor1Shade2)),
      cc::SkottieMapColor("cros.sys.illo.color2",
                          color_provider->GetColor(ui::kColorNativeColor2)),
      cc::SkottieMapColor("cros.sys.illo.color3",
                          color_provider->GetColor(ui::kColorNativeColor3)),
      cc::SkottieMapColor("cros.sys.illo.color4",
                          color_provider->GetColor(ui::kColorNativeColor4)),
      cc::SkottieMapColor("cros.sys.illo.color5",
                          color_provider->GetColor(ui::kColorNativeColor5)),
      cc::SkottieMapColor("cros.sys.illo.color6",
                          color_provider->GetColor(ui::kColorNativeColor6)),
      cc::SkottieMapColor("cros.sys.illo.base",
                          color_provider->GetColor(ui::kColorNativeBaseColor)),
      cc::SkottieMapColor(
          "cros.sys.illo.secondary",
          color_provider->GetColor(ui::kColorNativeSecondaryColor)),
      cc::SkottieMapColor(
          "cros.sys.illo.on-primary-container",
          color_provider->GetColor(ui::kColorNativeOnPrimaryContainerColor)),
      cc::SkottieMapColor(
          "cros.sys.illo.analog",
          color_provider->GetColor(ui::kColorNativeAnalogColor)),
      cc::SkottieMapColor("cros.sys.illo.muted",
                          color_provider->GetColor(ui::kColorNativeMutedColor)),
      cc::SkottieMapColor(
          "cros.sys.illo.complement",
          color_provider->GetColor(ui::kColorNativeComplementColor)),
      cc::SkottieMapColor(
          "cros.sys.illo.on-gradient",
          color_provider->GetColor(ui::kColorNativeOnGradientColor)),

      // TODO(b/329334699): Colors below are deprecated and will be removed when
      // the users are cleaned up.
      cc::SkottieMapColor("_CrOS_Color1",
                          color_provider->GetColor(ui::kColorNativeColor1)),
      cc::SkottieMapColor(
          "_CrOS_Color1Shade1",
          color_provider->GetColor(ui::kColorNativeColor1Shade1)),
      cc::SkottieMapColor(
          "_CrOS_Color1Shade2",
          color_provider->GetColor(ui::kColorNativeColor1Shade2)),
      cc::SkottieMapColor("_CrOS_Color2",
                          color_provider->GetColor(ui::kColorNativeColor2)),
      cc::SkottieMapColor("_CrOS_Color3",
                          color_provider->GetColor(ui::kColorNativeColor3)),
      cc::SkottieMapColor("_CrOS_Color4",
                          color_provider->GetColor(ui::kColorNativeColor4)),
      cc::SkottieMapColor("_CrOS_Color5",
                          color_provider->GetColor(ui::kColorNativeColor5)),
      cc::SkottieMapColor("_CrOS_Color6",
                          color_provider->GetColor(ui::kColorNativeColor6)),
      cc::SkottieMapColor("_CrOS_BaseColor",
                          color_provider->GetColor(ui::kColorNativeBaseColor)),
      cc::SkottieMapColor(
          "_CrOS_SecondaryColor",
          color_provider->GetColor(ui::kColorNativeSecondaryColor))};
}

// Used for a |ui::ImageModel::ImageGenerator|.
gfx::ImageSkia CreateImageSkiaWithCurrentTheme(
    std::vector<uint8_t> bytes,
    const ui::ColorProvider* color_provider) {
  auto content = std::make_unique<Animation>(
      cc::SkottieWrapper::UnsafeCreateSerializable(std::move(bytes)),
      CreateColorMap(color_provider));
  return CreateImageSkia(content.get());
}
#endif

}  // namespace

gfx::ImageSkia ParseLottieAsStillImage(std::vector<uint8_t> data) {
  auto content = std::make_unique<Animation>(
      cc::SkottieWrapper::UnsafeCreateSerializable(std::move(data)));
  return CreateImageSkia(content.get());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
ui::ImageModel ParseLottieAsThemedStillImage(std::vector<uint8_t> data) {
  const gfx::Size size = std::make_unique<Animation>(
                             cc::SkottieWrapper::UnsafeCreateSerializable(data))
                             ->GetOriginalSize();
  return ui::ImageModel::FromImageGenerator(
      base::BindRepeating(&CreateImageSkiaWithCurrentTheme, std::move(data)),
      size);
}
#endif

}  // namespace lottie
