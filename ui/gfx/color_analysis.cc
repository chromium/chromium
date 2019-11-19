// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_analysis.h"

#include <limits.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace color_utils {
namespace {

// RGBA KMean Constants
const int kNumberOfClusters = 4;
const int kNumberOfIterations = 50;

const HSL kDefaultLowerHSLBound = {-1, -1, 0.15};
const HSL kDefaultUpperHSLBound = {-1, -1, 0.85};

// Background Color Modification Constants
const SkColor kDefaultBgColor = SK_ColorWHITE;

// Support class to hold information about each cluster of pixel data in
// the KMean algorithm. While this class does not contain all of the points
// that exist in the cluster, it keeps track of the aggregate sum so it can
// compute the new center appropriately.
class KMeanCluster {
 public:
  KMeanCluster() {
    Reset();
  }

  void Reset() {
    centroid_[0] = centroid_[1] = centroid_[2] = 0;
    aggregate_[0] = aggregate_[1] = aggregate_[2] = 0;
    counter_ = 0;
    weight_ = 0;
  }

  inline void SetCentroid(uint8_t r, uint8_t g, uint8_t b) {
    centroid_[0] = r;
    centroid_[1] = g;
    centroid_[2] = b;
  }

  inline void GetCentroid(uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = centroid_[0];
    *g = centroid_[1];
    *b = centroid_[2];
  }

  inline bool IsAtCentroid(uint8_t r, uint8_t g, uint8_t b) {
    return r == centroid_[0] && g == centroid_[1] && b == centroid_[2];
  }

  // Recomputes the centroid of the cluster based on the aggregate data. The
  // number of points used to calculate this center is stored for weighting
  // purposes. The aggregate and counter are then cleared to be ready for the
  // next iteration.
  inline void RecomputeCentroid() {
    if (counter_ > 0) {
      centroid_[0] = static_cast<uint8_t>(aggregate_[0] / counter_);
      centroid_[1] = static_cast<uint8_t>(aggregate_[1] / counter_);
      centroid_[2] = static_cast<uint8_t>(aggregate_[2] / counter_);

      aggregate_[0] = aggregate_[1] = aggregate_[2] = 0;
      weight_ = counter_;
      counter_ = 0;
    }
  }

  inline void AddPoint(uint8_t r, uint8_t g, uint8_t b) {
    aggregate_[0] += r;
    aggregate_[1] += g;
    aggregate_[2] += b;
    ++counter_;
  }

  // Just returns the distance^2. Since we are comparing relative distances
  // there is no need to perform the expensive sqrt() operation.
  inline uint32_t GetDistanceSqr(uint8_t r, uint8_t g, uint8_t b) {
    return (r - centroid_[0]) * (r - centroid_[0]) +
           (g - centroid_[1]) * (g - centroid_[1]) +
           (b - centroid_[2]) * (b - centroid_[2]);
  }

  // In order to determine if we have hit convergence or not we need to see
  // if the centroid of the cluster has moved. This determines whether or
  // not the centroid is the same as the aggregate sum of points that will be
  // used to generate the next centroid.
  inline bool CompareCentroidWithAggregate() {
    if (counter_ == 0)
      return false;

    return aggregate_[0] / counter_ == centroid_[0] &&
           aggregate_[1] / counter_ == centroid_[1] &&
           aggregate_[2] / counter_ == centroid_[2];
  }

  // Returns the previous counter, which is used to determine the weight
  // of the cluster for sorting.
  inline uint32_t GetWeight() const {
    return weight_;
  }

  static bool SortKMeanClusterByWeight(const KMeanCluster& a,
                                       const KMeanCluster& b) {
    return a.GetWeight() > b.GetWeight();
  }

 private:
  uint8_t centroid_[3];

  // Holds the sum of all the points that make up this cluster. Used to
  // generate the next centroid as well as to check for convergence.
  uint32_t aggregate_[3];
  uint32_t counter_;

  // The weight of the cluster, determined by how many points were used
  // to generate the previous centroid.
  uint32_t weight_;
};

// Prominent color utilities ---------------------------------------------------

// A |ColorBox| represents a 3-dimensional region in a color space (an ordered
// set of colors). It is a range in the ordered set, with a low index and a high
// index. The diversity (volume) of the box is computed by looking at the range
// of color values it spans, where r, g, and b components are considered
// separately.
class ColorBox {
 public:
  explicit ColorBox(std::vector<SkColor>* color_space)
      : ColorBox(color_space, gfx::Range(0, color_space->size())) {}
  ColorBox(const ColorBox& other) = default;
  ColorBox& operator=(const ColorBox& other) = default;
  ~ColorBox() {}

