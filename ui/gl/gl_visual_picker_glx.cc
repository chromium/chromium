// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_visual_picker_glx.h"

#include <algorithm>
#include <bitset>
#include <cstring>
#include <numeric>
#include <vector>

#include "base/memory/singleton.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_glx.h"

namespace gl {

namespace {

bool IsArgbVisual(const XVisualInfo& visual) {
  auto bits = [](auto x) {
    return std::bitset<8 * sizeof(decltype(x))>(x).count();
  };
  auto bits_rgb =
      bits(visual.red_mask) + bits(visual.green_mask) + bits(visual.blue_mask);
  return static_cast<std::size_t>(visual.depth) > bits_rgb;
}

}  // anonymous namespace

// static
GLVisualPickerGLX* GLVisualPickerGLX::GetInstance() {
  return base::Singleton<GLVisualPickerGLX>::get();
}

XVisualInfo GLVisualPickerGLX::PickBestGlVisual(
    const std::vector<XVisualInfo>& visuals,
    bool want_alpha) const {
  // Find the highest scoring visual and return it.
  Visual* default_visual = DefaultVisual(display_, DefaultScreen(display_));
  int highest_score = -1;
  XVisualInfo best_visual;
  memset(&best_visual, 0, sizeof(best_visual));
  for (const XVisualInfo& const_visual_info : visuals) {
    int supports_gl, double_buffer, stereo, alpha_size, depth_size,
        stencil_size, num_multisample, visual_caveat;
    // glXGetConfig unfortunately doesn't use const.
    XVisualInfo* visual_info = const_cast<XVisualInfo*>(&const_visual_info);
    if (glXGetConfig(display_, visual_info, GLX_USE_GL, &supports_gl) ||
        !supports_gl ||
        glXGetConfig(display_, visual_info, GLX_DOUBLEBUFFER, &double_buffer) ||
        !double_buffer ||
        glXGetConfig(display_, visual_info, GLX_STEREO, &stereo) || stereo) {
      continue;
    }
    if (has_glx_visual_rating_) {
      if (glXGetConfig(display_, visual_info, GLX_VISUAL_CAVEAT_EXT,
                       &visual_caveat) ||
          visual_caveat != GLX_NONE_EXT) {
        continue;
      }
    }

    // Give precedence to the root visual if it satisfies the basic requirements
    // above.  This can avoid an expensive copy-on-present.
    if (const_visual_info.visual == default_visual)
      return const_visual_info;

    int score = 0;
    if (!has_glx_multisample_ ||
        (!glXGetConfig(display_, visual_info, GLX_SAMPLE_BUFFERS_ARB,
                       &num_multisample) &&
         num_multisample == 0)) {
      score++;
      if (!glXGetConfig(display_, visual_info, GLX_DEPTH_SIZE, &depth_size) &&
          depth_size == 0 &&
          !glXGetConfig(display_, visual_info, GLX_STENCIL_SIZE,
                        &stencil_size) &&
          stencil_size == 0) {
        score++;
        if (!glXGetConfig(display_, visual_info, GLX_ALPHA_SIZE, &alpha_size) &&
            (alpha_size > 0) == want_alpha) {
          score++;
        }
      }
    }

    if (score > highest_score) {
      highest_score = score;
      best_visual = const_visual_info;
    }
  }
  return best_visual;
}

XVisualInfo GLVisualPickerGLX::PickBestSystemVisual(
    const std::vector<XVisualInfo>& visuals) const {
  Visual* default_visual = DefaultVisual(display_, DefaultScreen(display_));
  auto it = std::find_if(visuals.begin(), visuals.end(),
                         [default_visual](const XVisualInfo& visual_info) {
                           return visual_info.visual == default_visual;
                         });
  DCHECK(it != visuals.end());

  const XVisualInfo& default_visual_info = *it;
  std::vector<XVisualInfo> filtered_visuals;
  std::copy_if(
      visuals.begin(), visuals.end(), std::back_inserter(filtered_visuals),
      [&default_visual_info](const XVisualInfo& visual_info) {
        const XVisualInfo& v1 = visual_info;
        const XVisualInfo& v2 = default_visual_info;
        return v1.c_class == v2.c_class && v1.depth == v2.depth &&
               v1.red_mask == v2.red_mask && v1.green_mask == v2.green_mask &&
               v1.blue_mask == v2.blue_mask &&
               v1.colormap_size == v2.colormap_size &&
               v1.bits_per_rgb == v2.bits_per_rgb;
      });
  return PickBestGlVisual(filtered_visuals, IsArgbVisual(default_visual_info));
}

XVisualInfo GLVisualPickerGLX::PickBestRgbaVisual(
    const std::vector<XVisualInfo>& visuals) const {
  // Filter the visuals by the best class.
  auto score = [](int c_class) {
    // A higher score is more preferable.
    switch (c_class) {
      case TrueColor:
        return 1;
      case DirectColor:
        return 0;
      default:
        return -1;
    }
  };
  int best_class_score =
      std::accumulate(visuals.begin(), visuals.end(), -1,
                      [&score](int acc, const XVisualInfo& visual_info) {
                        return std::max(acc, score(visual_info.c_class));
                      });
  std::vector<XVisualInfo> filtered_visuals;
  std::copy_if(visuals.begin(), visuals.end(),
               std::back_inserter(filtered_visuals),
               [best_class_score, &score](const XVisualInfo& visual_info) {
                 if (!IsArgbVisual(visual_info))
                   return false;
                 return score(visual_info.c_class) == best_class_score;
               });
  return PickBestGlVisual(filtered_visuals, true);
}

GLVisualPickerGLX::GLVisualPickerGLX() : display_(gfx::GetXDisplay()) {
  has_glx_visual_rating_ =
      GLSurfaceGLX::HasGLXExtension("GLX_EXT_visual_rating");
  has_glx_multisample_ = GLSurfaceGLX::HasGLXExtension("GLX_EXT_multisample");

  XVisualInfo visual_template;
  visual_template.screen = DefaultScreen(display_);

  // Get all of the visuals for the default screen.
  int n_visuals;
  gfx::XScopedPtr<XVisualInfo[]> x_visuals(
      XGetVisualInfo(display_, VisualScreenMask, &visual_template, &n_visuals));
  std::vector<XVisualInfo> visuals;
  for (int i = 0; i < n_visuals; i++)
    visuals.push_back(x_visuals[i]);

  system_visual_ = PickBestSystemVisual(visuals);
  rgba_visual_ = PickBestRgbaVisual(visuals);
}

GLVisualPickerGLX::~GLVisualPickerGLX() = default;

}  // namespace gl
