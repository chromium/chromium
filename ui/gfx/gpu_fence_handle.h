// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GPU_FENCE_HANDLE_H_
#define UI_GFX_GPU_FENCE_HANDLE_H_

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"

#if BUILDFLAG(IS_POSIX)
#include "base/files/scoped_file.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/event.h>
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace gfx {

// TODO(crbug.com/40728014): Make this a class instead of struct.
struct GFX_EXPORT GpuFenceHandle {
#if BUILDFLAG(IS_POSIX)
  using ScopedPlatformFence = base::ScopedFD;
#elif BUILDFLAG(IS_FUCHSIA)
  using ScopedPlatformFence = zx::event;
#elif BUILDFLAG(IS_WIN)
  using ScopedPlatformFence = base::win::ScopedHandle;
#endif

  GpuFenceHandle(const GpuFenceHandle&) = delete;
  GpuFenceHandle& operator=(const GpuFenceHandle&) = delete;

  GpuFenceHandle();
  GpuFenceHandle(GpuFenceHandle&& other);
  GpuFenceHandle& operator=(GpuFenceHandle&& other);
  ~GpuFenceHandle();

  bool is_null() const;

  // Returns an instance of |handle| which can be sent over IPC. This duplicates
  // the handle so that IPC code can take ownership of it without invalidating
  // |handle| itself.
  GpuFenceHandle Clone() const;

  // Takes ownership of 'scoped_fence'. This likely comes from a move
  // operation.
  void Adopt(ScopedPlatformFence scoped_fence);

  // Internally clears out 'owned_fence' ownership.
  void Reset();

  // Returns a owned fence object (in scope) and 'Reset's the handle's fence
  // object.
  ScopedPlatformFence Release();

  // Helper functions for platforms with unscoped underlying fence
  // representations.
#if BUILDFLAG(IS_POSIX)
  // Returns fd but the returned fd is not owned.
  int Peek() const;
#elif BUILDFLAG(IS_WIN)
  // Returns HANDLE but the returned HANDLE is not owned.
  HANDLE Peek() const;
#endif

  // Returns global total number of clones since last call.
  static uint32_t GetAndClearNumberOfClones();

 private:
  struct RefCountedScopedFence
      : public base::RefCountedThreadSafe<RefCountedScopedFence> {
    explicit RefCountedScopedFence(ScopedPlatformFence scoped_fd);

   private:
    ~RefCountedScopedFence();

    friend class base::RefCountedThreadSafe<RefCountedScopedFence>;
    friend struct GpuFenceHandle;

    ScopedPlatformFence scoped_fence_;
  };

  scoped_refptr<RefCountedScopedFence> smart_fence_;
};

}  // namespace gfx

#endif  // UI_GFX_GPU_FENCE_HANDLE_H_
