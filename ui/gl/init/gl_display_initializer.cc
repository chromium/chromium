// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_display_initializer.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace gl::init {

namespace {

void AddInitDisplay(std::vector<DisplayType>* init_displays,
                    DisplayType display_type) {
  // Make sure to not add the same display type twice.
  if (!base::Contains(*init_displays, display_type)) {
    init_displays->push_back(display_type);
  }
}

void GetEGLInitDisplays(bool supports_angle_d3d,
                        bool supports_angle_opengl,
                        bool supports_angle_null,
                        bool supports_angle_vulkan,
                        bool supports_angle_swiftshader,
                        bool supports_angle_opengl_egl,
                        bool supports_angle_metal,
                        const base::CommandLine* command_line,
                        std::vector<DisplayType>* init_displays) {
  // Check which experiment groups we're in. Check these early in the function
  // so that finch assigns a group before the final decision to use the API is
  // made. If we check too late, it will appear that some users are missing from
  // the group if they are falling back to another path due to crashes or
  // missing support.
  bool default_angle_opengl =
      base::FeatureList::IsEnabled(features::kDefaultANGLEOpenGL);
  bool default_angle_metal =
      base::FeatureList::IsEnabled(features::kDefaultANGLEMetal);
  bool default_angle_vulkan = features::IsDefaultANGLEVulkan();

  // If we're already requesting software GL, make sure we don't fallback to the
  // GPU
  bool force_software_gl =
      IsSoftwareGLImplementation(GetGLImplementationParts());

  std::string requested_renderer =
      force_software_gl
          ? kANGLEImplementationSwiftShaderName
          : command_line->GetSwitchValueASCII(switches::kUseANGLE);

  bool use_angle_default =
      !force_software_gl &&
      (!command_line->HasSwitch(switches::kUseANGLE) ||
       requested_renderer == kANGLEImplementationDefaultName);

  if (supports_angle_null &&
      (requested_renderer == kANGLEImplementationNullName ||
       gl::GetANGLEImplementation() == ANGLEImplementation::kNull)) {
    AddInitDisplay(init_displays, ANGLE_NULL);
    return;
  }

  // If no display has been explicitly requested and the DefaultANGLEOpenGL
  // experiment is enabled, try creating OpenGL displays first.
  // TODO(oetuaho@nvidia.com): Only enable this path on specific GPUs with a
  // blocklist entry. http://crbug.com/693090
  if (supports_angle_opengl && use_angle_default && default_angle_opengl) {
    AddInitDisplay(init_displays, ANGLE_OPENGL);
    AddInitDisplay(init_displays, ANGLE_OPENGLES);
  }

  if (supports_angle_metal && use_angle_default && default_angle_metal &&
      !GetGlWorkarounds().disable_metal) {
    AddInitDisplay(init_displays, ANGLE_METAL);
  }

  if (supports_angle_vulkan && use_angle_default && default_angle_vulkan) {
    AddInitDisplay(init_displays, ANGLE_VULKAN);
  }

  if (supports_angle_d3d) {
    if (use_angle_default) {
      // Default mode for ANGLE - try D3D11, else try D3D9
      if (!GetGlWorkarounds().disable_d3d11) {
        AddInitDisplay(init_displays, ANGLE_D3D11);
      }
      AddInitDisplay(init_displays, ANGLE_D3D9);
    } else {
      if (requested_renderer == kANGLEImplementationD3D11Name) {
        AddInitDisplay(init_displays, ANGLE_D3D11);
      } else if (requested_renderer == kANGLEImplementationD3D9Name) {
        AddInitDisplay(init_displays, ANGLE_D3D9);
      } else if (requested_renderer == kANGLEImplementationD3D11NULLName) {
        AddInitDisplay(init_displays, ANGLE_D3D11_NULL);
      } else if (requested_renderer == kANGLEImplementationD3D11on12Name) {
        AddInitDisplay(init_displays, ANGLE_D3D11on12);
      }
    }
  }

  if (supports_angle_opengl) {
    if (use_angle_default && !supports_angle_d3d) {
#if BUILDFLAG(IS_ANDROID)
      // Don't request desktopGL on android
      AddInitDisplay(init_displays, ANGLE_OPENGLES);
#else
      AddInitDisplay(init_displays, ANGLE_OPENGL);
      AddInitDisplay(init_displays, ANGLE_OPENGLES);
#endif  // BUILDFLAG(IS_ANDROID)
    } else {
      if (requested_renderer == kANGLEImplementationOpenGLName) {
        AddInitDisplay(init_displays, ANGLE_OPENGL);
      } else if (requested_renderer == kANGLEImplementationOpenGLESName) {
        AddInitDisplay(init_displays, ANGLE_OPENGLES);
      } else if (requested_renderer == kANGLEImplementationOpenGLNULLName) {
        AddInitDisplay(init_displays, ANGLE_OPENGL_NULL);
      } else if (requested_renderer == kANGLEImplementationOpenGLESNULLName) {
        AddInitDisplay(init_displays, ANGLE_OPENGLES_NULL);
      } else if (requested_renderer == kANGLEImplementationOpenGLEGLName &&
                 supports_angle_opengl_egl) {
        AddInitDisplay(init_displays, ANGLE_OPENGL_EGL);
      } else if (requested_renderer == kANGLEImplementationOpenGLESEGLName &&
                 supports_angle_opengl_egl) {
        AddInitDisplay(init_displays, ANGLE_OPENGLES_EGL);
      }
    }
  }

  if (supports_angle_vulkan) {
    if (use_angle_default) {
      if (!supports_angle_d3d && !supports_angle_opengl) {
        AddInitDisplay(init_displays, ANGLE_VULKAN);
      }
    } else if (requested_renderer == kANGLEImplementationVulkanName) {
      AddInitDisplay(init_displays, ANGLE_VULKAN);
    } else if (requested_renderer == kANGLEImplementationVulkanNULLName) {
      AddInitDisplay(init_displays, ANGLE_VULKAN_NULL);
    }
  }

  if (supports_angle_swiftshader) {
    if (requested_renderer == kANGLEImplementationSwiftShaderName ||
        requested_renderer == kANGLEImplementationSwiftShaderForWebGLName) {
      AddInitDisplay(init_displays, ANGLE_SWIFTSHADER);
    }
  }

  if (supports_angle_metal) {
    if (use_angle_default) {
      if (!supports_angle_opengl) {
        AddInitDisplay(init_displays, ANGLE_METAL);
      }
    } else if (requested_renderer == kANGLEImplementationMetalName) {
      AddInitDisplay(init_displays, ANGLE_METAL);
    } else if (requested_renderer == kANGLEImplementationMetalNULLName) {
      AddInitDisplay(init_displays, ANGLE_METAL_NULL);
    }
  }

  // If no displays are available due to missing angle extensions or invalid
  // flags, request the default display.
  if (init_displays->empty()) {
    init_displays->push_back(DEFAULT);
  }
}

}  // namespace

