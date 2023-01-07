// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_VERSION_INFO_H_
#define UI_GL_GL_VERSION_INFO_H_

#include <set>
#include <string>
#include "build/build_config.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/gl_export.h"

namespace gl {

struct GL_EXPORT GLVersionInfo {
  GLVersionInfo(const char* version_str,
                const char* renderer_str,
                const gfx::ExtensionSet& exts);

  GLVersionInfo(const GLVersionInfo&) = delete;
  GLVersionInfo& operator=(const GLVersionInfo&) = delete;

  bool IsAtLeastGL(unsigned major, unsigned minor) const {
    return !is_es && (major_version > major ||
                      (major_version == major && minor_version >= minor));
  }

  bool IsLowerThanGL(unsigned major, unsigned minor) const {
    return !is_es && (major_version < major ||
                      (major_version == major && minor_version < minor));
  }

  bool IsAtLeastGLES(unsigned major, unsigned minor) const {
    return is_es && (major_version > major ||
                     (major_version == major && minor_version >= minor));
  }

  bool BehavesLikeGLES() const {
    return is_es || is_desktop_core_profile;
  }

  bool SupportsFixedType() const {
    return is_es || IsAtLeastGL(4, 1);
  }
  struct VersionStrings {
    const char* gl_version;
    const char* glsl_version;
  };

  // Returns version strings for GL and GLSL (similar to glGetString(GL_VERSION)
  // and glGetString(GL_SHADING_LANGUAGE_VERSION) matching major/minor versions
  VersionStrings GetFakeVersionStrings(unsigned major, unsigned minor) const;

  // Returns true if the major/minor version was changed for any reasons and we
  // might need to propagate changes further, e.g. to Skia
  bool IsVersionSubstituted() const;

  // We need to emulate GL_ALPHA and GL_LUMINANCE and GL_LUMINANCE_ALPHA
  // texture formats on core profile and ES3, except for ANGLE and Swiftshader.
  bool NeedsLuminanceAlphaEmulation() const {
    return !is_angle && !is_swiftshader && (is_es3 || is_desktop_core_profile);
  }

  bool is_es = false;
  bool is_angle = false;
  bool is_d3d = false;
  bool is_mesa = false;
  bool is_swiftshader = false;
  bool is_angle_metal = false;
  bool is_angle_swiftshader = false;
  bool is_angle_vulkan = false;
  unsigned major_version = 0;
  unsigned minor_version = 0;
  bool is_es2 = false;
  bool is_es3 = false;
  bool is_desktop_core_profile = false;
  bool is_es3_capable = false;
  std::string driver_vendor;
  std::string driver_version;

  static void DisableES3ForTesting();

 private:
  void Initialize(const char* version_str,
                  const char* renderer_str,
                  const gfx::ExtensionSet& extensions);
  void ParseVersionString(const char* version_str);
  void ParseDriverInfo(const char* version_str);
  void ExtractDriverVendorANGLE(const char* renderer_str);
  bool IsES3Capable(const gfx::ExtensionSet& extensions) const;
};

}  // namespace gl

#endif // UI_GL_GL_VERSION_INFO_H_