  // Can't split if there's only one color in the box.
  bool CanSplit() const { return color_range_.length() > 1; }

  // Splits |this| in two and returns the other half.
  ColorBox Split() {
    // Calculate which component has the largest range...
    const uint8_t r_dimension = max_r_ - min_r_;
    const uint8_t g_dimension = max_g_ - min_g_;
    const uint8_t b_dimension = max_b_ - min_b_;
    const uint8_t long_dimension =
        std::max({r_dimension, g_dimension, b_dimension});
    const enum {
      RED,
      GREEN,
      BLUE,
    } channel = long_dimension == r_dimension
                    ? RED
                    : long_dimension == g_dimension ? GREEN : BLUE;

    // ... and sort along that axis.
    auto sort_function = [channel](SkColor a, SkColor b) {
      switch (channel) {
        case RED:
          return SkColorGetR(a) < SkColorGetR(b);
        case GREEN:
          return SkColorGetG(a) < SkColorGetG(b);
        case BLUE:
          return SkColorGetB(a) < SkColorGetB(b);
      }
      NOTREACHED();
      return SkColorGetB(a) < SkColorGetB(b);
    };
    // Just the portion of |color_space_| that's covered by this box should be
    // sorted.
    std::sort(color_space_->begin() + color_range_.start(),
              color_space_->begin() + color_range_.end(), sort_function);

    // Split at the first color value that's not less than the midpoint (mean of
    // the start and values).
    uint32_t split_point = color_range_.end() - 1;
    for (uint32_t i = color_range_.start() + 1; i < color_range_.end() - 1;
         ++i) {
      bool past_midpoint = false;
      switch (channel) {
        case RED:
          past_midpoint =
              static_cast<uint8_t>(SkColorGetR((*color_space_)[i])) >
              (min_r_ + max_r_) / 2;
          break;
        case GREEN:
          past_midpoint =
              static_cast<uint8_t>(SkColorGetG((*color_space_)[i])) >
              (min_g_ + max_g_) / 2;
          break;
        case BLUE:
          past_midpoint =
              static_cast<uint8_t>(SkColorGetB((*color_space_)[i])) >
              (min_b_ + max_b_) / 2;
          break;
      }
      if (past_midpoint) {
        split_point = i;
        break;
      }
    }

    // Break off half and return it.
    gfx::Range other_range = color_range_;
    other_range.set_end(split_point);
    ColorBox other_box(color_space_, other_range);

    // Keep the other half in |this| and recalculate our color bounds.
    color_range_.set_start(split_point);
    RecomputeBounds();
    return other_box;
  }

  // Returns the average color of this box, weighted by its popularity in
  // |color_counts|.
  Swatch GetWeightedAverageColor(
      const std::unordered_map<SkColor, int>& color_counts) const {
    size_t sum_r = 0;
    size_t sum_g = 0;
    size_t sum_b = 0;
    size_t total_count_in_box = 0;

    for (size_t i = color_range_.start(); i < color_range_.end(); ++i) {
      const SkColor color = (*color_space_)[i];
      const auto color_count_iter = color_counts.find(color);
      DCHECK(color_count_iter != color_counts.end());
      const size_t color_count = color_count_iter->second;

      total_count_in_box += color_count;
      sum_r += color_count * SkColorGetR(color);
      sum_g += color_count * SkColorGetG(color);
      sum_b += color_count * SkColorGetB(color);
    }

    return Swatch(
        SkColorSetRGB(
            std::round(static_cast<double>(sum_r) / total_count_in_box),
            std::round(static_cast<double>(sum_g) / total_count_in_box),
            std::round(static_cast<double>(sum_b) / total_count_in_box)),
        total_count_in_box);
  }

  static bool CompareByVolume(const ColorBox& a, const ColorBox& b) {
    return a.volume_ < b.volume_;
  }

 private:
  ColorBox(std::vector<SkColor>* color_space, const gfx::Range& color_range)
      : color_space_(color_space), color_range_(color_range) {
    RecomputeBounds();
  }

