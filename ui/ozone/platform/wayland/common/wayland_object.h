// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_OBJECT_H_
#define UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_OBJECT_H_

#include <memory>

#include "base/check.h"
#include "ui/ozone/platform/wayland/common/wayland.h"

struct wl_proxy;

namespace ui {
class WaylandConnection;
}

namespace wl {

template <typename T>
struct ObjectTraits;

using GlobalObjectFactory = void (*)(ui::WaylandConnection* connection,
                                     wl_registry* registry,
                                     uint32_t name,
                                     const std::string& interface,
                                     uint32_t version);

// This template forces T to declare a static Instantiate() method.  The
// subclass must implement it as follows:
//
// void Instantiate(WaylandConnection* connection,
//                  wl_registry* registry,
//                  uint32_t name,
//                  const std::string& interface,
//                  uint32_t version)
// - must bind the Wayland object and store it in the connection.
template <typename T>
class GlobalObjectRegistrar {
 public:
  GlobalObjectRegistrar() {
    [[maybe_unused]] GlobalObjectFactory Instantiate = T::Instantiate;
  }
};

struct Deleter {
  template <typename T>
  void operator()(T* obj) {
    ObjectTraits<T>::deleter(obj);
  }
};

template <typename T>
class Object : public std::unique_ptr<T, Deleter> {
 public:
  Object() {}
  explicit Object(T* obj) : std::unique_ptr<T, Deleter>(obj) {}

  uint32_t id() const {
    return wl_proxy_get_id(
        reinterpret_cast<wl_proxy*>(std::unique_ptr<T, Deleter>::get()));
  }
};

template <typename T>
wl::Object<T> Bind(wl_registry* registry, uint32_t name, uint32_t version) {
  DCHECK(ObjectTraits<T>::interface);
  return wl::Object<T>(wl::bind_registry<T>(
      registry, name, ObjectTraits<T>::interface, version));
}

template <>
struct ObjectTraits<wl_proxy> {
  // Interface is null for proxy.
  static const wl_interface* interface;
  static void (*deleter)(void*);
};

// Checks the given |available_version| exposed by the server against
// |min_version| and |max_version| supported by the client.
// Returns false (with rendering a warning) if |available_version| is less than
// the minimum supported version.
// Returns true otherwise, renders an info message if |available_version| is
// greater than the maximum supported one.
bool CanBind(const std::string& interface,
             uint32_t available_version,
             uint32_t min_version,
             uint32_t max_version);

}  // namespace wl

// Puts the forward declaration for struct TYPE and declares the template
// specialisation of ObjectTraits<TYPE>.
#define DECLARE_WAYLAND_OBJECT_TRAITS(TYPE) \
  struct TYPE;                              \
  namespace wl {                            \
  template <>                               \
  struct ObjectTraits<TYPE> {               \
    static const wl_interface* interface;   \
    static void (*deleter)(TYPE*);          \
  };                                        \
  }  // namespace wl

