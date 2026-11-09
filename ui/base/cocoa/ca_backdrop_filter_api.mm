// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/ca_backdrop_filter_api.h"

#include <cmath>

#include "base/bit_cast.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "ui/base/cocoa/remote_layer_api.h"

#if BUILDFLAG(IS_MAC)
// Private CoreAnimation structs and methods used for hardware-accelerated
// backdrop filters. Accessed strictly via dynamic runtime lookup below.
// Declared via a category on NSObject rather than redeclaring @interface
// CAFilter / CABackdropLayer so a future SDK roll exposing those headers cannot
// cause a duplicate interface error.
struct CAColorMatrix {
  float m11, m12, m13, m14, m15;
  float m21, m22, m23, m24, m25;
  float m31, m32, m33, m34, m35;
  float m41, m42, m43, m44, m45;
};

@interface NSObject (ChromiumCABackdropSPI)
+ (id)filterWithType:(NSString*)type;
- (NSArray<NSString*>*)inputKeys;
- (void)setWindowServerAware:(BOOL)window_server_aware;
@end
#endif  // BUILDFLAG(IS_MAC)

namespace ui {

namespace {

struct CABackdropSPI {
  Class backdrop_class = nil;
  Class filter_class = nil;
  bool supported = false;
};

const CABackdropSPI& GetCABackdropSPI() {
  static const CABackdropSPI spi = [] {
    CABackdropSPI result;
#if BUILDFLAG(IS_MAC)
    Class backdrop_class = NSClassFromString(@"CABackdropLayer");
    Class filter_class = NSClassFromString(@"CAFilter");
    if (!backdrop_class ||
        ![backdrop_class isSubclassOfClass:[CALayer class]] || !filter_class ||
        ![filter_class respondsToSelector:@selector(filterWithType:)] ||
        ![filter_class instancesRespondToSelector:@selector(inputKeys)]) {
      return result;
    }

    id blur_filter = [filter_class filterWithType:@"gaussianBlur"];
    NSArray<NSString*>* blur_keys = [blur_filter inputKeys];
    if (!blur_filter || ![blur_keys containsObject:@"inputRadius"] ||
        ![blur_keys containsObject:@"inputNormalizeEdges"]) {
      return result;
    }

    id color_matrix_filter = [filter_class filterWithType:@"colorMatrix"];
    NSArray<NSString*>* color_matrix_keys = [color_matrix_filter inputKeys];
    if (!color_matrix_filter ||
        ![color_matrix_keys containsObject:@"inputColorMatrix"]) {
      return result;
    }

    result.backdrop_class = backdrop_class;
    result.filter_class = filter_class;
    result.supported = true;
#endif  // BUILDFLAG(IS_MAC)
    return result;
  }();
  return spi;
}

}  // namespace

// Control use of CoreAnimation CABackdropLayer and CAFilter for hardware-
// accelerated CSS backdrop-filter on macOS.
BASE_FEATURE(kCoreAnimationBackdropFilter, base::FEATURE_DISABLED_BY_DEFAULT);

bool CABackdropLayerSupported() {
  return RemoteLayerAPISupported() &&
         base::FeatureList::IsEnabled(kCoreAnimationBackdropFilter) &&
         GetCABackdropSPI().supported;
}

CALayer* CreateCABackdropLayer() {
#if BUILDFLAG(IS_MAC)
  const CABackdropSPI& spi = GetCABackdropSPI();
  CHECK(spi.supported);
  CALayer* layer = [[spi.backdrop_class alloc] init];
  CHECK(layer);
  // Only sample content from this layer tree, never other WindowServer content
  // (e.g. other windows behind a translucent window). This is already the
  // default; set it explicitly in case the default changes.
  if ([layer respondsToSelector:@selector(setWindowServerAware:)]) {
    [layer setWindowServerAware:NO];
  }
  return layer;
#else
  NOTREACHED();
#endif
}

id CreateCABlurFilter(double blur_radius) {
#if BUILDFLAG(IS_MAC)
  CHECK_GT(blur_radius, 0.0);
  CHECK(std::isfinite(blur_radius));
  const CABackdropSPI& spi = GetCABackdropSPI();
  CHECK(spi.supported);
  id blur_filter = [spi.filter_class filterWithType:@"gaussianBlur"];
  CHECK(blur_filter);
  [blur_filter setValue:@(blur_radius) forKey:@"inputRadius"];
  // CoreAnimation has no mirror edge mode, which is what SkiaRenderer uses for
  // backdrop-filter blurs. Normalizing edges avoids darkening from sampling
  // transparent pixels, but results can still differ slightly within
  // `blur_radius` of the edges.
  [blur_filter setValue:@YES forKey:@"inputNormalizeEdges"];
  return blur_filter;
#else
  NOTREACHED();
#endif
}

id CreateCAColorMatrixFilter(const std::array<float, 20>& matrix) {
#if BUILDFLAG(IS_MAC)
  const CABackdropSPI& spi = GetCABackdropSPI();
  CHECK(spi.supported);
  id color_matrix_filter = [spi.filter_class filterWithType:@"colorMatrix"];
  CHECK(color_matrix_filter);
  CAColorMatrix ca_matrix = base::bit_cast<CAColorMatrix>(matrix);
  NSValue* matrix_value = [NSValue valueWithBytes:&ca_matrix
                                         objCType:@encode(CAColorMatrix)];
  [color_matrix_filter setValue:matrix_value forKey:@"inputColorMatrix"];
  return color_matrix_filter;
#else
  NOTREACHED();
#endif
}

}  // namespace ui
