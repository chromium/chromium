// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GPU_MEMORY_BUFFER_H_
#define UI_GFX_GPU_MEMORY_BUFFER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
class MemoryAllocatorDumpGuid;
}  // namespace trace_event
}  // namespace base

namespace gfx {

class ColorSpace;

// This interface typically correspond to a type of shared memory that is also
// shared with the GPU. A GPU memory buffer can be written to directly by
// regular CPU code, but can also be read by the GPU.
class COMPONENT_EXPORT(GFX) GpuMemoryBuffer {
 public:
  virtual ~GpuMemoryBuffer() {}

  // Maps each plane of the buffer into the client's address space so it can be
  // written to by the CPU. This call may block, for instance if the GPU needs
  // to finish accessing the buffer or if CPU caches need to be synchronized.
  // Returns false on failure.
  virtual bool Map() = 0;

  // Maps each plane of the buffer into the client's address space so it can be
  // written to by the CPU. The default implementation is blocking and just
  // calls Map(). However, on some platforms the implementations are
  // non-blocking. In that case the result callback will be executed on the
  // GpuMemoryThread if some work in the GPU service is required for mapping, or
  // will be executed immediately in the current sequence. Warning: Make sure
  // the GMB isn't destroyed before the callback is run otherwise GPU process
  // might try to write in destroyed shared memory region. Don't attempt to
  // Unmap or get memory before the callback is executed. Otherwise a CHECK will
  // fire.
  virtual void MapAsync(base::OnceCallback<void(bool)> result_cb);

  // Indicates if the `MapAsync` is non-blocking. Otherwise it's just calling
  // `Map()` directly.
  virtual bool AsyncMappingIsNonBlocking() const;

  // Returns a pointer to the memory address of a plane. Buffer must have been
  // successfully mapped using a call to Map() before calling this function.
  virtual void* memory(size_t plane) = 0;

  // Unmaps the buffer. It's illegal to use any pointer returned by memory()
  // after this has been called.
  virtual void Unmap() = 0;

  // Returns the size in pixels of the first plane of the buffer.
  virtual Size GetSize() const = 0;

  // Returns the format for the buffer.
  virtual BufferFormat GetFormat() const = 0;

  // Fills the stride in bytes for each plane of the buffer. The stride of
  // plane K is stored at index K-1 of the |stride| array.
  virtual int stride(size_t plane) const = 0;

#if BUILDFLAG(IS_APPLE)
  // Set the color space in which this buffer should be interpreted when used
  // as an overlay. Note that this will not impact texturing from the buffer.
  // Used only for GMBs backed by an IOSurface.
  virtual void SetColorSpace(const ColorSpace& color_space) {}
#endif

  // Returns a unique identifier associated with buffer.
  virtual GpuMemoryBufferId GetId() const = 0;

  // Returns the type of this buffer.
  virtual GpuMemoryBufferType GetType() const = 0;

  // Returns a platform specific handle for this buffer which in particular can
  // be sent over IPC. This duplicates file handles as appropriate, so that a
  // caller takes ownership of the returned handle.
  virtual GpuMemoryBufferHandle CloneHandle() const = 0;

#if BUILDFLAG(IS_WIN)
  // Used to set the use_premapped_memory flag in the GpuMemoryBufferImplDXGI to
  // indicate whether to use the premapped memory or not. It is only used with
  // MappableSI. See GpuMemoryBufferImplDXGI override for more details.
  virtual void SetUsePreMappedMemory(bool use_premapped_memory) {}
#endif

  // Dumps information about the memory backing the GpuMemoryBuffer to |pmd|.
  // The memory usage is attributed to |buffer_dump_guid|.
  // |tracing_process_id| uniquely identifies the process owning the memory.
  // |importance| is relevant only for the cases of co-ownership, the memory
  // gets attributed to the owner with the highest importance.
  virtual void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const = 0;
};

}  // namespace gfx

#endif  // UI_GFX_GPU_MEMORY_BUFFER_H_
