// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dcomp_surface_registry.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace gl {

DCOMPSurfaceRegistry* DCOMPSurfaceRegistry::GetInstance() {
  static base::NoDestructor<DCOMPSurfaceRegistry> instance;
  return instance.get();
}

DCOMPSurfaceRegistry::DCOMPSurfaceRegistry() = default;
DCOMPSurfaceRegistry::~DCOMPSurfaceRegistry() = default;

base::UnguessableToken DCOMPSurfaceRegistry::RegisterDCOMPSurfaceHandle(
    base::win::ScopedHandle surface) {
  DVLOG(1) << __func__;
  base::UnguessableToken token = base::UnguessableToken::Create();
  {
    base::AutoLock lock(lock_);
    DCHECK(surface_handle_map_.find(token) == surface_handle_map_.end());
    surface_handle_map_[token] = std::move(surface);
  }
  DVLOG(1) << __func__ << ": Surface handle registered with token " << token;
  return token;
}

void DCOMPSurfaceRegistry::UnregisterDCOMPSurfaceHandle(
    const base::UnguessableToken& token) {
  DVLOG(1) << __func__;
  base::AutoLock lock(lock_);
  surface_handle_map_.erase(token);
}

base::win::ScopedHandle DCOMPSurfaceRegistry::TakeDCOMPSurfaceHandle(
    const base::UnguessableToken& token) {
  DVLOG(1) << __func__;
  base::AutoLock lock(lock_);
  auto surface_iter = surface_handle_map_.find(token);
  if (surface_iter != surface_handle_map_.end()) {
    // Take ownership.
    auto surface_handle = std::move(surface_iter->second);
    surface_handle_map_.erase(surface_iter);
    return surface_handle;
  }

  DLOG(ERROR) << __func__ << ": No surface handle found for token " << token;
  return base::win::ScopedHandle();
}

}  // namespace gl
