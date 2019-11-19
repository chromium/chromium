// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_platform.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_IOS)
#include "base/mac/foundation_util.h"
#include "ui/gfx/image/image_skia_util_ios.h"
#elif defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#endif

namespace gfx {

namespace {

using RepresentationMap =
    std::map<Image::RepresentationType, std::unique_ptr<internal::ImageRep>>;

}  // namespace

namespace internal {

class ImageRepPNG;
class ImageRepSkia;
class ImageRepCocoa;
class ImageRepCocoaTouch;

// An ImageRep is the object that holds the backing memory for an Image. Each
// RepresentationType has an ImageRep subclass that is responsible for freeing
// the memory that the ImageRep holds. When an ImageRep is created, it expects
// to take ownership of the image, without having to retain it or increase its
// reference count.
class ImageRep {
 public:
  explicit ImageRep(Image::RepresentationType rep) : type_(rep) {}

  // Deletes the associated pixels of an ImageRep.
  virtual ~ImageRep() {}

  // Cast helpers ("fake RTTI").
  const ImageRepPNG* AsImageRepPNG() const {
    CHECK_EQ(type_, Image::kImageRepPNG);
    return reinterpret_cast<const ImageRepPNG*>(this);
  }
  ImageRepPNG* AsImageRepPNG() {
    return const_cast<ImageRepPNG*>(
        static_cast<const ImageRep*>(this)->AsImageRepPNG());
  }

  const ImageRepSkia* AsImageRepSkia() const {
    CHECK_EQ(type_, Image::kImageRepSkia);
    return reinterpret_cast<const ImageRepSkia*>(this);
  }
  ImageRepSkia* AsImageRepSkia() {
    return const_cast<ImageRepSkia*>(
        static_cast<const ImageRep*>(this)->AsImageRepSkia());
  }

#if defined(OS_IOS)
  const ImageRepCocoaTouch* AsImageRepCocoaTouch() const {
    CHECK_EQ(type_, Image::kImageRepCocoaTouch);
    return reinterpret_cast<const ImageRepCocoaTouch*>(this);
  }
  ImageRepCocoaTouch* AsImageRepCocoaTouch() {
    return const_cast<ImageRepCocoaTouch*>(
        static_cast<const ImageRep*>(this)->AsImageRepCocoaTouch());
  }
#elif defined(OS_MACOSX)
  const ImageRepCocoa* AsImageRepCocoa() const {
    CHECK_EQ(type_, Image::kImageRepCocoa);
    return reinterpret_cast<const ImageRepCocoa*>(this);
  }
  ImageRepCocoa* AsImageRepCocoa() {
    return const_cast<ImageRepCocoa*>(
        static_cast<const ImageRep*>(this)->AsImageRepCocoa());
  }
#endif

  Image::RepresentationType type() const { return type_; }

  virtual int Width() const = 0;
  virtual int Height() const = 0;
  virtual gfx::Size Size() const = 0;

 private:
  Image::RepresentationType type_;
};

class ImageRepPNG : public ImageRep {
 public:
  ImageRepPNG() : ImageRep(Image::kImageRepPNG) {
  }

  explicit ImageRepPNG(const std::vector<ImagePNGRep>& image_png_reps)
      : ImageRep(Image::kImageRepPNG), image_png_reps_(image_png_reps) {}

  ~ImageRepPNG() override {}

  int Width() const override { return Size().width(); }

  int Height() const override { return Size().height(); }

