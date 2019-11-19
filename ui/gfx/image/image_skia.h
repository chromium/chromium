// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_SKIA_H_
#define UI_GFX_IMAGE_IMAGE_SKIA_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace gfx {
class ImageSkiaSource;
class Size;

namespace internal {
class ImageSkiaStorage;
}  // namespace internal

namespace test {
class TestOnThread;
}

// Container for the same image at different densities, similar to NSImage.
// Image height and width are in DIP (Density Indepent Pixel) coordinates.
//
// ImageSkia should be used whenever possible instead of SkBitmap.
// Functions that mutate the image should operate on the gfx::ImageSkiaRep
// returned from ImageSkia::GetRepresentation, not on ImageSkia.
//
// NOTE: This class should *not* be used to store multiple logical sizes of an
// image (e.g., small, medium and large versions of an icon); use an ImageFamily
// for that. An ImageSkia represents an image of a single logical size, with
// potentially many different densities for high-DPI displays.
//
// ImageSkia is cheap to copy and intentionally supports copy semantics.
class GFX_EXPORT ImageSkia {
 public:
  typedef std::vector<ImageSkiaRep> ImageSkiaReps;

  // Creates an instance with no bitmaps.
  ImageSkia();

  // Creates an instance that will use the |source| to get the image
  // for scale factors. |size| specifes the size of the image in DIP.
  ImageSkia(std::unique_ptr<ImageSkiaSource> source, const gfx::Size& size);

  // Creates an instance that uses the |source|. The constructor loads the image
  // at |scale| and uses its dimensions to calculate the size in DIP.
  ImageSkia(std::unique_ptr<ImageSkiaSource> source, float scale);

  explicit ImageSkia(const gfx::ImageSkiaRep& image_rep);

  // Copies a reference to |other|'s storage.
  ImageSkia(const ImageSkia& other);

  // Copies a reference to |other|'s storage.
  ImageSkia& operator=(const ImageSkia& other);

  ~ImageSkia();

  // Changes the value of GetSupportedScales() to |scales|.
  static void SetSupportedScales(const std::vector<float>& scales);

  // Returns a vector with the scale factors which are supported by this
  // platform, in ascending order.
  static const std::vector<float>& GetSupportedScales();

  // Returns the maximum scale supported by this platform.
  static float GetMaxSupportedScale();

  // Creates an image from the passed in bitmap.
  // DIP width and height are based on scale factor of 1x.
  // Adds ref to passed in bitmap.
  // WARNING: The resulting image will be pixelated when painted on a high
  // density display.
  static ImageSkia CreateFrom1xBitmap(const SkBitmap& bitmap);

  // Returns a deep copy of this ImageSkia which has its own storage with
  // the ImageSkiaRep instances that this ImageSkia currently has.
  // This can be safely passed to and manipulated by another thread.
  // Note that this does NOT generate ImageSkiaReps from its source.
  // If you want to create a deep copy with ImageSkiaReps for supported
  // scale factors, you need to explicitly call
  // |EnsureRepsForSupportedScales()| first.
  ImageSkia DeepCopy() const;

  // Returns true if this object is backed by the same ImageSkiaStorage as
  // |other|. Will also return true if both images are isNull().
  bool BackedBySameObjectAs(const gfx::ImageSkia& other) const;

  // Returns a pointer that identifies the backing ImageSkiaStorage. Comparing
  // the results of this method from two ImageSkia objects is equivalent to
  // using BackedBySameObjectAs().
  const void* GetBackingObject() const;

  // Adds |image_rep| to the image reps contained by this object.
  void AddRepresentation(const gfx::ImageSkiaRep& image_rep);

  // Removes the image rep of |scale| if present.
  void RemoveRepresentation(float scale);

  // Returns true if the object owns an image rep whose density matches
  // |scale| exactly.
  bool HasRepresentation(float scale) const;

  // Returns the image rep whose density best matches |scale|.
  // Returns a null image rep if the object contains no image reps.
  const gfx::ImageSkiaRep& GetRepresentation(float scale) const;

  // Make the ImageSkia instance read-only. Note that this only prevent
  // modification from client code, and the storage may still be
  // modified by the source if any (thus, it's not thread safe).  This
  // detaches the storage from currently accessing sequence, so its safe
  // to pass it to another sequence as long as it is accessed only by that
  // sequence. If this ImageSkia's storage will be accessed by multiple
  // sequences, use |MakeThreadSafe()| method.
  void SetReadOnly();

  // Make the image thread safe by making the storage read only and remove
  // its source if any. All ImageSkia that shares the same storage will also
  // become thread safe. Note that in order to make it 100% thread safe,
  // this must be called before it's been passed to another sequence.
  void MakeThreadSafe();
  bool IsThreadSafe() const;

  // Returns true if this is a null object.
  bool isNull() const { return storage_.get() == NULL; }

  // Width and height of image in DIP coordinate system.
  int width() const;
  int height() const;
  gfx::Size size() const;

  // Returns pointer to 1x bitmap contained by this object. If there is no 1x
  // bitmap, the bitmap whose scale factor is closest to 1x is returned.
  // This function should only be used in unittests and on platforms which do
  // not support scale factors other than 1x.
  // TODO(pkotwicz): Return null SkBitmap when the object has no 1x bitmap.
  const SkBitmap* bitmap() const { return &GetBitmap(); }

  // Returns a vector with the image reps contained in this object.
  // There is no guarantee that this will return all images rep for
  // supported scale factors.
  std::vector<gfx::ImageSkiaRep> image_reps() const;

  // When the source is available, generates all ImageReps for
  // supported scale factors. This method is defined as const as
  // the state change in the storage is agnostic to the caller.
  void EnsureRepsForSupportedScales() const;

  // Clears cached representations for non-supported scale factors that are
  // based on |scale|.
  void RemoveUnsupportedRepresentationsForScale(float scale);

 private:
  friend class test::TestOnThread;
  FRIEND_TEST_ALL_PREFIXES(ImageSkiaTest, EmptyOnThreadTest);
  FRIEND_TEST_ALL_PREFIXES(ImageSkiaTest, StaticOnThreadTest);
  FRIEND_TEST_ALL_PREFIXES(ImageSkiaTest, SourceOnThreadTest);

  // Initialize ImageSkiaStorage with passed in parameters.
  // If the image rep's bitmap is empty, ImageStorage is set to NULL.
  void Init(const gfx::ImageSkiaRep& image_rep);

  const SkBitmap& GetBitmap() const;

  // Checks if the current sequence can read/modify the ImageSkia.
  bool CanRead() const;
  bool CanModify() const;

  // Detach the storage from the currently assigned sequence
  // so that other sequence can access the storage.
  void DetachStorageFromSequence();

  // A refptr so that ImageRepSkia can be copied cheaply.
  scoped_refptr<internal::ImageSkiaStorage> storage_;
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_SKIA_H_