  void RecomputeBounds() {
    DCHECK(!color_range_.is_reversed());
    DCHECK(!color_range_.is_empty());
    DCHECK_LE(color_range_.end(), color_space_->size());

    min_r_ = 0xFF;
    min_g_ = 0xFF;
    min_b_ = 0xFF;
    max_r_ = 0;
    max_g_ = 0;
    max_b_ = 0;

    for (uint32_t i = color_range_.start(); i < color_range_.end(); ++i) {
      SkColor color = (*color_space_)[i];
      min_r_ = std::min<uint8_t>(SkColorGetR(color), min_r_);
      min_g_ = std::min<uint8_t>(SkColorGetG(color), min_g_);
      min_b_ = std::min<uint8_t>(SkColorGetB(color), min_b_);
      max_r_ = std::max<uint8_t>(SkColorGetR(color), max_r_);
      max_g_ = std::max<uint8_t>(SkColorGetG(color), max_g_);
      max_b_ = std::max<uint8_t>(SkColorGetB(color), max_b_);
    }

    volume_ =
        (max_r_ - min_r_ + 1) * (max_g_ - min_g_ + 1) * (max_b_ - min_b_ + 1);
  }

  // The set of colors of which this box captures a subset. This vector is not
  // owned but may be modified during the split operation.
  std::vector<SkColor>* color_space_;

  // The range of indexes into |color_space_| that are part of this box.
  gfx::Range color_range_;

  // Cached min and max color component values for the colors in this box.
  uint8_t min_r_ = 0;
  uint8_t min_g_ = 0;
  uint8_t min_b_ = 0;
  uint8_t max_r_ = 0;
  uint8_t max_g_ = 0;
  uint8_t max_b_ = 0;

  // Cached volume value, which is the product of the range of each color
  // component.
  int volume_ = 0;
};

// Some color values should be ignored for the purposes of determining prominent
// colors.
bool IsInterestingColor(const SkColor& color) {
  const float average_channel_value =
      (SkColorGetR(color) + SkColorGetG(color) + SkColorGetB(color)) / 3.0f;
  // If a color is too close to white or black, ignore it.
  if (average_channel_value >= 237 || average_channel_value <= 22)
    return false;

  HSL hsl;
  SkColorToHSL(color, &hsl);
  return !(hsl.h >= 0.028f && hsl.h <= 0.10f && hsl.s <= 0.82f);
}

// Used to group lower_bound, upper_bound, goal HSL color together for prominent
// color calculation.
struct ColorBracket {
  HSL lower_bound = {-1};
  HSL upper_bound = {-1};
  HSL goal = {-1};
};

std::vector<Swatch> CalculateProminentColors(
    const SkBitmap& bitmap,
    const std::vector<ColorBracket>& color_brackets,
    const gfx::Rect& region,
    base::Optional<ColorSwatchFilter> filter) {
  DCHECK(!bitmap.empty());
  DCHECK(!bitmap.isNull());

  std::vector<Swatch> box_colors =
      CalculateColorSwatches(bitmap, 12, region, filter);

  std::vector<Swatch> best_colors(color_brackets.size(), Swatch());
  if (box_colors.empty())
    return best_colors;

  size_t max_weight = 0;
  for (auto& weighted : box_colors)
    max_weight = std::max(max_weight, weighted.population);

  // Given these box average colors, find the best one for each desired color
  // profile. "Best" in this case means the color which fits in the provided
  // bounds and comes closest to |goal|. It's possible that no color will fit in
  // the provided bounds, in which case we'll return an empty color.
  for (size_t i = 0; i < color_brackets.size(); ++i) {
    double best_suitability = 0;
    for (const auto& box_color : box_colors) {
      HSL hsl;
      SkColorToHSL(box_color.color, &hsl);
      if (!IsWithinHSLRange(hsl, color_brackets[i].lower_bound,
                            color_brackets[i].upper_bound)) {
        continue;
      }

      double suitability =
          (1 - std::abs(hsl.s - color_brackets[i].goal.s)) * 3 +
          (1 - std::abs(hsl.l - color_brackets[i].goal.l)) * 6.5 +
          (box_color.population / static_cast<float>(max_weight)) * 0.5;
      if (suitability > best_suitability) {
        best_suitability = suitability;
        best_colors[i] = box_color;
      }
    }
  }

  return best_colors;
}

} // namespace

KMeanImageSampler::KMeanImageSampler() {
}

KMeanImageSampler::~KMeanImageSampler() {
}

GridSampler::GridSampler() : calls_(0) {
}

GridSampler::~GridSampler() {
}

