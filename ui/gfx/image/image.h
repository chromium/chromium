// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An Image wraps an image any flavor, be it platform-native GdkBitmap/NSImage,
// or a SkBitmap. This also provides easy conversion to other image types
// through operator overloading. It will cache the converted representations
// internally to prevent double-conversion.
//
// The lifetime of both the initial representation and any converted ones are
// tied to the lifetime of the Image's internal storage. To allow Images to be
// cheaply passed around by value, the actual image data is stored in a ref-
// counted member. When all Images referencing this storage are deleted, the
// actual representations are deleted, too.
//
// Images can be empty, in which case they have no backing representation.
// Attempting to use an empty Image will result in a crash.

#ifndef UI_GFX_IMAGE_IMAGE_H_
#define UI_GFX_IMAGE_IMAGE_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_policy.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_MAC)
typedef struct CGColorSpace* CGColorSpaceRef;
#endif

class SkBitmap;

namespace base {
class RefCountedMemory;
}

namespace gfx {
struct ImagePNGRep;
class ImageSkia;
class Size;

namespace internal {
class ImageRep;
class ImageStorage;
}

class GFX_EXPORT Image {
 public:
  enum RepresentationType {
    kImageRepCocoa,
    kImageRepCocoaTouch,
    kImageRepSkia,
    kImageRepPNG,
  };

  // Creates an empty image with no representations.
  Image();

  // Creates a new image by copying the raw PNG-encoded input for use as the
  // default representation.
  explicit Image(const std::vector<ImagePNGRep>& image_reps);

  // Creates a new image by copying the ImageSkia for use as the default
  // representation.
  explicit Image(const ImageSkia& image);

#if BUILDFLAG(IS_IOS)
  // Retains |image|.
  explicit Image(UIImage* image);
#elif BUILDFLAG(IS_MAC)
  // Retains |image|.
  explicit Image(NSImage* image);
#endif

  // Initializes a new Image by AddRef()ing |other|'s internal storage.
  Image(const Image& other);

  // Moves a reference from |other| to the new image without changing the
  // reference count.
  Image(Image&& other) noexcept;

  // Copies a reference to |other|'s storage.
  Image& operator=(const Image& other);

  // Moves a reference from |other|'s storage without changing the reference
  // count.
  Image& operator=(Image&& other) noexcept;

  // Deletes the image and, if the only owner of the storage, all of its cached
  // representations.
  ~Image();

  // True iff both images are backed by the same storage.
  bool operator==(const Image& other) const;

  // Creates an image from the passed in 1x bitmap.
  // WARNING: The resulting image will be pixelated when painted on a high
  // density display.
  static Image CreateFrom1xBitmap(const SkBitmap& bitmap);

  // Creates an image from the PNG encoded input.
  // For example (from an std::vector):
  // std::vector<unsigned char> png = ...;
  // gfx::Image image = Image::CreateFrom1xPNGBytes(png);
  static Image CreateFrom1xPNGBytes(base::span<const uint8_t> input);

  // Creates an image from the PNG encoded input.
  static Image CreateFrom1xPNGBytes(
      const scoped_refptr<base::RefCountedMemory>& input);

  // Converts the Image to the desired representation and stores it internally.
  // The returned result is a weak pointer owned by and scoped to the life of
  // the Image. Must only be called if IsEmpty() is false.
  const SkBitmap* ToSkBitmap() const;
  const ImageSkia* ToImageSkia() const;
#if BUILDFLAG(IS_IOS)
  UIImage* ToUIImage() const;
#elif BUILDFLAG(IS_MAC)
  NSImage* ToNSImage() const;
#endif

  // Returns the raw PNG-encoded data for the bitmap at 1x. If the data is
  // unavailable, either because the image has no data for 1x or because it is
  // empty, an empty RefCountedBytes object is returned. NULL is never
  // returned.
  scoped_refptr<base::RefCountedMemory> As1xPNGBytes() const;

  // Same as ToSkBitmap(), but returns a null SkBitmap if this image is empty.
  SkBitmap AsBitmap() const;

  // Same as ToImageSkia(), but returns an empty ImageSkia if this
  // image is empty.
  ImageSkia AsImageSkia() const;

  // Same as ToNSImage(), but returns nil if this image is empty.
#if BUILDFLAG(IS_MAC)
  NSImage* AsNSImage() const;
#endif

  // Inspects the representations map to see if the given type exists.
  bool HasRepresentation(RepresentationType type) const;

  // Returns the number of representations.
  size_t RepresentationCount() const;

  // Returns true if this Image has no representations.
  bool IsEmpty() const;

  // Width and height of image in DIP coordinate system.
  int Width() const;
  int Height() const;
  gfx::Size Size() const;

 private:
  // Returns the type of the default representation.
  RepresentationType DefaultRepresentationType() const;

  // Returns the ImageRep of the appropriate type or NULL if there is no
  // representation of that type (and must_exist is false).
  const internal::ImageRep* GetRepresentation(RepresentationType rep_type,
                                              bool must_exist) const;

  // Stores a representation into the map. A representation of that type must
  // not already be in the map. Returns a pointer to the representation stored
  // inside the map.
  const internal::ImageRep* AddRepresentation(
      std::unique_ptr<internal::ImageRep> rep) const;

  // Getter should be used internally (unless a handle to the scoped_refptr is
  // needed) instead of directly accessing |storage_|, to ensure logical
  // constness is upheld.
  const internal::ImageStorage* storage() const { return storage_.get(); }
  internal::ImageStorage* storage() { return storage_.get(); }

  // Internal class that holds all the representations. This allows the Image to
  // be cheaply copied.
  scoped_refptr<internal::ImageStorage> storage_;
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_H_
