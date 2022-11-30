// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DCOMP_SURFACE_REGISTRY_H_
#define UI_GL_DCOMP_SURFACE_REGISTRY_H_

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "ui/gl/gl_export.h"

namespace gl {

// A registry in the GPU process for mapping an `UnguessableToken` to a
// Windows Direct Composition surface handle. This class is meant to be used as
// a singleton.
class GL_EXPORT DCOMPSurfaceRegistry {
 public:
  static DCOMPSurfaceRegistry* GetInstance();

  // Registers a surface handle and return the associated token, which will be
  // sent to `MediaFoundationRendererClient` and then to `DCOMPTexture` to take
  // the handle for direct composition rendering.
  base::UnguessableToken RegisterDCOMPSurfaceHandle(
      base::win::ScopedHandle surface);

  // Unregisters the surface handle associated with `token`. Called when the
  // `MediaFoundationRendererWrapper` with a registered handle is destructed.
  // No-op if the handle has already been taken by `DCOMPTexture`.
  void UnregisterDCOMPSurfaceHandle(const base::UnguessableToken& token);

  // `DCOMPTexture` calls this to take the ownership of the DCOMP surface handle
  // when it receives a token from the `MediaFoundationRendererClient` in the
  // render process.
  base::win::ScopedHandle TakeDCOMPSurfaceHandle(
      const base::UnguessableToken& token);

 private:
  friend base::NoDestructor<DCOMPSurfaceRegistry>;

  DCOMPSurfaceRegistry();
  ~DCOMPSurfaceRegistry();

  base::flat_map<base::UnguessableToken, base::win::ScopedHandle>
      surface_handle_map_;
};

}  // namespace gl

#endif  // UI_GL_DCOMP_SURFACE_REGISTRY_H_
