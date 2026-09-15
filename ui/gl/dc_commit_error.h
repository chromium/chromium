// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DC_COMMIT_ERROR_H_
#define UI_GL_DC_COMMIT_ERROR_H_

#include <windows.h>

#include <optional>

namespace gl {

struct CommitError {
  // The source of the commit error. This should correspond with exactly one
  // place in code to make identifying the cause of errors easier.
  enum class Reason {
    kUnknown,
    kIDCompositionDeviceCommit,
    kSolidColorSurfacePoolCreateSurface,
    kSolidColorSurfaceBeginDraw,
    kSolidColorSurfaceEndDraw,
    kSolidColorSurfaceCreateRenderTargetView,
    kSolidColorTexturePoolCreateD3D12Resource,
    kSolidColorTexturePoolCreateSharedTextureMemory,
    kSolidColorTexturePoolCreateDawnSharedTexture,
    kSolidColorTexturePoolBeginAccess,
    kIDCompositionDevice6PresentCompositionTextures,
    kInitializeVideoProcessorD3D11DeviceAsVideoDevice,
    kInitializeVideoProcessorCreateVideoProcessorEnumerator,
    kInitializeVideoProcessorCreateVideoProcessor,
    kPresentToSwapChainCreateSurfaceFromHandle,
    kPresentToSwapChainCreateVideoProcessorInputView,
    kPresentToSwapChainCreateVideoProcessorOutputView,
    kPresentToSwapChainVideoProcessorBlt,
    kPresentToSwapChainCreateSwapChainForCompositionSurfaceHandle,
    kPresentToSwapChainFirstPresent,
    kPresentToSwapChainPresentBuffer,
    kPresentToSwapChainPresent,
    kPresentToSwapChainSdrRevertMissingVideoProcessor,
    kPresentToSwapChainSdrRevertCreateVideoProcessorOutputView,
    kPresentToSwapChainSdrRevertSetColorSpace,
    kUploadVideoImageInvalidPixmapData,
    kUploadVideoImageInvalidPixmapSize,
    kUploadVideoImageInvalidPixmapStride,
    kUploadVideoImageCreateStagingTexture,
    kUploadVideoImageMapStagingTexture,
    kUploadVideoImageCreateCopyTexture,
    kMaxValue = kUploadVideoImageCreateCopyTexture,
  };

  Reason reason = Reason::kUnknown;

  // If set, the error was caused by a Windows API and this is the HRESULT. If
  // not set, the error was not caused by a Windows API or we did not explicitly
  // copy out the failing HRESULT for the given `reason`.
  std::optional<HRESULT> hr;
};

}  // namespace gl

#endif  // UI_GL_DC_COMMIT_ERROR_H_
