// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/visual_picker_glx.h"

#include <algorithm>
#include <bitset>
#include <cstring>
#include <map>
#include <numeric>

#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/glx.h"

namespace x11 {

namespace {

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

constexpr uint32_t GLX_VISUAL_CAVEAT_EXT = 0x20;
constexpr uint32_t GLX_X_VISUAL_TYPE = 0x22;

constexpr uint32_t GLX_NONE_EXT = 0x8000;
constexpr uint32_t GLX_VISUAL_ID = 0x800B;
constexpr uint32_t GLX_RENDER_TYPE = 0x8011;

constexpr uint32_t GLX_SAMPLE_BUFFERS_ARB = 100000;

bool IsArgbVisual(const Connection::VisualInfo& visual) {
  auto bits = [](auto x) {
    return std::bitset<8 * sizeof(decltype(x))>(x).count();
  };
  auto bits_rgb = bits(visual.visual_type->red_mask) +
                  bits(visual.visual_type->green_mask) +
                  bits(visual.visual_type->blue_mask);
  return static_cast<std::size_t>(visual.format->depth) > bits_rgb;
}

// Used to filter visuals by the best class.
int VisualScore(VisualClass c_class) {
  // A higher score is more preferable.
  switch (c_class) {
    case VisualClass::TrueColor:
      return 1;
    case VisualClass::DirectColor:
      return 0;
    default:
      return -1;
  }
}

VisualId PickBestGlVisual(
    Connection* connection,
    const Glx::GetVisualConfigsReply& configs,
    base::RepeatingCallback<bool(const Connection::VisualInfo&)> pred,
    bool want_alpha) {
  int highest_score = -1;
  VisualId best_visual{};
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
    for (const uint32_t prop : static_props) {
      props[prop] = configs.property_list[i++];
    }
    const size_t extra_props =
        (configs.num_properties - std::size(static_props)) / 2;
    for (size_t j = 0; j < extra_props; j++) {
      const auto key = configs.property_list[i++];
      const auto value = configs.property_list[i++];
      // Mesa adds a (0, 0) key-value pair at the end of each property list.
      if (!key) {
        continue;
      }
      props[key] = value;
    }

    auto get = [&](uint32_t key) {
      auto it = props.find(key);
      return it == props.end() ? 0 : it->second;
    };

    const auto visual_id = static_cast<VisualId>(get(GLX_VISUAL_ID));
    const auto* info = connection->GetVisualInfoFromId(visual_id);
    if (!pred.Run(*info)) {
      continue;
    }

    if (!get(GLX_DOUBLEBUFFER) || get(GLX_STEREO)) {
      continue;
    }

    auto caveat = get(GLX_VISUAL_CAVEAT_EXT);
    if (caveat && caveat != GLX_NONE_EXT) {
      continue;
    }

    // Give precedence to the root visual if it satisfies the basic
    // requirements above.  This can avoid an expensive copy-on-present.
    if (visual_id == connection->default_root_visual().visual_id) {
      return visual_id;
    }

    int score = 0;
    if (get(GLX_SAMPLE_BUFFERS_ARB) == 0) {
      score++;
      if (get(GLX_DEPTH_SIZE) == 0 && get(GLX_STENCIL_SIZE) == 0) {
        score++;
        const bool has_alpha = get(GLX_ALPHA_SIZE) > 0;
        if (has_alpha == want_alpha) {
          score++;
        }
      }
    }

    if (score > highest_score) {
      highest_score = score;
      best_visual = visual_id;
    }
  }

  return best_visual;
}

VisualId PickBestSystemVisual(Connection* connection,
                              const Glx::GetVisualConfigsReply& configs) {
  Connection::VisualInfo default_visual_info = *connection->GetVisualInfoFromId(
      connection->default_root_visual().visual_id);

  auto is_compatible_with_root_visual =
      [](const Connection::VisualInfo& default_visual_info,
         const Connection::VisualInfo& visual_info) {
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
      connection, configs,
      base::BindRepeating(is_compatible_with_root_visual, default_visual_info),
      IsArgbVisual(default_visual_info));
}

VisualId PickBestRgbaVisual(Connection* connection,
                            const Glx::GetVisualConfigsReply& configs) {
  int best_class_score = -1;
  for (const auto& depth : connection->default_screen().allowed_depths) {
    for (const auto& vis : depth.visuals) {
      best_class_score = std::max(best_class_score, VisualScore(vis.c_class));
    }
  }
  auto pred = [](int best_class_score,
                 const Connection::VisualInfo& visual_info) {
    if (!IsArgbVisual(visual_info)) {
      return false;
    }
    return VisualScore(visual_info.visual_type->c_class) == best_class_score;
  };
  return PickBestGlVisual(connection, configs,
                          base::BindRepeating(pred, best_class_score), true);
}

}  // anonymous namespace

void PickBestVisuals(Connection* connection,
                     VisualId& system_visual,
                     VisualId& rgba_visual) {
  auto screen = static_cast<uint32_t>(connection->DefaultScreenId());
  if (auto configs = connection->glx().GetVisualConfigs(screen).Sync()) {
    system_visual = PickBestSystemVisual(connection, *configs.reply);
    rgba_visual = PickBestRgbaVisual(connection, *configs.reply);
  }
}

}  // namespace x11
