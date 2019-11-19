// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SWAP_CHAIN_PRESENTER_H_
#define UI_GL_SWAP_CHAIN_PRESENTER_H_

#include <windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

#include "base/containers/circular_deque.h"
#include "base/power_monitor/power_monitor.h"
#include "base/win/scoped_handle.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/dc_renderer_layer_params.h"

namespace gl {
class DCLayerTree;
class GLImageDXGI;
class GLImageMemory;

// SwapChainPresenter holds a swap chain, direct composition visuals, and other
// associated resources for a single overlay layer.  It is updated by calling
// PresentToSwapChain(), and can update or recreate resources as necessary.
class SwapChainPresenter : public base::PowerObserver {
 public:
  SwapChainPresenter(DCLayerTree* layer_tree,
                     Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
                     Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device);
  ~SwapChainPresenter() override;

  // Present the given overlay to swap chain.  Returns true on success.
  bool PresentToSwapChain(const ui::DCRendererLayerParams& overlay);

  const Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain() const {
    return swap_chain_;
  }

  const Microsoft::WRL::ComPtr<IDCompositionVisual2>& visual() const {
    return clip_visual_;
  }

 private:
  // Mapped to DirectCompositonVideoPresentationMode UMA enum.  Do not remove or
  // remap existing entries!
  enum class VideoPresentationMode {
    kZeroCopyDecodeSwapChain = 0,
    kUploadAndVideoProcessorBlit = 1,
    kBindAndVideoProcessorBlit = 2,
    kMaxValue = kBindAndVideoProcessorBlit,
  };

  // Mapped to DecodeSwapChainNotUsedReason UMA enum.  Do not remove or remap
  // existing entries.
  enum class DecodeSwapChainNotUsedReason {
    kSoftwareFrame = 0,
    kNv12NotSupported = 1,
    kFailedToPresent = 2,
    kNonDecoderTexture = 3,
    kSharedTexture = 4,
    kIncompatibleTransform = 5,
    kUnitaryTextureArray = 6,
    kMaxValue = kUnitaryTextureArray,
  };

  // This keeps track of whether the previous 30 frames used Overlays or GPU
  // composition to present.
  class PresentationHistory {
   public:
    static const int kPresentsToStore = 30;

    PresentationHistory();
    ~PresentationHistory();

    void AddSample(DXGI_FRAME_PRESENTATION_MODE mode);

    void Clear();
    bool Valid() const;
    int composed_count() const;

   private:
    base::circular_deque<DXGI_FRAME_PRESENTATION_MODE> presents_;
    int composed_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(PresentationHistory);
  };

  // Upload given YUV buffers to an NV12 texture that can be used to create
  // video processor input view.  Returns nullptr on failure.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> UploadVideoImages(
      GLImageMemory* y_image_memory,
      GLImageMemory* uv_image_memory);

  // Releases resources that might hold indirect references to the swap chain.
  void ReleaseSwapChainResources();

  // Recreate swap chain using given size.  Use preferred YUV format if
  // |use_yuv_swap_chain| is true, or BGRA otherwise.  Sets flags based on
  // |protected_video_type|. Returns true on success.
  bool ReallocateSwapChain(const gfx::Size& swap_chain_size,
                           bool use_yuv_swap_chain,
                           gfx::ProtectedVideoType protected_video_type,
                           bool z_order);

  // Returns true if YUV swap chain should be preferred over BGRA swap chain.
  // This changes over time based on stats recorded in |presentation_history|.
  bool ShouldUseYUVSwapChain(gfx::ProtectedVideoType protected_video_type);

  // Perform a blit using video processor from given input texture to swap chain
  // backbuffer. |input_texture| is the input texture (array), and |input_level|
  // is the index of the texture in the texture array.  |keyed_mutex| is
  // optional, and is used to lock the resource for reading.  |content_rect| is
  // subrectangle of the input texture that should be blitted to swap chain, and
  // |src_color_space| is the color space of the video.
  bool VideoProcessorBlt(Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture,
                         UINT input_level,
                         Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex,
                         const gfx::Rect& content_rect,
                         const gfx::ColorSpace& src_color_space);

  // Returns optimal swap chain size for given layer.
  gfx::Size CalculateSwapChainSize(const ui::DCRendererLayerParams& params);

