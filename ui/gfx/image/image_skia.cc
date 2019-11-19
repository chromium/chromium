// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/switches.h"

namespace gfx {
namespace {

// static
gfx::ImageSkiaRep& NullImageRep() {
  static base::NoDestructor<ImageSkiaRep> null_image_rep;
  return *null_image_rep;
}

std::vector<float>* g_supported_scales = NULL;

// The difference to fall back to the smaller scale factor rather than the
// larger one. For example, assume 1.20 is requested but only 1.0 and 2.0 are
// supported. In that case, not fall back to 2.0 but 1.0, and then expand
// the image to 1.25.
const float kFallbackToSmallerScaleDiff = 0.20f;

// Maps to the closest supported scale. Returns an exact match, a smaller
// scale within 0.2 units, the nearest larger scale, or the min/max
// supported scale.
float MapToSupportedScale(float scale) {
  for (float supported_scale : *g_supported_scales) {
    if (supported_scale + kFallbackToSmallerScaleDiff >= scale)
      return supported_scale;
  }
  return g_supported_scales->back();
}

}  // namespace

namespace internal {
namespace {

ImageSkiaRep ScaleImageSkiaRep(const ImageSkiaRep& rep, float target_scale) {
  if (rep.is_null() || rep.scale() == target_scale)
    return rep;

  gfx::Size scaled_size =
      gfx::ScaleToCeiledSize(rep.pixel_size(), target_scale / rep.scale());
  return ImageSkiaRep(
      skia::ImageOperations::Resize(rep.GetBitmap(),
                                    skia::ImageOperations::RESIZE_LANCZOS3,
                                    scaled_size.width(), scaled_size.height()),
      target_scale);
}

}  // namespace

// A helper class such that ImageSkia can be cheaply copied. ImageSkia holds a
// refptr instance of ImageSkiaStorage, which in turn holds all of ImageSkia's
// information. Using a |base::SequenceChecker| on a
// |base::RefCountedThreadSafe| subclass may sound strange but is necessary to
// turn the 'thread-non-safe modifiable ImageSkiaStorage' into the 'thread-safe
// read-only ImageSkiaStorage'.
class ImageSkiaStorage : public base::RefCountedThreadSafe<ImageSkiaStorage> {
 public:
  ImageSkiaStorage(std::unique_ptr<ImageSkiaSource> source,
                   const gfx::Size& size);
  ImageSkiaStorage(std::unique_ptr<ImageSkiaSource> source, float scale);

  bool has_source() const { return source_ != nullptr; }
  std::vector<gfx::ImageSkiaRep>& image_reps() { return image_reps_; }
  const gfx::Size& size() const { return size_; }
  bool read_only() const { return read_only_; }

  void DeleteSource();
  void SetReadOnly();
  void DetachFromSequence();

  // Checks if the current thread can safely modify the storage.
  bool CanModify() const;

  // Checks if the current thread can safely read the storage.
  bool CanRead() const;

  // Add a new representation. This checks if the scale of the added image
  // is not 1.0f, and mark the existing rep as scaled to make
  // the image high DPI aware.
  void AddRepresentation(const ImageSkiaRep& image);

  // Returns whether the underlying image source can provide a representation at
  // any scale.  In this case, the caller is guaranteed that
  // FindRepresentation(..., true) will always succeed.
  bool HasRepresentationAtAllScales() const;

  // Returns the iterator of the image rep whose density best matches
  // |scale|. If the image for the |scale| doesn't exist in the storage and
  // |storage| is set, it fetches new image by calling
  // |ImageSkiaSource::GetImageForScale|. There are two modes to deal with
  // arbitrary scale factors.
  // 1: Invoke GetImageForScale with requested scale and if the source
  //   returns the image with different scale (if the image doesn't exist in
  //   resource, for example), it will fallback to closest image rep.
  // 2: Invoke GetImageForScale with the closest known scale to the requested
  //   one and rescale the image.
  // Right now only Windows uses 2 and other platforms use 1 by default.
  // TODO(mukai, oshima): abandon 1 code path and use 2 for every platforms.
  std::vector<ImageSkiaRep>::iterator FindRepresentation(
      float scale,
      bool fetch_new_image) const;

 private:
  friend class base::RefCountedThreadSafe<ImageSkiaStorage>;

  virtual ~ImageSkiaStorage();

  // Vector of bitmaps and their associated scale.
  std::vector<gfx::ImageSkiaRep> image_reps_;

