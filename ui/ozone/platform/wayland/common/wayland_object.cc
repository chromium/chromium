// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/common/wayland_object.h"

#include <aura-shell-client-protocol.h>
#include <cursor-shapes-unstable-v1-client-protocol.h>
#include <extended-drag-unstable-v1-client-protocol.h>
#include <gtk-primary-selection-client-protocol.h>
#include <keyboard-extension-unstable-v1-client-protocol.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <linux-explicit-synchronization-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <primary-selection-unstable-v1-client-protocol.h>
#include <text-input-unstable-v1-client-protocol.h>
#include <viewporter-client-protocol.h>
#include <wayland-cursor.h>
#include <wayland-drm-client-protocol.h>
#include <xdg-decoration-unstable-v1-client-protocol.h>
#include <xdg-foreign-unstable-v1-client-protocol.h>
#include <xdg-shell-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

namespace wl {
namespace {

void delete_keyboard(wl_keyboard* keyboard) {
  if (wl::get_version_of_object(keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION)
    wl_keyboard_release(keyboard);
  else
    wl_keyboard_destroy(keyboard);
}

void delete_pointer(wl_pointer* pointer) {
  if (wl::get_version_of_object(pointer) >= WL_POINTER_RELEASE_SINCE_VERSION)
    wl_pointer_release(pointer);
  else
    wl_pointer_destroy(pointer);
}

void delete_seat(wl_seat* seat) {
  if (wl::get_version_of_object(seat) >= WL_SEAT_RELEASE_SINCE_VERSION)
    wl_seat_release(seat);
  else
    wl_seat_destroy(seat);
}

void delete_touch(wl_touch* touch) {
  if (wl::get_version_of_object(touch) >= WL_TOUCH_RELEASE_SINCE_VERSION)
    wl_touch_release(touch);
  else
    wl_touch_destroy(touch);
}

void delete_data_device(wl_data_device* data_device) {
  if (wl::get_version_of_object(data_device) >=
      WL_DATA_DEVICE_RELEASE_SINCE_VERSION) {
    wl_data_device_release(data_device);
  } else {
    wl_data_device_destroy(data_device);
  }
}

}  // namespace

const wl_interface* ObjectTraits<zxdg_decoration_manager_v1>::interface =
    &zxdg_decoration_manager_v1_interface;
void (*ObjectTraits<zxdg_decoration_manager_v1>::deleter)(
    zxdg_decoration_manager_v1*) = &zxdg_decoration_manager_v1_destroy;

const wl_interface* ObjectTraits<zxdg_toplevel_decoration_v1>::interface =
    &zxdg_toplevel_decoration_v1_interface;
void (*ObjectTraits<zxdg_toplevel_decoration_v1>::deleter)(
    zxdg_toplevel_decoration_v1*) = &zxdg_toplevel_decoration_v1_destroy;

const wl_interface*
    ObjectTraits<gtk_primary_selection_device_manager>::interface =
        &gtk_primary_selection_device_manager_interface;
void (*ObjectTraits<gtk_primary_selection_device_manager>::deleter)(
    gtk_primary_selection_device_manager*) =
    &gtk_primary_selection_device_manager_destroy;

const wl_interface* ObjectTraits<gtk_primary_selection_device>::interface =
    &gtk_primary_selection_device_interface;
void (*ObjectTraits<gtk_primary_selection_device>::deleter)(
    gtk_primary_selection_device*) = &gtk_primary_selection_device_destroy;

const wl_interface* ObjectTraits<gtk_primary_selection_offer>::interface =
    &gtk_primary_selection_offer_interface;
void (*ObjectTraits<gtk_primary_selection_offer>::deleter)(
    gtk_primary_selection_offer*) = &gtk_primary_selection_offer_destroy;

const wl_interface* ObjectTraits<gtk_primary_selection_source>::interface =
    &gtk_primary_selection_source_interface;
void (*ObjectTraits<gtk_primary_selection_source>::deleter)(
    gtk_primary_selection_source*) = &gtk_primary_selection_source_destroy;

const wl_interface*
    ObjectTraits<zwp_primary_selection_device_manager_v1>::interface =
        &zwp_primary_selection_device_manager_v1_interface;
void (*ObjectTraits<zwp_primary_selection_device_manager_v1>::deleter)(
    zwp_primary_selection_device_manager_v1*) =
    &zwp_primary_selection_device_manager_v1_destroy;

const wl_interface* ObjectTraits<zwp_primary_selection_device_v1>::interface =
    &zwp_primary_selection_device_v1_interface;
void (*ObjectTraits<zwp_primary_selection_device_v1>::deleter)(
    zwp_primary_selection_device_v1*) =
    &zwp_primary_selection_device_v1_destroy;

const wl_interface* ObjectTraits<zwp_primary_selection_offer_v1>::interface =
    &zwp_primary_selection_offer_v1_interface;
void (*ObjectTraits<zwp_primary_selection_offer_v1>::deleter)(
    zwp_primary_selection_offer_v1*) = &zwp_primary_selection_offer_v1_destroy;

const wl_interface* ObjectTraits<zwp_primary_selection_source_v1>::interface =
    &zwp_primary_selection_source_v1_interface;
void (*ObjectTraits<zwp_primary_selection_source_v1>::deleter)(
    zwp_primary_selection_source_v1*) =
    &zwp_primary_selection_source_v1_destroy;

const wl_interface* ObjectTraits<wl_buffer>::interface = &wl_buffer_interface;
void (*ObjectTraits<wl_buffer>::deleter)(wl_buffer*) = &wl_buffer_destroy;

const wl_interface* ObjectTraits<wl_callback>::interface =
    &wl_callback_interface;
void (*ObjectTraits<wl_callback>::deleter)(wl_callback*) = &wl_callback_destroy;

const wl_interface* ObjectTraits<wl_compositor>::interface =
    &wl_compositor_interface;
void (*ObjectTraits<wl_compositor>::deleter)(wl_compositor*) =
    &wl_compositor_destroy;

void (*ObjectTraits<wl_cursor_theme>::deleter)(wl_cursor_theme*) =
    &wl_cursor_theme_destroy;

const wl_interface* ObjectTraits<wl_data_device_manager>::interface =
    &wl_data_device_manager_interface;
void (*ObjectTraits<wl_data_device_manager>::deleter)(wl_data_device_manager*) =
    &wl_data_device_manager_destroy;

const wl_interface* ObjectTraits<wl_data_device>::interface =
    &wl_data_device_interface;
void (*ObjectTraits<wl_data_device>::deleter)(wl_data_device*) =
    &delete_data_device;

const wl_interface* ObjectTraits<wl_data_offer>::interface =
    &wl_data_offer_interface;
void (*ObjectTraits<wl_data_offer>::deleter)(wl_data_offer*) =
    &wl_data_offer_destroy;

const wl_interface* ObjectTraits<wl_data_source>::interface =
    &wl_data_source_interface;
void (*ObjectTraits<wl_data_source>::deleter)(wl_data_source*) =
    &wl_data_source_destroy;

const wl_interface* ObjectTraits<wl_drm>::interface = &wl_drm_interface;
void (*ObjectTraits<wl_drm>::deleter)(wl_drm*) = &wl_drm_destroy;

const wl_interface* ObjectTraits<wl_display>::interface = &wl_display_interface;
void (*ObjectTraits<wl_display>::deleter)(wl_display*) = &wl_display_disconnect;

const wl_interface* ObjectTraits<wl_output>::interface = &wl_output_interface;
void (*ObjectTraits<wl_output>::deleter)(wl_output*) = &wl_output_destroy;

const wl_interface* ObjectTraits<wl_keyboard>::interface =
    &wl_keyboard_interface;
void (*ObjectTraits<wl_keyboard>::deleter)(wl_keyboard*) = &delete_keyboard;

const wl_interface* ObjectTraits<wl_pointer>::interface = &wl_pointer_interface;
void (*ObjectTraits<wl_pointer>::deleter)(wl_pointer*) = &delete_pointer;

const wl_interface* ObjectTraits<wl_registry>::interface =
    &wl_registry_interface;
void (*ObjectTraits<wl_registry>::deleter)(wl_registry*) = &wl_registry_destroy;

const wl_interface* ObjectTraits<wl_region>::interface = &wl_region_interface;
void (*ObjectTraits<wl_region>::deleter)(wl_region*) = &wl_region_destroy;

const wl_interface* ObjectTraits<wl_seat>::interface = &wl_seat_interface;
void (*ObjectTraits<wl_seat>::deleter)(wl_seat*) = &delete_seat;

const wl_interface* ObjectTraits<wl_shm>::interface = &wl_shm_interface;
void (*ObjectTraits<wl_shm>::deleter)(wl_shm*) = &wl_shm_destroy;

const wl_interface* ObjectTraits<wl_shm_pool>::interface =
    &wl_shm_pool_interface;
void (*ObjectTraits<wl_shm_pool>::deleter)(wl_shm_pool*) = &wl_shm_pool_destroy;

const wl_interface* ObjectTraits<wl_surface>::interface = &wl_surface_interface;
void (*ObjectTraits<wl_surface>::deleter)(wl_surface*) = &wl_surface_destroy;

const wl_interface* ObjectTraits<wl_subcompositor>::interface =
    &wl_subcompositor_interface;
void (*ObjectTraits<wl_subcompositor>::deleter)(wl_subcompositor*) =
    &wl_subcompositor_destroy;

const wl_interface* ObjectTraits<wl_subsurface>::interface =
    &wl_subsurface_interface;
void (*ObjectTraits<wl_subsurface>::deleter)(wl_subsurface*) =
    &wl_subsurface_destroy;

const wl_interface* ObjectTraits<wl_touch>::interface = &wl_touch_interface;
void (*ObjectTraits<wl_touch>::deleter)(wl_touch*) = &delete_touch;

const wl_interface* ObjectTraits<wp_presentation>::interface =
    &wp_presentation_interface;
void (*ObjectTraits<wp_presentation>::deleter)(wp_presentation*) =
    &wp_presentation_destroy;

const wl_interface* ObjectTraits<struct wp_presentation_feedback>::interface =
    &wp_presentation_feedback_interface;
void (*ObjectTraits<struct wp_presentation_feedback>::deleter)(
    struct wp_presentation_feedback*) = &wp_presentation_feedback_destroy;

const wl_interface* ObjectTraits<wp_viewport>::interface =
    &wp_viewport_interface;
void (*ObjectTraits<wp_viewport>::deleter)(wp_viewport*) = &wp_viewport_destroy;

const wl_interface* ObjectTraits<wp_viewporter>::interface =
    &wp_viewporter_interface;
void (*ObjectTraits<wp_viewporter>::deleter)(wp_viewporter*) =
    &wp_viewporter_destroy;

const wl_interface* ObjectTraits<xdg_wm_base>::interface =
    &xdg_wm_base_interface;
void (*ObjectTraits<xdg_wm_base>::deleter)(xdg_wm_base*) = &xdg_wm_base_destroy;

const wl_interface* ObjectTraits<xdg_surface>::interface =
    &xdg_surface_interface;
void (*ObjectTraits<xdg_surface>::deleter)(xdg_surface*) = &xdg_surface_destroy;

const wl_interface* ObjectTraits<xdg_toplevel>::interface =
    &xdg_toplevel_interface;
void (*ObjectTraits<xdg_toplevel>::deleter)(xdg_toplevel*) =
    &xdg_toplevel_destroy;

const wl_interface* ObjectTraits<xdg_popup>::interface = &xdg_popup_interface;
void (*ObjectTraits<xdg_popup>::deleter)(xdg_popup*) = &xdg_popup_destroy;

const wl_interface* ObjectTraits<xdg_positioner>::interface =
    &xdg_positioner_interface;
void (*ObjectTraits<xdg_positioner>::deleter)(xdg_positioner*) =
    &xdg_positioner_destroy;

const wl_interface* ObjectTraits<zaura_shell>::interface =
    &zaura_shell_interface;
void (*ObjectTraits<zaura_shell>::deleter)(zaura_shell*) = &zaura_shell_destroy;

const wl_interface* ObjectTraits<zaura_surface>::interface =
    &zaura_surface_interface;
void (*ObjectTraits<zaura_surface>::deleter)(zaura_surface*) =
    &zaura_surface_destroy;

const wl_interface* ObjectTraits<zcr_cursor_shapes_v1>::interface =
    &zcr_cursor_shapes_v1_interface;
void (*ObjectTraits<zcr_cursor_shapes_v1>::deleter)(zcr_cursor_shapes_v1*) =
    &zcr_cursor_shapes_v1_destroy;

const wl_interface* ObjectTraits<zcr_extended_drag_v1>::interface =
    &zcr_extended_drag_v1_interface;
void (*ObjectTraits<zcr_extended_drag_v1>::deleter)(zcr_extended_drag_v1*) =
    &zcr_extended_drag_v1_destroy;

const wl_interface* ObjectTraits<zcr_extended_drag_source_v1>::interface =
    &zcr_extended_drag_source_v1_interface;
void (*ObjectTraits<zcr_extended_drag_source_v1>::deleter)(
    zcr_extended_drag_source_v1*) = &zcr_extended_drag_source_v1_destroy;

const wl_interface* ObjectTraits<zcr_extended_drag_offer_v1>::interface =
    &zcr_extended_drag_offer_v1_interface;
void (*ObjectTraits<zcr_extended_drag_offer_v1>::deleter)(
    zcr_extended_drag_offer_v1*) = &zcr_extended_drag_offer_v1_destroy;

const wl_interface* ObjectTraits<zcr_keyboard_extension_v1>::interface =
    &zcr_keyboard_extension_v1_interface;
void (*ObjectTraits<zcr_keyboard_extension_v1>::deleter)(
    zcr_keyboard_extension_v1*) = &zcr_keyboard_extension_v1_destroy;

const wl_interface* ObjectTraits<zcr_extended_keyboard_v1>::interface =
    &zcr_extended_keyboard_v1_interface;
void (*ObjectTraits<zcr_extended_keyboard_v1>::deleter)(
    zcr_extended_keyboard_v1*) = &zcr_extended_keyboard_v1_destroy;

const wl_interface* ObjectTraits<zwp_linux_dmabuf_v1>::interface =
    &zwp_linux_dmabuf_v1_interface;
void (*ObjectTraits<zwp_linux_dmabuf_v1>::deleter)(zwp_linux_dmabuf_v1*) =
    &zwp_linux_dmabuf_v1_destroy;

const wl_interface* ObjectTraits<zwp_linux_buffer_release_v1>::interface =
    &zwp_linux_buffer_release_v1_interface;
void (*ObjectTraits<zwp_linux_buffer_release_v1>::deleter)(
    zwp_linux_buffer_release_v1*) = &zwp_linux_buffer_release_v1_destroy;

const wl_interface*
    ObjectTraits<zwp_linux_explicit_synchronization_v1>::interface =
        &zwp_linux_explicit_synchronization_v1_interface;
void (*ObjectTraits<zwp_linux_explicit_synchronization_v1>::deleter)(
    zwp_linux_explicit_synchronization_v1*) =
    &zwp_linux_explicit_synchronization_v1_destroy;

const wl_interface*
    ObjectTraits<zwp_linux_surface_synchronization_v1>::interface =
        &zwp_linux_surface_synchronization_v1_interface;
void (*ObjectTraits<zwp_linux_surface_synchronization_v1>::deleter)(
    zwp_linux_surface_synchronization_v1*) =
    &zwp_linux_surface_synchronization_v1_destroy;

const wl_interface* ObjectTraits<zxdg_shell_v6>::interface =
    &zxdg_shell_v6_interface;
void (*ObjectTraits<zxdg_shell_v6>::deleter)(zxdg_shell_v6*) =
    &zxdg_shell_v6_destroy;

const wl_interface* ObjectTraits<zxdg_surface_v6>::interface =
    &zxdg_surface_v6_interface;
void (*ObjectTraits<zxdg_surface_v6>::deleter)(zxdg_surface_v6*) =
    &zxdg_surface_v6_destroy;

const wl_interface* ObjectTraits<zxdg_toplevel_v6>::interface =
    &zxdg_toplevel_v6_interface;
void (*ObjectTraits<zxdg_toplevel_v6>::deleter)(zxdg_toplevel_v6*) =
    &zxdg_toplevel_v6_destroy;

const wl_interface* ObjectTraits<zxdg_popup_v6>::interface =
    &zxdg_popup_v6_interface;
void (*ObjectTraits<zxdg_popup_v6>::deleter)(zxdg_popup_v6*) =
    &zxdg_popup_v6_destroy;

const wl_interface* ObjectTraits<zxdg_positioner_v6>::interface =
    &zxdg_positioner_v6_interface;
void (*ObjectTraits<zxdg_positioner_v6>::deleter)(zxdg_positioner_v6*) =
    &zxdg_positioner_v6_destroy;

const wl_interface* ObjectTraits<zwp_text_input_manager_v1>::interface =
    &zwp_text_input_manager_v1_interface;
void (*ObjectTraits<zwp_text_input_manager_v1>::deleter)(
    zwp_text_input_manager_v1*) = &zwp_text_input_manager_v1_destroy;

const wl_interface* ObjectTraits<zwp_text_input_v1>::interface =
    &zwp_text_input_v1_interface;
void (*ObjectTraits<zwp_text_input_v1>::deleter)(zwp_text_input_v1*) =
    &zwp_text_input_v1_destroy;

const wl_interface* ObjectTraits<zxdg_exporter_v1>::interface =
    &zxdg_exporter_v1_interface;
void (*ObjectTraits<zxdg_exporter_v1>::deleter)(zxdg_exporter_v1*) =
    &zxdg_exporter_v1_destroy;

const wl_interface* ObjectTraits<zxdg_exported_v1>::interface =
    &zxdg_exported_v1_interface;
void (*ObjectTraits<zxdg_exported_v1>::deleter)(zxdg_exported_v1*) =
    &zxdg_exported_v1_destroy;

}  // namespace wl
