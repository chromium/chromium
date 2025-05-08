// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GPU_MEMORY_BUFFER_H_
#define UI_GFX_GPU_MEMORY_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/component_export.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "build/build_config.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/generic_shared_memory_id.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "ui/gfx/native_pixmap_handle.h"
#elif BUILDFLAG(IS_APPLE)
#include "ui/gfx/mac/io_surface.h"
#elif BUILDFLAG(IS_WIN)
#include <optional>

#include "base/types/token_type.h"
#include "base/win/scoped_handle.h"
#elif BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_hardware_buffer_handle.h"
#endif

namespace base {
namespace trace_event {
class ProcessMemoryDump;
class MemoryAllocatorDumpGuid;
}  // namespace trace_event
}  // namespace base

namespace mojo {
template <typename, typename>
struct StructTraits;
template <typename, typename>
struct UnionTraits;
}  // namespace mojo

namespace gfx {
namespace mojom {
class DXGIHandleDataView;
class GpuMemoryBufferPlatformHandleDataView;
}  // namespace mojom

class ColorSpace;

enum GpuMemoryBufferType {
  EMPTY_BUFFER,
  SHARED_MEMORY_BUFFER,
#if BUILDFLAG(IS_APPLE)
  IO_SURFACE_BUFFER,
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  NATIVE_PIXMAP,
#endif
#if BUILDFLAG(IS_WIN)
  DXGI_SHARED_HANDLE,
#endif
#if BUILDFLAG(IS_ANDROID)
  ANDROID_HARDWARE_BUFFER,
#endif
};

using GpuMemoryBufferId = GenericSharedMemoryId;

#if BUILDFLAG(IS_WIN)
using DXGIHandleToken = base::TokenType<class DXGIHandleTokenTypeMarker>;

// A simple type that bundles together the various bits for working with DXGI
// handles in Chrome. It consists of:
// - `buffer_handle`: the shared handle to the DXGI resource
// - `token`: A strongly-typed UnguessableToken used to determine if two
//            instances of `DXGIHandle` represent the same underlying shared
//            handle. Needed because the handle itself may be duplicated virtual
//            `DuplicateHandle()`.
// - `region`: An optional shared memory region. A DXGI handle's buffer can only
//             be read in the GPU process; this is used to provide support for
//             `Map()`ing the buffer in other processes. Under the hood, this is
//             implemented by having the GPU process copy the buffer into a
//             shared memory region that other processes can read.
class COMPONENT_EXPORT(GFX) DXGIHandle {
 public:
  // Creates a DXGIHandle suitable for use in a barebones unit test. The
  // `buffer_handle` won't actually usable as a DXGI shared handle, so this
  // helper is not suitable for integration tests.
  static DXGIHandle CreateFakeForTest();

  // Constructs an instance where `IsValid() == false`.
  DXGIHandle();
  ~DXGIHandle();

  // Constructs an instance, taking ownership of `scoped_handle` and associating
  // it with a new DXGIHandleToken. `scoped_handle` must be a valid handle.
  explicit DXGIHandle(base::win::ScopedHandle scoped_handle);
  // Typically only used by IPC deserialization. `buffer_handle` must be valid,
  // but `region` may be invalid.
  DXGIHandle(base::win::ScopedHandle buffer_handle,
             const DXGIHandleToken& token,
             base::UnsafeSharedMemoryRegion region);

  DXGIHandle(DXGIHandle&&);
  DXGIHandle& operator=(DXGIHandle&&);

  DXGIHandle(const DXGIHandle&) = delete;
  DXGIHandle& operator=(const DXGIHandle&) = delete;

  // Whether or not `this` has a valid underlying platform handle. This method
  // can return true even if `region()` is not a valid shmem region.
  bool IsValid() const;
  // Creates a copy of `this`. The underlying `buffer_handle` is duplicated into
  // a new handle, but `token()` will be preserved, so callers should use
  // `token()` to check if two `DXGIHandle`s actually refer to the same
  // resource.
  DXGIHandle Clone() const;
  // Similar to above but also associate `region` with the cloned `DXGIHandle`.
  //
  // Precondition: `this` must not have an associated region, i.e.
  // `region().IsValid()` is false.
  DXGIHandle CloneWithRegion(base::UnsafeSharedMemoryRegion region) const;

  HANDLE buffer_handle() const { return buffer_handle_.Get(); }
  base::win::ScopedHandle TakeBufferHandle();

  const DXGIHandleToken& token() const { return token_; }

  const base::UnsafeSharedMemoryRegion& region() const { return region_; }
  base::UnsafeSharedMemoryRegion TakeRegion() { return std::move(region_); }

