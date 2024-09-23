// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CA_LAYER_RESULT_H_
#define UI_GFX_CA_LAYER_RESULT_H_

namespace gfx {

// This is the result of ProcessForCALayerOverlays() and is for macOS only.
// This enum is used for histogram states and should only have new values added
// to the end before COUNT. tools/metrics/histograms/enums.xml should be updated
// together.
// All changes made to enum CALayerResult should be added to
// ui/gfx/mojom/ca_layer_result.mojom.

enum CALayerResult {
  kCALayerSuccess = 0,
  kCALayerFailedUnknown = 1,
  // kCALayerFailedIOSurfaceNotCandidate = 2,
  // kCALayerFailedStreamVideoNotCandidate = 3,
  // kCALayerFailedStreamVideoTransform = 4,
  kCALayerFailedTextureNotCandidate = 5,
  // kCALayerFailedTextureYFlipped = 6,
  kCALayerFailedTileNotCandidate = 7,
  kCALayerFailedQuadBlendMode = 8,
  // kCALayerFailedQuadTransform = 9,
  kCALayerFailedQuadClipping = 10,
  kCALayerFailedDebugBoarder = 11,
  kCALayerFailedPictureContent = 12,
  // kCALayerFailedRenderPass = 13,
  kCALayerFailedSurfaceContent = 14,
  // kCALayerFailedYUVVideoContent = 15,
  kCALayerFailedDifferentClipSettings = 16,
  // kCALayerFailedDifferentVertexOpacities = 17,
  // kCALayerFailedRenderPassfilterScale = 18,
  kCALayerFailedRenderPassBackdropFilters = 19,
  kCALayerFailedRenderPassPassMask = 20,
  kCALayerFailedRenderPassFilterOperation = 21,
  kCALayerFailedRenderPassSortingContextId = 22,
  kCALayerFailedTooManyRenderPassDrawQuads = 23,
  // kCALayerFailedQuadRoundedCorner = 24,
  // kCALayerFailedQuadRoundedCornerClipMismatch = 25,
  kCALayerFailedQuadRoundedCornerNotUniform = 26,
  kCALayerFailedTooManyQuads = 27,
  // kCALayerFailedYUVNotCandidate = 28,
  // kCALayerFailedYUVTexcoordMismatch = 29,
  // kCALayerFailedYUVInvalidPlanes = 30,
  kCALayerFailedCopyRequests = 31,
  kCALayerFailedOverlayDisabled = 32,
  kCALayerFailedVideoCaptureEnabled = 33,
  kCALayerUnknownDidNotSwap = 34,  // For gpu_bench_marking only
  kCALayerUnknownNoWidget = 35,    // For gpu_bench_marking only
  kMaxValue = kCALayerUnknownNoWidget,
};
}  // namespace gfx

#endif  // UI_GFX_CA_LAYER_RESULT_H_
