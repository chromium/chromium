// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/mojom/gl_implementation_mojom_traits.h"
#include "base/notreached.h"

namespace mojo {

// static
gl::mojom::GLImplementation
EnumTraits<gl::mojom::GLImplementation, gl::GLImplementation>::ToMojom(
    gl::GLImplementation impl) {
  switch (impl) {
    case gl::kGLImplementationNone:
      return gl::mojom::GLImplementation::kGLImplementationNone;
    case gl::kGLImplementationEGLGLES2:
      return gl::mojom::GLImplementation::kGLImplementationEGLGLES2;
    case gl::kGLImplementationMockGL:
      return gl::mojom::GLImplementation::kGLImplementationMockGL;
    case gl::kGLImplementationStubGL:
      return gl::mojom::GLImplementation::kGLImplementationStubGL;
    case gl::kGLImplementationDisabled:
      return gl::mojom::GLImplementation::kGLImplementationDisabled;
    case gl::kGLImplementationEGLANGLE:
      return gl::mojom::GLImplementation::kGLImplementationEGLANGLE;
  }
  NOTREACHED();
}

// static
gl::GLImplementation
EnumTraits<gl::mojom::GLImplementation, gl::GLImplementation>::FromMojom(
    gl::mojom::GLImplementation input) {
  switch (input) {
    case gl::mojom::GLImplementation::kGLImplementationNone:
      return gl::kGLImplementationNone;
    case gl::mojom::GLImplementation::kGLImplementationEGLGLES2:
      return gl::kGLImplementationEGLGLES2;
    case gl::mojom::GLImplementation::kGLImplementationMockGL:
      return gl::kGLImplementationMockGL;
    case gl::mojom::GLImplementation::kGLImplementationStubGL:
      return gl::kGLImplementationStubGL;
    case gl::mojom::GLImplementation::kGLImplementationDisabled:
      return gl::kGLImplementationDisabled;
    case gl::mojom::GLImplementation::kGLImplementationEGLANGLE:
      return gl::kGLImplementationEGLANGLE;
  }
  NOTREACHED();
}

// static
gl::mojom::ANGLEImplementation
EnumTraits<gl::mojom::ANGLEImplementation, gl::ANGLEImplementation>::ToMojom(
    gl::ANGLEImplementation impl) {
  switch (impl) {
    case gl::ANGLEImplementation::kNone:
      return gl::mojom::ANGLEImplementation::kNone;
    case gl::ANGLEImplementation::kD3D9:
      return gl::mojom::ANGLEImplementation::kD3D9;
    case gl::ANGLEImplementation::kD3D11:
      return gl::mojom::ANGLEImplementation::kD3D11;
    case gl::ANGLEImplementation::kOpenGL:
      return gl::mojom::ANGLEImplementation::kOpenGL;
    case gl::ANGLEImplementation::kOpenGLES:
      return gl::mojom::ANGLEImplementation::kOpenGLES;
    case gl::ANGLEImplementation::kNull:
      return gl::mojom::ANGLEImplementation::kNull;
    case gl::ANGLEImplementation::kVulkan:
      return gl::mojom::ANGLEImplementation::kVulkan;
    case gl::ANGLEImplementation::kSwiftShader:
      return gl::mojom::ANGLEImplementation::kSwiftShader;
    case gl::ANGLEImplementation::kMetal:
      return gl::mojom::ANGLEImplementation::kMetal;
    case gl::ANGLEImplementation::kD3D11Warp:
      return gl::mojom::ANGLEImplementation::kD3D11Warp;
    case gl::ANGLEImplementation::kDefault:
      return gl::mojom::ANGLEImplementation::kDefault;
  }
  NOTREACHED();
}

// static
gl::ANGLEImplementation
EnumTraits<gl::mojom::ANGLEImplementation, gl::ANGLEImplementation>::FromMojom(
    gl::mojom::ANGLEImplementation input) {
  switch (input) {
    case gl::mojom::ANGLEImplementation::kNone:
      return gl::ANGLEImplementation::kNone;
    case gl::mojom::ANGLEImplementation::kD3D9:
      return gl::ANGLEImplementation::kD3D9;
    case gl::mojom::ANGLEImplementation::kD3D11:
      return gl::ANGLEImplementation::kD3D11;
    case gl::mojom::ANGLEImplementation::kOpenGL:
      return gl::ANGLEImplementation::kOpenGL;
    case gl::mojom::ANGLEImplementation::kOpenGLES:
      return gl::ANGLEImplementation::kOpenGLES;
    case gl::mojom::ANGLEImplementation::kNull:
      return gl::ANGLEImplementation::kNull;
    case gl::mojom::ANGLEImplementation::kVulkan:
      return gl::ANGLEImplementation::kVulkan;
    case gl::mojom::ANGLEImplementation::kSwiftShader:
      return gl::ANGLEImplementation::kSwiftShader;
    case gl::mojom::ANGLEImplementation::kMetal:
      return gl::ANGLEImplementation::kMetal;
    case gl::mojom::ANGLEImplementation::kD3D11Warp:
      return gl::ANGLEImplementation::kD3D11Warp;
    case gl::mojom::ANGLEImplementation::kDefault:
      return gl::ANGLEImplementation::kDefault;
  }
  NOTREACHED();
}

// static
bool StructTraits<gl::mojom::GLImplementationPartsDataView,
                  gl::GLImplementationParts>::
    Read(gl::mojom::GLImplementationPartsDataView data,
         gl::GLImplementationParts* out) {
  return data.ReadGl(&out->gl) && data.ReadAngle(&out->angle);
}

}  // namespace mojo