// For convenience, keep aphabetical order in this list.
DECLARE_WAYLAND_OBJECT_TRAITS(augmented_surface)
DECLARE_WAYLAND_OBJECT_TRAITS(augmented_sub_surface)
DECLARE_WAYLAND_OBJECT_TRAITS(gtk_primary_selection_device)
DECLARE_WAYLAND_OBJECT_TRAITS(gtk_primary_selection_device_manager)
DECLARE_WAYLAND_OBJECT_TRAITS(gtk_primary_selection_offer)
DECLARE_WAYLAND_OBJECT_TRAITS(gtk_primary_selection_source)
DECLARE_WAYLAND_OBJECT_TRAITS(gtk_shell1)
DECLARE_WAYLAND_OBJECT_TRAITS(gtk_surface1)
DECLARE_WAYLAND_OBJECT_TRAITS(org_kde_kwin_idle)
DECLARE_WAYLAND_OBJECT_TRAITS(org_kde_kwin_idle_timeout)
DECLARE_WAYLAND_OBJECT_TRAITS(overlay_prioritizer)
DECLARE_WAYLAND_OBJECT_TRAITS(overlay_prioritized_surface)
DECLARE_WAYLAND_OBJECT_TRAITS(surface_augmenter)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_buffer)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_callback)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_compositor)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_cursor_theme)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_data_device_manager)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_data_device)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_data_offer)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_data_source)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_display)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_drm)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_event_queue)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_keyboard)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_output)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_pointer)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_registry)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_region)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_seat)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_shm)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_shm_pool)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_subcompositor)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_subsurface)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_surface)
DECLARE_WAYLAND_OBJECT_TRAITS(wl_touch)
DECLARE_WAYLAND_OBJECT_TRAITS(wp_presentation)
DECLARE_WAYLAND_OBJECT_TRAITS(wp_presentation_feedback)
DECLARE_WAYLAND_OBJECT_TRAITS(wp_single_pixel_buffer_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(wp_viewport)
DECLARE_WAYLAND_OBJECT_TRAITS(wp_viewporter)
DECLARE_WAYLAND_OBJECT_TRAITS(wp_content_type_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(wp_content_type_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(wp_cursor_shape_device_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(wp_cursor_shape_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(wp_fractional_scale_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(wp_fractional_scale_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(xdg_activation_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(xdg_activation_token_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(xdg_popup)
DECLARE_WAYLAND_OBJECT_TRAITS(xdg_positioner)
DECLARE_WAYLAND_OBJECT_TRAITS(xdg_surface)
DECLARE_WAYLAND_OBJECT_TRAITS(xdg_toplevel)
DECLARE_WAYLAND_OBJECT_TRAITS(xdg_toplevel_drag_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(xdg_toplevel_drag_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(xdg_toplevel_icon_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(xdg_toplevel_icon_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(xdg_wm_base)
DECLARE_WAYLAND_OBJECT_TRAITS(zaura_output)
DECLARE_WAYLAND_OBJECT_TRAITS(zaura_output_manager)
DECLARE_WAYLAND_OBJECT_TRAITS(zaura_output_manager_v2)
DECLARE_WAYLAND_OBJECT_TRAITS(zaura_shell)
DECLARE_WAYLAND_OBJECT_TRAITS(zaura_surface)
DECLARE_WAYLAND_OBJECT_TRAITS(zaura_toplevel)
DECLARE_WAYLAND_OBJECT_TRAITS(zaura_popup)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_cursor_shapes_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_color_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_color_management_output_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_color_management_surface_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_color_space_creator_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_color_space_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_blending_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_alpha_compositing_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_keyboard_extension_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_extended_keyboard_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_extended_drag_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_extended_drag_source_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_extended_drag_offer_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_extended_text_input_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_pointer_stylus_v2)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_touch_stylus_v2)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_stylus_v2)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_text_input_extension_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zcr_touchpad_haptics_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_idle_inhibit_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_idle_inhibitor_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_keyboard_shortcuts_inhibit_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_keyboard_shortcuts_inhibitor_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_linux_buffer_release_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_linux_buffer_params_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_linux_dmabuf_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_linux_explicit_synchronization_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_linux_surface_synchronization_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_locked_pointer_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_pointer_constraints_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_pointer_gesture_pinch_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_pointer_gesture_hold_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_pointer_gestures_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_primary_selection_device_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_primary_selection_device_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_primary_selection_offer_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_primary_selection_source_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_relative_pointer_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_relative_pointer_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_text_input_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_text_input_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_text_input_manager_v3)
DECLARE_WAYLAND_OBJECT_TRAITS(zwp_text_input_v3)
DECLARE_WAYLAND_OBJECT_TRAITS(zxdg_decoration_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zxdg_exporter_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zxdg_exported_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zxdg_exporter_v2)
DECLARE_WAYLAND_OBJECT_TRAITS(zxdg_exported_v2)
DECLARE_WAYLAND_OBJECT_TRAITS(zxdg_output_manager_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zxdg_output_v1)
DECLARE_WAYLAND_OBJECT_TRAITS(zxdg_toplevel_decoration_v1)

#endif  // UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_OBJECT_H_