int GridSampler::GetSample(int width, int height) {
  // Hand-drawn bitmaps often have special outlines or feathering at the edges.
  // Start our sampling inset from the top and left edges. For example, a 10x10
  // image with 4 clusters would be sampled like this:
  // ..........
  // .0.4.8....
  // ..........
  // .1.5.9....
  // ..........
  // .2.6......
  // ..........
  // .3.7......
  // ..........
  // But don't inset if the image is too narrow or too short.
  const int kInsetX = (width > 2 ? 1 : 0);
  const int kInsetY = (height > 2 ? 1 : 0);
  int x = kInsetX + (calls_ / kNumberOfClusters) *
                        ((width - 2 * kInsetX) / kNumberOfClusters);
  int y = kInsetY + (calls_ % kNumberOfClusters) *
                        ((height - 2 * kInsetY) / kNumberOfClusters);
  int index = x + (y * width);
  ++calls_;
  return index % (width * height);
}

SkColor FindClosestColor(const uint8_t* image,
                         int width,
                         int height,
                         SkColor color) {
  uint8_t in_r = SkColorGetR(color);
  uint8_t in_g = SkColorGetG(color);
  uint8_t in_b = SkColorGetB(color);
  // Search using distance-squared to avoid expensive sqrt() operations.
  int best_distance_squared = std::numeric_limits<int32_t>::max();
  SkColor best_color = color;
  const uint8_t* byte = image;
  for (int i = 0; i < width * height; ++i) {
    uint8_t b = *(byte++);
    uint8_t g = *(byte++);
    uint8_t r = *(byte++);
    uint8_t a = *(byte++);
    // Ignore fully transparent pixels.
    if (a == 0)
      continue;
    int distance_squared =
        (in_b - b) * (in_b - b) +
        (in_g - g) * (in_g - g) +
        (in_r - r) * (in_r - r);
    if (distance_squared < best_distance_squared) {
      best_distance_squared = distance_squared;
      best_color = SkColorSetRGB(r, g, b);
    }
  }
  return best_color;
}

// For a 16x16 icon on an Intel Core i5 this function takes approximately
// 0.5 ms to run.
// TODO(port): This code assumes the CPU architecture is little-endian.
SkColor CalculateKMeanColorOfBuffer(uint8_t* decoded_data,
                                    int img_width,
                                    int img_height,
                                    const HSL& lower_bound,
                                    const HSL& upper_bound,
                                    KMeanImageSampler* sampler,
                                    bool find_closest) {
  SkColor color = kDefaultBgColor;
  if (img_width > 0 && img_height > 0) {
    std::vector<KMeanCluster> clusters;
    clusters.resize(static_cast<size_t>(kNumberOfClusters), KMeanCluster());

    // Pick a starting point for each cluster
    auto new_cluster = clusters.begin();
    while (new_cluster != clusters.end()) {
      // Try up to 10 times to find a unique color. If no unique color can be
      // found, destroy this cluster.
      bool color_unique = false;
      for (int i = 0; i < 10; ++i) {
        int pixel_pos = sampler->GetSample(img_width, img_height) %
            (img_width * img_height);

        uint8_t b = decoded_data[pixel_pos * 4];
        uint8_t g = decoded_data[pixel_pos * 4 + 1];
        uint8_t r = decoded_data[pixel_pos * 4 + 2];
        uint8_t a = decoded_data[pixel_pos * 4 + 3];
        // Skip fully transparent pixels as they usually contain black in their
        // RGB channels but do not contribute to the visual image.
        if (a == 0)
          continue;

        // Loop through the previous clusters and check to see if we have seen
        // this color before.
        color_unique = true;
        for (auto cluster = clusters.begin(); cluster != new_cluster;
             ++cluster) {
          if (cluster->IsAtCentroid(r, g, b)) {
            color_unique = false;
            break;
          }
        }

        // If we have a unique color set the center of the cluster to
        // that color.
        if (color_unique) {
          new_cluster->SetCentroid(r, g, b);
          break;
        }
      }

      // If we don't have a unique color erase this cluster.
      if (!color_unique) {
        new_cluster = clusters.erase(new_cluster);
      } else {
        // Have to increment the iterator here, otherwise the increment in the
        // for loop will skip a cluster due to the erase if the color wasn't
        // unique.
        ++new_cluster;
      }
    }

    // If all pixels in the image are transparent we will have no clusters.
    if (clusters.empty())
      return color;

    bool convergence = false;
    for (int iteration = 0;
        iteration < kNumberOfIterations && !convergence;
        ++iteration) {

      // Loop through each pixel so we can place it in the appropriate cluster.
      uint8_t* pixel = decoded_data;
      uint8_t* decoded_data_end = decoded_data + (img_width * img_height * 4);
      while (pixel < decoded_data_end) {
        uint8_t b = *(pixel++);
        uint8_t g = *(pixel++);
        uint8_t r = *(pixel++);
        uint8_t a = *(pixel++);
        // Skip transparent pixels, see above.
        if (a == 0)
          continue;

        uint32_t distance_sqr_to_closest_cluster = UINT_MAX;
        auto closest_cluster = clusters.begin();

        // Figure out which cluster this color is closest to in RGB space.
        for (auto cluster = clusters.begin(); cluster != clusters.end();
             ++cluster) {
          uint32_t distance_sqr = cluster->GetDistanceSqr(r, g, b);

          if (distance_sqr < distance_sqr_to_closest_cluster) {
            distance_sqr_to_closest_cluster = distance_sqr;
            closest_cluster = cluster;
          }
        }

        closest_cluster->AddPoint(r, g, b);
      }

      // Calculate the new cluster centers and see if we've converged or not.
      convergence = true;
      for (auto cluster = clusters.begin(); cluster != clusters.end();
           ++cluster) {
        convergence &= cluster->CompareCentroidWithAggregate();

        cluster->RecomputeCentroid();
      }
    }

    // Sort the clusters by population so we can tell what the most popular
    // color is.
    std::sort(clusters.begin(), clusters.end(),
              KMeanCluster::SortKMeanClusterByWeight);

    // Loop through the clusters to figure out which cluster has an appropriate
    // color. Skip any that are too bright/dark and go in order of weight.
    for (auto cluster = clusters.begin(); cluster != clusters.end();
         ++cluster) {
      uint8_t r, g, b;
      cluster->GetCentroid(&r, &g, &b);

      SkColor current_color = SkColorSetARGB(SK_AlphaOPAQUE, r, g, b);
      HSL hsl;
      SkColorToHSL(current_color, &hsl);
      if (IsWithinHSLRange(hsl, lower_bound, upper_bound)) {
        // If we found a valid color just set it and break. We don't want to
        // check the other ones.
        color = current_color;
        break;
      } else if (cluster == clusters.begin()) {
        // We haven't found a valid color, but we are at the first color so
        // set the color anyway to make sure we at least have a value here.
        color = current_color;
      }
    }
  }

  // The K-mean cluster center will not usually be a color that appears in the
  // image.  If desired, find a color that actually appears.
  return find_closest
             ? FindClosestColor(decoded_data, img_width, img_height, color)
             : color;
}