  // Update direct composition visuals for layer with given swap chain size.
  void UpdateVisuals(const ui::DCRendererLayerParams& params,
                     const gfx::Size& swap_chain_size);

  // Try presenting to a decode swap chain based on various conditions such as
  // global state (e.g. finch, NV12 support), texture flags, and transform.
  // Returns true on success.  See PresentToDecodeSwapChain() for more info.
  bool TryPresentToDecodeSwapChain(GLImageDXGI* nv12_image,
                                   const gfx::Rect& content_rect,
                                   const gfx::Size& swap_chain_size);

  // Present to a decode swap chain created from compatible video decoder
  // buffers using given |nv12_image| with destination size |swap_chain_size|.
  // Returns true on success.
  bool PresentToDecodeSwapChain(GLImageDXGI* nv12_image,
                                const gfx::Rect& content_rect,
                                const gfx::Size& swap_chain_size);

  // Records presentation statistics in UMA and traces (for pixel tests) for the
  // current swap chain which could either be a regular flip swap chain or a
  // decode swap chain.
  void RecordPresentationStatistics();

  // base::PowerObserver
  void OnPowerStateChange(bool on_battery_power) override;

  // If connected with a power source, let the Intel video processor to do
  // the upscaling because it produces better results.
  bool ShouldUseVideoProcessorScaling();

  // Layer tree instance that owns this swap chain presenter.
  DCLayerTree* layer_tree_ = nullptr;

  // Current size of swap chain.
  gfx::Size swap_chain_size_;

  // Whether the current swap chain is using the preferred YUV format.
  bool is_yuv_swapchain_ = false;

  // Whether the swap chain was reallocated, and next present will be the first.
  bool first_present_ = false;

  // Whether the current swap chain is presenting protected video, software
  // or hardware protection.
  gfx::ProtectedVideoType protected_video_type_ =
      gfx::ProtectedVideoType::kClear;

  // Presentation history to track if swap chain was composited or used hardware
  // overlays.
  PresentationHistory presentation_history_;

  // Whether creating a YUV swap chain failed.
  bool failed_to_create_yuv_swapchain_ = false;

  // Set to true when PresentToDecodeSwapChain fails for the first time after
  // which we won't attempt to use decode swap chain again.
  bool failed_to_present_decode_swapchain_ = false;

  // Number of frames since we switched from YUV to BGRA swap chain, or
  // vice-versa.
  int frames_since_color_space_change_ = 0;

  // This struct is used to cache information about what visuals are currently
  // being presented so that properties that aren't changed aren't sent to
  // DirectComposition.
  struct VisualInfo {
    gfx::Point offset;
    gfx::Transform transform;
    bool is_clipped = false;
    gfx::Rect clip_rect;
    int z_order = 0;
  } visual_info_;

  // Direct composition visual containing the swap chain content.  Child of
  // |clip_visual_|.
  Microsoft::WRL::ComPtr<IDCompositionVisual2> content_visual_;

  // Direct composition visual that applies the clip rect.  Parent of
  // |content_visual_|, and root of the visual tree for this layer.
  Microsoft::WRL::ComPtr<IDCompositionVisual2> clip_visual_;

  // GLImages that were presented in the last frame.
  ui::DCRendererLayerParams::OverlayImages last_presented_images_;

  // NV12 staging texture used for software decoded YUV buffers.  Mapped to CPU
  // for copying from YUV buffers.  Texture usage is DYNAMIC or STAGING.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;
  // Used to copy from staging texture with usage STAGING for workarounds.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> copy_texture_;
  gfx::Size staging_texture_size_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;

  // Handle returned by DCompositionCreateSurfaceHandle() used to create YUV
  // swap chain that can be used for direct composition.
  base::win::ScopedHandle swap_chain_handle_;

  // Video processor output view created from swap chain back buffer.  Must be
  // cached for performance reasons.
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> output_view_;

  Microsoft::WRL::ComPtr<IDXGIResource> decode_resource_;
  Microsoft::WRL::ComPtr<IDXGIDecodeSwapChain> decode_swap_chain_;
  Microsoft::WRL::ComPtr<IUnknown> decode_surface_;
  bool is_on_battery_power_;

  DISALLOW_COPY_AND_ASSIGN(SwapChainPresenter);
};

}  // namespace gl

#endif  // UI_GL_SWAP_CHAIN_PRESENTER_H_
