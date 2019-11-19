// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SURFACE_TRANSPORT_DIB_H_
#define UI_SURFACE_TRANSPORT_DIB_H_

#include <stddef.h>
#include <memory>

#include "base/macros.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "build/build_config.h"
#include "ui/surface/surface_export.h"

class SkCanvas;

// -----------------------------------------------------------------------------
// A TransportDIB is a block of memory that is used to transport pixels
// between processes: from the renderer process to the browser, and
// between renderer and plugin processes.
// -----------------------------------------------------------------------------
class SURFACE_EXPORT TransportDIB {
 public:
  ~TransportDIB();

  // Creates and maps a new TransportDIB with a shared memory region.
  // Returns nullptr on failure.
  static std::unique_ptr<TransportDIB> Map(
      base::UnsafeSharedMemoryRegion region);

  // Creates a new TransportDIB with a shared memory region. This always returns
  // a valid pointer. The DIB is not mapped.
  static std::unique_ptr<TransportDIB> CreateWithHandle(
      base::UnsafeSharedMemoryRegion region);

  // Returns a canvas using the memory of this TransportDIB. The returned
  // pointer will be owned by the caller. The bitmap will be of the given size,
  // which should fit inside this memory. Bitmaps returned will be either
  // opaque or have premultiplied alpha.
  //
  // On POSIX, this |TransportDIB| will be mapped if not already. On Windows,
  // this |TransportDIB| will NOT be mapped and should not be mapped prior,
  // because PlatformCanvas will map the file internally.
  //
  // Will return NULL on allocation failure. This could be because the image
  // is too large to map into the current process' address space.
  std::unique_ptr<SkCanvas> GetPlatformCanvas(int w, int h, bool opaque);

  // Map the DIB into the current process if it is not already. This is used to
  // map a DIB that has already been created. Returns true if the DIB is mapped.
  bool Map();

  // Return a pointer to the shared memory.
  void* memory() const;

  // Return the maximum size of the shared memory. This is not the amount of
  // data which is valid, you have to know that via other means, this is simply
  // the maximum amount that /could/ be valid.
  size_t size() const { return size_; }

  // Returns a pointer to the UnsafeSharedMemoryRegion object that backs the
  // transport dib.
  base::UnsafeSharedMemoryRegion* shared_memory_region();

 private:
  // Verifies that the dib can hold a canvas of the requested dimensions.
  bool VerifyCanvasSize(int w, int h);

  explicit TransportDIB(base::UnsafeSharedMemoryRegion region);

  base::UnsafeSharedMemoryRegion shm_region_;
  base::WritableSharedMemoryMapping shm_mapping_;
  size_t size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TransportDIB);
};

#endif  // UI_SURFACE_TRANSPORT_DIB_H_