  gfx::Size Size() const override {
    // Read the PNG data to get the image size, caching it.
    if (!size_cache_) {
      for (auto it = image_reps().begin(); it != image_reps().end(); ++it) {
        if (it->scale == 1.0f) {
          size_cache_ = it->Size();
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
  mutable base::Optional<gfx::Size> size_cache_;

  DISALLOW_COPY_AND_ASSIGN(ImageRepPNG);
};

class ImageRepSkia : public ImageRep {
 public:
  explicit ImageRepSkia(ImageSkia image)
      : ImageRep(Image::kImageRepSkia), image_(image) {}

  ~ImageRepSkia() override {}

  int Width() const override { return image_.width(); }

  int Height() const override { return image_.height(); }

  gfx::Size Size() const override { return image_.size(); }

  const ImageSkia* image() const { return &image_; }
  ImageSkia* image() { return &image_; }

 private:
  ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(ImageRepSkia);
};

#if defined(OS_IOS)
class ImageRepCocoaTouch : public ImageRep {
 public:
  explicit ImageRepCocoaTouch(UIImage* image)
      : ImageRep(Image::kImageRepCocoaTouch),
        image_(image) {
    CHECK(image_);
    base::mac::NSObjectRetain(image_);
  }

  ~ImageRepCocoaTouch() override {
    base::mac::NSObjectRelease(image_);
    image_ = nil;
  }

  int Width() const override { return Size().width(); }

  int Height() const override { return Size().height(); }

  gfx::Size Size() const override { return internal::UIImageSize(image_); }

  UIImage* image() const { return image_; }

 private:
  UIImage* image_;

  DISALLOW_COPY_AND_ASSIGN(ImageRepCocoaTouch);
};
#elif defined(OS_MACOSX)
class ImageRepCocoa : public ImageRep {
 public:
  explicit ImageRepCocoa(NSImage* image)
      : ImageRep(Image::kImageRepCocoa),
        image_(image) {
    CHECK(image_);
    base::mac::NSObjectRetain(image_);
  }

  ~ImageRepCocoa() override {
    base::mac::NSObjectRelease(image_);
    image_ = nil;
  }

  int Width() const override { return Size().width(); }

  int Height() const override { return Size().height(); }

  gfx::Size Size() const override { return internal::NSImageSize(image_); }

  NSImage* image() const { return image_; }

 private:
  NSImage* image_;

  DISALLOW_COPY_AND_ASSIGN(ImageRepCocoa);
};
#endif  // defined(OS_MACOSX)

// The Storage class acts similarly to the pixels in a SkBitmap: the Image
// class holds a refptr instance of Storage, which in turn holds all the
// ImageReps. This way, the Image can be cheaply copied.
//
// This class is deliberately not RefCountedThreadSafe. Making it so does not
// solve threading issues, as gfx::Image and its internal classes are
// themselves not threadsafe.
class ImageStorage : public base::RefCounted<ImageStorage> {
 public:
  explicit ImageStorage(Image::RepresentationType default_type)
      : default_representation_type_(default_type)
#if defined(OS_MACOSX) && !defined(OS_IOS)
        ,
        default_representation_color_space_(
            base::mac::GetGenericRGBColorSpace())
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
  {
  }

  Image::RepresentationType default_representation_type() const {
    DCHECK(IsOnValidSequence());
    return default_representation_type_;
  }

  bool HasRepresentation(Image::RepresentationType type) const {
    DCHECK(IsOnValidSequence());
    return representations_.count(type) != 0;
  }

  size_t RepresentationCount() const {
    DCHECK(IsOnValidSequence());
    return representations_.size();
  }

  const ImageRep* GetRepresentation(Image::RepresentationType rep_type,
                                    bool must_exist) const {
    DCHECK(IsOnValidSequence());
    RepresentationMap::const_iterator it = representations_.find(rep_type);
    if (it == representations_.end()) {
      CHECK(!must_exist);
      return nullptr;
    }
    return it->second.get();
  }

  const ImageRep* AddRepresentation(std::unique_ptr<ImageRep> rep) const {
    DCHECK(IsOnValidSequence());
    Image::RepresentationType type = rep->type();
    auto result = representations_.emplace(type, std::move(rep));

    // insert should not fail (implies that there was already a representation
    // of that type in the map).
    CHECK(result.second) << "type was already in map.";

    return result.first->second.get();
  }

#if defined(OS_MACOSX) && !defined(OS_IOS)
  void set_default_representation_color_space(CGColorSpaceRef color_space) {
    DCHECK(IsOnValidSequence());
    default_representation_color_space_ = color_space;
  }
  CGColorSpaceRef default_representation_color_space() const {
    DCHECK(IsOnValidSequence());
    return default_representation_color_space_;
  }
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

 private:
  friend class base::RefCounted<ImageStorage>;

  ~ImageStorage() {}

  // The type of image that was passed to the constructor. This key will always
  // exist in the |representations_| map.
  Image::RepresentationType default_representation_type_;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // The default representation's colorspace. This is used for converting to
  // NSImage. This field exists to compensate for PNGCodec not writing or
  // reading colorspace ancillary chunks. (sRGB, iCCP).
  // Not owned.
  CGColorSpaceRef default_representation_color_space_;
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  // All the representations of an Image. Size will always be at least one, with
  // more for any converted representations.
  mutable RepresentationMap representations_;

  DISALLOW_COPY_AND_ASSIGN(ImageStorage);
};

}  // namespace internal

Image::Image() {
  // |storage_| is null for empty Images.
}

Image::Image(const std::vector<ImagePNGRep>& image_reps) {
  // Do not store obviously invalid ImagePNGReps.
  std::vector<ImagePNGRep> filtered;
  for (size_t i = 0; i < image_reps.size(); ++i) {
    if (image_reps[i].raw_data.get() && image_reps[i].raw_data->size())
      filtered.push_back(image_reps[i]);
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

#if defined(OS_IOS)
Image::Image(UIImage* image) {
  if (image) {
    storage_ = new internal::ImageStorage(Image::kImageRepCocoaTouch);
    AddRepresentation(std::make_unique<internal::ImageRepCocoaTouch>(image));
  }
}
#elif defined(OS_MACOSX)
Image::Image(NSImage* image) {
  if (image) {
    storage_ = new internal::ImageStorage(Image::kImageRepCocoa);
    AddRepresentation(std::make_unique<internal::ImageRepCocoa>(image));
  }
}
#endif

Image::Image(const Image& other) = default;

Image::Image(Image&& other) noexcept = default;

Image& Image::operator=(const Image& other) = default;

Image& Image::operator=(Image&& other) noexcept = default;

Image::~Image() {}

// static
Image Image::CreateFrom1xBitmap(const SkBitmap& bitmap) {
  return Image(ImageSkia::CreateFrom1xBitmap(bitmap));
}

// static
Image Image::CreateFrom1xPNGBytes(const unsigned char* input,
                                  size_t input_size) {
  if (input_size == 0u)
    return Image();

  scoped_refptr<base::RefCountedBytes> raw_data(new base::RefCountedBytes());
  raw_data->data().assign(input, input + input_size);

  return CreateFrom1xPNGBytes(raw_data);
}

Image Image::CreateFrom1xPNGBytes(
    const scoped_refptr<base::RefCountedMemory>& input) {
  if (!input.get() || input->size() == 0u)
    return Image();

  std::vector<ImagePNGRep> image_reps;
  image_reps.push_back(ImagePNGRep(input, 1.0f));
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
#if defined(OS_IOS)
      case kImageRepCocoaTouch: {
        const internal::ImageRepCocoaTouch* native_rep =
            GetRepresentation(kImageRepCocoaTouch, true)
                ->AsImageRepCocoaTouch();
        scoped_rep = std::make_unique<internal::ImageRepSkia>(
            ImageSkia(ImageSkiaFromUIImage(native_rep->image())));
        break;
      }
#elif defined(OS_MACOSX)
      case kImageRepCocoa: {
        const internal::ImageRepCocoa* native_rep =
            GetRepresentation(kImageRepCocoa, true)->AsImageRepCocoa();
        scoped_rep = std::make_unique<internal::ImageRepSkia>(
            ImageSkia(ImageSkiaFromNSImage(native_rep->image())));
        break;
      }
#endif
      default:
        NOTREACHED();
    }
    CHECK(scoped_rep);
    rep = AddRepresentation(std::move(scoped_rep));
  }
  return rep->AsImageRepSkia()->image();
}

#if defined(OS_IOS)
UIImage* Image::ToUIImage() const {
  const internal::ImageRep* rep = GetRepresentation(kImageRepCocoaTouch, false);
  if (!rep) {
    std::unique_ptr<internal::ImageRep> scoped_rep;
    switch (DefaultRepresentationType()) {
      case kImageRepPNG: {
        const internal::ImageRepPNG* png_rep =
            GetRepresentation(kImageRepPNG, true)->AsImageRepPNG();
        scoped_rep = std::make_unique<internal::ImageRepCocoaTouch>(
            internal::UIImageFromPNG(png_rep->image_reps()));
        break;
      }
      case kImageRepSkia: {
        const internal::ImageRepSkia* skia_rep =
            GetRepresentation(kImageRepSkia, true)->AsImageRepSkia();
        UIImage* image = UIImageFromImageSkia(*skia_rep->image());
        scoped_rep = std::make_unique<internal::ImageRepCocoaTouch>(image);
        break;
      }
      default:
        NOTREACHED();
    }
    CHECK(scoped_rep);
    rep = AddRepresentation(std::move(scoped_rep));
  }
  return rep->AsImageRepCocoaTouch()->image();
}
#elif defined(OS_MACOSX)
NSImage* Image::ToNSImage() const {
  const internal::ImageRep* rep = GetRepresentation(kImageRepCocoa, false);
  if (!rep) {
    std::unique_ptr<internal::ImageRep> scoped_rep;
    CGColorSpaceRef default_representation_color_space =
        storage()->default_representation_color_space();

    switch (DefaultRepresentationType()) {
      case kImageRepPNG: {
        const internal::ImageRepPNG* png_rep =
            GetRepresentation(kImageRepPNG, true)->AsImageRepPNG();
        scoped_rep =
            std::make_unique<internal::ImageRepCocoa>(internal::NSImageFromPNG(
                png_rep->image_reps(), default_representation_color_space));
        break;
      }
      case kImageRepSkia: {
        const internal::ImageRepSkia* skia_rep =
            GetRepresentation(kImageRepSkia, true)->AsImageRepSkia();
        NSImage* image = NSImageFromImageSkiaWithColorSpace(*skia_rep->image(),
            default_representation_color_space);
        scoped_rep = std::make_unique<internal::ImageRepCocoa>(image);
        break;
      }
      default:
        NOTREACHED();
    }
    CHECK(scoped_rep);
    rep = AddRepresentation(std::move(scoped_rep));
  }
  return rep->AsImageRepCocoa()->image();
}
#endif

scoped_refptr<base::RefCountedMemory> Image::As1xPNGBytes() const {
  if (IsEmpty())
    return new base::RefCountedBytes();

  const internal::ImageRep* rep = GetRepresentation(kImageRepPNG, false);

  if (rep) {
    const std::vector<ImagePNGRep>& image_png_reps =
        rep->AsImageRepPNG()->image_reps();
    for (size_t i = 0; i < image_png_reps.size(); ++i) {
      if (image_png_reps[i].scale == 1.0f)
        return image_png_reps[i].raw_data;
    }
    return new base::RefCountedBytes();
  }

  scoped_refptr<base::RefCountedMemory> png_bytes;
  switch (DefaultRepresentationType()) {
#if defined(OS_IOS)
    case kImageRepCocoaTouch: {
      const internal::ImageRepCocoaTouch* cocoa_touch_rep =
          GetRepresentation(kImageRepCocoaTouch, true)->AsImageRepCocoaTouch();
      png_bytes = internal::Get1xPNGBytesFromUIImage(
          cocoa_touch_rep->image());
      break;
    }
#elif defined(OS_MACOSX)
    case kImageRepCocoa: {
      const internal::ImageRepCocoa* cocoa_rep =
          GetRepresentation(kImageRepCocoa, true)->AsImageRepCocoa();
      png_bytes = internal::Get1xPNGBytesFromNSImage(cocoa_rep->image());
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
      NOTREACHED();
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
  image_png_reps.push_back(ImagePNGRep(png_bytes, 1.0f));
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

#if defined(OS_MACOSX) && !defined(OS_IOS)
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

#if defined(OS_MACOSX)  && !defined(OS_IOS)
void Image::SetSourceColorSpace(CGColorSpaceRef color_space) {
  if (storage())
    storage()->set_default_representation_color_space(color_space);
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

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
