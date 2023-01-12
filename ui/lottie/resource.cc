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
      cc::SkottieWrapper::CreateSerializable(std::move(bytes)),
      CreateColorMap(color_provider));
  return CreateImageSkia(content.get());
}
#endif

// Converts from |std::string| to |std::vector<uint8_t>|.
std::vector<uint8_t> StringToBytes(const std::string& bytes_string) {
  const uint8_t* bytes_pointer =
      reinterpret_cast<const uint8_t*>(bytes_string.data());
  return std::vector<uint8_t>(bytes_pointer,
                              bytes_pointer + bytes_string.size());
}

}  // namespace

gfx::ImageSkia ParseLottieAsStillImage(const std::string& bytes_string) {
  auto content = std::make_unique<Animation>(
      cc::SkottieWrapper::CreateSerializable(StringToBytes(bytes_string)));
  return CreateImageSkia(content.get());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
ui::ImageModel ParseLottieAsThemedStillImage(const std::string& bytes_string) {
  std::vector<uint8_t> bytes = StringToBytes(bytes_string);
  const gfx::Size size =
      std::make_unique<Animation>(cc::SkottieWrapper::CreateSerializable(bytes))
          ->GetOriginalSize();
  return ui::ImageModel::FromImageGenerator(
      base::BindRepeating(&CreateImageSkiaWithCurrentTheme, std::move(bytes)),
      size);
}
#endif

}  // namespace lottie
