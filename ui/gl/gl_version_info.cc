// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_version_info.h"

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/version.h"

namespace {

bool DesktopCoreCommonCheck(
    bool is_es, unsigned major_version, unsigned minor_version) {
  return (!is_es &&
          ((major_version == 3 && minor_version >= 2) ||
           major_version > 3));
}

static bool disable_es3_for_testing = false;

}  // namespace

namespace gl {

GLVersionInfo::GLVersionInfo(const char* version_str,
                             const char* renderer_str,
                             const gfx::ExtensionSet& extensions)
    : is_es(false),
      is_angle(false),
      is_d3d(false),
      is_mesa(false),
      is_swiftshader(false),
      major_version(0),
      minor_version(0),
      is_es2(false),
      is_es3(false),
      is_desktop_core_profile(false),
      is_es3_capable(false) {
  Initialize(version_str, renderer_str, extensions);
}

void GLVersionInfo::Initialize(const char* version_str,
                               const char* renderer_str,
                               const gfx::ExtensionSet& extensions) {
  if (version_str)
    ParseVersionString(version_str);
  if (renderer_str) {
    is_angle = base::StartsWith(renderer_str, "ANGLE",
                                base::CompareCase::SENSITIVE);
    is_mesa = base::StartsWith(renderer_str, "Mesa",
                               base::CompareCase::SENSITIVE);
    is_swiftshader = base::StartsWith(renderer_str, "Google SwiftShader",
                                      base::CompareCase::SENSITIVE);

    // An ANGLE renderer string contains "Direct3D9", "Direct3DEx", or
    // "Direct3D11" on D3D backends.
    std::string renderer_string = std::string(renderer_str);
    is_d3d = renderer_string.find("Direct3D") != std::string::npos;
    // (is_d3d should only be possible if is_angle is true.)
    DCHECK(!is_d3d || is_angle);
    if (is_angle && driver_vendor == "ANGLE")
      ExtractDriverVendorANGLE(renderer_str);
  }
  is_desktop_core_profile =
      DesktopCoreCommonCheck(is_es, major_version, minor_version) &&
      !gfx::HasExtension(extensions, "GL_ARB_compatibility");
  is_es3_capable = IsES3Capable(extensions);

  // Post-fixup in case the user requested disabling ES3 capability
  // for testing purposes.
  if (disable_es3_for_testing) {
    is_es3_capable = false;
    if (is_es) {
      major_version = 2;
      minor_version = 0;
      is_es2 = true;
      is_es3 = false;
    } else {
      major_version = 3;
      minor_version = 2;
    }
  }
}

void GLVersionInfo::ParseVersionString(const char* version_str) {
  // Make sure the outputs are always initialized.
  major_version = 0;
  minor_version = 0;
  is_es = false;
  is_es2 = false;
  is_es3 = false;
  if (!version_str)
    return;
  base::StringPiece lstr(version_str);
  constexpr base::StringPiece kESPrefix = "OpenGL ES ";
  if (base::StartsWith(lstr, kESPrefix, base::CompareCase::SENSITIVE)) {
    is_es = true;
    lstr.remove_prefix(kESPrefix.size());
  }
  std::vector<base::StringPiece> pieces = base::SplitStringPiece(
      lstr, " -()@", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (pieces.size() == 0) {
    // This should never happen, but let's just tolerant bad driver behavior.
    return;
  }

  if (is_es) {
    // Desktop GL doesn't specify the GL_VERSION format, but ES spec requires
    // the string to be in the format of "OpenGL ES major.minor other_info".
    DCHECK_LE(3u, pieces[0].size());
    if (pieces[0][pieces[0].size() - 1] == 'V') {
      // On Nexus 6 with Android N, GL_VERSION string is not spec compliant.
      // There is no space between "3.1" and "V@104.0".
      pieces[0].remove_suffix(1);
    }
  }
  std::string gl_version;
  pieces[0].CopyToString(&gl_version);
  base::Version version(gl_version);
  if (version.IsValid()) {
    if (version.components().size() >= 1) {
      major_version = version.components()[0];
    }
    if (version.components().size() >= 2) {
      minor_version = version.components()[1];
    }
    if (is_es) {
      if (major_version == 2)
        is_es2 = true;
      if (major_version == 3)
        is_es3 = true;
    }
  }

  if (pieces.size() == 1)
    return;

  constexpr base::StringPiece kVendors[] = {
      "ANGLE", "Mesa", "INTEL", "NVIDIA", "ATI", "FireGL", "Chromium", "APPLE"};
  for (size_t ii = 1; ii < pieces.size(); ++ii) {
    for (auto vendor : kVendors) {
      if (pieces[ii] == vendor) {
        vendor.CopyToString(&driver_vendor);
        if (ii + 1 < pieces.size())
          pieces[ii + 1].CopyToString(&driver_version);
        return;
      }
    }
  }
  if (pieces.size() == 2) {
    if (pieces[1][0] == 'V')
      pieces[1].remove_prefix(1);
    pieces[1].CopyToString(&driver_version);
    return;
  }
  constexpr base::StringPiece kMaliPrefix = "v1.r";
  if (base::StartsWith(pieces[1], kMaliPrefix, base::CompareCase::SENSITIVE)) {
    // Mali drivers: v1.r12p0-04rel0.44f2946824bb8739781564bffe2110c9
    pieces[1].remove_prefix(kMaliPrefix.size());
    std::vector<base::StringPiece> numbers = base::SplitStringPiece(
        pieces[1], "p", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (numbers.size() != 2)
      return;
    std::vector<base::StringPiece> parts = base::SplitStringPiece(
        pieces[2], ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() != 2)
      return;
    driver_vendor = "ARM";
    numbers[0].CopyToString(&driver_version);
    driver_version += ".";
    numbers[1].AppendToString(&driver_version);
    driver_version += ".";
    parts[0].AppendToString(&driver_version);
    return;
  }
  for (size_t ii = 1; ii < pieces.size(); ++ii) {
    if (pieces[ii].find('.') != std::string::npos) {
      pieces[ii].CopyToString(&driver_version);
      return;
    }
  }
}

void GLVersionInfo::ExtractDriverVendorANGLE(const char* renderer_str) {
  DCHECK(renderer_str);
  DCHECK(is_angle);
  DCHECK_EQ("ANGLE", driver_vendor);
  base::StringPiece rstr(renderer_str);
  DCHECK(base::StartsWith(rstr, "ANGLE (", base::CompareCase::SENSITIVE));
  rstr = rstr.substr(sizeof("ANGLE (") - 1, rstr.size() - sizeof("ANGLE ("));
  if (base::StartsWith(rstr, "Vulkan ", base::CompareCase::SENSITIVE)) {
    size_t pos = rstr.find('(');
    if (pos != std::string::npos)
      rstr = rstr.substr(pos + 1, rstr.size() - 2);
  }

  if (base::StartsWith(rstr, "NVIDIA ", base::CompareCase::SENSITIVE))
    driver_vendor = "ANGLE (NVIDIA)";
  else if (base::StartsWith(rstr, "Radeon ", base::CompareCase::SENSITIVE))
    driver_vendor = "ANGLE (AMD)";
  else if (base::StartsWith(rstr, "Intel", base::CompareCase::SENSITIVE)) {
    std::vector<base::StringPiece> pieces = base::SplitStringPiece(
        rstr, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (size_t ii = 0; ii < pieces.size(); ++ii) {
      if (base::StartsWith(pieces[ii], "Intel(R) ",
                           base::CompareCase::SENSITIVE)) {
        driver_vendor = "ANGLE (Intel)";
        break;
      }
    }
  }
}

bool GLVersionInfo::IsES3Capable(const gfx::ExtensionSet& extensions) const {
  // Version ES3 capable without extensions needed.
  if (IsAtLeastGLES(3, 0) || IsAtLeastGL(4, 2)) {
    return true;
  }

  // Don't try supporting ES3 on ES2, or desktop before 3.3.
  if (is_es || !IsAtLeastGL(3, 3)) {
    return false;
  }

  bool has_transform_feedback =
      (IsAtLeastGL(4, 0) ||
       gfx::HasExtension(extensions, "GL_ARB_transform_feedback2"));

  // This code used to require the GL_ARB_gpu_shader5 extension in order to
  // have support for dynamic indexing of sampler arrays, which was
  // optionally supported in ESSL 1.00. However, since this is expressly
  // forbidden in ESSL 3.00, and some desktop drivers (specifically
  // Mesa/Gallium on AMD GPUs) don't support it, we no longer require it.

  // tex storage is available in core spec since GL 4.2.
  bool has_tex_storage =
      gfx::HasExtension(extensions, "GL_ARB_texture_storage");

  // TODO(cwallez) check for texture related extensions. See crbug.com/623577

  return (has_transform_feedback && has_tex_storage);
}

void GLVersionInfo::DisableES3ForTesting() {
  disable_es3_for_testing = true;
}

bool GLVersionInfo::IsVersionSubstituted() const {
  // This is the only reason we're changing versions right now
  return disable_es3_for_testing;
}

GLVersionInfo::VersionStrings GLVersionInfo::GetFakeVersionStrings(
    unsigned major,
    unsigned minor) const {
  VersionStrings result;
  if (is_es) {
    if (major == 2) {
      result.gl_version = "OpenGL ES 2.0";
      result.glsl_version = "OpenGL ES GLSL ES 1.00";
    } else if (major == 3) {
      result.gl_version = "OpenGL ES 3.0";
      result.glsl_version = "OpenGL ES GLSL ES 3.00";
    } else {
      NOTREACHED();
    }
  } else {
    if (major == 4 && minor == 1) {
      result.gl_version = "4.1";
      result.glsl_version = "4.10";
    } else if (major == 4 && minor == 0) {
      result.gl_version = "4.0";
      result.glsl_version = "4.00";
    } else if (major == 3 && minor == 3) {
      result.gl_version = "3.3";
      result.glsl_version = "3.30";
    } else if (major == 3 && minor == 2) {
      result.gl_version = "3.2";
      result.glsl_version = "1.50";
    } else if (major == 3 && minor == 1) {
      result.gl_version = "3.1";
      result.glsl_version = "1.40";
    } else if (major == 3 && minor == 0) {
      result.gl_version = "3.0";
      result.glsl_version = "1.30";
    } else if (major == 2 && minor == 1) {
      result.gl_version = "2.1";
      result.glsl_version = "1.20";
    } else if (major == 2 && minor == 0) {
      result.gl_version = "2.0";
      result.glsl_version = "1.10";
    } else {
      NOTREACHED();
    }
  }
  return result;
}

}  // namespace gl