SkColor CalculateKMeanColorOfPNG(scoped_refptr<base::RefCountedMemory> png,
                                 const HSL& lower_bound,
                                 const HSL& upper_bound,
                                 KMeanImageSampler* sampler) {
  int img_width = 0;
  int img_height = 0;
  std::vector<uint8_t> decoded_data;
  SkColor color = kDefaultBgColor;

  if (png.get() && png->size() &&
      gfx::PNGCodec::Decode(png->front(), png->size(),
                            gfx::PNGCodec::FORMAT_BGRA, &decoded_data,
                            &img_width, &img_height)) {
    return CalculateKMeanColorOfBuffer(&decoded_data[0], img_width, img_height,
                                       lower_bound, upper_bound, sampler, true);
  }
  return color;
}

SkColor CalculateKMeanColorOfPNG(scoped_refptr<base::RefCountedMemory> png) {
  GridSampler sampler;
  return CalculateKMeanColorOfPNG(
      png, kDefaultLowerHSLBound, kDefaultUpperHSLBound, &sampler);
}

SkColor CalculateKMeanColorOfBitmap(const SkBitmap& bitmap,
                                    int height,
                                    const HSL& lower_bound,
                                    const HSL& upper_bound,
                                    bool find_closest) {
  // Clamp the height being used to the height of the provided image (otherwise,
  // we can end up creating a larger buffer than we have data for, and the end
  // of the buffer will remain uninitialized after we copy/UnPreMultiply the
  // image data into it).
  height = base::ClampToRange(height, 0, bitmap.height());

  // SkBitmap uses pre-multiplied alpha but the KMean clustering function
  // above uses non-pre-multiplied alpha. Transform the bitmap before we
  // analyze it because the function reads each pixel multiple times.
  int pixel_count = bitmap.width() * height;
  std::unique_ptr<uint32_t[]> image(new uint32_t[pixel_count]);

  // Un-premultiplies each pixel in bitmap into the buffer. Requires
  // approximately 10 microseconds for a 16x16 icon on an Intel Core i5.
  uint32_t* in = static_cast<uint32_t*>(bitmap.getPixels());
  uint32_t* out = image.get();
  for (int i = 0; i < pixel_count; ++i)
    *out++ = SkUnPreMultiply::PMColorToColor(*in++);

  GridSampler sampler;
  return CalculateKMeanColorOfBuffer(reinterpret_cast<uint8_t*>(image.get()),
                                     bitmap.width(), height, lower_bound,
                                     upper_bound, &sampler, find_closest);
}

