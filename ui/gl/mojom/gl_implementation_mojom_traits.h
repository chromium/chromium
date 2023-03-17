// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_MOJOM_GL_IMPLEMENTATION_MOJOM_TRAITS_H_
#define UI_GL_MOJOM_GL_IMPLEMENTATION_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/mojom/gl_implementation.mojom.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(GL_MOJOM)
    EnumTraits<gl::mojom::GLImplementation, gl::GLImplementation> {
  static gl::mojom::GLImplementation ToMojom(gl::GLImplementation impl);
  static bool FromMojom(gl::mojom::GLImplementation input,
                        gl::GLImplementation* out);
};

template <>
struct COMPONENT_EXPORT(GL_MOJOM)
    EnumTraits<gl::mojom::ANGLEImplementation, gl::ANGLEImplementation> {
  static gl::mojom::ANGLEImplementation ToMojom(gl::ANGLEImplementation impl);
  static bool FromMojom(gl::mojom::ANGLEImplementation input,
                        gl::ANGLEImplementation* out);
};

template <>
struct COMPONENT_EXPORT(GL_MOJOM)
    StructTraits<gl::mojom::GLImplementationPartsDataView,
                 gl::GLImplementationParts> {
  static bool Read(gl::mojom::GLImplementationPartsDataView data,
                   gl::GLImplementationParts* out);

  static gl::GLImplementation gl(const gl::GLImplementationParts& input) {
    return input.gl;
  }

  static gl::ANGLEImplementation angle(const gl::GLImplementationParts& input) {
    return input.angle;
  }
};

}  // namespace mojo

#endif  // UI_GL_MOJOM_GL_IMPLEMENTATION_MOJOM_TRAITS_H_
