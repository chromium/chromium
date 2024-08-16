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
bool EnumTraits<gl::mojom::GLImplementation, gl::GLImplementation>::FromMojom(
    gl::mojom::GLImplementation input,
    gl::GLImplementation* out) {
  switch (input) {
    case gl::mojom::GLImplementation::kGLImplementationNone:
      *out = gl::kGLImplementationNone;
      return true;
    case gl::mojom::GLImplementation::kGLImplementationEGLGLES2:
      *out = gl::kGLImplementationEGLGLES2;
      return true;
    case gl::mojom::GLImplementation::kGLImplementationMockGL:
      *out = gl::kGLImplementationMockGL;
      return true;
    case gl::mojom::GLImplementation::kGLImplementationStubGL:
      *out = gl::kGLImplementationStubGL;
      return true;
    case gl::mojom::GLImplementation::kGLImplementationDisabled:
      *out = gl::kGLImplementationDisabled;
      return true;
    case gl::mojom::GLImplementation::kGLImplementationEGLANGLE:
      *out = gl::kGLImplementationEGLANGLE;
      return true;
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
    case gl::ANGLEImplementation::kDefault:
      return gl::mojom::ANGLEImplementation::kDefault;
  }
  NOTREACHED();
}

// static
bool EnumTraits<gl::mojom::ANGLEImplementation, gl::ANGLEImplementation>::
    FromMojom(gl::mojom::ANGLEImplementation input,
              gl::ANGLEImplementation* out) {
  switch (input) {
    case gl::mojom::ANGLEImplementation::kNone:
      *out = gl::ANGLEImplementation::kNone;
      return true;
    case gl::mojom::ANGLEImplementation::kD3D9:
      *out = gl::ANGLEImplementation::kD3D9;
      return true;
    case gl::mojom::ANGLEImplementation::kD3D11:
      *out = gl::ANGLEImplementation::kD3D11;
      return true;
    case gl::mojom::ANGLEImplementation::kOpenGL:
      *out = gl::ANGLEImplementation::kOpenGL;
      return true;
    case gl::mojom::ANGLEImplementation::kOpenGLES:
      *out = gl::ANGLEImplementation::kOpenGLES;
      return true;
    case gl::mojom::ANGLEImplementation::kNull:
      *out = gl::ANGLEImplementation::kNull;
      return true;
    case gl::mojom::ANGLEImplementation::kVulkan:
      *out = gl::ANGLEImplementation::kVulkan;
      return true;
    case gl::mojom::ANGLEImplementation::kSwiftShader:
      *out = gl::ANGLEImplementation::kSwiftShader;
      return true;
    case gl::mojom::ANGLEImplementation::kMetal:
      *out = gl::ANGLEImplementation::kMetal;
      return true;
    case gl::mojom::ANGLEImplementation::kDefault:
      *out = gl::ANGLEImplementation::kDefault;
      return true;
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