void GetEGLInitDisplaysForTesting(bool supports_angle_d3d,  // IN-TEST
                                  bool supports_angle_opengl,
                                  bool supports_angle_null,
                                  bool supports_angle_vulkan,
                                  bool supports_angle_swiftshader,
                                  bool supports_angle_opengl_egl,
                                  bool supports_angle_metal,
                                  const base::CommandLine* command_line,
                                  std::vector<DisplayType>* init_displays) {
  GetEGLInitDisplays(supports_angle_d3d, supports_angle_opengl,
                     supports_angle_null, supports_angle_vulkan,
                     supports_angle_swiftshader, supports_angle_opengl_egl,
                     supports_angle_metal, command_line, init_displays);
}

void GetDisplayInitializationParams(bool* supports_angle,
                                    std::vector<DisplayType>* init_displays) {
  std::vector<GLImplementationParts> allowed_impls =
      GetAllowedGLImplementations();

  bool supports_angle_d3d = false;
  bool supports_angle_opengl = false;
  bool supports_angle_null = false;
  bool supports_angle_vulkan = false;
  bool supports_angle_swiftshader = false;
  bool supports_angle_opengl_egl = false;
  bool supports_angle_metal = false;
  // Check for availability of ANGLE extensions.
  if (g_driver_egl.client_ext.b_EGL_ANGLE_platform_angle) {
    supports_angle_d3d =
        g_driver_egl.client_ext.b_EGL_ANGLE_platform_angle_d3d &&
        (gl::GLImplementationParts(gl::ANGLEImplementation::kD3D9)
             .IsAllowed(allowed_impls) ||
         gl::GLImplementationParts(gl::ANGLEImplementation::kD3D11)
             .IsAllowed(allowed_impls));
    supports_angle_opengl =
        g_driver_egl.client_ext.b_EGL_ANGLE_platform_angle_opengl &&
        (gl::GLImplementationParts(gl::ANGLEImplementation::kOpenGL)
             .IsAllowed(allowed_impls) ||
         gl::GLImplementationParts(gl::ANGLEImplementation::kOpenGLES)
             .IsAllowed(allowed_impls));
    supports_angle_null =
        g_driver_egl.client_ext.b_EGL_ANGLE_platform_angle_null;
    supports_angle_vulkan =
        g_driver_egl.client_ext.b_EGL_ANGLE_platform_angle_vulkan &&
        gl::GLImplementationParts(gl::ANGLEImplementation::kVulkan)
            .IsAllowed(allowed_impls);
    supports_angle_swiftshader =
        g_driver_egl.client_ext
            .b_EGL_ANGLE_platform_angle_device_type_swiftshader &&
        gl::GLImplementationParts(gl::ANGLEImplementation::kSwiftShader)
            .IsAllowed(allowed_impls);
    supports_angle_opengl_egl =
        g_driver_egl.client_ext
            .b_EGL_ANGLE_platform_angle_device_type_egl_angle;
    supports_angle_metal =
        g_driver_egl.client_ext.b_EGL_ANGLE_platform_angle_metal &&
        gl::GLImplementationParts(gl::ANGLEImplementation::kMetal)
            .IsAllowed(allowed_impls);
  }

  *supports_angle = supports_angle_d3d || supports_angle_opengl ||
                    supports_angle_null || supports_angle_vulkan ||
                    supports_angle_swiftshader || supports_angle_metal;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  GetEGLInitDisplays(supports_angle_d3d, supports_angle_opengl,
                     supports_angle_null, supports_angle_vulkan,
                     supports_angle_swiftshader, supports_angle_opengl_egl,
                     supports_angle_metal, command_line, init_displays);
}

bool InitializeDisplay(GLDisplayEGL* display,
                       EGLDisplayPlatform native_display) {
  if (display->GetDisplay() != nullptr) {
    return true;
  }

  bool supports_angle = false;
  std::vector<DisplayType> init_displays;
  GetDisplayInitializationParams(&supports_angle, &init_displays);
  return display->Initialize(supports_angle, init_displays, native_display);
}

}  // namespace gl::init
