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
#include "ui/gfx/gpu_memory_buffer.h"

namespace gfx {

// A thread-safe ref-counted wrapper around D3D11Fence and its shared handle.
// Fences are opened for each D3D11 device the fence is waited on. Thread-safe
// ref-counting is used so that the same fence object can be referred to in
// multiple places e.g. as the signaling fence or in list of fences to wait for
// next access, and also multiple threads e.g. the gpu main thread and the media
// service thread. This class must be externally synchronized.
class GFX_EXPORT D3DSharedFence
    : public base::RefCountedThreadSafe<D3DSharedFence> {
 public:
  // Create a new ID3D11Fence with initial value 0 on given
  // |d3d11_signal_device|. The provided device is considered the owning device
  // for the fence, and is the device used for signaling the fence.
  static scoped_refptr<D3DSharedFence> CreateForD3D11(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_signal_device);

  // Create from an existing ID3D11Fence with a specified value to wait on. The
  // |d3d11_signal_device| is passed explicitly in the case that the device
  // signaling the fence is different than the device that created it.
  // |fence_value| is the initial value this fence will wait on.
  static scoped_refptr<D3DSharedFence> CreateFromD3D11Fence(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_signal_device,
      Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence,
      uint64_t fence_value);

  // Create from existing scoped shared handle e.g. from IPC. The ID3D11Fence
  // is lazily created on Wait or Signal for the device provided to those calls.
  static scoped_refptr<D3DSharedFence> CreateFromScopedHandle(
      base::win::ScopedHandle fence_handle,
      const DXGIHandleToken& fence_token);

  // Create from existing shared handle e.g. from Dawn. Doesn't take ownership
  // of |shared_handle| and duplicates it instead. The ID3D11Fence is lazily
  // created on Wait or Signal for the device provided to those calls.
  static scoped_refptr<D3DSharedFence> CreateFromUnownedHandle(
      HANDLE shared_handle);

  // Returns true if fences are supported i.e. if ID3D11Device5 is supported.
  static bool IsSupported(ID3D11Device* d3d11_device);

  // Return the shared handle for the fence.
  HANDLE GetSharedHandle() const;

  // Clone the shared handle for IPC.
  base::win::ScopedHandle CloneSharedHandle();

  // Returns the fence value that was last known to be signaled on this fence,
  // and should be used for waiting.
  uint64_t GetFenceValue() const;

  // Returns unique identifier for this fence when used across processes.
  const DXGIHandleToken& GetDXGIHandleToken() const;

  // Returns the D3D11 device if this fence was created using CreateForD3D11.
  Microsoft::WRL::ComPtr<ID3D11Device> GetD3D11Device() const;

  // Returns true if the underlying fence object is the same as |shared_handle|.
  bool IsSameFenceAsHandle(HANDLE shared_handle) const;

  // Updates fence value to |fence_value| provided that it's larger.
  void Update(uint64_t fence_value);

  // Issue a wait for the fence on the immediate context of |d3d11_device| using
  // |fence_value_|. The wait is skipped if the passed in device is the same as
  // |d3d11_signal_device_|. Returns true on success.
  bool WaitD3D11(Microsoft::WRL::ComPtr<ID3D11Device> d3d11_wait_device);

  // Increment |fence_value_| and issue a signal for the fence on the immediate
  // context of |d3d11_signal_device_| using |fence_value_|. Returns true on
  // success.
  bool IncrementAndSignalD3D11();

 private:
  friend class base::RefCountedThreadSafe<D3DSharedFence>;

  // 5 D3D11 devices ought to be enough for anybody.
  static constexpr size_t kMaxD3D11FenceMapSize = 5;

  explicit D3DSharedFence(base::win::ScopedHandle shared_handle,
                          const DXGIHandleToken& dxgi_token);

  ~D3DSharedFence();

  // Owned shared handle corresponding to the fence.
  base::win::ScopedHandle shared_handle_;

  // Value last known to be signaled for this fence to be used for future waits.
  uint64_t fence_value_ = 0;

  // Unique identifier for this fence when used across processes.
  DXGIHandleToken dxgi_token_;

  // If present, this is the D3D11 device that the fence was created on, and
  // used to signal |d3d11_signal_fence_|. Can be null if the fence will be
  // signaled externally.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_signal_device_;

  // If present, this is the D3D11 fence object this fence was created with and
  // used for signaling.
  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_signal_fence_;

  // Map of D3D11 device to D3D11 fence objects used by WaitD3D11().
  base::LRUCache<Microsoft::WRL::ComPtr<ID3D11Device>,
                 Microsoft::WRL::ComPtr<ID3D11Fence>>
      d3d11_wait_fence_map_;
};

}  // namespace gfx

#endif  // UI_GFX_WIN_D3D_SHARED_FENCE_H_