  std::unique_ptr<ImageSkiaSource> source_;

  // Size of the image in DIP.
  gfx::Size size_;

  bool read_only_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(ImageSkiaStorage);
};

ImageSkiaStorage::ImageSkiaStorage(std::unique_ptr<ImageSkiaSource> source,
                                   const gfx::Size& size)
    : source_(std::move(source)), size_(size), read_only_(false) {}

ImageSkiaStorage::ImageSkiaStorage(std::unique_ptr<ImageSkiaSource> source,
                                   float scale)
    : source_(std::move(source)), read_only_(false) {
  DCHECK(source_);
  auto it = FindRepresentation(scale, true);
  if (it == image_reps_.end() || it->is_null())
    source_.reset();
  else
    size_.SetSize(it->GetWidth(), it->GetHeight());
}

void ImageSkiaStorage::DeleteSource() {
  source_.reset();
}

void ImageSkiaStorage::SetReadOnly() {
  read_only_ = true;
}

void ImageSkiaStorage::DetachFromSequence() {
  sequence_checker_.DetachFromSequence();
}

bool ImageSkiaStorage::CanModify() const {
  return !read_only_ && sequence_checker_.CalledOnValidSequence();
}

bool ImageSkiaStorage::CanRead() const {
  return (read_only_ && !source_) || sequence_checker_.CalledOnValidSequence();
}

void ImageSkiaStorage::AddRepresentation(const ImageSkiaRep& image) {
  // Explicitly adding a representation makes no sense for images that
  // inherently have representations at all scales already.
  DCHECK(!HasRepresentationAtAllScales());

  if (image.scale() != 1.0f) {
    ImageSkia::ImageSkiaReps::iterator it;
    for (it = image_reps_.begin(); it < image_reps_.end(); ++it) {
      if (it->unscaled()) {
        DCHECK_EQ(1.0f, it->scale());
        *it = ImageSkiaRep(it->GetBitmap(), it->scale());
        break;
      }
    }
  }
  image_reps_.push_back(image);
}

bool ImageSkiaStorage::HasRepresentationAtAllScales() const {
  return source_ && source_->HasRepresentationAtAllScales();
}

std::vector<ImageSkiaRep>::iterator ImageSkiaStorage::FindRepresentation(
    float scale,
    bool fetch_new_image) const {
  ImageSkiaStorage* non_const = const_cast<ImageSkiaStorage*>(this);

  auto closest_iter = non_const->image_reps().end();
  auto exact_iter = non_const->image_reps().end();
  float smallest_diff = std::numeric_limits<float>::max();
  for (auto it = non_const->image_reps().begin(); it < image_reps_.end();
       ++it) {
    if (it->scale() == scale) {
      // found exact match
      fetch_new_image = false;
      if (it->is_null())
        continue;
      exact_iter = it;
      break;
    }
    float diff = std::abs(it->scale() - scale);
    if (diff < smallest_diff && !it->is_null()) {
      closest_iter = it;
      smallest_diff = diff;
    }
  }

  if (fetch_new_image && source_.get()) {
    DCHECK(sequence_checker_.CalledOnValidSequence())
        << "An ImageSkia with the source must be accessed by the same "
           "sequence.";

    ImageSkiaRep image;
    float resource_scale = scale;
    if (!HasRepresentationAtAllScales() && g_supported_scales)
      resource_scale = MapToSupportedScale(scale);
    if (scale != resource_scale) {
      auto iter = FindRepresentation(resource_scale, fetch_new_image);
      CHECK(iter != image_reps_.end());
      image = iter->unscaled() ? (*iter) : ScaleImageSkiaRep(*iter, scale);
    } else {
      image = source_->GetImageForScale(scale);
      // Image may be missing for the specified scale in some cases, such like
      // looking up 2x resources but the 2x resource pack is missing. Fall back
      // to 1x and re-scale it.
      if (image.is_null() && scale != 1.0f)
        image = ScaleImageSkiaRep(source_->GetImageForScale(1.0f), scale);
    }

    // If the source returned the new image, store it.
    if (!image.is_null() &&
        std::find_if(image_reps_.begin(), image_reps_.end(),
                     [&image](const ImageSkiaRep& rep) {
                       return rep.scale() == image.scale();
                     }) == image_reps_.end()) {
      non_const->image_reps().push_back(image);
    }

    // If the result image's scale isn't same as the expected scale, create a
    // null ImageSkiaRep with the |scale| so that the next lookup will fall back
    // to the closest scale.
    if (image.is_null() || image.scale() != scale) {
      non_const->image_reps().push_back(ImageSkiaRep(SkBitmap(), scale));
    }

    // image_reps_ must have the exact much now, so find again.
    return FindRepresentation(scale, false);
  }
  return exact_iter != image_reps_.end() ? exact_iter : closest_iter;
}

ImageSkiaStorage::~ImageSkiaStorage() = default;

}  // internal

ImageSkia::ImageSkia() {}

ImageSkia::ImageSkia(std::unique_ptr<ImageSkiaSource> source,
                     const gfx::Size& size)
    : storage_(
          base::MakeRefCounted<internal::ImageSkiaStorage>(std::move(source),
                                                           size)) {
  DCHECK(storage_->has_source());
  // No other thread has reference to this, so it's safe to detach the sequence.
  DetachStorageFromSequence();
}

ImageSkia::ImageSkia(std::unique_ptr<ImageSkiaSource> source, float scale)
    : storage_(
          base::MakeRefCounted<internal::ImageSkiaStorage>(std::move(source),
                                                           scale)) {
  if (!storage_->has_source())
    storage_ = nullptr;
  // No other thread has reference to this, so it's safe to detach the sequence.
  DetachStorageFromSequence();
}

ImageSkia::ImageSkia(const ImageSkiaRep& image_rep) {
  Init(image_rep);
  // No other thread has reference to this, so it's safe to detach the sequence.
  DetachStorageFromSequence();
}

ImageSkia::ImageSkia(const ImageSkia& other) : storage_(other.storage_) {
}

ImageSkia& ImageSkia::operator=(const ImageSkia& other) {
  storage_ = other.storage_;
  return *this;
}

ImageSkia::~ImageSkia() {
}

// static
void ImageSkia::SetSupportedScales(const std::vector<float>& supported_scales) {
  if (g_supported_scales != NULL)
    delete g_supported_scales;
  g_supported_scales = new std::vector<float>(supported_scales);
  std::sort(g_supported_scales->begin(), g_supported_scales->end());
}

// static
const std::vector<float>& ImageSkia::GetSupportedScales() {
  DCHECK(g_supported_scales != NULL);
  return *g_supported_scales;
}

// static
float ImageSkia::GetMaxSupportedScale() {
  return g_supported_scales->back();
}

// static
ImageSkia ImageSkia::CreateFrom1xBitmap(const SkBitmap& bitmap) {
  return ImageSkia(ImageSkiaRep(bitmap, 0.0f));
}

ImageSkia ImageSkia::DeepCopy() const {
  ImageSkia copy;
  if (isNull())
    return copy;

  CHECK(CanRead());

  std::vector<gfx::ImageSkiaRep>& reps = storage_->image_reps();
  for (auto iter = reps.begin(); iter != reps.end(); ++iter) {
    copy.AddRepresentation(*iter);
  }
  // The copy has its own storage. Detach the copy from the current
  // sequence so that other sequences can use this.
  if (!copy.isNull())
    copy.storage_->DetachFromSequence();
  return copy;
}

bool ImageSkia::BackedBySameObjectAs(const gfx::ImageSkia& other) const {
  return storage_.get() == other.storage_.get();
}

const void* ImageSkia::GetBackingObject() const {
  return storage_.get();
}

void ImageSkia::AddRepresentation(const ImageSkiaRep& image_rep) {
  DCHECK(!image_rep.is_null());

  // TODO(oshima): This method should be called |SetRepresentation|
  // and replace the existing rep if there is already one with the
  // same scale so that we can guarantee that a ImageSkia instance contains only
  // one image rep per scale. This is not possible now as ImageLoader currently
  // stores need this feature, but this needs to be fixed.
  if (isNull()) {
    Init(image_rep);
  } else {
    CHECK(CanModify());
    // If someone is adding ImageSkia explicitly, check if we should
    // make the image high DPI aware.
    storage_->AddRepresentation(image_rep);
  }
}

void ImageSkia::RemoveRepresentation(float scale) {
  if (isNull())
    return;
  CHECK(CanModify());

  ImageSkiaReps& image_reps = storage_->image_reps();
  auto it = storage_->FindRepresentation(scale, false);
  if (it != image_reps.end() && it->scale() == scale)
    image_reps.erase(it);
}

bool ImageSkia::HasRepresentation(float scale) const {
  if (isNull())
    return false;
  CHECK(CanRead());

  // This check is not only faster than FindRepresentation(), it's important for
  // getting the right answer in cases of image types that are not based on
  // discrete preset underlying representations, which otherwise might report
  // "false" for this if GetRepresentation() has not yet been called for this
  // |scale|.
  if (storage_->HasRepresentationAtAllScales())
    return true;

  auto it = storage_->FindRepresentation(scale, false);
  return (it != storage_->image_reps().end() && it->scale() == scale);
}

const ImageSkiaRep& ImageSkia::GetRepresentation(float scale) const {
  if (isNull())
    return NullImageRep();

  CHECK(CanRead());

  auto it = storage_->FindRepresentation(scale, true);
  if (it == storage_->image_reps().end())
    return NullImageRep();

  return *it;
}

void ImageSkia::SetReadOnly() {
  CHECK(storage_.get());
  storage_->SetReadOnly();
  DetachStorageFromSequence();
}

void ImageSkia::MakeThreadSafe() {
  CHECK(storage_.get());
  EnsureRepsForSupportedScales();
  // Delete source as we no longer needs it.
  if (storage_.get())
    storage_->DeleteSource();
  storage_->SetReadOnly();
  CHECK(IsThreadSafe());
}

bool ImageSkia::IsThreadSafe() const {
  return !storage_.get() || (storage_->read_only() && !storage_->has_source());
}

int ImageSkia::width() const {
  return isNull() ? 0 : storage_->size().width();
}

gfx::Size ImageSkia::size() const {
  return gfx::Size(width(), height());
}

int ImageSkia::height() const {
  return isNull() ? 0 : storage_->size().height();
}

std::vector<ImageSkiaRep> ImageSkia::image_reps() const {
  if (isNull())
    return std::vector<ImageSkiaRep>();

  CHECK(CanRead());

  ImageSkiaReps internal_image_reps = storage_->image_reps();
  // Create list of image reps to return, skipping null image reps which were
  // added for caching purposes only.
  ImageSkiaReps image_reps;
  for (auto it = internal_image_reps.begin(); it != internal_image_reps.end();
       ++it) {
    if (!it->is_null())
      image_reps.push_back(*it);
  }

  return image_reps;
}

void ImageSkia::EnsureRepsForSupportedScales() const {
  DCHECK(g_supported_scales != NULL);
  // Don't check ReadOnly because the source may generate images even for read
  // only ImageSkia. Concurrent access will be protected by
  // |DCHECK(sequence_checker_.CalledOnValidSequence())| in FindRepresentation.
  if (storage_.get() && storage_->has_source()) {
    for (std::vector<float>::const_iterator it = g_supported_scales->begin();
         it != g_supported_scales->end(); ++it)
      storage_->FindRepresentation(*it, true);
  }
}

void ImageSkia::RemoveUnsupportedRepresentationsForScale(float scale) {
  for (const ImageSkiaRep& image_rep_to_test : image_reps()) {
    const float test_scale = image_rep_to_test.scale();
    if (test_scale != scale && MapToSupportedScale(test_scale) == scale)
      RemoveRepresentation(test_scale);
  }
}

void ImageSkia::Init(const ImageSkiaRep& image_rep) {
  if (image_rep.GetBitmap().drawsNothing()) {
    storage_.reset();
    return;
  }
  storage_ = new internal::ImageSkiaStorage(
      NULL, gfx::Size(image_rep.GetWidth(), image_rep.GetHeight()));
  storage_->image_reps().push_back(image_rep);
}

const SkBitmap& ImageSkia::GetBitmap() const {
  if (isNull()) {
    // Callers expect a ImageSkiaRep even if it is |isNull()|.
    // TODO(pkotwicz): Fix this.
    return NullImageRep().GetBitmap();
  }

  // TODO(oshima): This made a few tests flaky on Windows.
  // Fix the root cause and re-enable this. crbug.com/145623.
#if !defined(OS_WIN)
  CHECK(CanRead());
#endif

  auto it = storage_->FindRepresentation(1.0f, true);
  if (it != storage_->image_reps().end())
    return it->GetBitmap();
  return NullImageRep().GetBitmap();
}

bool ImageSkia::CanRead() const {
  return !storage_.get() || storage_->CanRead();
}

bool ImageSkia::CanModify() const {
  return !storage_.get() || storage_->CanModify();
}

void ImageSkia::DetachStorageFromSequence() {
  if (storage_.get())
    storage_->DetachFromSequence();
}

}  // namespace gfx
