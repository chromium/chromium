// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_version_info.h"

#include <map>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/version.h"

namespace gl {

GLVersionInfo::GLVersionInfo(const char* version_str,
                             const char* renderer_str,
                             const gfx::ExtensionSet& extensions) {
  Initialize(version_str, renderer_str, extensions);
}

void GLVersionInfo::Initialize(const char* version_str,
                               const char* renderer_str,
                               const gfx::ExtensionSet& extensions) {
  if (version_str) {
    ParseVersionString(version_str);
    ParseDriverInfo(version_str);
  }
  // ANGLE's version string does not contain useful information for
  // GLVersionInfo. If we are going to parse the version string and we're using
  // ANGLE, we must also parse ANGLE's renderer string, which contains the
  // driver's version string.
  DCHECK(renderer_str || driver_vendor != "ANGLE");
  if (renderer_str) {
    std::string renderer_string = std::string(renderer_str);

    is_angle = base::StartsWith(renderer_str, "ANGLE",
                                base::CompareCase::SENSITIVE);
    is_mesa = base::StartsWith(renderer_str, "Mesa",
                               base::CompareCase::SENSITIVE);
    if (is_angle) {
      is_angle_swiftshader =
          renderer_string.find("SwiftShader Device") != std::string::npos;
      is_angle_vulkan = renderer_string.find("Vulkan") != std::string::npos;
      is_angle_metal = renderer_string.find("ANGLE Metal") != std::string::npos;
    }

    is_swiftshader = base::StartsWith(renderer_str, "Google SwiftShader",
                                      base::CompareCase::SENSITIVE);
    // An ANGLE renderer string contains "Direct3D9", "Direct3DEx", or
    // "Direct3D11" on D3D backends.
    is_d3d = renderer_string.find("Direct3D") != std::string::npos;
    // (is_d3d should only be possible if is_angle is true.)
    DCHECK(!is_d3d || is_angle);
    if (is_angle && driver_vendor == "ANGLE")
      ExtractDriverVendorANGLE(renderer_str);
  }
}

void GLVersionInfo::ParseVersionString(const char* version_str) {
  // Make sure the outputs are always initialized.
  major_version = 0;
  minor_version = 0;
  is_es2 = false;
  is_es3 = false;
  if (!version_str)
    return;
  std::string_view lstr(version_str);
  constexpr std::string_view kESPrefix = "OpenGL ES ";
  if (!base::StartsWith(lstr, kESPrefix, base::CompareCase::SENSITIVE)) {
    LOG(FATAL) << "Chrome runs only on top of OpenGL ES "
               << "through either ANGLE or native: " << "VERSION = "
               << version_str;
  }
  lstr.remove_prefix(kESPrefix.size());
  std::vector<std::string_view> pieces = base::SplitStringPiece(
      lstr, " -()@", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (pieces.size() == 0) {
    // This should never happen, but let's just tolerate bad driver behavior.
    return;
  }

  // ES spec requires the string to be in the format of "OpenGL ES major.minor
  // other_info".
  DCHECK_LE(3u, pieces[0].size());
  if (pieces[0].size() > 0 && pieces[0].back() == 'V') {
    // On Nexus 6 with Android N, GL_VERSION string is not spec compliant.
    // There is no space between "3.1" and "V@104.0".
    pieces[0].remove_suffix(1);
  }
  std::string gl_version(pieces[0]);
  base::Version version(gl_version);
  if (version.IsValid()) {
    if (version.components().size() >= 1) {
      major_version = version.components()[0];
    }
    if (version.components().size() >= 2) {
      minor_version = version.components()[1];
    }
    if (major_version == 2) {
      is_es2 = true;
    }
    if (major_version == 3) {
      is_es3 = true;
    }
  }
}

void GLVersionInfo::ParseDriverInfo(const char* version_str) {
  if (!version_str)
    return;
  std::string_view lstr(version_str);
  constexpr std::string_view kESPrefix = "OpenGL ES ";
  if (base::StartsWith(lstr, kESPrefix, base::CompareCase::SENSITIVE)) {
    lstr.remove_prefix(kESPrefix.size());
  }
  std::vector<std::string_view> pieces = base::SplitStringPiece(
      lstr, " -()@", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (pieces.size() == 0) {
    // This should never happen, but let's just tolerate bad driver behavior.
    return;
  }

  // ES spec requires the string to be in the format of
  // "OpenGL ES major.minor other_info".
  DCHECK_LE(3u, pieces[0].size());
  if (pieces[0].size() > 0 && pieces[0].back() == 'V') {
    // On Nexus 6 with Android N, GL_VERSION string is not spec compliant.
    // There is no space between "3.1" and "V@104.0".
    pieces[0].remove_suffix(1);
  }

  if (pieces.size() == 1)
    return;

  // Map key strings to driver vendors. We assume the key string is followed by
  // the driver version.
  const std::map<std::string_view, std::string_view> kVendors = {
      {"ANGLE", "ANGLE"},       {"Mesa", "Mesa"},   {"INTEL", "INTEL"},
      {"NVIDIA", "NVIDIA"},     {"ATI", "ATI"},     {"FireGL", "FireGL"},
      {"Chromium", "Chromium"}, {"APPLE", "APPLE"}, {"AMD", "AMD"},
      {"Metal", "Apple"}};
  for (size_t ii = 1; ii < pieces.size(); ++ii) {
    for (auto vendor : kVendors) {
      if (pieces[ii] == vendor.first) {
        driver_vendor = vendor.second;
        if (ii + 1 < pieces.size()) {
          driver_version = pieces[ii + 1];
        }
        return;
      }
    }
  }
  if (pieces.size() == 2) {
    if (pieces[1][0] == 'V')
      pieces[1].remove_prefix(1);
    driver_version = pieces[1];
    return;
  }
  constexpr std::string_view kMaliPrefix = "v1.r";
  if (base::StartsWith(pieces[1], kMaliPrefix, base::CompareCase::SENSITIVE)) {
    // Mali drivers: v1.r12p0-04rel0.44f2946824bb8739781564bffe2110c9
    pieces[1].remove_prefix(kMaliPrefix.size());
    std::vector<std::string_view> numbers = base::SplitStringPiece(
        pieces[1], "p", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (numbers.size() != 2)
      return;
    std::vector<std::string_view> parts = base::SplitStringPiece(
        pieces[2], ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() != 2)
      return;
    driver_vendor = "ARM";
    driver_version = base::StrCat({numbers[0], ".", numbers[1], ".", parts[0]});
    return;
  }
  for (size_t ii = 1; ii < pieces.size(); ++ii) {
    if (pieces[ii].find('.') != std::string::npos) {
      driver_version = pieces[ii];
      return;
    }
  }
}

void GLVersionInfo::ExtractDriverVendorANGLE(const char* renderer_str) {
  DCHECK(renderer_str);
  DCHECK(is_angle);
  DCHECK_EQ("ANGLE", driver_vendor);
  std::string_view rstr(renderer_str);
  DCHECK(base::StartsWith(rstr, "ANGLE (", base::CompareCase::SENSITIVE));
  rstr = rstr.substr(sizeof("ANGLE (") - 1, rstr.size() - sizeof("ANGLE ("));

  // ANGLE's renderer string returns a format matching ANGLE (GL_VENDOR,
  // GL_RENDERER, GL_VERSION)
  std::vector<std::string_view> gl_strings = base::SplitStringPiece(
      rstr, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // The 3rd part of the renderer string contains the native driver's version
  // string. We should parse it here to override anything parsed from ANGLE's
  // GL_VERSION string, which only contains information about the ANGLE version.
  if (gl_strings.size() >= 3) {
    // The first part of the renderer string contains the native driver's
    // vendor string.
    driver_vendor = gl_strings[0];

    std::string native_version_str;
    base::TrimString(gl_strings[2], ")", &native_version_str);
    ParseDriverInfo(native_version_str.c_str());
    return;
  }

  if (base::StartsWith(rstr, "Vulkan ", base::CompareCase::SENSITIVE)) {
    size_t pos = rstr.find('(');
    if (pos != std::string::npos)
      rstr = rstr.substr(pos + 1, rstr.size() - 2);
  }
  if (is_angle_swiftshader) {
    DCHECK(base::StartsWith(rstr, "SwiftShader", base::CompareCase::SENSITIVE));
    driver_vendor = "ANGLE (Google)";
  }
  if (is_angle_metal) {
    DCHECK(base::StartsWith(rstr, "ANGLE Metal", base::CompareCase::SENSITIVE));
  }
  if (base::StartsWith(rstr, "NVIDIA ", base::CompareCase::SENSITIVE))
    driver_vendor = "ANGLE (NVIDIA)";
  else if (base::StartsWith(rstr, "Radeon ", base::CompareCase::SENSITIVE))
    driver_vendor = "ANGLE (AMD)";
  else if (base::StartsWith(rstr, "Intel", base::CompareCase::SENSITIVE)) {
    std::vector<std::string_view> pieces = base::SplitStringPiece(
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

GLVersionInfo::VersionStrings GLVersionInfo::GetFakeVersionStrings(
    unsigned major,
    unsigned minor) const {
  VersionStrings result;
  if (major == 2) {
    result.gl_version = "OpenGL ES 2.0";
    result.glsl_version = "OpenGL ES GLSL ES 1.00";
  } else if (major == 3) {
    result.gl_version = "OpenGL ES 3.0";
    result.glsl_version = "OpenGL ES GLSL ES 3.00";
  } else {
    NOTREACHED_IN_MIGRATION();
  }
  return result;
}

}  // namespace gl