SkColor CalculateKMeanColorOfBitmap(const SkBitmap& bitmap) {
  return CalculateKMeanColorOfBitmap(
      bitmap, bitmap.height(), kDefaultLowerHSLBound, kDefaultUpperHSLBound,
      true);
}

const int kMaxConsideredPixelsForSwatches = 10007;

// This algorithm is a port of Android's Palette API. Compare to package
// android.support.v7.graphics and see that code for additional high-level
// explanation of this algorithm. There are some minor differences:
//   * This code doesn't exclude the same color from being used for
//   different color profiles.
//   * This code doesn't try to heuristically derive missing colors from
//   existing colors.
std::vector<Swatch> CalculateColorSwatches(
    const SkBitmap& bitmap,
    size_t max_swatches,
    const gfx::Rect& region,
    base::Optional<ColorSwatchFilter> filter) {
  DCHECK(!bitmap.empty());
  DCHECK(!bitmap.isNull());
  DCHECK(!region.IsEmpty());
  DCHECK_LE(region.width(), bitmap.width());
  DCHECK_LE(region.height(), bitmap.height());

  const int pixel_count = region.width() * region.height();

  // For better performance, only consider at most 10k pixels (evenly
  // distributed throughout the image). This has a very minor impact on the
  // outcome but improves runtime substantially for large images. 10,007 is a
  // prime number to reduce the chance of picking an unrepresentative sample.
  const int pixel_increment =
      std::max(1, pixel_count / kMaxConsideredPixelsForSwatches);
  std::unordered_map<SkColor, int> color_counts(
      kMaxConsideredPixelsForSwatches);

  // First extract all colors into counts.
  for (int i = 0; i < pixel_count; i += pixel_increment) {
    const int x = region.x() + (i % region.width());
    const int y = region.y() + (i / region.width());

    const SkColor pixel = bitmap.getColor(x, y);
    if (SkColorGetA(pixel) == SK_AlphaTRANSPARENT)
      continue;

    color_counts[pixel]++;
  }

  // Now throw out some uninteresting colors if there is a filter.
  std::vector<SkColor> interesting_colors;
  interesting_colors.reserve(color_counts.size());
  for (auto color_count : color_counts) {
    SkColor color = color_count.first;
    if (!filter || filter->Run(color))
      interesting_colors.push_back(color);
  }

  if (interesting_colors.empty())
    return {};

  // Group the colors into "boxes" and repeatedly split the most voluminous box.
  // We stop the process when a box can no longer be split (there's only one
  // color in it) or when the number of color boxes reaches |max_colors|.
  //
  // Boxes are sorted by volume with the most voluminous at the front of the PQ.
  std::priority_queue<ColorBox, std::vector<ColorBox>,
                      bool (*)(const ColorBox&, const ColorBox&)>
      boxes(&ColorBox::CompareByVolume);
  boxes.emplace(&interesting_colors);
  while (boxes.size() < max_swatches) {
    auto box = boxes.top();
    if (!box.CanSplit())
      break;
    boxes.pop();
    boxes.push(box.Split());
    boxes.push(box);
  }

  // Now extract a single color to represent each box. This is the average color
  // in the box, weighted by the frequency of that color in the source image.
  size_t max_weight = 0;
  std::vector<Swatch> box_colors;
  box_colors.reserve(max_swatches);
  while (!boxes.empty()) {
    box_colors.push_back(boxes.top().GetWeightedAverageColor(color_counts));
    boxes.pop();
    max_weight = std::max(max_weight, box_colors.back().population);
  }

  return box_colors;
}

