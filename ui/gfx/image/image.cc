// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image.h"

#include <algorithm>
#include <map>
#include <ostream>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_internal.h"
#include "ui/gfx/image/image_platform.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"

#if BUILDFLAG(IS_IOS)
#include "base/apple/foundation_util.h"
#include "ui/gfx/image/image_skia_util_ios.h"
#elif BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#include "base/mac/mac_util.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#endif

namespace gfx {

namespace internal {

ImageRep::ImageRep(Image::RepresentationType rep) : type_(rep) {}

ImageRep::~ImageRep() = default;

const ImageRepPNG* ImageRep::AsImageRepPNG() const {
  CHECK_EQ(type_, Image::kImageRepPNG);
  return reinterpret_cast<const ImageRepPNG*>(this);
}
ImageRepPNG* ImageRep::AsImageRepPNG() {
  return const_cast<ImageRepPNG*>(
      static_cast<const ImageRep*>(this)->AsImageRepPNG());
}

const ImageRepSkia* ImageRep::AsImageRepSkia() const {
  CHECK_EQ(type_, Image::kImageRepSkia);
  return reinterpret_cast<const ImageRepSkia*>(this);
}
ImageRepSkia* ImageRep::AsImageRepSkia() {
  return const_cast<ImageRepSkia*>(
      static_cast<const ImageRep*>(this)->AsImageRepSkia());
}

class ImageRepPNG final : public ImageRep {
 public:
  ImageRepPNG() : ImageRep(Image::kImageRepPNG) {
  }

  explicit ImageRepPNG(const std::vector<ImagePNGRep>& image_png_reps)
      : ImageRep(Image::kImageRepPNG), image_png_reps_(image_png_reps) {}

  ImageRepPNG(const ImageRepPNG&) = delete;
  ImageRepPNG& operator=(const ImageRepPNG&) = delete;

  ~ImageRepPNG() override = default;

  int Width() const override { return Size().width(); }

  int Height() const override { return Size().height(); }

  gfx::Size Size() const override {
    // Read the PNG data to get the image size, caching it.
    if (!size_cache_) {
      for (const auto& it : image_reps()) {
        if (it.scale == 1.0f) {
          size_cache_ = it.Size();
          return *size_cache_;
        }
      }
      size_cache_ = gfx::Size();
    }

    return *size_cache_;
  }

  const std::vector<ImagePNGRep>& image_reps() const { return image_png_reps_; }

 private:
  std::vector<ImagePNGRep> image_png_reps_;

  // Cached to avoid having to parse the raw data multiple times.
  mutable std::optional<gfx::Size> size_cache_;
};

class ImageRepSkia final : public ImageRep {
 public:
  explicit ImageRepSkia(ImageSkia image)
      : ImageRep(Image::kImageRepSkia), image_(image) {}

  ImageRepSkia(const ImageRepSkia&) = delete;
  ImageRepSkia& operator=(const ImageRepSkia&) = delete;

  ~ImageRepSkia() override = default;

  int Width() const override { return image_.width(); }

  int Height() const override { return image_.height(); }

  gfx::Size Size() const override { return image_.size(); }

  const ImageSkia* image() const { return &image_; }
  ImageSkia* image() { return &image_; }

 private:
  ImageSkia image_;
};

ImageStorage::ImageStorage(Image::RepresentationType default_type)
    : default_representation_type_(default_type) {}

ImageStorage::~ImageStorage() = default;

Image::RepresentationType ImageStorage::default_representation_type() const {
  DCHECK(IsOnValidSequence());
  return default_representation_type_;
}

bool ImageStorage::HasRepresentation(Image::RepresentationType type) const {
  DCHECK(IsOnValidSequence());
  return representations_.count(type) != 0;
}

size_t ImageStorage::RepresentationCount() const {
  DCHECK(IsOnValidSequence());
  return representations_.size();
}

const ImageRep* ImageStorage::GetRepresentation(
    Image::RepresentationType rep_type,
    bool must_exist) const {
  DCHECK(IsOnValidSequence());
  auto it = representations_.find(rep_type);
  if (it == representations_.end()) {
    CHECK(!must_exist);
    return nullptr;
  }
  return it->second.get();
}

const ImageRep* ImageStorage::AddRepresentation(
    std::unique_ptr<ImageRep> rep) const {
  DCHECK(IsOnValidSequence());
  Image::RepresentationType type = rep->type();
  auto result = representations_.emplace(type, std::move(rep));

  // insert should not fail (implies that there was already a representation
  // of that type in the map).
  CHECK(result.second) << "type was already in map.";

  return result.first->second.get();
}

}  // namespace internal

