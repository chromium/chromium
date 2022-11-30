// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/visual_picker_glx.h"

#include <algorithm>
#include <bitset>
#include <cstring>
#include <map>
#include <numeric>
#include <vector>

#include "base/memory/singleton.h"
#include "ui/gfx/x/future.h"

// These constants are obtained from GL/glx.h and GL/glxext.h.
constexpr uint32_t GLX_LEVEL = 3;
constexpr uint32_t GLX_DOUBLEBUFFER = 5;
constexpr uint32_t GLX_STEREO = 6;
constexpr uint32_t GLX_BUFFER_SIZE = 2;
constexpr uint32_t GLX_AUX_BUFFERS = 7;
constexpr uint32_t GLX_RED_SIZE = 8;
constexpr uint32_t GLX_GREEN_SIZE = 9;
constexpr uint32_t GLX_BLUE_SIZE = 10;
constexpr uint32_t GLX_ALPHA_SIZE = 11;
constexpr uint32_t GLX_DEPTH_SIZE = 12;
constexpr uint32_t GLX_STENCIL_SIZE = 13;
constexpr uint32_t GLX_ACCUM_RED_SIZE = 14;
constexpr uint32_t GLX_ACCUM_GREEN_SIZE = 15;
constexpr uint32_t GLX_ACCUM_BLUE_SIZE = 16;
constexpr uint32_t GLX_ACCUM_ALPHA_SIZE = 17;

constexpr uint32_t GLX_CONFIG_CAVEAT = 0x20;
constexpr uint32_t GLX_VISUAL_CAVEAT_EXT = 0x20;
constexpr uint32_t GLX_X_VISUAL_TYPE = 0x22;

constexpr uint32_t GLX_BIND_TO_TEXTURE_TARGETS_EXT = 0x20D3;

constexpr uint32_t GLX_NONE = 0x8000;
constexpr uint32_t GLX_NONE_EXT = 0x8000;
constexpr uint32_t GLX_TRUE_COLOR = 0x8002;
constexpr uint32_t GLX_VISUAL_ID = 0x800B;
constexpr uint32_t GLX_DRAWABLE_TYPE = 0x8010;
constexpr uint32_t GLX_RENDER_TYPE = 0x8011;
constexpr uint32_t GLX_FBCONFIG_ID = 0x8013;

constexpr uint32_t GLX_PIXMAP_BIT = 0x00000002;
constexpr uint32_t GLX_TEXTURE_2D_BIT_EXT = 0x00000002;

constexpr uint32_t GLX_SAMPLE_BUFFERS_ARB = 100000;
constexpr uint32_t GLX_SAMPLES = 100001;

constexpr uint32_t GL_FALSE = 0;

