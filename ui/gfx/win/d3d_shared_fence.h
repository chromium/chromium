// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_D3D_SHARED_FENCE_H_
#define UI_GFX_WIN_D3D_SHARED_FENCE_H_

#include <d3d11.h>
#include <d3d11_4.h>
#include <wrl/client.h>

#include "base/containers/lru_cache.h"
#include "base/memory/ref_counted.h"
#include "base/win/scoped_handle.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// A ref-counted wrapper around D3D11Fence and its shared handle. Fences are
// opened for each D3D11 device the fence is waited on. Ref-counting is used so
// that the same fence object can be referred to in multiple places e.g. as the
// signaling fence or in list of fences to wait for next access.
class GFX_EXPORT D3DSharedFence : public base::RefCounted<D3DSharedFence> {
 public:
  // Create a new ID3D11Fence with initial value 0 on given |d3d11_device|. The
  // provided device is considered the owning device for the fence, and is the
  // device used for signaling the fence.
  static scoped_refptr<D3DSharedFence> CreateForD3D11(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

  // Create from existing shared handle e.g. from Dawn. Doesn't take ownership
  // of |shared_handle| and duplicates it instead. The ID3D11Fence is lazily
  // created on Wait or Signal for the device provided to those calls.
  static scoped_refptr<D3DSharedFence> CreateFromHandle(HANDLE shared_handle);

  // Returns true if fences are supported i.e. if ID3D11Device5 is supported.
  static bool IsSupported(ID3D11Device* d3d11_device);

  // Return the shared handle for the fence.
  HANDLE GetSharedHandle() const;

  // Returns the fence value that was last known to be signaled on this fence,
  // and should be used for waiting.
  uint64_t GetFenceValue() const;

  // Returns the D3D11 device if this fence was created using CreateForD3D11.
  Microsoft::WRL::ComPtr<ID3D11Device> GetD3D11Device() const;

  // Returns true if the underlying fence object is the same as |shared_handle|.
  bool IsSameFenceAsHandle(HANDLE shared_handle) const;

  // Updates fence value to |fence_value| provided that it's larger.
  void Update(uint64_t fence_value);

  // Issue a wait for the fence on the immediate context of |d3d11_device| using
  // |wait_value|. The wait is skipped if the passed in device is the same as
  // |d3d11_device_|. Returns true on success.
  bool WaitD3D11(Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

  // Issue a signal for the fence on the immediate context of |d3d11_device_|
  // using |signal_value|. Returns true on success.
  bool IncrementAndSignalD3D11();

 private:
  friend class base::RefCounted<D3DSharedFence>;

  // 5 D3D11 devices ought to be enough for anybody.
  static constexpr size_t kMaxD3D11FenceMapSize = 5;

  explicit D3DSharedFence(base::win::ScopedHandle shared_handle);
  ~D3DSharedFence();

  // Owned shared handle corresponding to the fence.
  base::win::ScopedHandle shared_handle_;

  // Value last known to be signaled for this fence to be used for future waits.
  uint64_t fence_value_ = 0;

  // If present, this is the D3D11 device that the fence was created on, and
  // used to signal |d3d11_fence_|.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;

  // If present, this is the D3D11 fence object this fence was created with and
  // used for signaling.
  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_signal_fence_;

  // Map of D3D11 device to D3D11 fence objects used by WaitD3D11().
  base::LRUCache<Microsoft::WRL::ComPtr<ID3D11Device>,
                 Microsoft::WRL::ComPtr<ID3D11Fence>>
      d3d11_wait_fence_map_;
};

}  // namespace gfx

#endif