  // |storage_| is null for empty Images.
Image::Image() = default;

Image::Image(const std::vector<ImagePNGRep>& image_reps) {
  // Do not store obviously invalid ImagePNGReps.
  std::vector<ImagePNGRep> filtered;
  for (const auto& image_rep : image_reps) {
    if (image_rep.raw_data.get() && image_rep.raw_data->size())
      filtered.push_back(image_rep);
  }

  if (filtered.empty())
    return;

  storage_ = new internal::ImageStorage(Image::kImageRepPNG);
  AddRepresentation(std::make_unique<internal::ImageRepPNG>(filtered));
}

Image::Image(const ImageSkia& image) {
  if (!image.isNull()) {
    storage_ = new internal::ImageStorage(Image::kImageRepSkia);
    AddRepresentation(std::make_unique<internal::ImageRepSkia>(image));
  }
}

Image::Image(const Image& other) = default;

Image::Image(Image&& other) noexcept = default;

Image& Image::operator=(const Image& other) = default;

Image& Image::operator=(Image&& other) noexcept = default;

Image::~Image() = default;

bool Image::operator==(const Image& other) const {
  return storage_ == other.storage_;
}

// static
Image Image::CreateFrom1xBitmap(const SkBitmap& bitmap) {
  return Image(ImageSkia::CreateFrom1xBitmap(bitmap));
}

// static
Image Image::CreateFrom1xPNGBytes(base::span<const uint8_t> input) {
  if (input.empty()) {
    return Image();
  }
  return CreateFrom1xPNGBytes(
      base::MakeRefCounted<base::RefCountedBytes>(input));
}

Image Image::CreateFrom1xPNGBytes(
    const scoped_refptr<base::RefCountedMemory>& input) {
  if (!input.get() || input->size() == 0u)
    return Image();

  std::vector<ImagePNGRep> image_reps;
  image_reps.emplace_back(input, 1.0f);
  return Image(image_reps);
}

const SkBitmap* Image::ToSkBitmap() const {
  // Possibly create and cache an intermediate ImageRepSkia.
  return ToImageSkia()->bitmap();
}

const ImageSkia* Image::ToImageSkia() const {
  const internal::ImageRep* rep = GetRepresentation(kImageRepSkia, false);
  if (!rep) {
    std::unique_ptr<internal::ImageRep> scoped_rep;
    switch (DefaultRepresentationType()) {
      case kImageRepPNG: {
        const internal::ImageRepPNG* png_rep =
            GetRepresentation(kImageRepPNG, true)->AsImageRepPNG();
        scoped_rep = std::make_unique<internal::ImageRepSkia>(
            internal::ImageSkiaFromPNG(png_rep->image_reps()));
        break;
      }
#if BUILDFLAG(IS_IOS)
      case kImageRepCocoaTouch: {
        const internal::ImageRepCocoaTouch* native_rep =
            GetRepresentation(kImageRepCocoaTouch, true)
                ->AsImageRepCocoaTouch();
        scoped_rep = std::make_unique<internal::ImageRepSkia>(
            ImageSkiaFromUIImage(UIImageOfImageRepCocoaTouch(native_rep)));
        break;
      }
#elif BUILDFLAG(IS_MAC)
      case kImageRepCocoa: {
        const internal::ImageRepCocoa* native_rep =
            GetRepresentation(kImageRepCocoa, true)->AsImageRepCocoa();
        scoped_rep = std::make_unique<internal::ImageRepSkia>(
            ImageSkiaFromNSImage(NSImageOfImageRepCocoa(native_rep)));
        break;
      }
#endif
      default:
        NOTREACHED_IN_MIGRATION();
    }
    CHECK(scoped_rep);
    rep = AddRepresentation(std::move(scoped_rep));
  }
  return rep->AsImageRepSkia()->image();
}

#if BUILDFLAG(IS_IOS)
UIImage* Image::ToUIImage() const {
  const internal::ImageRep* rep = GetRepresentation(kImageRepCocoaTouch, false);
  if (!rep) {
    std::unique_ptr<internal::ImageRep> scoped_rep;
    switch (DefaultRepresentationType()) {
      case kImageRepPNG: {
        const internal::ImageRepPNG* png_rep =
            GetRepresentation(kImageRepPNG, true)->AsImageRepPNG();
        scoped_rep = internal::MakeImageRepCocoaTouch(
            internal::UIImageFromPNG(png_rep->image_reps()));
        break;
      }
      case kImageRepSkia: {
        const internal::ImageRepSkia* skia_rep =
            GetRepresentation(kImageRepSkia, true)->AsImageRepSkia();
        UIImage* image = UIImageFromImageSkia(*skia_rep->image());
        scoped_rep = internal::MakeImageRepCocoaTouch(image);
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
    }
    CHECK(scoped_rep);
    rep = AddRepresentation(std::move(scoped_rep));
  }
  return UIImageOfImageRepCocoaTouch(rep->AsImageRepCocoaTouch());
}
#elif BUILDFLAG(IS_MAC)
NSImage* Image::ToNSImage() const {
  const internal::ImageRep* rep = GetRepresentation(kImageRepCocoa, false);
  if (!rep) {
    std::unique_ptr<internal::ImageRep> scoped_rep;

    switch (DefaultRepresentationType()) {
      case kImageRepPNG: {
        const internal::ImageRepPNG* png_rep =
            GetRepresentation(kImageRepPNG, true)->AsImageRepPNG();
        scoped_rep = internal::MakeImageRepCocoa(
            internal::NSImageFromPNG(png_rep->image_reps()));
        break;
      }
      case kImageRepSkia: {
        const internal::ImageRepSkia* skia_rep =
            GetRepresentation(kImageRepSkia, true)->AsImageRepSkia();
        NSImage* image = NSImageFromImageSkia(*skia_rep->image());
        scoped_rep = internal::MakeImageRepCocoa(image);
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
    }
    CHECK(scoped_rep);
    rep = AddRepresentation(std::move(scoped_rep));
  }
  return NSImageOfImageRepCocoa(rep->AsImageRepCocoa());
}
#endif

scoped_refptr<base::RefCountedMemory> Image::As1xPNGBytes() const {
  if (IsEmpty())
    return new base::RefCountedBytes();

  const internal::ImageRep* rep = GetRepresentation(kImageRepPNG, false);

  if (rep) {
    const std::vector<ImagePNGRep>& image_png_reps =
        rep->AsImageRepPNG()->image_reps();
    for (const auto& image_png_rep : image_png_reps) {
      if (image_png_rep.scale == 1.0f)
        return image_png_rep.raw_data;
    }
    return new base::RefCountedBytes();
  }

  scoped_refptr<base::RefCountedMemory> png_bytes;
  switch (DefaultRepresentationType()) {
#if BUILDFLAG(IS_IOS)
    case kImageRepCocoaTouch: {
      const internal::ImageRepCocoaTouch* cocoa_touch_rep =
          GetRepresentation(kImageRepCocoaTouch, true)->AsImageRepCocoaTouch();
      png_bytes = internal::Get1xPNGBytesFromUIImage(
          internal::UIImageOfImageRepCocoaTouch(cocoa_touch_rep));
      break;
    }
#elif BUILDFLAG(IS_MAC)
    case kImageRepCocoa: {
      const internal::ImageRepCocoa* cocoa_rep =
          GetRepresentation(kImageRepCocoa, true)->AsImageRepCocoa();
      png_bytes = internal::Get1xPNGBytesFromNSImage(
          internal::NSImageOfImageRepCocoa(cocoa_rep));
      break;
    }
#endif
    case kImageRepSkia: {
      const internal::ImageRepSkia* skia_rep =
          GetRepresentation(kImageRepSkia, true)->AsImageRepSkia();
      png_bytes = internal::Get1xPNGBytesFromImageSkia(skia_rep->image());
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
  if (!png_bytes.get() || !png_bytes->size()) {
    // Add an ImageRepPNG with no data such that the conversion is not
    // attempted each time we want the PNG bytes.
    AddRepresentation(base::WrapUnique(new internal::ImageRepPNG()));
    return new base::RefCountedBytes();
  }

  // Do not insert representations for scale factors other than 1x even if
  // they are available because:
  // - Only the 1x PNG bytes can be accessed.
  // - ImageRepPNG is not used as an intermediate type in converting to a
  //   final type eg (converting from ImageRepSkia to ImageRepPNG to get an
  //   ImageRepCocoa).
  std::vector<ImagePNGRep> image_png_reps;
  image_png_reps.emplace_back(png_bytes, 1.0f);
  AddRepresentation(
      base::WrapUnique(new internal::ImageRepPNG(image_png_reps)));
  return png_bytes;
}

SkBitmap Image::AsBitmap() const {
  return IsEmpty() ? SkBitmap() : *ToSkBitmap();
}

ImageSkia Image::AsImageSkia() const {
  return IsEmpty() ? ImageSkia() : *ToImageSkia();
}

#if BUILDFLAG(IS_MAC)
NSImage* Image::AsNSImage() const {
  return IsEmpty() ? nil : ToNSImage();
}
#endif

bool Image::HasRepresentation(RepresentationType type) const {
  return storage() && storage()->HasRepresentation(type);
}

size_t Image::RepresentationCount() const {
  return storage() ? storage()->RepresentationCount() : 0;
}

bool Image::IsEmpty() const {
  return RepresentationCount() == 0;
}

int Image::Width() const {
  if (IsEmpty())
    return 0;
  return GetRepresentation(DefaultRepresentationType(), true)->Width();
}

int Image::Height() const {
  if (IsEmpty())
    return 0;
  return GetRepresentation(DefaultRepresentationType(), true)->Height();
}

gfx::Size Image::Size() const {
  if (IsEmpty())
    return gfx::Size();
  return GetRepresentation(DefaultRepresentationType(), true)->Size();
}

Image::RepresentationType Image::DefaultRepresentationType() const {
  CHECK(storage());
  return storage()->default_representation_type();
}

const internal::ImageRep* Image::GetRepresentation(RepresentationType rep_type,
                                                   bool must_exist) const {
  CHECK(storage());
  return storage()->GetRepresentation(rep_type, must_exist);
}

const internal::ImageRep* Image::AddRepresentation(
    std::unique_ptr<internal::ImageRep> rep) const {
  CHECK(storage());
  return storage()->AddRepresentation(std::move(rep));
}

}  // namespace gfx