namespace ui {

namespace {

bool IsArgbVisual(const x11::Connection::VisualInfo& visual) {
  auto bits = [](auto x) {
    return std::bitset<8 * sizeof(decltype(x))>(x).count();
  };
  auto bits_rgb = bits(visual.visual_type->red_mask) +
                  bits(visual.visual_type->green_mask) +
                  bits(visual.visual_type->blue_mask);
  return static_cast<std::size_t>(visual.format->depth) > bits_rgb;
}

// Used to filter visuals by the best class.
int VisualScore(x11::VisualClass c_class) {
  // A higher score is more preferable.
  switch (c_class) {
    case x11::VisualClass::TrueColor:
      return 1;
    case x11::VisualClass::DirectColor:
      return 0;
    default:
      return -1;
  }
}

}  // anonymous namespace

// static
VisualPickerGlx* VisualPickerGlx::GetInstance() {
  return base::Singleton<VisualPickerGlx>::get();
}

x11::Glx::FbConfig VisualPickerGlx::GetFbConfigForFormat(
    gfx::BufferFormat format) {
  if (!config_map_)
    FillConfigMap();
  auto it = config_map_->find(format);
  return it == config_map_->end() ? x11::Glx::FbConfig{} : it->second;
}

x11::VisualId VisualPickerGlx::PickBestGlVisual(
    const x11::Glx::GetVisualConfigsReply& configs,
    base::RepeatingCallback<bool(const x11::Connection::VisualInfo&)> pred,
    bool want_alpha) const {
  int highest_score = -1;
  x11::VisualId best_visual{};
  for (size_t cfg = 0; cfg < configs.num_visuals; cfg++) {
    size_t i = cfg * configs.num_properties;
    std::map<uint32_t, uint32_t> props;
    static constexpr uint32_t static_props[] = {
        GLX_VISUAL_ID,       GLX_X_VISUAL_TYPE,    GLX_RENDER_TYPE,
        GLX_RED_SIZE,        GLX_GREEN_SIZE,       GLX_BLUE_SIZE,
        GLX_ALPHA_SIZE,      GLX_ACCUM_RED_SIZE,   GLX_ACCUM_GREEN_SIZE,
        GLX_ACCUM_BLUE_SIZE, GLX_ACCUM_ALPHA_SIZE, GLX_DOUBLEBUFFER,
        GLX_STEREO,          GLX_BUFFER_SIZE,      GLX_DEPTH_SIZE,
        GLX_STENCIL_SIZE,    GLX_AUX_BUFFERS,      GLX_LEVEL,
    };
    for (const uint32_t prop : static_props)
      props[prop] = configs.property_list[i++];
    const size_t extra_props =
        (configs.num_properties - std::size(static_props)) / 2;
    for (size_t j = 0; j < extra_props; j++) {
      const auto key = configs.property_list[i++];
      const auto value = configs.property_list[i++];
      // Mesa adds a (0, 0) key-value pair at the end of each property list.
      if (!key)
        continue;
      props[key] = value;
    }

    auto get = [&](uint32_t key) {
      auto it = props.find(key);
      return it == props.end() ? 0 : it->second;
    };

    const auto visual_id = static_cast<x11::VisualId>(get(GLX_VISUAL_ID));
    const auto* info = connection_->GetVisualInfoFromId(visual_id);
    if (!pred.Run(*info))
      continue;

    if (!get(GLX_DOUBLEBUFFER) || get(GLX_STEREO))
      continue;

    auto caveat = get(GLX_VISUAL_CAVEAT_EXT);
    if (caveat && caveat != GLX_NONE_EXT)
      continue;

    // Give precedence to the root visual if it satisfies the basic
    // requirements above.  This can avoid an expensive copy-on-present.
    if (visual_id == connection_->default_root_visual().visual_id)
      return visual_id;

    int score = 0;
    if (get(GLX_SAMPLE_BUFFERS_ARB) == 0) {
      score++;
      if (get(GLX_DEPTH_SIZE) == 0 && get(GLX_STENCIL_SIZE) == 0) {
        score++;
        const bool has_alpha = get(GLX_ALPHA_SIZE) > 0;
        if (has_alpha == want_alpha)
          score++;
      }
    }

    if (score > highest_score) {
      highest_score = score;
      best_visual = visual_id;
    }
  }

  return best_visual;
}

x11::VisualId VisualPickerGlx::PickBestSystemVisual(
    const x11::Glx::GetVisualConfigsReply& configs) const {
  x11::Connection::VisualInfo default_visual_info =
      *connection_->GetVisualInfoFromId(
          connection_->default_root_visual().visual_id);

  auto is_compatible_with_root_visual =
      [](const x11::Connection::VisualInfo& default_visual_info,
         const x11::Connection::VisualInfo& visual_info) {
        const auto& dvt = *default_visual_info.visual_type;
        const auto& vt = *visual_info.visual_type;
        return vt.c_class == dvt.c_class &&
               visual_info.format->depth == default_visual_info.format->depth &&
               vt.red_mask == dvt.red_mask && vt.green_mask == dvt.green_mask &&
               vt.blue_mask == dvt.blue_mask &&
               vt.colormap_entries == dvt.colormap_entries &&
               vt.bits_per_rgb_value == dvt.bits_per_rgb_value;
      };

  return PickBestGlVisual(
      configs,
      base::BindRepeating(is_compatible_with_root_visual, default_visual_info),
      IsArgbVisual(default_visual_info));
}

x11::VisualId VisualPickerGlx::PickBestRgbaVisual(
    const x11::Glx::GetVisualConfigsReply& configs) const {
  int best_class_score = -1;
  for (const auto& depth : connection_->default_screen().allowed_depths) {
    for (const auto& vis : depth.visuals)
      best_class_score = std::max(best_class_score, VisualScore(vis.c_class));
  }
  auto pred = [](int best_class_score,
                 const x11::Connection::VisualInfo& visual_info) {
    if (!IsArgbVisual(visual_info))
      return false;
    return VisualScore(visual_info.visual_type->c_class) == best_class_score;
  };
  return PickBestGlVisual(configs, base::BindRepeating(pred, best_class_score),
                          true);
}

void VisualPickerGlx::FillConfigMap() {
  DCHECK(!config_map_);
  config_map_ =
      std::make_unique<base::flat_map<gfx::BufferFormat, x11::Glx::FbConfig>>();

  if (auto configs = connection_->glx()
                         .GetFBConfigs({static_cast<uint32_t>(
                             connection_->DefaultScreenId())})
                         .Sync()) {
    const auto n_cfgs = configs->num_FB_configs;
    const auto n_props = configs->num_properties;

    // Iterate from back to front since "preferred" FB configs appear earlier.
    for (size_t cfg = n_cfgs; cfg-- > 0;) {
      std::map<uint32_t, uint32_t> props;
      for (size_t prop = 0; prop < n_props; prop++) {
        size_t i = 2 * cfg * n_props + 2 * prop;
        const auto key = configs->property_list[i];
        const auto value = configs->property_list[i + 1];
        props[key] = value;
      }

      auto get = [&](uint32_t key) {
        auto it = props.find(key);
        return it == props.end() ? 0 : it->second;
      };

      // Each config must have an ID.
      auto id = get(GLX_FBCONFIG_ID);
      DCHECK(id);
      auto fbconfig = static_cast<x11::Glx::FbConfig>(id);

      // Ensure the config is compatible with pixmap drawing.
      if (!(get(GLX_DRAWABLE_TYPE) & GLX_PIXMAP_BIT))
        continue;

      // Ensure we can bind to GL_TEXTURE_2D.
      if (!(get(GLX_BIND_TO_TEXTURE_TARGETS_EXT) & GLX_TEXTURE_2D_BIT_EXT))
        continue;

      // No double-buffering.
      if (get(GLX_DOUBLEBUFFER) != GL_FALSE)
        continue;

      // Prefer true-color over direct-color.
      if (get(GLX_X_VISUAL_TYPE) != GLX_TRUE_COLOR)
        continue;

      // No caveats.
      auto caveat = get(GLX_CONFIG_CAVEAT);
      if (caveat && caveat != GLX_NONE)
        continue;

      // No antialiasing needed.
      if (get(GLX_SAMPLES))
        continue;

      // No depth buffer needed.
      if (get(GLX_DEPTH_SIZE))
        continue;

      auto r = get(GLX_RED_SIZE);
      auto g = get(GLX_GREEN_SIZE);
      auto b = get(GLX_BLUE_SIZE);
      auto a = get(GLX_ALPHA_SIZE);
      if (r == 5 && g == 6 && b == 5 && a == 0)
        (*config_map_)[gfx::BufferFormat::BGR_565] = fbconfig;
      else if (r == 8 && g == 8 && b == 8 && a == 0)
        (*config_map_)[gfx::BufferFormat::BGRX_8888] = fbconfig;
      else if (r == 10 && g == 10 && b == 10 && a == 0)
        (*config_map_)[gfx::BufferFormat::BGRA_1010102] = fbconfig;
      else if (r == 8 && g == 8 && b == 8 && a == 8)
        (*config_map_)[gfx::BufferFormat::BGRA_8888] = fbconfig;
    }
  }
}

VisualPickerGlx::VisualPickerGlx() : connection_(x11::Connection::Get()) {
  auto configs =
      connection_->glx()
          .GetVisualConfigs(
              {static_cast<uint32_t>(connection_->DefaultScreenId())})
          .Sync();

  if (configs) {
    system_visual_ = PickBestSystemVisual(*configs.reply);
    rgba_visual_ = PickBestRgbaVisual(*configs.reply);
  }
}

VisualPickerGlx::~VisualPickerGlx() = default;

}  // namespace ui
