// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_object.h"

#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <text-input-unstable-v1-client-protocol.h>
#include <wayland-client.h>
#include <xdg-shell-unstable-v5-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

namespace wl {
namespace {

void delete_keyboard(wl_keyboard* keyboard) {
  if (wl_keyboard_get_version(keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION)
    wl_keyboard_release(keyboard);
  else
    wl_keyboard_destroy(keyboard);
}

void delete_pointer(wl_pointer* pointer) {
  if (wl_pointer_get_version(pointer) >= WL_POINTER_RELEASE_SINCE_VERSION)
    wl_pointer_release(pointer);
  else
    wl_pointer_destroy(pointer);
}

void delete_seat(wl_seat* seat) {
  if (wl_seat_get_version(seat) >= WL_SEAT_RELEASE_SINCE_VERSION)
    wl_seat_release(seat);
  else
    wl_seat_destroy(seat);
}

void delete_touch(wl_touch* touch) {
  if (wl_touch_get_version(touch) >= WL_TOUCH_RELEASE_SINCE_VERSION)
    wl_touch_release(touch);
  else
    wl_touch_destroy(touch);
}

void delete_data_device(wl_data_device* data_device) {
  if (wl_data_device_get_version(data_device) >=
      WL_DATA_DEVICE_RELEASE_SINCE_VERSION) {
    wl_data_device_release(data_device);
  } else {
    wl_data_device_destroy(data_device);
  }
}

}  // namespace

const wl_interface* ObjectTraits<wl_buffer>::interface = &wl_buffer_interface;
void (*ObjectTraits<wl_buffer>::deleter)(wl_buffer*) = &wl_buffer_destroy;

const wl_interface* ObjectTraits<wl_callback>::interface =
    &wl_callback_interface;
void (*ObjectTraits<wl_callback>::deleter)(wl_callback*) = &wl_callback_destroy;

const wl_interface* ObjectTraits<wl_compositor>::interface =
    &wl_compositor_interface;
void (*ObjectTraits<wl_compositor>::deleter)(wl_compositor*) =
    &wl_compositor_destroy;

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

const wl_interface* ObjectTraits<xdg_shell>::interface = &xdg_shell_interface;
void (*ObjectTraits<xdg_shell>::deleter)(xdg_shell*) = &xdg_shell_destroy;

const wl_interface* ObjectTraits<xdg_surface>::interface =
    &xdg_surface_interface;
void (*ObjectTraits<xdg_surface>::deleter)(xdg_surface*) = &xdg_surface_destroy;

const wl_interface* ObjectTraits<xdg_popup>::interface = &xdg_popup_interface;
void (*ObjectTraits<xdg_popup>::deleter)(xdg_popup*) = &xdg_popup_destroy;

const wl_interface* ObjectTraits<zwp_linux_dmabuf_v1>::interface =
    &zwp_linux_dmabuf_v1_interface;
void (*ObjectTraits<zwp_linux_dmabuf_v1>::deleter)(zwp_linux_dmabuf_v1*) =
    &zwp_linux_dmabuf_v1_destroy;

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

}  // namespace wl
