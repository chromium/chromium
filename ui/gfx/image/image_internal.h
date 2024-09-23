// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This holds internal declarations for the machinery of gfx::Image. These are
// only for the internal use of gfx::Image; do not use them elsewhere.

#ifndef UI_GFX_IMAGE_IMAGE_INTERNAL_H_
#define UI_GFX_IMAGE_IMAGE_INTERNAL_H_

#include <map>
#include <memory>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "ui/gfx/image/image.h"

#if BUILDFLAG(IS_MAC)
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace gfx::internal {

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
  ImageRep(const ImageRep&) = delete;
  ImageRep& operator=(const ImageRep&) = delete;

  // Deletes the associated pixels of an ImageRep.
  virtual ~ImageRep();

  // Cast helpers ("fake RTTI").
  const ImageRepPNG* AsImageRepPNG() const;
  ImageRepPNG* AsImageRepPNG();

  const ImageRepSkia* AsImageRepSkia() const;
  ImageRepSkia* AsImageRepSkia();

#if BUILDFLAG(IS_IOS)
  const ImageRepCocoaTouch* AsImageRepCocoaTouch() const;
  ImageRepCocoaTouch* AsImageRepCocoaTouch();
#elif BUILDFLAG(IS_MAC)
  const ImageRepCocoa* AsImageRepCocoa() const;
  ImageRepCocoa* AsImageRepCocoa();
#endif

  Image::RepresentationType type() const { return type_; }

  virtual int Width() const = 0;
  virtual int Height() const = 0;
  virtual gfx::Size Size() const = 0;

 protected:
  explicit ImageRep(Image::RepresentationType rep);

 private:
  Image::RepresentationType type_;
};

// The Storage class acts similarly to the pixels in a SkBitmap: the Image
// class holds a refptr instance of Storage, which in turn holds all the
// ImageReps. This way, the Image can be cheaply copied.
//
// This class is deliberately not RefCountedThreadSafe. Making it so does not
// solve threading issues, as gfx::Image and its internal classes are
// themselves not threadsafe.
class ImageStorage : public base::RefCounted<ImageStorage> {
 public:
  explicit ImageStorage(Image::RepresentationType default_type);

  ImageStorage(const ImageStorage&) = delete;
  ImageStorage& operator=(const ImageStorage&) = delete;

  Image::RepresentationType default_representation_type() const;
  bool HasRepresentation(Image::RepresentationType type) const;
  size_t RepresentationCount() const;

  const ImageRep* GetRepresentation(Image::RepresentationType rep_type,
                                    bool must_exist) const;
  const ImageRep* AddRepresentation(std::unique_ptr<ImageRep> rep) const;

#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/40286491): Remove callers of this function.
  void set_default_representation_color_space(CGColorSpaceRef color_space) {}
#endif  // BUILDFLAG(IS_MAC)

 private:
  friend class base::RefCounted<ImageStorage>;

  ~ImageStorage();

  // The type of image that was passed to the constructor. This key will always
  // exist in the |representations_| map.
  Image::RepresentationType default_representation_type_;

  // All the representations of an Image. Size will always be at least one, with
  // more for any converted representations.
  mutable std::map<Image::RepresentationType,
                   std::unique_ptr<internal::ImageRep>>
      representations_;
};

}  // namespace gfx::internal

#endif  // UI_GFX_IMAGE_IMAGE_INTERNAL_H_
