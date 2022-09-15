// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_FAMILY_H_
#define UI_GFX_IMAGE_IMAGE_FAMILY_H_

#include <iterator>
#include <map>
#include <utility>

#include "ui/gfx/gfx_export.h"
#include "ui/gfx/image/image.h"

namespace gfx {
class ImageSkia;
class Size;

// A collection of images at different sizes. The images should be different
// representations of the same basic concept (for example, an icon) at various
// sizes and (optionally) aspect ratios. A method is provided for finding the
// most appropriate image to fit in a given rectangle.
//
// NOTE: This is not appropriate for storing an image at a single logical pixel
// size, with high-DPI bitmap versions; use an Image or ImageSkia for that. Each
// image in an ImageFamily should have a different logical size (and may also
// include high-DPI representations).
class GFX_EXPORT ImageFamily {
 private:
  // An <aspect ratio, DIP width> pair.
  // A 0x0 image has aspect ratio 1.0. 0xN and Nx0 images are treated as 0x0.
  struct MapKey : std::pair<float, int> {
    MapKey(float aspect, int width)
        : std::pair<float, int>(aspect, width) {}

    float aspect() const { return first; }

    int width() const { return second; }
  };

 public:
  // Type for iterating over all images in the family, in order.
  // Dereferencing this iterator returns a gfx::Image.
  class GFX_EXPORT const_iterator {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = const gfx::Image;
    using difference_type = std::ptrdiff_t;
    using pointer = const gfx::Image*;
    using reference = const gfx::Image&;

    const_iterator();

    const_iterator(const const_iterator& other);

    ~const_iterator();

    const_iterator& operator++() {
      ++map_iterator_;
      return *this;
    }

    const_iterator operator++(int /*unused*/) {
      const_iterator result(*this);
      ++(*this);
      return result;
    }

    const_iterator& operator--() {
      --map_iterator_;
      return *this;
    }

    const_iterator operator--(int /*unused*/) {
      const_iterator result(*this);
      --(*this);
      return result;
    }

    bool operator==(const const_iterator& other) const {
      return map_iterator_ == other.map_iterator_;
    }

    bool operator!=(const const_iterator& other) const {
      return map_iterator_ != other.map_iterator_;
    }

    const gfx::Image& operator*() const {
      return map_iterator_->second;
    }

    const gfx::Image* operator->() const {
      return &**this;
    }

   private:
    friend class ImageFamily;

    explicit const_iterator(
        const std::map<MapKey, gfx::Image>::const_iterator& other);

    std::map<MapKey, gfx::Image>::const_iterator map_iterator_;
  };

  ImageFamily();
  ImageFamily(ImageFamily&& other);

  // Even though the Images in the family are copyable (reference-counted), the
  // family itself should not be implicitly copied, as it would result in a
  // shallow clone of the entire map and updates to many reference counts.
  // ImageFamily can be explicitly Clone()d, but std::move is preferred.
  ImageFamily(const ImageFamily&) = delete;
  ImageFamily& operator=(const ImageFamily&) = delete;

  ~ImageFamily();

  ImageFamily& operator=(ImageFamily&& other);

  // Gets an iterator to the first image.
  const_iterator begin() const { return const_iterator(map_.begin()); }
  // Gets an iterator to one after the last image.
  const_iterator end() const { return const_iterator(map_.end()); }

  // Determines whether the image family has no images in it.
  bool empty() const { return map_.empty(); }

  // Removes all images from the family.
  void clear() { return map_.clear(); }

  // Creates a shallow copy of the family. The Images inside share their backing
  // store with the original Images.
  ImageFamily Clone() const;

  // Adds an image to the family. If another image is already present at the
  // same size, it will be overwritten.
  void Add(const gfx::Image& image);

  // Adds an image to the family. If another image is already present at the
  // same size, it will be overwritten.
  void Add(const gfx::ImageSkia& image_skia);

  // Gets the best image to use in a rectangle of |width|x|height|.
  // Gets an image at the same aspect ratio as |width|:|height|, if possible, or
  // if not, the closest aspect ratio. Among images of that aspect ratio,
  // returns the smallest image with both its width and height bigger or equal
  // to the requested size. If none exists, returns the largest image of that
  // aspect ratio. If there are no images in the family, returns NULL.
  const gfx::Image* GetBest(int width, int height) const;

  // Gets the best image to use in a rectangle of |size|.
  // Gets an image at the same aspect ratio as |size.width()|:|size.height()|,
  // if possible, or if not, the closest aspect ratio. Among images of that
  // aspect ratio, returns the smallest image with both its width and height
  // bigger or equal to the requested size. If none exists, returns the largest
  // image of that aspect ratio. If there are no images in the family, returns
  // NULL.
  const gfx::Image* GetBest(const gfx::Size& size) const;

  // Gets an image of size |width|x|height|. If no image of that exact size
  // exists, chooses the nearest larger image using GetBest() and scales it to
  // the desired size. If there are no images in the family, returns an empty
  // image.
  gfx::Image CreateExact(int width, int height) const;

  // Gets an image of size |size|. If no image of that exact size exists,
  // chooses the nearest larger image using GetBest() and scales it to the
  // desired size. If there are no images in the family, returns an empty image.
  gfx::Image CreateExact(const gfx::Size& size) const;

 private:
  // Find the closest aspect ratio in the map to |desired_aspect|.
  // Ties are broken by the thinner aspect.
  // |map_| must not be empty. |desired_aspect| must be > 0.0.
  float GetClosestAspect(float desired_aspect) const;

  // Gets an image with aspect ratio |aspect|, at the best size for |width|.
  // Returns the smallest image of aspect ratio |aspect| with its width bigger
  // or equal to |width|. If none exists, returns the largest image of aspect
  // ratio |aspect|. Behavior is undefined if there is not at least one image in
  // |map_| of aspect ratio |aspect|.
  const gfx::Image* GetWithExactAspect(float aspect, int width) const;

  // Map from (aspect ratio, width) to image.
  std::map<MapKey, gfx::Image> map_;
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_FAMILY_H_
