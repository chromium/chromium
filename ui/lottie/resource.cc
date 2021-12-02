// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/lottie/resource.h"

#include <memory>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/record_paint_canvas.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/lottie/animation.h"

namespace lottie {

gfx::ImageSkiaRep ParseLottieAsStillImage(
    const base::RefCountedString& bytes_string) {
  const uint8_t* bytes_pointer = bytes_string.front_as<uint8_t>();
  std::unique_ptr<lottie::Animation> content =
      std::make_unique<lottie::Animation>(
          cc::SkottieWrapper::CreateSerializable(std::vector<uint8_t>(
              bytes_pointer, bytes_pointer + bytes_string.size())));
  const gfx::Size size = content->GetOriginalSize();

  scoped_refptr<cc::DisplayItemList> display_item_list =
      base::MakeRefCounted<cc::DisplayItemList>(
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer);
  display_item_list->StartPaint();

  cc::RecordPaintCanvas record_canvas(
      display_item_list.get(), SkRect::MakeWH(SkFloatToScalar(size.width()),
                                              SkFloatToScalar(size.height())));
  gfx::Canvas canvas(&record_canvas, 1.0);
#if DCHECK_IS_ON()
  gfx::Rect clip_rect;
  DCHECK(canvas.GetClipBounds(&clip_rect));
  DCHECK(clip_rect.Contains(gfx::Rect(size)));
#endif
  content->PaintFrame(&canvas, 0.f, size);

  display_item_list->EndPaintOfPairedEnd();
  display_item_list->Finalize();
  return gfx::ImageSkiaRep(display_item_list->ReleaseAsRecord(), size, 0.f);
}

}  // namespace lottie