std::vector<color_utils::Swatch> CalculateProminentColorsOfBitmap(
    const SkBitmap& bitmap,
    const std::vector<ColorProfile>& color_profiles,
    gfx::Rect* region,
    ColorSwatchFilter filter) {
  if (color_profiles.empty())
    return std::vector<Swatch>();

  size_t size = color_profiles.size();
  if (bitmap.empty() || bitmap.isNull())
    return std::vector<Swatch>(size, Swatch());

  // The hue is not relevant to our bounds or goal colors.
  std::vector<ColorBracket> color_brackets(size);
  for (size_t i = 0; i < size; ++i) {
    switch (color_profiles[i].luma) {
      case LumaRange::ANY:
        color_brackets[i].lower_bound.l = 0;
        color_brackets[i].upper_bound.l = 1;
        color_brackets[i].goal.l = 0.5f;
        break;
      case LumaRange::LIGHT:
        color_brackets[i].lower_bound.l = 0.55f;
        color_brackets[i].upper_bound.l = 1;
        color_brackets[i].goal.l = 0.74f;
        break;
      case LumaRange::NORMAL:
        color_brackets[i].lower_bound.l = 0.3f;
        color_brackets[i].upper_bound.l = 0.7f;
        color_brackets[i].goal.l = 0.5f;
        break;
      case LumaRange::DARK:
        color_brackets[i].lower_bound.l = 0;
        color_brackets[i].upper_bound.l = 0.45f;
        color_brackets[i].goal.l = 0.26f;
        break;
    }

    switch (color_profiles[i].saturation) {
      case SaturationRange::ANY:
        color_brackets[i].lower_bound.s = 0;
        color_brackets[i].upper_bound.s = 1;
        color_brackets[i].goal.s = 0.5f;
        break;
      case SaturationRange::VIBRANT:
        color_brackets[i].lower_bound.s = 0.35f;
        color_brackets[i].upper_bound.s = 1;
        color_brackets[i].goal.s = 1;
        break;
      case SaturationRange::MUTED:
        color_brackets[i].lower_bound.s = 0;
        color_brackets[i].upper_bound.s = 0.4f;
        color_brackets[i].goal.s = 0.3f;
        break;
    }
  }

  return CalculateProminentColors(
      bitmap, color_brackets,
      region ? *region : gfx::Rect(bitmap.width(), bitmap.height()),
      filter.is_null() ? base::BindRepeating(&IsInterestingColor) : filter);
}

gfx::Matrix3F ComputeColorCovariance(const SkBitmap& bitmap) {
  // First need basic stats to normalize each channel separately.
  gfx::Matrix3F covariance = gfx::Matrix3F::Zeros();
  if (!bitmap.getPixels())
    return covariance;

  // Assume ARGB_8888 format.
  DCHECK(bitmap.colorType() == kN32_SkColorType);

  int64_t r_sum = 0;
  int64_t g_sum = 0;
  int64_t b_sum = 0;
  int64_t rr_sum = 0;
  int64_t gg_sum = 0;
  int64_t bb_sum = 0;
  int64_t rg_sum = 0;
  int64_t rb_sum = 0;
  int64_t gb_sum = 0;

  for (int y = 0; y < bitmap.height(); ++y) {
    SkPMColor* current_color = static_cast<uint32_t*>(bitmap.getAddr32(0, y));
    for (int x = 0; x < bitmap.width(); ++x, ++current_color) {
      SkColor c = SkUnPreMultiply::PMColorToColor(*current_color);
      SkColor r = SkColorGetR(c);
      SkColor g = SkColorGetG(c);
      SkColor b = SkColorGetB(c);

      r_sum += r;
      g_sum += g;
      b_sum += b;
      rr_sum += r * r;
      gg_sum += g * g;
      bb_sum += b * b;
      rg_sum += r * g;
      rb_sum += r * b;
      gb_sum += g * b;
    }
  }

  // Covariance (not normalized) is E(X*X.t) - m * m.t and this is how it
  // is calculated below.
  // Each row below represents a row of the matrix describing (co)variances
  // of R, G and B channels with (R, G, B)
  int pixel_n = bitmap.width() * bitmap.height();
  covariance.set(
      static_cast<float>(
          static_cast<double>(rr_sum) / pixel_n -
              static_cast<double>(r_sum * r_sum) / pixel_n / pixel_n),
      static_cast<float>(
          static_cast<double>(rg_sum) / pixel_n -
              static_cast<double>(r_sum * g_sum) / pixel_n / pixel_n),
      static_cast<float>(
          static_cast<double>(rb_sum) / pixel_n -
              static_cast<double>(r_sum * b_sum) / pixel_n / pixel_n),
      static_cast<float>(
          static_cast<double>(rg_sum) / pixel_n -
              static_cast<double>(r_sum * g_sum) / pixel_n / pixel_n),
      static_cast<float>(
          static_cast<double>(gg_sum) / pixel_n -
              static_cast<double>(g_sum * g_sum) / pixel_n / pixel_n),
      static_cast<float>(
          static_cast<double>(gb_sum) / pixel_n -
              static_cast<double>(g_sum * b_sum) / pixel_n / pixel_n),
      static_cast<float>(
          static_cast<double>(rb_sum) / pixel_n -
              static_cast<double>(r_sum * b_sum) / pixel_n / pixel_n),
      static_cast<float>(
          static_cast<double>(gb_sum) / pixel_n -
              static_cast<double>(g_sum * b_sum) / pixel_n / pixel_n),
      static_cast<float>(
          static_cast<double>(bb_sum) / pixel_n -
              static_cast<double>(b_sum * b_sum) / pixel_n / pixel_n));
  return covariance;
}

