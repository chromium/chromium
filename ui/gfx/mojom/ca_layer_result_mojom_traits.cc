// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/ca_layer_result_mojom_traits.h"
#include "build/build_config.h"

namespace mojo {

#if BUILDFLAG(IS_APPLE)
// static
gfx::mojom::CALayerResult
EnumTraits<gfx::mojom::CALayerResult, gfx::CALayerResult>::ToMojom(
    gfx::CALayerResult ca_layer_error_code) {
  switch (ca_layer_error_code) {
    case gfx::kCALayerSuccess:  // = 0,
      return gfx::mojom::CALayerResult::kCALayerSuccess;
    case gfx::kCALayerFailedUnknown:  // = 1,
      return gfx::mojom::CALayerResult::kCALayerFailedUnknown;
    case gfx::kCALayerFailedTextureNotCandidate:  // = 5,
      return gfx::mojom::CALayerResult::kCALayerFailedTextureNotCandidate;
    case gfx::kCALayerFailedTileNotCandidate:  // = 7,
      return gfx::mojom::CALayerResult::kCALayerFailedTileNotCandidate;
    case gfx::kCALayerFailedQuadBlendMode:  // = 8,
      return gfx::mojom::CALayerResult::kCALayerFailedQuadBlendMode;
    case gfx::kCALayerFailedQuadClipping:  // = 10,
      return gfx::mojom::CALayerResult::kCALayerFailedQuadClipping;
    case gfx::kCALayerFailedDebugBoarder:  // = 11,
      return gfx::mojom::CALayerResult::kCALayerFailedDebugBoarder;
    case gfx::kCALayerFailedPictureContent:  // = 12,
      return gfx::mojom::CALayerResult::kCALayerFailedPictureContent;
    case gfx::kCALayerFailedSurfaceContent:  // = 14,
      return gfx::mojom::CALayerResult::kCALayerFailedSurfaceContent;
    case gfx::kCALayerFailedDifferentClipSettings:  // = 16,
      return gfx::mojom::CALayerResult::kCALayerFailedDifferentClipSettings;
    case gfx::kCALayerFailedRenderPassBackdropFilters:  // = 19,
      return gfx::mojom::CALayerResult::kCALayerFailedRenderPassBackdropFilters;
    case gfx::kCALayerFailedRenderPassPassMask:  // = 20,
      return gfx::mojom::CALayerResult::kCALayerFailedRenderPassPassMask;
    case gfx::kCALayerFailedRenderPassFilterOperation:  // = 21,
      return gfx::mojom::CALayerResult::kCALayerFailedRenderPassFilterOperation;
    case gfx::kCALayerFailedRenderPassSortingContextId:  // = 22,
      return gfx::mojom::CALayerResult::
          kCALayerFailedRenderPassSortingContextId;
    case gfx::kCALayerFailedTooManyRenderPassDrawQuads:  // = 23,
      return gfx::mojom::CALayerResult::
          kCALayerFailedTooManyRenderPassDrawQuads;
    case gfx::kCALayerFailedQuadRoundedCornerNotUniform:  // = 26,
      return gfx::mojom::CALayerResult::
          kCALayerFailedQuadRoundedCornerNotUniform;
    case gfx::kCALayerFailedTooManyQuads:  // = 27,
      return gfx::mojom::CALayerResult::kCALayerFailedTooManyQuads;
    case gfx::kCALayerFailedCopyRequests:  // = 31,
      return gfx::mojom::CALayerResult::kCALayerFailedCopyRequests;
    case gfx::kCALayerFailedOverlayDisabled:  // = 32,
      return gfx::mojom::CALayerResult::kCALayerFailedOverlayDisabled;
    case gfx::kCALayerFailedVideoCaptureEnabled:  // = 33,
      return gfx::mojom::CALayerResult::kCALayerFailedVideoCaptureEnabled;
    case gfx::kCALayerUnknownDidNotSwap:  // = 34,
      NOTREACHED_IN_MIGRATION();
      return gfx::mojom::CALayerResult::kCALayerFailedUnknown;
    case gfx::kCALayerUnknownNoWidget:  // = 35,
      NOTREACHED_IN_MIGRATION();
      return gfx::mojom::CALayerResult::kCALayerFailedUnknown;
  }

  NOTREACHED_IN_MIGRATION() << "CALayer result:" << ca_layer_error_code;
  return gfx::mojom::CALayerResult::kCALayerFailedUnknown;
}

// static
bool EnumTraits<gfx::mojom::CALayerResult, gfx::CALayerResult>::FromMojom(
    gfx::mojom::CALayerResult input,
    gfx::CALayerResult* out) {
  switch (input) {
    case gfx::mojom::CALayerResult::kCALayerSuccess:  // = 0
      *out = gfx::kCALayerSuccess;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedUnknown:  // = 1
      *out = gfx::kCALayerFailedUnknown;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedTextureNotCandidate:  // = 5
      *out = gfx::kCALayerFailedTextureNotCandidate;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedTileNotCandidate:  // = 7
      *out = gfx::kCALayerFailedTileNotCandidate;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedQuadBlendMode:  // = 8
      *out = gfx::kCALayerFailedQuadBlendMode;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedQuadClipping:  // = 10
      *out = gfx::kCALayerFailedQuadClipping;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedDebugBoarder:  // = 11
      *out = gfx::kCALayerFailedDebugBoarder;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedPictureContent:  // = 12
      *out = gfx::kCALayerFailedPictureContent;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedSurfaceContent:  // = 14
      *out = gfx::kCALayerFailedSurfaceContent;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedDifferentClipSettings:  // =
                                                                          // 16
      *out = gfx::kCALayerFailedDifferentClipSettings;
      return true;
    case gfx::mojom::CALayerResult::
        kCALayerFailedRenderPassBackdropFilters:  // = 19
      *out = gfx::kCALayerFailedRenderPassBackdropFilters;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedRenderPassPassMask:  // = 20
      *out = gfx::kCALayerFailedRenderPassPassMask;
      return true;
    case gfx::mojom::CALayerResult::
        kCALayerFailedRenderPassFilterOperation:  // = 21
      *out = gfx::kCALayerFailedRenderPassFilterOperation;
      return true;
    case gfx::mojom::CALayerResult::
        kCALayerFailedRenderPassSortingContextId:  // = 22
      *out = gfx::kCALayerFailedRenderPassSortingContextId;
      return true;
    case gfx::mojom::CALayerResult::
        kCALayerFailedTooManyRenderPassDrawQuads:  // = 23
      *out = gfx::kCALayerFailedTooManyRenderPassDrawQuads;
      return true;
    case gfx::mojom::CALayerResult::
        kCALayerFailedQuadRoundedCornerNotUniform:  // = 26
      *out = gfx::kCALayerFailedQuadRoundedCornerNotUniform;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedTooManyQuads:  // = 27
      *out = gfx::kCALayerFailedTooManyQuads;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedCopyRequests:  // = 31
      *out = gfx::kCALayerFailedCopyRequests;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedOverlayDisabled:  // = 32
      *out = gfx::kCALayerFailedOverlayDisabled;
      return true;
    case gfx::mojom::CALayerResult::kCALayerFailedVideoCaptureEnabled:  // = 33
      *out = gfx::kCALayerFailedVideoCaptureEnabled;
      return true;
  }

  NOTREACHED_IN_MIGRATION() << "Invalid CALayer result: " << input;
  return false;
}
#endif

}  // namespace mojo
