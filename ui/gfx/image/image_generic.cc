// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_platform.h"

#include <memory>
#include <set>
#include <utility>

#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_source.h"

namespace gfx {
namespace internal {

namespace {

// Returns a 16x16 red image to visually show error in decoding PNG.
ImageSkia GetErrorImageSkia() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseARGB(0xff, 0xff, 0, 0);
  return ImageSkia(ImageSkiaRep(bitmap, 1.0f));
}

class PNGImageSource : public ImageSkiaSource {
 public:
  PNGImageSource() {}
  ~PNGImageSource() override {}

  ImageSkiaRep GetImageForScale(float scale) override {
    if (image_skia_reps_.empty())
      return ImageSkiaRep();

    const ImageSkiaRep* rep = nullptr;
    // gfx::ImageSkia passes one of the resource scale factors. The source
    // should return:
    // 1) The ImageSkiaRep with the highest scale if all available
    // scales are smaller than |scale|.
    // 2) The ImageSkiaRep with the smallest one that is larger than |scale|.
    for (auto iter = image_skia_reps_.begin(); iter != image_skia_reps_.end();
         ++iter) {
      if ((*iter).scale() == scale)
        return (*iter);
      if (!rep || rep->scale() < (*iter).scale())
        rep = &(*iter);
      if (rep->scale() >= scale)
        break;
    }
    return rep ? *rep : ImageSkiaRep();
  }

  const gfx::Size size() const { return size_; }

  bool AddPNGData(const ImagePNGRep& png_rep) {
    const gfx::ImageSkiaRep rep = ToImageSkiaRep(png_rep);
    if (rep.is_null())
      return false;
    if (size_.IsEmpty())
      size_ = gfx::Size(rep.GetWidth(), rep.GetHeight());
    image_skia_reps_.insert(rep);
    return true;
  }

  static ImageSkiaRep ToImageSkiaRep(const ImagePNGRep& png_rep) {
    scoped_refptr<base::RefCountedMemory> raw_data = png_rep.raw_data;
    CHECK(raw_data.get());
    SkBitmap bitmap;
    if (!PNGCodec::Decode(raw_data->front(), raw_data->size(), &bitmap)) {
      LOG(ERROR) << "Unable to decode PNG for " << png_rep.scale << ".";
      return ImageSkiaRep();
    }
    return ImageSkiaRep(bitmap, png_rep.scale);
  }

 private:
  struct Compare {
    bool operator()(const ImageSkiaRep& rep1, const ImageSkiaRep& rep2) const {
      return rep1.scale() < rep2.scale();
    }
  };

  typedef std::set<ImageSkiaRep, Compare> ImageSkiaRepSet;
  ImageSkiaRepSet image_skia_reps_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(PNGImageSource);
};

}  // namespace

ImageSkia ImageSkiaFromPNG(const std::vector<ImagePNGRep>& image_png_reps) {
  if (image_png_reps.empty())
    return GetErrorImageSkia();
  std::unique_ptr<PNGImageSource> image_source(new PNGImageSource);

  for (size_t i = 0; i < image_png_reps.size(); ++i) {
    if (!image_source->AddPNGData(image_png_reps[i]))
      return GetErrorImageSkia();
  }
  const gfx::Size& size = image_source->size();
  DCHECK(!size.IsEmpty());
  if (size.IsEmpty())
    return GetErrorImageSkia();
  return ImageSkia(std::move(image_source), size);
}

scoped_refptr<base::RefCountedMemory> Get1xPNGBytesFromImageSkia(
    const ImageSkia* image_skia) {
  ImageSkiaRep image_skia_rep = image_skia->GetRepresentation(1.0f);

  scoped_refptr<base::RefCountedBytes> png_bytes(new base::RefCountedBytes());
  if (image_skia_rep.scale() != 1.0f ||
      !PNGCodec::EncodeBGRASkBitmap(image_skia_rep.GetBitmap(), false,
                                    &png_bytes->data())) {
    return nullptr;
  }
  return png_bytes;
}

}  // namespace internal
}  // namespace gfx