 private:
  friend mojo::StructTraits<mojom::DXGIHandleDataView, DXGIHandle>;

  base::win::ScopedHandle buffer_handle_;
  DXGIHandleToken token_;
  base::UnsafeSharedMemoryRegion region_;
};
#endif

// TODO(crbug.com/40584691): Convert this to a proper class to ensure the state
// is always consistent, particularly that the only one handle is set at the
// same time and it corresponds to |type|.
struct COMPONENT_EXPORT(GFX) GpuMemoryBufferHandle {
  static constexpr GpuMemoryBufferId kInvalidId = GpuMemoryBufferId(-1);

  GpuMemoryBufferHandle();
  explicit GpuMemoryBufferHandle(base::UnsafeSharedMemoryRegion region);
#if BUILDFLAG(IS_WIN)
  explicit GpuMemoryBufferHandle(DXGIHandle handle);
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  explicit GpuMemoryBufferHandle(gfx::NativePixmapHandle native_pixmap_handle);
#endif
#if BUILDFLAG(IS_ANDROID)
  explicit GpuMemoryBufferHandle(
      base::android::ScopedHardwareBufferHandle handle);
#endif
  GpuMemoryBufferHandle(GpuMemoryBufferHandle&& other);
  GpuMemoryBufferHandle& operator=(GpuMemoryBufferHandle&& other);
  ~GpuMemoryBufferHandle();

  GpuMemoryBufferHandle Clone() const;
  bool is_null() const { return type == EMPTY_BUFFER; }

  // The shared memory region may only be used with SHARED_MEMORY_BUFFER and
  // DXGI_SHARED_HANDLE. In the case of DXGI handles, the actual contents of the
  // buffer can only be accessed from the GPU process, so `Map()`ing the buffer
  // into memory actually requires an IPC to the GPU process, which then copies
  // the contents into the shmem region so it can be accessed from other
  // processes.
  const base::UnsafeSharedMemoryRegion& region() const& {
    // For now, allow this to transparently forward as appropriate for DXGI or
    // shmem handles in production. However, this will be a hard CHECK() in the
    // future.
    CHECK_EQ(type, SHARED_MEMORY_BUFFER, base::NotFatalUntil::M138);
    switch (type) {
      case SHARED_MEMORY_BUFFER:
        return region_;
#if BUILDFLAG(IS_WIN)
      case DXGI_SHARED_HANDLE:
        return dxgi_handle_.region();
#endif  // BUILDFLAG(IS_WIN)
      default:
        NOTREACHED();
    }
  }
  base::UnsafeSharedMemoryRegion region() && {
    CHECK_EQ(type, SHARED_MEMORY_BUFFER);
    type = EMPTY_BUFFER;
    return std::move(region_);
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  const NativePixmapHandle& native_pixmap_handle() const& {
    CHECK_EQ(type, NATIVE_PIXMAP);
    return native_pixmap_handle_;
  }
  NativePixmapHandle native_pixmap_handle() && {
    CHECK_EQ(type, NATIVE_PIXMAP);
    type = EMPTY_BUFFER;
    return std::move(native_pixmap_handle_);
  }
#endif

#if BUILDFLAG(IS_WIN)
  const DXGIHandle& dxgi_handle() const& {
    CHECK_EQ(type, DXGI_SHARED_HANDLE);
    return dxgi_handle_;
  }
  DXGIHandle dxgi_handle() && {
    CHECK_EQ(type, DXGI_SHARED_HANDLE);
    type = EMPTY_BUFFER;
    return std::move(dxgi_handle_);
  }
#endif  // BUILDFLAG(IS_WIN)

  GpuMemoryBufferType type = GpuMemoryBufferType::EMPTY_BUFFER;
  GpuMemoryBufferId id{0};

  uint32_t offset = 0;
  uint32_t stride = 0;

#if BUILDFLAG(IS_APPLE)
  ScopedIOSurface io_surface;
#elif BUILDFLAG(IS_ANDROID)
  base::android::ScopedHardwareBufferHandle android_hardware_buffer;
#endif

 private:
  friend mojo::UnionTraits<mojom::GpuMemoryBufferPlatformHandleDataView,
                           GpuMemoryBufferHandle>;

  // This naming isn't entirely styleguide-compliant, but per the TODO, the end
  // goal is to make `this` an encapsulated class.
  base::UnsafeSharedMemoryRegion region_;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  NativePixmapHandle native_pixmap_handle_;
#endif

#if BUILDFLAG(IS_WIN)
  DXGIHandle dxgi_handle_;
#endif  // BUILDFLAG(IS_WIN)
};

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