bool ApplyColorReduction(const SkBitmap& source_bitmap,
                         const gfx::Vector3dF& color_transform,
                         bool fit_to_range,
                         SkBitmap* target_bitmap) {
  DCHECK(target_bitmap);
  DCHECK(source_bitmap.getPixels());
  DCHECK(target_bitmap->getPixels());
  DCHECK_EQ(kN32_SkColorType, source_bitmap.colorType());
  DCHECK_EQ(kAlpha_8_SkColorType, target_bitmap->colorType());
  DCHECK_EQ(source_bitmap.height(), target_bitmap->height());
  DCHECK_EQ(source_bitmap.width(), target_bitmap->width());
  DCHECK(!source_bitmap.empty());

  // Elements of color_transform are explicitly off-loaded to local values for
  // efficiency reasons. Note that in practice images may correspond to entire
  // tab captures.
  float t0 = 0.0;
  float tr = color_transform.x();
  float tg = color_transform.y();
  float tb = color_transform.z();

  if (fit_to_range) {
    // We will figure out min/max in a preprocessing step and adjust
    // actual_transform as required.
    float max_val = std::numeric_limits<float>::min();
    float min_val = std::numeric_limits<float>::max();
    for (int y = 0; y < source_bitmap.height(); ++y) {
      const SkPMColor* source_color_row = static_cast<SkPMColor*>(
          source_bitmap.getAddr32(0, y));
      for (int x = 0; x < source_bitmap.width(); ++x) {
        SkColor c = SkUnPreMultiply::PMColorToColor(source_color_row[x]);
        uint8_t r = SkColorGetR(c);
        uint8_t g = SkColorGetG(c);
        uint8_t b = SkColorGetB(c);
        float gray_level = tr * r + tg * g + tb * b;
        max_val = std::max(max_val, gray_level);
        min_val = std::min(min_val, gray_level);
      }
    }

    // Adjust the transform so that the result is scaling.
    float scale = 0.0;
    t0 = -min_val;
    if (max_val > min_val)
      scale = 255.0f / (max_val - min_val);
    t0 *= scale;
    tr *= scale;
    tg *= scale;
    tb *= scale;
  }

  for (int y = 0; y < source_bitmap.height(); ++y) {
    const SkPMColor* source_color_row = static_cast<SkPMColor*>(
        source_bitmap.getAddr32(0, y));
    uint8_t* target_color_row = target_bitmap->getAddr8(0, y);
    for (int x = 0; x < source_bitmap.width(); ++x) {
      SkColor c = SkUnPreMultiply::PMColorToColor(source_color_row[x]);
      uint8_t r = SkColorGetR(c);
      uint8_t g = SkColorGetG(c);
      uint8_t b = SkColorGetB(c);

      float gl = t0 + tr * r + tg * g + tb * b;
      if (gl < 0)
        gl = 0;
      if (gl > 0xFF)
        gl = 0xFF;
      target_color_row[x] = static_cast<uint8_t>(gl);
    }
  }

  return true;
}

bool ComputePrincipalComponentImage(const SkBitmap& source_bitmap,
                                    SkBitmap* target_bitmap) {
  if (!target_bitmap) {
    NOTREACHED();
    return false;
  }

  gfx::Matrix3F covariance = ComputeColorCovariance(source_bitmap);
  gfx::Matrix3F eigenvectors = gfx::Matrix3F::Zeros();
  gfx::Vector3dF eigenvals = covariance.SolveEigenproblem(&eigenvectors);
  gfx::Vector3dF principal = eigenvectors.get_column(0);
  if (eigenvals == gfx::Vector3dF() || principal == gfx::Vector3dF())
    return false;  // This may happen for some edge cases.
  return ApplyColorReduction(source_bitmap, principal, true, target_bitmap);
}

}  // color_utils
