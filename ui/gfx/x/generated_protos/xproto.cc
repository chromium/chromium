// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    dri3 \
//    glx \
//    randr \
//    render \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xfixes \
//    xinput \
//    xkb \
//    xproto \
//    xtest

#include "xproto.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

XProto::XProto(Connection* connection) : connection_(connection) {}

template <>
COMPONENT_EXPORT(X11)
Setup Read<Setup>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  Setup obj;

  auto& status = obj.status;
  auto& protocol_major_version = obj.protocol_major_version;
  auto& protocol_minor_version = obj.protocol_minor_version;
  auto& length = obj.length;
  auto& release_number = obj.release_number;
  auto& resource_id_base = obj.resource_id_base;
  auto& resource_id_mask = obj.resource_id_mask;
  auto& motion_buffer_size = obj.motion_buffer_size;
  uint16_t vendor_len{};
  auto& maximum_request_length = obj.maximum_request_length;
  uint8_t roots_len{};
  uint8_t pixmap_formats_len{};
  auto& image_byte_order = obj.image_byte_order;
  auto& bitmap_format_bit_order = obj.bitmap_format_bit_order;
  auto& bitmap_format_scanline_unit = obj.bitmap_format_scanline_unit;
  auto& bitmap_format_scanline_pad = obj.bitmap_format_scanline_pad;
  auto& min_keycode = obj.min_keycode;
  auto& max_keycode = obj.max_keycode;
  auto& vendor = obj.vendor;
  auto& pixmap_formats = obj.pixmap_formats;
  auto& roots = obj.roots;

  // status
  Read(&status, &buf);

  // pad0
  Pad(&buf, 1);

  // protocol_major_version
  Read(&protocol_major_version, &buf);

  // protocol_minor_version
  Read(&protocol_minor_version, &buf);

  // length
  Read(&length, &buf);

  // release_number
  Read(&release_number, &buf);

  // resource_id_base
  Read(&resource_id_base, &buf);

  // resource_id_mask
  Read(&resource_id_mask, &buf);

  // motion_buffer_size
  Read(&motion_buffer_size, &buf);

  // vendor_len
  Read(&vendor_len, &buf);

  // maximum_request_length
  Read(&maximum_request_length, &buf);

  // roots_len
  Read(&roots_len, &buf);

  // pixmap_formats_len
  Read(&pixmap_formats_len, &buf);

  // image_byte_order
  uint8_t tmp0;
  Read(&tmp0, &buf);
  image_byte_order = static_cast<ImageOrder>(tmp0);

  // bitmap_format_bit_order
  uint8_t tmp1;
  Read(&tmp1, &buf);
  bitmap_format_bit_order = static_cast<ImageOrder>(tmp1);

  // bitmap_format_scanline_unit
  Read(&bitmap_format_scanline_unit, &buf);

  // bitmap_format_scanline_pad
  Read(&bitmap_format_scanline_pad, &buf);

  // min_keycode
  Read(&min_keycode, &buf);

  // max_keycode
  Read(&max_keycode, &buf);

  // pad1
  Pad(&buf, 4);

  // vendor
  vendor.resize(vendor_len);
  for (auto& vendor_elem : vendor) {
    // vendor_elem
    Read(&vendor_elem, &buf);
  }

  // pad2
  Align(&buf, 4);

  // pixmap_formats
  pixmap_formats.resize(pixmap_formats_len);
  for (auto& pixmap_formats_elem : pixmap_formats) {
    // pixmap_formats_elem
    {
      auto& depth = pixmap_formats_elem.depth;
      auto& bits_per_pixel = pixmap_formats_elem.bits_per_pixel;
      auto& scanline_pad = pixmap_formats_elem.scanline_pad;

      // depth
      Read(&depth, &buf);

      // bits_per_pixel
      Read(&bits_per_pixel, &buf);

      // scanline_pad
      Read(&scanline_pad, &buf);

      // pad0
      Pad(&buf, 5);
    }
  }

  // roots
  roots.resize(roots_len);
  for (auto& roots_elem : roots) {
    // roots_elem
    {
      auto& root = roots_elem.root;
      auto& default_colormap = roots_elem.default_colormap;
      auto& white_pixel = roots_elem.white_pixel;
      auto& black_pixel = roots_elem.black_pixel;
      auto& current_input_masks = roots_elem.current_input_masks;
      auto& width_in_pixels = roots_elem.width_in_pixels;
      auto& height_in_pixels = roots_elem.height_in_pixels;
      auto& width_in_millimeters = roots_elem.width_in_millimeters;
      auto& height_in_millimeters = roots_elem.height_in_millimeters;
      auto& min_installed_maps = roots_elem.min_installed_maps;
      auto& max_installed_maps = roots_elem.max_installed_maps;
      auto& root_visual = roots_elem.root_visual;
      auto& backing_stores = roots_elem.backing_stores;
      auto& save_unders = roots_elem.save_unders;
      auto& root_depth = roots_elem.root_depth;
      uint8_t allowed_depths_len{};
      auto& allowed_depths = roots_elem.allowed_depths;

      // root
      Read(&root, &buf);

      // default_colormap
      Read(&default_colormap, &buf);

      // white_pixel
      Read(&white_pixel, &buf);

      // black_pixel
      Read(&black_pixel, &buf);

      // current_input_masks
      uint32_t tmp2;
      Read(&tmp2, &buf);
      current_input_masks = static_cast<EventMask>(tmp2);

      // width_in_pixels
      Read(&width_in_pixels, &buf);

      // height_in_pixels
      Read(&height_in_pixels, &buf);

      // width_in_millimeters
      Read(&width_in_millimeters, &buf);

      // height_in_millimeters
      Read(&height_in_millimeters, &buf);

      // min_installed_maps
      Read(&min_installed_maps, &buf);

      // max_installed_maps
      Read(&max_installed_maps, &buf);

      // root_visual
      Read(&root_visual, &buf);

      // backing_stores
      uint8_t tmp3;
      Read(&tmp3, &buf);
      backing_stores = static_cast<BackingStore>(tmp3);

      // save_unders
      Read(&save_unders, &buf);

      // root_depth
      Read(&root_depth, &buf);

      // allowed_depths_len
      Read(&allowed_depths_len, &buf);

      // allowed_depths
      allowed_depths.resize(allowed_depths_len);
      for (auto& allowed_depths_elem : allowed_depths) {
        // allowed_depths_elem
        {
          auto& depth = allowed_depths_elem.depth;
          uint16_t visuals_len{};
          auto& visuals = allowed_depths_elem.visuals;

          // depth
          Read(&depth, &buf);

          // pad0
          Pad(&buf, 1);

          // visuals_len
          Read(&visuals_len, &buf);

          // pad1
          Pad(&buf, 4);

          // visuals
          visuals.resize(visuals_len);
          for (auto& visuals_elem : visuals) {
            // visuals_elem
            {
              auto& visual_id = visuals_elem.visual_id;
              auto& c_class = visuals_elem.c_class;
              auto& bits_per_rgb_value = visuals_elem.bits_per_rgb_value;
              auto& colormap_entries = visuals_elem.colormap_entries;
              auto& red_mask = visuals_elem.red_mask;
              auto& green_mask = visuals_elem.green_mask;
              auto& blue_mask = visuals_elem.blue_mask;

              // visual_id
              Read(&visual_id, &buf);

              // c_class
              uint8_t tmp4;
              Read(&tmp4, &buf);
              c_class = static_cast<VisualClass>(tmp4);

              // bits_per_rgb_value
              Read(&bits_per_rgb_value, &buf);

              // colormap_entries
              Read(&colormap_entries, &buf);

              // red_mask
              Read(&red_mask, &buf);

              // green_mask
              Read(&green_mask, &buf);

              // blue_mask
              Read(&blue_mask, &buf);

              // pad0
              Pad(&buf, 4);
            }
          }
        }
      }
    }
  }

  return obj;
}

template <>
COMPONENT_EXPORT(X11)
WriteBuffer Write<KeyEvent>(const KeyEvent& obj) {
  WriteBuffer buf;

  auto& detail = obj.detail;
  auto& sequence = obj.sequence;
  auto& time = obj.time;
  auto& root = obj.root;
  auto& event = obj.event;
  auto& child = obj.child;
  auto& root_x = obj.root_x;
  auto& root_y = obj.root_y;
  auto& event_x = obj.event_x;
  auto& event_y = obj.event_y;
  auto& state = obj.state;
  auto& same_screen = obj.same_screen;

  // response_type
  uint8_t response_type = obj.opcode;
  buf.Write(&response_type);

  // detail
  buf.Write(&detail);

  // sequence
  buf.Write(&sequence);

  // time
  buf.Write(&time);

  // root
  buf.Write(&root);

  // event
  buf.Write(&event);

  // child
  buf.Write(&child);

  // root_x
  buf.Write(&root_x);

  // root_y
  buf.Write(&root_y);

  // event_x
  buf.Write(&event_x);

  // event_y
  buf.Write(&event_y);

  // state
  uint16_t tmp5;
  tmp5 = static_cast<uint16_t>(state);
  buf.Write(&tmp5);

  // same_screen
  buf.Write(&same_screen);

  // pad0
  Pad(&buf, 1);

  return buf;
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<KeyEvent>(KeyEvent* event_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& detail = (*event_).detail;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& root = (*event_).root;
  auto& event = (*event_).event;
  auto& child = (*event_).child;
  auto& root_x = (*event_).root_x;
  auto& root_y = (*event_).root_y;
  auto& event_x = (*event_).event_x;
  auto& event_y = (*event_).event_y;
  auto& state = (*event_).state;
  auto& same_screen = (*event_).same_screen;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // detail
  Read(&detail, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // root
  Read(&root, &buf);

  // event
  Read(&event, &buf);

  // child
  Read(&child, &buf);

  // root_x
  Read(&root_x, &buf);

  // root_y
  Read(&root_y, &buf);

  // event_x
  Read(&event_x, &buf);

  // event_y
  Read(&event_y, &buf);

  // state
  uint16_t tmp6;
  Read(&tmp6, &buf);
  state = static_cast<KeyButMask>(tmp6);

  // same_screen
  Read(&same_screen, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
WriteBuffer Write<ButtonEvent>(const ButtonEvent& obj) {
  WriteBuffer buf;

  auto& detail = obj.detail;
  auto& sequence = obj.sequence;
  auto& time = obj.time;
  auto& root = obj.root;
  auto& event = obj.event;
  auto& child = obj.child;
  auto& root_x = obj.root_x;
  auto& root_y = obj.root_y;
  auto& event_x = obj.event_x;
  auto& event_y = obj.event_y;
  auto& state = obj.state;
  auto& same_screen = obj.same_screen;

  // response_type
  uint8_t response_type = obj.opcode;
  buf.Write(&response_type);

  // detail
  buf.Write(&detail);

  // sequence
  buf.Write(&sequence);

  // time
  buf.Write(&time);

  // root
  buf.Write(&root);

  // event
  buf.Write(&event);

  // child
  buf.Write(&child);

  // root_x
  buf.Write(&root_x);

  // root_y
  buf.Write(&root_y);

  // event_x
  buf.Write(&event_x);

  // event_y
  buf.Write(&event_y);

  // state
  uint16_t tmp7;
  tmp7 = static_cast<uint16_t>(state);
  buf.Write(&tmp7);

  // same_screen
  buf.Write(&same_screen);

  // pad0
  Pad(&buf, 1);

  return buf;
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<ButtonEvent>(ButtonEvent* event_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& detail = (*event_).detail;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& root = (*event_).root;
  auto& event = (*event_).event;
  auto& child = (*event_).child;
  auto& root_x = (*event_).root_x;
  auto& root_y = (*event_).root_y;
  auto& event_x = (*event_).event_x;
  auto& event_y = (*event_).event_y;
  auto& state = (*event_).state;
  auto& same_screen = (*event_).same_screen;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // detail
  Read(&detail, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // root
  Read(&root, &buf);

  // event
  Read(&event, &buf);

  // child
  Read(&child, &buf);

  // root_x
  Read(&root_x, &buf);

  // root_y
  Read(&root_y, &buf);

  // event_x
  Read(&event_x, &buf);

  // event_y
  Read(&event_y, &buf);

  // state
  uint16_t tmp8;
  Read(&tmp8, &buf);
  state = static_cast<KeyButMask>(tmp8);

  // same_screen
  Read(&same_screen, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
WriteBuffer Write<MotionNotifyEvent>(const MotionNotifyEvent& obj) {
  WriteBuffer buf;

  auto& detail = obj.detail;
  auto& sequence = obj.sequence;
  auto& time = obj.time;
  auto& root = obj.root;
  auto& event = obj.event;
  auto& child = obj.child;
  auto& root_x = obj.root_x;
  auto& root_y = obj.root_y;
  auto& event_x = obj.event_x;
  auto& event_y = obj.event_y;
  auto& state = obj.state;
  auto& same_screen = obj.same_screen;

  // response_type
  uint8_t response_type = 6;
  buf.Write(&response_type);

  // detail
  uint8_t tmp9;
  tmp9 = static_cast<uint8_t>(detail);
  buf.Write(&tmp9);

  // sequence
  buf.Write(&sequence);

  // time
  buf.Write(&time);

  // root
  buf.Write(&root);

  // event
  buf.Write(&event);

  // child
  buf.Write(&child);

  // root_x
  buf.Write(&root_x);

  // root_y
  buf.Write(&root_y);

  // event_x
  buf.Write(&event_x);

  // event_y
  buf.Write(&event_y);

  // state
  uint16_t tmp10;
  tmp10 = static_cast<uint16_t>(state);
  buf.Write(&tmp10);

  // same_screen
  buf.Write(&same_screen);

  // pad0
  Pad(&buf, 1);

  return buf;
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<MotionNotifyEvent>(MotionNotifyEvent* event_,
                                  ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& detail = (*event_).detail;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& root = (*event_).root;
  auto& event = (*event_).event;
  auto& child = (*event_).child;
  auto& root_x = (*event_).root_x;
  auto& root_y = (*event_).root_y;
  auto& event_x = (*event_).event_x;
  auto& event_y = (*event_).event_y;
  auto& state = (*event_).state;
  auto& same_screen = (*event_).same_screen;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // detail
  uint8_t tmp11;
  Read(&tmp11, &buf);
  detail = static_cast<Motion>(tmp11);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // root
  Read(&root, &buf);

  // event
  Read(&event, &buf);

  // child
  Read(&child, &buf);

  // root_x
  Read(&root_x, &buf);

  // root_y
  Read(&root_y, &buf);

  // event_x
  Read(&event_x, &buf);

  // event_y
  Read(&event_y, &buf);

  // state
  uint16_t tmp12;
  Read(&tmp12, &buf);
  state = static_cast<KeyButMask>(tmp12);

  // same_screen
  Read(&same_screen, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<CrossingEvent>(CrossingEvent* event_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& detail = (*event_).detail;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& root = (*event_).root;
  auto& event = (*event_).event;
  auto& child = (*event_).child;
  auto& root_x = (*event_).root_x;
  auto& root_y = (*event_).root_y;
  auto& event_x = (*event_).event_x;
  auto& event_y = (*event_).event_y;
  auto& state = (*event_).state;
  auto& mode = (*event_).mode;
  auto& same_screen_focus = (*event_).same_screen_focus;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // detail
  uint8_t tmp13;
  Read(&tmp13, &buf);
  detail = static_cast<NotifyDetail>(tmp13);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // root
  Read(&root, &buf);

  // event
  Read(&event, &buf);

  // child
  Read(&child, &buf);

  // root_x
  Read(&root_x, &buf);

  // root_y
  Read(&root_y, &buf);

  // event_x
  Read(&event_x, &buf);

  // event_y
  Read(&event_y, &buf);

  // state
  uint16_t tmp14;
  Read(&tmp14, &buf);
  state = static_cast<KeyButMask>(tmp14);

  // mode
  uint8_t tmp15;
  Read(&tmp15, &buf);
  mode = static_cast<NotifyMode>(tmp15);

  // same_screen_focus
  Read(&same_screen_focus, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<FocusEvent>(FocusEvent* event_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& detail = (*event_).detail;
  auto& sequence = (*event_).sequence;
  auto& event = (*event_).event;
  auto& mode = (*event_).mode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // detail
  uint8_t tmp16;
  Read(&tmp16, &buf);
  detail = static_cast<NotifyDetail>(tmp16);

  // sequence
  Read(&sequence, &buf);

  // event
  Read(&event, &buf);

  // mode
  uint8_t tmp17;
  Read(&tmp17, &buf);
  mode = static_cast<NotifyMode>(tmp17);

  // pad0
  Pad(&buf, 3);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<KeymapNotifyEvent>(KeymapNotifyEvent* event_,
                                  ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& keys = (*event_).keys;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // keys
  for (auto& keys_elem : keys) {
    // keys_elem
    Read(&keys_elem, &buf);
  }

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
WriteBuffer Write<ExposeEvent>(const ExposeEvent& obj) {
  WriteBuffer buf;

  auto& sequence = obj.sequence;
  auto& window = obj.window;
  auto& x = obj.x;
  auto& y = obj.y;
  auto& width = obj.width;
  auto& height = obj.height;
  auto& count = obj.count;

  // response_type
  uint8_t response_type = 12;
  buf.Write(&response_type);

  // pad0
  Pad(&buf, 1);

  // sequence
  buf.Write(&sequence);

  // window
  buf.Write(&window);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // count
  buf.Write(&count);

  // pad1
  Pad(&buf, 2);

  return buf;
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<ExposeEvent>(ExposeEvent* event_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& window = (*event_).window;
  auto& x = (*event_).x;
  auto& y = (*event_).y;
  auto& width = (*event_).width;
  auto& height = (*event_).height;
  auto& count = (*event_).count;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // window
  Read(&window, &buf);

  // x
  Read(&x, &buf);

  // y
  Read(&y, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // count
  Read(&count, &buf);

  // pad1
  Pad(&buf, 2);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<GraphicsExposureEvent>(GraphicsExposureEvent* event_,
                                      ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& drawable = (*event_).drawable;
  auto& x = (*event_).x;
  auto& y = (*event_).y;
  auto& width = (*event_).width;
  auto& height = (*event_).height;
  auto& minor_opcode = (*event_).minor_opcode;
  auto& count = (*event_).count;
  auto& major_opcode = (*event_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // drawable
  Read(&drawable, &buf);

  // x
  Read(&x, &buf);

  // y
  Read(&y, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // count
  Read(&count, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad1
  Pad(&buf, 3);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<NoExposureEvent>(NoExposureEvent* event_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& drawable = (*event_).drawable;
  auto& minor_opcode = (*event_).minor_opcode;
  auto& major_opcode = (*event_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // drawable
  Read(&drawable, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad1
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<VisibilityNotifyEvent>(VisibilityNotifyEvent* event_,
                                      ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& window = (*event_).window;
  auto& state = (*event_).state;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // window
  Read(&window, &buf);

  // state
  uint8_t tmp18;
  Read(&tmp18, &buf);
  state = static_cast<Visibility>(tmp18);

  // pad1
  Pad(&buf, 3);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<CreateNotifyEvent>(CreateNotifyEvent* event_,
                                  ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& parent = (*event_).parent;
  auto& window = (*event_).window;
  auto& x = (*event_).x;
  auto& y = (*event_).y;
  auto& width = (*event_).width;
  auto& height = (*event_).height;
  auto& border_width = (*event_).border_width;
  auto& override_redirect = (*event_).override_redirect;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // parent
  Read(&parent, &buf);

  // window
  Read(&window, &buf);

  // x
  Read(&x, &buf);

  // y
  Read(&y, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // border_width
  Read(&border_width, &buf);

  // override_redirect
  Read(&override_redirect, &buf);

  // pad1
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<DestroyNotifyEvent>(DestroyNotifyEvent* event_,
                                   ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& event = (*event_).event;
  auto& window = (*event_).window;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // event
  Read(&event, &buf);

  // window
  Read(&window, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
WriteBuffer Write<UnmapNotifyEvent>(const UnmapNotifyEvent& obj) {
  WriteBuffer buf;

  auto& sequence = obj.sequence;
  auto& event = obj.event;
  auto& window = obj.window;
  auto& from_configure = obj.from_configure;

  // response_type
  uint8_t response_type = 18;
  buf.Write(&response_type);

  // pad0
  Pad(&buf, 1);

  // sequence
  buf.Write(&sequence);

  // event
  buf.Write(&event);

  // window
  buf.Write(&window);

  // from_configure
  buf.Write(&from_configure);

  // pad1
  Pad(&buf, 3);

  return buf;
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<UnmapNotifyEvent>(UnmapNotifyEvent* event_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& event = (*event_).event;
  auto& window = (*event_).window;
  auto& from_configure = (*event_).from_configure;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // event
  Read(&event, &buf);

  // window
  Read(&window, &buf);

  // from_configure
  Read(&from_configure, &buf);

  // pad1
  Pad(&buf, 3);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<MapNotifyEvent>(MapNotifyEvent* event_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& event = (*event_).event;
  auto& window = (*event_).window;
  auto& override_redirect = (*event_).override_redirect;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // event
  Read(&event, &buf);

  // window
  Read(&window, &buf);

  // override_redirect
  Read(&override_redirect, &buf);

  // pad1
  Pad(&buf, 3);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<MapRequestEvent>(MapRequestEvent* event_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& parent = (*event_).parent;
  auto& window = (*event_).window;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // parent
  Read(&parent, &buf);

  // window
  Read(&window, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<ReparentNotifyEvent>(ReparentNotifyEvent* event_,
                                    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& event = (*event_).event;
  auto& window = (*event_).window;
  auto& parent = (*event_).parent;
  auto& x = (*event_).x;
  auto& y = (*event_).y;
  auto& override_redirect = (*event_).override_redirect;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // event
  Read(&event, &buf);

  // window
  Read(&window, &buf);

  // parent
  Read(&parent, &buf);

  // x
  Read(&x, &buf);

  // y
  Read(&y, &buf);

  // override_redirect
  Read(&override_redirect, &buf);

  // pad1
  Pad(&buf, 3);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<ConfigureNotifyEvent>(ConfigureNotifyEvent* event_,
                                     ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& event = (*event_).event;
  auto& window = (*event_).window;
  auto& above_sibling = (*event_).above_sibling;
  auto& x = (*event_).x;
  auto& y = (*event_).y;
  auto& width = (*event_).width;
  auto& height = (*event_).height;
  auto& border_width = (*event_).border_width;
  auto& override_redirect = (*event_).override_redirect;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // event
  Read(&event, &buf);

  // window
  Read(&window, &buf);

  // above_sibling
  Read(&above_sibling, &buf);

  // x
  Read(&x, &buf);

  // y
  Read(&y, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // border_width
  Read(&border_width, &buf);

  // override_redirect
  Read(&override_redirect, &buf);

  // pad1
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<ConfigureRequestEvent>(ConfigureRequestEvent* event_,
                                      ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& stack_mode = (*event_).stack_mode;
  auto& sequence = (*event_).sequence;
  auto& parent = (*event_).parent;
  auto& window = (*event_).window;
  auto& sibling = (*event_).sibling;
  auto& x = (*event_).x;
  auto& y = (*event_).y;
  auto& width = (*event_).width;
  auto& height = (*event_).height;
  auto& border_width = (*event_).border_width;
  auto& value_mask = (*event_).value_mask;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // stack_mode
  uint8_t tmp19;
  Read(&tmp19, &buf);
  stack_mode = static_cast<StackMode>(tmp19);

  // sequence
  Read(&sequence, &buf);

  // parent
  Read(&parent, &buf);

  // window
  Read(&window, &buf);

  // sibling
  Read(&sibling, &buf);

  // x
  Read(&x, &buf);

  // y
  Read(&y, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // border_width
  Read(&border_width, &buf);

  // value_mask
  uint16_t tmp20;
  Read(&tmp20, &buf);
  value_mask = static_cast<ConfigWindow>(tmp20);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<GravityNotifyEvent>(GravityNotifyEvent* event_,
                                   ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& event = (*event_).event;
  auto& window = (*event_).window;
  auto& x = (*event_).x;
  auto& y = (*event_).y;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // event
  Read(&event, &buf);

  // window
  Read(&window, &buf);

  // x
  Read(&x, &buf);

  // y
  Read(&y, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<ResizeRequestEvent>(ResizeRequestEvent* event_,
                                   ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& window = (*event_).window;
  auto& width = (*event_).width;
  auto& height = (*event_).height;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // window
  Read(&window, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<CirculateEvent>(CirculateEvent* event_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& event = (*event_).event;
  auto& window = (*event_).window;
  auto& place = (*event_).place;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // event
  Read(&event, &buf);

  // window
  Read(&window, &buf);

  // pad1
  Pad(&buf, 4);

  // place
  uint8_t tmp21;
  Read(&tmp21, &buf);
  place = static_cast<Place>(tmp21);

  // pad2
  Pad(&buf, 3);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
WriteBuffer Write<PropertyNotifyEvent>(const PropertyNotifyEvent& obj) {
  WriteBuffer buf;

  auto& sequence = obj.sequence;
  auto& window = obj.window;
  auto& atom = obj.atom;
  auto& time = obj.time;
  auto& state = obj.state;

  // response_type
  uint8_t response_type = 28;
  buf.Write(&response_type);

  // pad0
  Pad(&buf, 1);

  // sequence
  buf.Write(&sequence);

  // window
  buf.Write(&window);

  // atom
  buf.Write(&atom);

  // time
  buf.Write(&time);

  // state
  uint8_t tmp22;
  tmp22 = static_cast<uint8_t>(state);
  buf.Write(&tmp22);

  // pad1
  Pad(&buf, 3);

  return buf;
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<PropertyNotifyEvent>(PropertyNotifyEvent* event_,
                                    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& window = (*event_).window;
  auto& atom = (*event_).atom;
  auto& time = (*event_).time;
  auto& state = (*event_).state;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // window
  Read(&window, &buf);

  // atom
  Read(&atom, &buf);

  // time
  Read(&time, &buf);

  // state
  uint8_t tmp23;
  Read(&tmp23, &buf);
  state = static_cast<Property>(tmp23);

  // pad1
  Pad(&buf, 3);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<SelectionClearEvent>(SelectionClearEvent* event_,
                                    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& owner = (*event_).owner;
  auto& selection = (*event_).selection;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // owner
  Read(&owner, &buf);

  // selection
  Read(&selection, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<SelectionRequestEvent>(SelectionRequestEvent* event_,
                                      ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& owner = (*event_).owner;
  auto& requestor = (*event_).requestor;
  auto& selection = (*event_).selection;
  auto& target = (*event_).target;
  auto& property = (*event_).property;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // owner
  Read(&owner, &buf);

  // requestor
  Read(&requestor, &buf);

  // selection
  Read(&selection, &buf);

  // target
  Read(&target, &buf);

  // property
  Read(&property, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
WriteBuffer Write<SelectionNotifyEvent>(const SelectionNotifyEvent& obj) {
  WriteBuffer buf;

  auto& sequence = obj.sequence;
  auto& time = obj.time;
  auto& requestor = obj.requestor;
  auto& selection = obj.selection;
  auto& target = obj.target;
  auto& property = obj.property;

  // response_type
  uint8_t response_type = 31;
  buf.Write(&response_type);

  // pad0
  Pad(&buf, 1);

  // sequence
  buf.Write(&sequence);

  // time
  buf.Write(&time);

  // requestor
  buf.Write(&requestor);

  // selection
  buf.Write(&selection);

  // target
  buf.Write(&target);

  // property
  buf.Write(&property);

  return buf;
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<SelectionNotifyEvent>(SelectionNotifyEvent* event_,
                                     ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& requestor = (*event_).requestor;
  auto& selection = (*event_).selection;
  auto& target = (*event_).target;
  auto& property = (*event_).property;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // requestor
  Read(&requestor, &buf);

  // selection
  Read(&selection, &buf);

  // target
  Read(&target, &buf);

  // property
  Read(&property, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<ColormapNotifyEvent>(ColormapNotifyEvent* event_,
                                    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& window = (*event_).window;
  auto& colormap = (*event_).colormap;
  auto& c_new = (*event_).c_new;
  auto& state = (*event_).state;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // window
  Read(&window, &buf);

  // colormap
  Read(&colormap, &buf);

  // c_new
  Read(&c_new, &buf);

  // state
  uint8_t tmp24;
  Read(&tmp24, &buf);
  state = static_cast<ColormapState>(tmp24);

  // pad1
  Pad(&buf, 2);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
WriteBuffer Write<ClientMessageEvent>(const ClientMessageEvent& obj) {
  WriteBuffer buf;

  auto& format = obj.format;
  auto& sequence = obj.sequence;
  auto& window = obj.window;
  auto& type = obj.type;
  auto& data = obj.data;

  // response_type
  uint8_t response_type = 33;
  buf.Write(&response_type);

  // format
  buf.Write(&format);

  // sequence
  buf.Write(&sequence);

  // window
  buf.Write(&window);

  // type
  buf.Write(&type);

  // data
  buf.Write(&data);

  return buf;
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<ClientMessageEvent>(ClientMessageEvent* event_,
                                   ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& format = (*event_).format;
  auto& sequence = (*event_).sequence;
  auto& window = (*event_).window;
  auto& type = (*event_).type;
  auto& data = (*event_).data;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // format
  Read(&format, &buf);

  // sequence
  Read(&sequence, &buf);

  // window
  Read(&window, &buf);

  // type
  Read(&type, &buf);

  // data
  Read(&data, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<MappingNotifyEvent>(MappingNotifyEvent* event_,
                                   ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& request = (*event_).request;
  auto& first_keycode = (*event_).first_keycode;
  auto& count = (*event_).count;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // request
  uint8_t tmp25;
  Read(&tmp25, &buf);
  request = static_cast<Mapping>(tmp25);

  // first_keycode
  Read(&first_keycode, &buf);

  // count
  Read(&count, &buf);

  // pad1
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<GeGenericEvent>(GeGenericEvent* event_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // pad0
  Pad(&buf, 22);

  Align(&buf, 4);
  CHECK_EQ(buf.offset, 32 + 4 * length);
}

std::string RequestError::ToString() const {
  std::stringstream ss_;
  ss_ << "RequestError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<RequestError>(RequestError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string ValueError::ToString() const {
  std::stringstream ss_;
  ss_ << "ValueError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<ValueError>(ValueError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string WindowError::ToString() const {
  std::stringstream ss_;
  ss_ << "WindowError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<WindowError>(WindowError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string PixmapError::ToString() const {
  std::stringstream ss_;
  ss_ << "PixmapError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<PixmapError>(PixmapError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string AtomError::ToString() const {
  std::stringstream ss_;
  ss_ << "AtomError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<AtomError>(AtomError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string CursorError::ToString() const {
  std::stringstream ss_;
  ss_ << "CursorError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<CursorError>(CursorError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string FontError::ToString() const {
  std::stringstream ss_;
  ss_ << "FontError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<FontError>(FontError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string MatchError::ToString() const {
  std::stringstream ss_;
  ss_ << "MatchError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<MatchError>(MatchError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string DrawableError::ToString() const {
  std::stringstream ss_;
  ss_ << "DrawableError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<DrawableError>(DrawableError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string AccessError::ToString() const {
  std::stringstream ss_;
  ss_ << "AccessError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<AccessError>(AccessError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string AllocError::ToString() const {
  std::stringstream ss_;
  ss_ << "AllocError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<AllocError>(AllocError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string ColormapError::ToString() const {
  std::stringstream ss_;
  ss_ << "ColormapError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<ColormapError>(ColormapError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string GContextError::ToString() const {
  std::stringstream ss_;
  ss_ << "GContextError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<GContextError>(GContextError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string IDChoiceError::ToString() const {
  std::stringstream ss_;
  ss_ << "IDChoiceError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<IDChoiceError>(IDChoiceError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string NameError::ToString() const {
  std::stringstream ss_;
  ss_ << "NameError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<NameError>(NameError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string LengthError::ToString() const {
  std::stringstream ss_;
  ss_ << "LengthError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<LengthError>(LengthError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

std::string ImplementationError::ToString() const {
  std::stringstream ss_;
  ss_ << "ImplementationError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<ImplementationError>(ImplementationError* error_,
                                    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

Future<void> XProto::CreateWindow(const CreateWindowRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& depth = request.depth;
  auto& wid = request.wid;
  auto& parent = request.parent;
  auto& x = request.x;
  auto& y = request.y;
  auto& width = request.width;
  auto& height = request.height;
  auto& border_width = request.border_width;
  auto& c_class = request.c_class;
  auto& visual = request.visual;
  CreateWindowAttribute value_mask{};
  auto& value_list = request;

  // major_opcode
  uint8_t major_opcode = 1;
  buf.Write(&major_opcode);

  // depth
  buf.Write(&depth);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // wid
  buf.Write(&wid);

  // parent
  buf.Write(&parent);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // border_width
  buf.Write(&border_width);

  // c_class
  uint16_t tmp26;
  tmp26 = static_cast<uint16_t>(c_class);
  buf.Write(&tmp26);

  // visual
  buf.Write(&visual);

  // value_mask
  SwitchVar(CreateWindowAttribute::BackPixmap,
            value_list.background_pixmap.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BackPixel,
            value_list.background_pixel.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BorderPixmap,
            value_list.border_pixmap.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BorderPixel,
            value_list.border_pixel.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BitGravity,
            value_list.bit_gravity.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::WinGravity,
            value_list.win_gravity.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BackingStore,
            value_list.backing_store.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BackingPlanes,
            value_list.backing_planes.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BackingPixel,
            value_list.backing_pixel.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::OverrideRedirect,
            value_list.override_redirect.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::SaveUnder, value_list.save_under.has_value(),
            true, &value_mask);
  SwitchVar(CreateWindowAttribute::EventMask, value_list.event_mask.has_value(),
            true, &value_mask);
  SwitchVar(CreateWindowAttribute::DontPropagate,
            value_list.do_not_propogate_mask.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::Colormap, value_list.colormap.has_value(),
            true, &value_mask);
  SwitchVar(CreateWindowAttribute::Cursor, value_list.cursor.has_value(), true,
            &value_mask);
  uint32_t tmp27;
  tmp27 = static_cast<uint32_t>(value_mask);
  buf.Write(&tmp27);

  // value_list
  auto value_list_expr = value_mask;
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackPixmap)) {
    auto& background_pixmap = *value_list.background_pixmap;

    // background_pixmap
    buf.Write(&background_pixmap);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackPixel)) {
    auto& background_pixel = *value_list.background_pixel;

    // background_pixel
    buf.Write(&background_pixel);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BorderPixmap)) {
    auto& border_pixmap = *value_list.border_pixmap;

    // border_pixmap
    buf.Write(&border_pixmap);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BorderPixel)) {
    auto& border_pixel = *value_list.border_pixel;

    // border_pixel
    buf.Write(&border_pixel);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BitGravity)) {
    auto& bit_gravity = *value_list.bit_gravity;

    // bit_gravity
    uint32_t tmp28;
    tmp28 = static_cast<uint32_t>(bit_gravity);
    buf.Write(&tmp28);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::WinGravity)) {
    auto& win_gravity = *value_list.win_gravity;

    // win_gravity
    uint32_t tmp29;
    tmp29 = static_cast<uint32_t>(win_gravity);
    buf.Write(&tmp29);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackingStore)) {
    auto& backing_store = *value_list.backing_store;

    // backing_store
    uint32_t tmp30;
    tmp30 = static_cast<uint32_t>(backing_store);
    buf.Write(&tmp30);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackingPlanes)) {
    auto& backing_planes = *value_list.backing_planes;

    // backing_planes
    buf.Write(&backing_planes);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackingPixel)) {
    auto& backing_pixel = *value_list.backing_pixel;

    // backing_pixel
    buf.Write(&backing_pixel);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::OverrideRedirect)) {
    auto& override_redirect = *value_list.override_redirect;

    // override_redirect
    buf.Write(&override_redirect);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::SaveUnder)) {
    auto& save_under = *value_list.save_under;

    // save_under
    buf.Write(&save_under);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::EventMask)) {
    auto& event_mask = *value_list.event_mask;

    // event_mask
    uint32_t tmp31;
    tmp31 = static_cast<uint32_t>(event_mask);
    buf.Write(&tmp31);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::DontPropagate)) {
    auto& do_not_propogate_mask = *value_list.do_not_propogate_mask;

    // do_not_propogate_mask
    uint32_t tmp32;
    tmp32 = static_cast<uint32_t>(do_not_propogate_mask);
    buf.Write(&tmp32);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::Colormap)) {
    auto& colormap = *value_list.colormap;

    // colormap
    buf.Write(&colormap);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::Cursor)) {
    auto& cursor = *value_list.cursor;

    // cursor
    buf.Write(&cursor);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "CreateWindow", false);
}

Future<void> XProto::CreateWindow(
    const uint8_t& depth,
    const Window& wid,
    const Window& parent,
    const int16_t& x,
    const int16_t& y,
    const uint16_t& width,
    const uint16_t& height,
    const uint16_t& border_width,
    const WindowClass& c_class,
    const VisualId& visual,
    const std::optional<Pixmap>& background_pixmap,
    const std::optional<uint32_t>& background_pixel,
    const std::optional<Pixmap>& border_pixmap,
    const std::optional<uint32_t>& border_pixel,
    const std::optional<Gravity>& bit_gravity,
    const std::optional<Gravity>& win_gravity,
    const std::optional<BackingStore>& backing_store,
    const std::optional<uint32_t>& backing_planes,
    const std::optional<uint32_t>& backing_pixel,
    const std::optional<Bool32>& override_redirect,
    const std::optional<Bool32>& save_under,
    const std::optional<EventMask>& event_mask,
    const std::optional<EventMask>& do_not_propogate_mask,
    const std::optional<ColorMap>& colormap,
    const std::optional<Cursor>& cursor) {
  return XProto::CreateWindow(CreateWindowRequest{depth,
                                                  wid,
                                                  parent,
                                                  x,
                                                  y,
                                                  width,
                                                  height,
                                                  border_width,
                                                  c_class,
                                                  visual,
                                                  background_pixmap,
                                                  background_pixel,
                                                  border_pixmap,
                                                  border_pixel,
                                                  bit_gravity,
                                                  win_gravity,
                                                  backing_store,
                                                  backing_planes,
                                                  backing_pixel,
                                                  override_redirect,
                                                  save_under,
                                                  event_mask,
                                                  do_not_propogate_mask,
                                                  colormap,
                                                  cursor});
}

Future<void> XProto::ChangeWindowAttributes(
    const ChangeWindowAttributesRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  CreateWindowAttribute value_mask{};
  auto& value_list = request;

  // major_opcode
  uint8_t major_opcode = 2;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // value_mask
  SwitchVar(CreateWindowAttribute::BackPixmap,
            value_list.background_pixmap.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BackPixel,
            value_list.background_pixel.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BorderPixmap,
            value_list.border_pixmap.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BorderPixel,
            value_list.border_pixel.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BitGravity,
            value_list.bit_gravity.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::WinGravity,
            value_list.win_gravity.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BackingStore,
            value_list.backing_store.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BackingPlanes,
            value_list.backing_planes.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::BackingPixel,
            value_list.backing_pixel.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::OverrideRedirect,
            value_list.override_redirect.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::SaveUnder, value_list.save_under.has_value(),
            true, &value_mask);
  SwitchVar(CreateWindowAttribute::EventMask, value_list.event_mask.has_value(),
            true, &value_mask);
  SwitchVar(CreateWindowAttribute::DontPropagate,
            value_list.do_not_propogate_mask.has_value(), true, &value_mask);
  SwitchVar(CreateWindowAttribute::Colormap, value_list.colormap.has_value(),
            true, &value_mask);
  SwitchVar(CreateWindowAttribute::Cursor, value_list.cursor.has_value(), true,
            &value_mask);
  uint32_t tmp33;
  tmp33 = static_cast<uint32_t>(value_mask);
  buf.Write(&tmp33);

  // value_list
  auto value_list_expr = value_mask;
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackPixmap)) {
    auto& background_pixmap = *value_list.background_pixmap;

    // background_pixmap
    buf.Write(&background_pixmap);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackPixel)) {
    auto& background_pixel = *value_list.background_pixel;

    // background_pixel
    buf.Write(&background_pixel);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BorderPixmap)) {
    auto& border_pixmap = *value_list.border_pixmap;

    // border_pixmap
    buf.Write(&border_pixmap);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BorderPixel)) {
    auto& border_pixel = *value_list.border_pixel;

    // border_pixel
    buf.Write(&border_pixel);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BitGravity)) {
    auto& bit_gravity = *value_list.bit_gravity;

    // bit_gravity
    uint32_t tmp34;
    tmp34 = static_cast<uint32_t>(bit_gravity);
    buf.Write(&tmp34);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::WinGravity)) {
    auto& win_gravity = *value_list.win_gravity;

    // win_gravity
    uint32_t tmp35;
    tmp35 = static_cast<uint32_t>(win_gravity);
    buf.Write(&tmp35);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackingStore)) {
    auto& backing_store = *value_list.backing_store;

    // backing_store
    uint32_t tmp36;
    tmp36 = static_cast<uint32_t>(backing_store);
    buf.Write(&tmp36);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackingPlanes)) {
    auto& backing_planes = *value_list.backing_planes;

    // backing_planes
    buf.Write(&backing_planes);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::BackingPixel)) {
    auto& backing_pixel = *value_list.backing_pixel;

    // backing_pixel
    buf.Write(&backing_pixel);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::OverrideRedirect)) {
    auto& override_redirect = *value_list.override_redirect;

    // override_redirect
    buf.Write(&override_redirect);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::SaveUnder)) {
    auto& save_under = *value_list.save_under;

    // save_under
    buf.Write(&save_under);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::EventMask)) {
    auto& event_mask = *value_list.event_mask;

    // event_mask
    uint32_t tmp37;
    tmp37 = static_cast<uint32_t>(event_mask);
    buf.Write(&tmp37);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::DontPropagate)) {
    auto& do_not_propogate_mask = *value_list.do_not_propogate_mask;

    // do_not_propogate_mask
    uint32_t tmp38;
    tmp38 = static_cast<uint32_t>(do_not_propogate_mask);
    buf.Write(&tmp38);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::Colormap)) {
    auto& colormap = *value_list.colormap;

    // colormap
    buf.Write(&colormap);
  }
  if (CaseAnd(value_list_expr, CreateWindowAttribute::Cursor)) {
    auto& cursor = *value_list.cursor;

    // cursor
    buf.Write(&cursor);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ChangeWindowAttributes", false);
}

Future<void> XProto::ChangeWindowAttributes(
    const Window& window,
    const std::optional<Pixmap>& background_pixmap,
    const std::optional<uint32_t>& background_pixel,
    const std::optional<Pixmap>& border_pixmap,
    const std::optional<uint32_t>& border_pixel,
    const std::optional<Gravity>& bit_gravity,
    const std::optional<Gravity>& win_gravity,
    const std::optional<BackingStore>& backing_store,
    const std::optional<uint32_t>& backing_planes,
    const std::optional<uint32_t>& backing_pixel,
    const std::optional<Bool32>& override_redirect,
    const std::optional<Bool32>& save_under,
    const std::optional<EventMask>& event_mask,
    const std::optional<EventMask>& do_not_propogate_mask,
    const std::optional<ColorMap>& colormap,
    const std::optional<Cursor>& cursor) {
  return XProto::ChangeWindowAttributes(ChangeWindowAttributesRequest{
      window, background_pixmap, background_pixel, border_pixmap, border_pixel,
      bit_gravity, win_gravity, backing_store, backing_planes, backing_pixel,
      override_redirect, save_under, event_mask, do_not_propogate_mask,
      colormap, cursor});
}

Future<GetWindowAttributesReply> XProto::GetWindowAttributes(
    const GetWindowAttributesRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 3;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<GetWindowAttributesReply>(
      &buf, "GetWindowAttributes", false);
}

Future<GetWindowAttributesReply> XProto::GetWindowAttributes(
    const Window& window) {
  return XProto::GetWindowAttributes(GetWindowAttributesRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetWindowAttributesReply> detail::ReadReply<
    GetWindowAttributesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetWindowAttributesReply>();

  auto& backing_store = (*reply).backing_store;
  auto& sequence = (*reply).sequence;
  auto& visual = (*reply).visual;
  auto& c_class = (*reply).c_class;
  auto& bit_gravity = (*reply).bit_gravity;
  auto& win_gravity = (*reply).win_gravity;
  auto& backing_planes = (*reply).backing_planes;
  auto& backing_pixel = (*reply).backing_pixel;
  auto& save_under = (*reply).save_under;
  auto& map_is_installed = (*reply).map_is_installed;
  auto& map_state = (*reply).map_state;
  auto& override_redirect = (*reply).override_redirect;
  auto& colormap = (*reply).colormap;
  auto& all_event_masks = (*reply).all_event_masks;
  auto& your_event_mask = (*reply).your_event_mask;
  auto& do_not_propagate_mask = (*reply).do_not_propagate_mask;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // backing_store
  uint8_t tmp39;
  Read(&tmp39, &buf);
  backing_store = static_cast<BackingStore>(tmp39);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // visual
  Read(&visual, &buf);

  // c_class
  uint16_t tmp40;
  Read(&tmp40, &buf);
  c_class = static_cast<WindowClass>(tmp40);

  // bit_gravity
  uint8_t tmp41;
  Read(&tmp41, &buf);
  bit_gravity = static_cast<Gravity>(tmp41);

  // win_gravity
  uint8_t tmp42;
  Read(&tmp42, &buf);
  win_gravity = static_cast<Gravity>(tmp42);

  // backing_planes
  Read(&backing_planes, &buf);

  // backing_pixel
  Read(&backing_pixel, &buf);

  // save_under
  Read(&save_under, &buf);

  // map_is_installed
  Read(&map_is_installed, &buf);

  // map_state
  uint8_t tmp43;
  Read(&tmp43, &buf);
  map_state = static_cast<MapState>(tmp43);

  // override_redirect
  Read(&override_redirect, &buf);

  // colormap
  Read(&colormap, &buf);

  // all_event_masks
  uint32_t tmp44;
  Read(&tmp44, &buf);
  all_event_masks = static_cast<EventMask>(tmp44);

  // your_event_mask
  uint32_t tmp45;
  Read(&tmp45, &buf);
  your_event_mask = static_cast<EventMask>(tmp45);

  // do_not_propagate_mask
  uint16_t tmp46;
  Read(&tmp46, &buf);
  do_not_propagate_mask = static_cast<EventMask>(tmp46);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::DestroyWindow(const DestroyWindowRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 4;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "DestroyWindow", false);
}

Future<void> XProto::DestroyWindow(const Window& window) {
  return XProto::DestroyWindow(DestroyWindowRequest{window});
}

Future<void> XProto::DestroySubwindows(
    const DestroySubwindowsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 5;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "DestroySubwindows", false);
}

Future<void> XProto::DestroySubwindows(const Window& window) {
  return XProto::DestroySubwindows(DestroySubwindowsRequest{window});
}

Future<void> XProto::ChangeSaveSet(const ChangeSaveSetRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& mode = request.mode;
  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 6;
  buf.Write(&major_opcode);

  // mode
  uint8_t tmp47;
  tmp47 = static_cast<uint8_t>(mode);
  buf.Write(&tmp47);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ChangeSaveSet", false);
}

Future<void> XProto::ChangeSaveSet(const SetMode& mode, const Window& window) {
  return XProto::ChangeSaveSet(ChangeSaveSetRequest{mode, window});
}

Future<void> XProto::ReparentWindow(const ReparentWindowRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& parent = request.parent;
  auto& x = request.x;
  auto& y = request.y;

  // major_opcode
  uint8_t major_opcode = 7;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // parent
  buf.Write(&parent);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ReparentWindow", false);
}

Future<void> XProto::ReparentWindow(const Window& window,
                                    const Window& parent,
                                    const int16_t& x,
                                    const int16_t& y) {
  return XProto::ReparentWindow(ReparentWindowRequest{window, parent, x, y});
}

Future<void> XProto::MapWindow(const MapWindowRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 8;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "MapWindow", false);
}

Future<void> XProto::MapWindow(const Window& window) {
  return XProto::MapWindow(MapWindowRequest{window});
}

Future<void> XProto::MapSubwindows(const MapSubwindowsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 9;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "MapSubwindows", false);
}

Future<void> XProto::MapSubwindows(const Window& window) {
  return XProto::MapSubwindows(MapSubwindowsRequest{window});
}

Future<void> XProto::UnmapWindow(const UnmapWindowRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 10;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "UnmapWindow", false);
}

Future<void> XProto::UnmapWindow(const Window& window) {
  return XProto::UnmapWindow(UnmapWindowRequest{window});
}

Future<void> XProto::UnmapSubwindows(const UnmapSubwindowsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 11;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "UnmapSubwindows", false);
}

Future<void> XProto::UnmapSubwindows(const Window& window) {
  return XProto::UnmapSubwindows(UnmapSubwindowsRequest{window});
}

Future<void> XProto::ConfigureWindow(const ConfigureWindowRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  ConfigWindow value_mask{};
  auto& value_list = request;

  // major_opcode
  uint8_t major_opcode = 12;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // value_mask
  SwitchVar(ConfigWindow::X, value_list.x.has_value(), true, &value_mask);
  SwitchVar(ConfigWindow::Y, value_list.y.has_value(), true, &value_mask);
  SwitchVar(ConfigWindow::Width, value_list.width.has_value(), true,
            &value_mask);
  SwitchVar(ConfigWindow::Height, value_list.height.has_value(), true,
            &value_mask);
  SwitchVar(ConfigWindow::BorderWidth, value_list.border_width.has_value(),
            true, &value_mask);
  SwitchVar(ConfigWindow::Sibling, value_list.sibling.has_value(), true,
            &value_mask);
  SwitchVar(ConfigWindow::StackMode, value_list.stack_mode.has_value(), true,
            &value_mask);
  uint16_t tmp48;
  tmp48 = static_cast<uint16_t>(value_mask);
  buf.Write(&tmp48);

  // pad1
  Pad(&buf, 2);

  // value_list
  auto value_list_expr = value_mask;
  if (CaseAnd(value_list_expr, ConfigWindow::X)) {
    auto& x = *value_list.x;

    // x
    buf.Write(&x);
  }
  if (CaseAnd(value_list_expr, ConfigWindow::Y)) {
    auto& y = *value_list.y;

    // y
    buf.Write(&y);
  }
  if (CaseAnd(value_list_expr, ConfigWindow::Width)) {
    auto& width = *value_list.width;

    // width
    buf.Write(&width);
  }
  if (CaseAnd(value_list_expr, ConfigWindow::Height)) {
    auto& height = *value_list.height;

    // height
    buf.Write(&height);
  }
  if (CaseAnd(value_list_expr, ConfigWindow::BorderWidth)) {
    auto& border_width = *value_list.border_width;

    // border_width
    buf.Write(&border_width);
  }
  if (CaseAnd(value_list_expr, ConfigWindow::Sibling)) {
    auto& sibling = *value_list.sibling;

    // sibling
    buf.Write(&sibling);
  }
  if (CaseAnd(value_list_expr, ConfigWindow::StackMode)) {
    auto& stack_mode = *value_list.stack_mode;

    // stack_mode
    uint32_t tmp49;
    tmp49 = static_cast<uint32_t>(stack_mode);
    buf.Write(&tmp49);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ConfigureWindow", false);
}

Future<void> XProto::ConfigureWindow(
    const Window& window,
    const std::optional<int32_t>& x,
    const std::optional<int32_t>& y,
    const std::optional<uint32_t>& width,
    const std::optional<uint32_t>& height,
    const std::optional<uint32_t>& border_width,
    const std::optional<Window>& sibling,
    const std::optional<StackMode>& stack_mode) {
  return XProto::ConfigureWindow(ConfigureWindowRequest{
      window, x, y, width, height, border_width, sibling, stack_mode});
}

Future<void> XProto::CirculateWindow(const CirculateWindowRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& direction = request.direction;
  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 13;
  buf.Write(&major_opcode);

  // direction
  uint8_t tmp50;
  tmp50 = static_cast<uint8_t>(direction);
  buf.Write(&tmp50);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "CirculateWindow", false);
}

Future<void> XProto::CirculateWindow(const Circulate& direction,
                                     const Window& window) {
  return XProto::CirculateWindow(CirculateWindowRequest{direction, window});
}

Future<GetGeometryReply> XProto::GetGeometry(
    const GetGeometryRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;

  // major_opcode
  uint8_t major_opcode = 14;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  Align(&buf, 4);

  return connection_->SendRequest<GetGeometryReply>(&buf, "GetGeometry", false);
}

Future<GetGeometryReply> XProto::GetGeometry(const Drawable& drawable) {
  return XProto::GetGeometry(GetGeometryRequest{drawable});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetGeometryReply> detail::ReadReply<GetGeometryReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetGeometryReply>();

  auto& depth = (*reply).depth;
  auto& sequence = (*reply).sequence;
  auto& root = (*reply).root;
  auto& x = (*reply).x;
  auto& y = (*reply).y;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  auto& border_width = (*reply).border_width;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // depth
  Read(&depth, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // root
  Read(&root, &buf);

  // x
  Read(&x, &buf);

  // y
  Read(&y, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // border_width
  Read(&border_width, &buf);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<QueryTreeReply> XProto::QueryTree(const QueryTreeRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 15;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<QueryTreeReply>(&buf, "QueryTree", false);
}

Future<QueryTreeReply> XProto::QueryTree(const Window& window) {
  return XProto::QueryTree(QueryTreeRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<QueryTreeReply> detail::ReadReply<QueryTreeReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<QueryTreeReply>();

  auto& sequence = (*reply).sequence;
  auto& root = (*reply).root;
  auto& parent = (*reply).parent;
  uint16_t children_len{};
  auto& children = (*reply).children;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // root
  Read(&root, &buf);

  // parent
  Read(&parent, &buf);

  // children_len
  Read(&children_len, &buf);

  // pad1
  Pad(&buf, 14);

  // children
  children.resize(length);
  for (auto& children_elem : children) {
    // children_elem
    Read(&children_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<InternAtomReply> XProto::InternAtom(const InternAtomRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& only_if_exists = request.only_if_exists;
  uint16_t name_len{};
  auto& name = request.name;

  // major_opcode
  uint8_t major_opcode = 16;
  buf.Write(&major_opcode);

  // only_if_exists
  buf.Write(&only_if_exists);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // name_len
  name_len = name.size();
  buf.Write(&name_len);

  // pad0
  Pad(&buf, 2);

  // name
  CHECK_EQ(static_cast<size_t>(name_len), name.size());
  for (auto& name_elem : name) {
    // name_elem
    buf.Write(&name_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<InternAtomReply>(&buf, "InternAtom", false);
}

Future<InternAtomReply> XProto::InternAtom(const uint8_t& only_if_exists,
                                           const std::string& name) {
  return XProto::InternAtom(InternAtomRequest{only_if_exists, name});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<InternAtomReply> detail::ReadReply<InternAtomReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<InternAtomReply>();

  auto& sequence = (*reply).sequence;
  auto& atom = (*reply).atom;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // atom
  Read(&atom, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<GetAtomNameReply> XProto::GetAtomName(
    const GetAtomNameRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& atom = request.atom;

  // major_opcode
  uint8_t major_opcode = 17;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // atom
  buf.Write(&atom);

  Align(&buf, 4);

  return connection_->SendRequest<GetAtomNameReply>(&buf, "GetAtomName", false);
}

Future<GetAtomNameReply> XProto::GetAtomName(const Atom& atom) {
  return XProto::GetAtomName(GetAtomNameRequest{atom});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetAtomNameReply> detail::ReadReply<GetAtomNameReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetAtomNameReply>();

  auto& sequence = (*reply).sequence;
  uint16_t name_len{};
  auto& name = (*reply).name;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // name_len
  Read(&name_len, &buf);

  // pad1
  Pad(&buf, 22);

  // name
  name.resize(name_len);
  for (auto& name_elem : name) {
    // name_elem
    Read(&name_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::ChangeProperty(const ChangePropertyRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& mode = request.mode;
  auto& window = request.window;
  auto& property = request.property;
  auto& type = request.type;
  auto& format = request.format;
  auto& data_len = request.data_len;
  auto& data = request.data;

  // major_opcode
  uint8_t major_opcode = 18;
  buf.Write(&major_opcode);

  // mode
  uint8_t tmp51;
  tmp51 = static_cast<uint8_t>(mode);
  buf.Write(&tmp51);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // property
  buf.Write(&property);

  // type
  buf.Write(&type);

  // format
  buf.Write(&format);

  // pad0
  Pad(&buf, 3);

  // data_len
  buf.Write(&data_len);

  // data
  buf.AppendSizedBuffer(data);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ChangeProperty", false);
}

Future<void> XProto::ChangeProperty(
    const PropMode& mode,
    const Window& window,
    const Atom& property,
    const Atom& type,
    const uint8_t& format,
    const uint32_t& data_len,
    const scoped_refptr<base::RefCountedMemory>& data) {
  return XProto::ChangeProperty(ChangePropertyRequest{
      mode, window, property, type, format, data_len, data});
}

Future<void> XProto::DeleteProperty(const DeletePropertyRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& property = request.property;

  // major_opcode
  uint8_t major_opcode = 19;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // property
  buf.Write(&property);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "DeleteProperty", false);
}

Future<void> XProto::DeleteProperty(const Window& window,
                                    const Atom& property) {
  return XProto::DeleteProperty(DeletePropertyRequest{window, property});
}

Future<GetPropertyReply> XProto::GetProperty(
    const GetPropertyRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& c_delete = request.c_delete;
  auto& window = request.window;
  auto& property = request.property;
  auto& type = request.type;
  auto& long_offset = request.long_offset;
  auto& long_length = request.long_length;

  // major_opcode
  uint8_t major_opcode = 20;
  buf.Write(&major_opcode);

  // c_delete
  buf.Write(&c_delete);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // property
  buf.Write(&property);

  // type
  buf.Write(&type);

  // long_offset
  buf.Write(&long_offset);

  // long_length
  buf.Write(&long_length);

  Align(&buf, 4);

  return connection_->SendRequest<GetPropertyReply>(&buf, "GetProperty", false);
}

Future<GetPropertyReply> XProto::GetProperty(const uint8_t& c_delete,
                                             const Window& window,
                                             const Atom& property,
                                             const Atom& type,
                                             const uint32_t& long_offset,
                                             const uint32_t& long_length) {
  return XProto::GetProperty(GetPropertyRequest{
      c_delete, window, property, type, long_offset, long_length});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetPropertyReply> detail::ReadReply<GetPropertyReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetPropertyReply>();

  auto& format = (*reply).format;
  auto& sequence = (*reply).sequence;
  auto& type = (*reply).type;
  auto& bytes_after = (*reply).bytes_after;
  auto& value_len = (*reply).value_len;
  auto& value = (*reply).value;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // format
  Read(&format, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // type
  Read(&type, &buf);

  // bytes_after
  Read(&bytes_after, &buf);

  // value_len
  Read(&value_len, &buf);

  // pad0
  Pad(&buf, 12);

  // value
  value = buffer->ReadAndAdvance((value_len) * ((format) / (8)));

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<ListPropertiesReply> XProto::ListProperties(
    const ListPropertiesRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 21;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<ListPropertiesReply>(&buf, "ListProperties",
                                                       false);
}

Future<ListPropertiesReply> XProto::ListProperties(const Window& window) {
  return XProto::ListProperties(ListPropertiesRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<ListPropertiesReply> detail::ReadReply<ListPropertiesReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<ListPropertiesReply>();

  auto& sequence = (*reply).sequence;
  uint16_t atoms_len{};
  auto& atoms = (*reply).atoms;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // atoms_len
  Read(&atoms_len, &buf);

  // pad1
  Pad(&buf, 22);

  // atoms
  atoms.resize(atoms_len);
  for (auto& atoms_elem : atoms) {
    // atoms_elem
    Read(&atoms_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::SetSelectionOwner(
    const SetSelectionOwnerRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& owner = request.owner;
  auto& selection = request.selection;
  auto& time = request.time;

  // major_opcode
  uint8_t major_opcode = 22;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // owner
  buf.Write(&owner);

  // selection
  buf.Write(&selection);

  // time
  buf.Write(&time);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SetSelectionOwner", false);
}

Future<void> XProto::SetSelectionOwner(const Window& owner,
                                       const Atom& selection,
                                       const Time& time) {
  return XProto::SetSelectionOwner(
      SetSelectionOwnerRequest{owner, selection, time});
}

Future<GetSelectionOwnerReply> XProto::GetSelectionOwner(
    const GetSelectionOwnerRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& selection = request.selection;

  // major_opcode
  uint8_t major_opcode = 23;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // selection
  buf.Write(&selection);

  Align(&buf, 4);

  return connection_->SendRequest<GetSelectionOwnerReply>(
      &buf, "GetSelectionOwner", false);
}

Future<GetSelectionOwnerReply> XProto::GetSelectionOwner(
    const Atom& selection) {
  return XProto::GetSelectionOwner(GetSelectionOwnerRequest{selection});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetSelectionOwnerReply> detail::ReadReply<
    GetSelectionOwnerReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetSelectionOwnerReply>();

  auto& sequence = (*reply).sequence;
  auto& owner = (*reply).owner;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // owner
  Read(&owner, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::ConvertSelection(const ConvertSelectionRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& requestor = request.requestor;
  auto& selection = request.selection;
  auto& target = request.target;
  auto& property = request.property;
  auto& time = request.time;

  // major_opcode
  uint8_t major_opcode = 24;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // requestor
  buf.Write(&requestor);

  // selection
  buf.Write(&selection);

  // target
  buf.Write(&target);

  // property
  buf.Write(&property);

  // time
  buf.Write(&time);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ConvertSelection", false);
}

Future<void> XProto::ConvertSelection(const Window& requestor,
                                      const Atom& selection,
                                      const Atom& target,
                                      const Atom& property,
                                      const Time& time) {
  return XProto::ConvertSelection(
      ConvertSelectionRequest{requestor, selection, target, property, time});
}

Future<void> XProto::SendEvent(const SendEventRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& propagate = request.propagate;
  auto& destination = request.destination;
  auto& event_mask = request.event_mask;
  auto& event = request.event;
  size_t event_len = event.size();

  // major_opcode
  uint8_t major_opcode = 25;
  buf.Write(&major_opcode);

  // propagate
  buf.Write(&propagate);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // destination
  buf.Write(&destination);

  // event_mask
  uint32_t tmp52;
  tmp52 = static_cast<uint32_t>(event_mask);
  buf.Write(&tmp52);

  // event
  for (auto& event_elem : event) {
    // event_elem
    buf.Write(&event_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SendEvent", false);
}

Future<void> XProto::SendEvent(const uint8_t& propagate,
                               const Window& destination,
                               const EventMask& event_mask,
                               const std::array<char, 32>& event) {
  return XProto::SendEvent(
      SendEventRequest{propagate, destination, event_mask, event});
}

Future<GrabPointerReply> XProto::GrabPointer(
    const GrabPointerRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& owner_events = request.owner_events;
  auto& grab_window = request.grab_window;
  auto& event_mask = request.event_mask;
  auto& pointer_mode = request.pointer_mode;
  auto& keyboard_mode = request.keyboard_mode;
  auto& confine_to = request.confine_to;
  auto& cursor = request.cursor;
  auto& time = request.time;

  // major_opcode
  uint8_t major_opcode = 26;
  buf.Write(&major_opcode);

  // owner_events
  buf.Write(&owner_events);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // grab_window
  buf.Write(&grab_window);

  // event_mask
  uint16_t tmp53;
  tmp53 = static_cast<uint16_t>(event_mask);
  buf.Write(&tmp53);

  // pointer_mode
  uint8_t tmp54;
  tmp54 = static_cast<uint8_t>(pointer_mode);
  buf.Write(&tmp54);

  // keyboard_mode
  uint8_t tmp55;
  tmp55 = static_cast<uint8_t>(keyboard_mode);
  buf.Write(&tmp55);

  // confine_to
  buf.Write(&confine_to);

  // cursor
  buf.Write(&cursor);

  // time
  buf.Write(&time);

  Align(&buf, 4);

  return connection_->SendRequest<GrabPointerReply>(&buf, "GrabPointer", false);
}

Future<GrabPointerReply> XProto::GrabPointer(const uint8_t& owner_events,
                                             const Window& grab_window,
                                             const EventMask& event_mask,
                                             const GrabMode& pointer_mode,
                                             const GrabMode& keyboard_mode,
                                             const Window& confine_to,
                                             const Cursor& cursor,
                                             const Time& time) {
  return XProto::GrabPointer(
      GrabPointerRequest{owner_events, grab_window, event_mask, pointer_mode,
                         keyboard_mode, confine_to, cursor, time});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GrabPointerReply> detail::ReadReply<GrabPointerReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GrabPointerReply>();

  auto& status = (*reply).status;
  auto& sequence = (*reply).sequence;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // status
  uint8_t tmp56;
  Read(&tmp56, &buf);
  status = static_cast<GrabStatus>(tmp56);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::UngrabPointer(const UngrabPointerRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& time = request.time;

  // major_opcode
  uint8_t major_opcode = 27;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // time
  buf.Write(&time);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "UngrabPointer", false);
}

Future<void> XProto::UngrabPointer(const Time& time) {
  return XProto::UngrabPointer(UngrabPointerRequest{time});
}

Future<void> XProto::GrabButton(const GrabButtonRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& owner_events = request.owner_events;
  auto& grab_window = request.grab_window;
  auto& event_mask = request.event_mask;
  auto& pointer_mode = request.pointer_mode;
  auto& keyboard_mode = request.keyboard_mode;
  auto& confine_to = request.confine_to;
  auto& cursor = request.cursor;
  auto& button = request.button;
  auto& modifiers = request.modifiers;

  // major_opcode
  uint8_t major_opcode = 28;
  buf.Write(&major_opcode);

  // owner_events
  buf.Write(&owner_events);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // grab_window
  buf.Write(&grab_window);

  // event_mask
  uint16_t tmp57;
  tmp57 = static_cast<uint16_t>(event_mask);
  buf.Write(&tmp57);

  // pointer_mode
  uint8_t tmp58;
  tmp58 = static_cast<uint8_t>(pointer_mode);
  buf.Write(&tmp58);

  // keyboard_mode
  uint8_t tmp59;
  tmp59 = static_cast<uint8_t>(keyboard_mode);
  buf.Write(&tmp59);

  // confine_to
  buf.Write(&confine_to);

  // cursor
  buf.Write(&cursor);

  // button
  uint8_t tmp60;
  tmp60 = static_cast<uint8_t>(button);
  buf.Write(&tmp60);

  // pad0
  Pad(&buf, 1);

  // modifiers
  uint16_t tmp61;
  tmp61 = static_cast<uint16_t>(modifiers);
  buf.Write(&tmp61);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "GrabButton", false);
}

Future<void> XProto::GrabButton(const uint8_t& owner_events,
                                const Window& grab_window,
                                const EventMask& event_mask,
                                const GrabMode& pointer_mode,
                                const GrabMode& keyboard_mode,
                                const Window& confine_to,
                                const Cursor& cursor,
                                const ButtonIndex& button,
                                const ModMask& modifiers) {
  return XProto::GrabButton(
      GrabButtonRequest{owner_events, grab_window, event_mask, pointer_mode,
                        keyboard_mode, confine_to, cursor, button, modifiers});
}

Future<void> XProto::UngrabButton(const UngrabButtonRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& button = request.button;
  auto& grab_window = request.grab_window;
  auto& modifiers = request.modifiers;

  // major_opcode
  uint8_t major_opcode = 29;
  buf.Write(&major_opcode);

  // button
  uint8_t tmp62;
  tmp62 = static_cast<uint8_t>(button);
  buf.Write(&tmp62);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // grab_window
  buf.Write(&grab_window);

  // modifiers
  uint16_t tmp63;
  tmp63 = static_cast<uint16_t>(modifiers);
  buf.Write(&tmp63);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "UngrabButton", false);
}

Future<void> XProto::UngrabButton(const ButtonIndex& button,
                                  const Window& grab_window,
                                  const ModMask& modifiers) {
  return XProto::UngrabButton(
      UngrabButtonRequest{button, grab_window, modifiers});
}

Future<void> XProto::ChangeActivePointerGrab(
    const ChangeActivePointerGrabRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cursor = request.cursor;
  auto& time = request.time;
  auto& event_mask = request.event_mask;

  // major_opcode
  uint8_t major_opcode = 30;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cursor
  buf.Write(&cursor);

  // time
  buf.Write(&time);

  // event_mask
  uint16_t tmp64;
  tmp64 = static_cast<uint16_t>(event_mask);
  buf.Write(&tmp64);

  // pad1
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ChangeActivePointerGrab", false);
}

Future<void> XProto::ChangeActivePointerGrab(const Cursor& cursor,
                                             const Time& time,
                                             const EventMask& event_mask) {
  return XProto::ChangeActivePointerGrab(
      ChangeActivePointerGrabRequest{cursor, time, event_mask});
}

Future<GrabKeyboardReply> XProto::GrabKeyboard(
    const GrabKeyboardRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& owner_events = request.owner_events;
  auto& grab_window = request.grab_window;
  auto& time = request.time;
  auto& pointer_mode = request.pointer_mode;
  auto& keyboard_mode = request.keyboard_mode;

  // major_opcode
  uint8_t major_opcode = 31;
  buf.Write(&major_opcode);

  // owner_events
  buf.Write(&owner_events);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // grab_window
  buf.Write(&grab_window);

  // time
  buf.Write(&time);

  // pointer_mode
  uint8_t tmp65;
  tmp65 = static_cast<uint8_t>(pointer_mode);
  buf.Write(&tmp65);

  // keyboard_mode
  uint8_t tmp66;
  tmp66 = static_cast<uint8_t>(keyboard_mode);
  buf.Write(&tmp66);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<GrabKeyboardReply>(&buf, "GrabKeyboard",
                                                     false);
}

Future<GrabKeyboardReply> XProto::GrabKeyboard(const uint8_t& owner_events,
                                               const Window& grab_window,
                                               const Time& time,
                                               const GrabMode& pointer_mode,
                                               const GrabMode& keyboard_mode) {
  return XProto::GrabKeyboard(GrabKeyboardRequest{
      owner_events, grab_window, time, pointer_mode, keyboard_mode});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GrabKeyboardReply> detail::ReadReply<GrabKeyboardReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GrabKeyboardReply>();

  auto& status = (*reply).status;
  auto& sequence = (*reply).sequence;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // status
  uint8_t tmp67;
  Read(&tmp67, &buf);
  status = static_cast<GrabStatus>(tmp67);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::UngrabKeyboard(const UngrabKeyboardRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& time = request.time;

  // major_opcode
  uint8_t major_opcode = 32;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // time
  buf.Write(&time);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "UngrabKeyboard", false);
}

Future<void> XProto::UngrabKeyboard(const Time& time) {
  return XProto::UngrabKeyboard(UngrabKeyboardRequest{time});
}

Future<void> XProto::GrabKey(const GrabKeyRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& owner_events = request.owner_events;
  auto& grab_window = request.grab_window;
  auto& modifiers = request.modifiers;
  auto& key = request.key;
  auto& pointer_mode = request.pointer_mode;
  auto& keyboard_mode = request.keyboard_mode;

  // major_opcode
  uint8_t major_opcode = 33;
  buf.Write(&major_opcode);

  // owner_events
  buf.Write(&owner_events);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // grab_window
  buf.Write(&grab_window);

  // modifiers
  uint16_t tmp68;
  tmp68 = static_cast<uint16_t>(modifiers);
  buf.Write(&tmp68);

  // key
  buf.Write(&key);

  // pointer_mode
  uint8_t tmp69;
  tmp69 = static_cast<uint8_t>(pointer_mode);
  buf.Write(&tmp69);

  // keyboard_mode
  uint8_t tmp70;
  tmp70 = static_cast<uint8_t>(keyboard_mode);
  buf.Write(&tmp70);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "GrabKey", false);
}

Future<void> XProto::GrabKey(const uint8_t& owner_events,
                             const Window& grab_window,
                             const ModMask& modifiers,
                             const KeyCode& key,
                             const GrabMode& pointer_mode,
                             const GrabMode& keyboard_mode) {
  return XProto::GrabKey(GrabKeyRequest{owner_events, grab_window, modifiers,
                                        key, pointer_mode, keyboard_mode});
}

Future<void> XProto::UngrabKey(const UngrabKeyRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& key = request.key;
  auto& grab_window = request.grab_window;
  auto& modifiers = request.modifiers;

  // major_opcode
  uint8_t major_opcode = 34;
  buf.Write(&major_opcode);

  // key
  buf.Write(&key);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // grab_window
  buf.Write(&grab_window);

  // modifiers
  uint16_t tmp71;
  tmp71 = static_cast<uint16_t>(modifiers);
  buf.Write(&tmp71);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "UngrabKey", false);
}

Future<void> XProto::UngrabKey(const KeyCode& key,
                               const Window& grab_window,
                               const ModMask& modifiers) {
  return XProto::UngrabKey(UngrabKeyRequest{key, grab_window, modifiers});
}

Future<void> XProto::AllowEvents(const AllowEventsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& mode = request.mode;
  auto& time = request.time;

  // major_opcode
  uint8_t major_opcode = 35;
  buf.Write(&major_opcode);

  // mode
  uint8_t tmp72;
  tmp72 = static_cast<uint8_t>(mode);
  buf.Write(&tmp72);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // time
  buf.Write(&time);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "AllowEvents", false);
}

Future<void> XProto::AllowEvents(const Allow& mode, const Time& time) {
  return XProto::AllowEvents(AllowEventsRequest{mode, time});
}

Future<void> XProto::GrabServer(const GrabServerRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 36;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "GrabServer", false);
}

Future<void> XProto::GrabServer() {
  return XProto::GrabServer(GrabServerRequest{});
}

Future<void> XProto::UngrabServer(const UngrabServerRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 37;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "UngrabServer", false);
}

Future<void> XProto::UngrabServer() {
  return XProto::UngrabServer(UngrabServerRequest{});
}

Future<QueryPointerReply> XProto::QueryPointer(
    const QueryPointerRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 38;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<QueryPointerReply>(&buf, "QueryPointer",
                                                     false);
}

Future<QueryPointerReply> XProto::QueryPointer(const Window& window) {
  return XProto::QueryPointer(QueryPointerRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<QueryPointerReply> detail::ReadReply<QueryPointerReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<QueryPointerReply>();

  auto& same_screen = (*reply).same_screen;
  auto& sequence = (*reply).sequence;
  auto& root = (*reply).root;
  auto& child = (*reply).child;
  auto& root_x = (*reply).root_x;
  auto& root_y = (*reply).root_y;
  auto& win_x = (*reply).win_x;
  auto& win_y = (*reply).win_y;
  auto& mask = (*reply).mask;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // same_screen
  Read(&same_screen, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // root
  Read(&root, &buf);

  // child
  Read(&child, &buf);

  // root_x
  Read(&root_x, &buf);

  // root_y
  Read(&root_y, &buf);

  // win_x
  Read(&win_x, &buf);

  // win_y
  Read(&win_y, &buf);

  // mask
  uint16_t tmp73;
  Read(&tmp73, &buf);
  mask = static_cast<KeyButMask>(tmp73);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<GetMotionEventsReply> XProto::GetMotionEvents(
    const GetMotionEventsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& start = request.start;
  auto& stop = request.stop;

  // major_opcode
  uint8_t major_opcode = 39;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // start
  buf.Write(&start);

  // stop
  buf.Write(&stop);

  Align(&buf, 4);

  return connection_->SendRequest<GetMotionEventsReply>(&buf, "GetMotionEvents",
                                                        false);
}

Future<GetMotionEventsReply> XProto::GetMotionEvents(const Window& window,
                                                     const Time& start,
                                                     const Time& stop) {
  return XProto::GetMotionEvents(GetMotionEventsRequest{window, start, stop});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetMotionEventsReply> detail::ReadReply<GetMotionEventsReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetMotionEventsReply>();

  auto& sequence = (*reply).sequence;
  uint32_t events_len{};
  auto& events = (*reply).events;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // events_len
  Read(&events_len, &buf);

  // pad1
  Pad(&buf, 20);

  // events
  events.resize(events_len);
  for (auto& events_elem : events) {
    // events_elem
    {
      auto& time = events_elem.time;
      auto& x = events_elem.x;
      auto& y = events_elem.y;

      // time
      Read(&time, &buf);

      // x
      Read(&x, &buf);

      // y
      Read(&y, &buf);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<TranslateCoordinatesReply> XProto::TranslateCoordinates(
    const TranslateCoordinatesRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& src_window = request.src_window;
  auto& dst_window = request.dst_window;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;

  // major_opcode
  uint8_t major_opcode = 40;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // src_window
  buf.Write(&src_window);

  // dst_window
  buf.Write(&dst_window);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  Align(&buf, 4);

  return connection_->SendRequest<TranslateCoordinatesReply>(
      &buf, "TranslateCoordinates", false);
}

Future<TranslateCoordinatesReply> XProto::TranslateCoordinates(
    const Window& src_window,
    const Window& dst_window,
    const int16_t& src_x,
    const int16_t& src_y) {
  return XProto::TranslateCoordinates(
      TranslateCoordinatesRequest{src_window, dst_window, src_x, src_y});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<TranslateCoordinatesReply> detail::ReadReply<
    TranslateCoordinatesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<TranslateCoordinatesReply>();

  auto& same_screen = (*reply).same_screen;
  auto& sequence = (*reply).sequence;
  auto& child = (*reply).child;
  auto& dst_x = (*reply).dst_x;
  auto& dst_y = (*reply).dst_y;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // same_screen
  Read(&same_screen, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // child
  Read(&child, &buf);

  // dst_x
  Read(&dst_x, &buf);

  // dst_y
  Read(&dst_y, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::WarpPointer(const WarpPointerRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& src_window = request.src_window;
  auto& dst_window = request.dst_window;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& src_width = request.src_width;
  auto& src_height = request.src_height;
  auto& dst_x = request.dst_x;
  auto& dst_y = request.dst_y;

  // major_opcode
  uint8_t major_opcode = 41;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // src_window
  buf.Write(&src_window);

  // dst_window
  buf.Write(&dst_window);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // src_width
  buf.Write(&src_width);

  // src_height
  buf.Write(&src_height);

  // dst_x
  buf.Write(&dst_x);

  // dst_y
  buf.Write(&dst_y);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "WarpPointer", false);
}

Future<void> XProto::WarpPointer(const Window& src_window,
                                 const Window& dst_window,
                                 const int16_t& src_x,
                                 const int16_t& src_y,
                                 const uint16_t& src_width,
                                 const uint16_t& src_height,
                                 const int16_t& dst_x,
                                 const int16_t& dst_y) {
  return XProto::WarpPointer(WarpPointerRequest{src_window, dst_window, src_x,
                                                src_y, src_width, src_height,
                                                dst_x, dst_y});
}

Future<void> XProto::SetInputFocus(const SetInputFocusRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& revert_to = request.revert_to;
  auto& focus = request.focus;
  auto& time = request.time;

  // major_opcode
  uint8_t major_opcode = 42;
  buf.Write(&major_opcode);

  // revert_to
  uint8_t tmp74;
  tmp74 = static_cast<uint8_t>(revert_to);
  buf.Write(&tmp74);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // focus
  buf.Write(&focus);

  // time
  buf.Write(&time);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SetInputFocus", false);
}

Future<void> XProto::SetInputFocus(const InputFocus& revert_to,
                                   const Window& focus,
                                   const Time& time) {
  return XProto::SetInputFocus(SetInputFocusRequest{revert_to, focus, time});
}

Future<GetInputFocusReply> XProto::GetInputFocus(
    const GetInputFocusRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 43;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<GetInputFocusReply>(&buf, "GetInputFocus",
                                                      false);
}

Future<GetInputFocusReply> XProto::GetInputFocus() {
  return XProto::GetInputFocus(GetInputFocusRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetInputFocusReply> detail::ReadReply<GetInputFocusReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetInputFocusReply>();

  auto& revert_to = (*reply).revert_to;
  auto& sequence = (*reply).sequence;
  auto& focus = (*reply).focus;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // revert_to
  uint8_t tmp75;
  Read(&tmp75, &buf);
  revert_to = static_cast<InputFocus>(tmp75);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // focus
  Read(&focus, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<QueryKeymapReply> XProto::QueryKeymap(
    const QueryKeymapRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 44;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<QueryKeymapReply>(&buf, "QueryKeymap", false);
}

Future<QueryKeymapReply> XProto::QueryKeymap() {
  return XProto::QueryKeymap(QueryKeymapRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<QueryKeymapReply> detail::ReadReply<QueryKeymapReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<QueryKeymapReply>();

  auto& sequence = (*reply).sequence;
  auto& keys = (*reply).keys;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // keys
  for (auto& keys_elem : keys) {
    // keys_elem
    Read(&keys_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::OpenFont(const OpenFontRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& fid = request.fid;
  uint16_t name_len{};
  auto& name = request.name;

  // major_opcode
  uint8_t major_opcode = 45;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // fid
  buf.Write(&fid);

  // name_len
  name_len = name.size();
  buf.Write(&name_len);

  // pad1
  Pad(&buf, 2);

  // name
  CHECK_EQ(static_cast<size_t>(name_len), name.size());
  for (auto& name_elem : name) {
    // name_elem
    buf.Write(&name_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "OpenFont", false);
}

Future<void> XProto::OpenFont(const Font& fid, const std::string& name) {
  return XProto::OpenFont(OpenFontRequest{fid, name});
}

Future<void> XProto::CloseFont(const CloseFontRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& font = request.font;

  // major_opcode
  uint8_t major_opcode = 46;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // font
  buf.Write(&font);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "CloseFont", false);
}

Future<void> XProto::CloseFont(const Font& font) {
  return XProto::CloseFont(CloseFontRequest{font});
}

Future<QueryFontReply> XProto::QueryFont(const QueryFontRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& font = request.font;

  // major_opcode
  uint8_t major_opcode = 47;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // font
  buf.Write(&font);

  Align(&buf, 4);

  return connection_->SendRequest<QueryFontReply>(&buf, "QueryFont", false);
}

Future<QueryFontReply> XProto::QueryFont(const Fontable& font) {
  return XProto::QueryFont(QueryFontRequest{font});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<QueryFontReply> detail::ReadReply<QueryFontReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<QueryFontReply>();

  auto& sequence = (*reply).sequence;
  auto& min_bounds = (*reply).min_bounds;
  auto& max_bounds = (*reply).max_bounds;
  auto& min_char_or_byte2 = (*reply).min_char_or_byte2;
  auto& max_char_or_byte2 = (*reply).max_char_or_byte2;
  auto& default_char = (*reply).default_char;
  uint16_t properties_len{};
  auto& draw_direction = (*reply).draw_direction;
  auto& min_byte1 = (*reply).min_byte1;
  auto& max_byte1 = (*reply).max_byte1;
  auto& all_chars_exist = (*reply).all_chars_exist;
  auto& font_ascent = (*reply).font_ascent;
  auto& font_descent = (*reply).font_descent;
  uint32_t char_infos_len{};
  auto& properties = (*reply).properties;
  auto& char_infos = (*reply).char_infos;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // min_bounds
  {
    auto& left_side_bearing = min_bounds.left_side_bearing;
    auto& right_side_bearing = min_bounds.right_side_bearing;
    auto& character_width = min_bounds.character_width;
    auto& ascent = min_bounds.ascent;
    auto& descent = min_bounds.descent;
    auto& attributes = min_bounds.attributes;

    // left_side_bearing
    Read(&left_side_bearing, &buf);

    // right_side_bearing
    Read(&right_side_bearing, &buf);

    // character_width
    Read(&character_width, &buf);

    // ascent
    Read(&ascent, &buf);

    // descent
    Read(&descent, &buf);

    // attributes
    Read(&attributes, &buf);
  }

  // pad1
  Pad(&buf, 4);

  // max_bounds
  {
    auto& left_side_bearing = max_bounds.left_side_bearing;
    auto& right_side_bearing = max_bounds.right_side_bearing;
    auto& character_width = max_bounds.character_width;
    auto& ascent = max_bounds.ascent;
    auto& descent = max_bounds.descent;
    auto& attributes = max_bounds.attributes;

    // left_side_bearing
    Read(&left_side_bearing, &buf);

    // right_side_bearing
    Read(&right_side_bearing, &buf);

    // character_width
    Read(&character_width, &buf);

    // ascent
    Read(&ascent, &buf);

    // descent
    Read(&descent, &buf);

    // attributes
    Read(&attributes, &buf);
  }

  // pad2
  Pad(&buf, 4);

  // min_char_or_byte2
  Read(&min_char_or_byte2, &buf);

  // max_char_or_byte2
  Read(&max_char_or_byte2, &buf);

  // default_char
  Read(&default_char, &buf);

  // properties_len
  Read(&properties_len, &buf);

  // draw_direction
  uint8_t tmp76;
  Read(&tmp76, &buf);
  draw_direction = static_cast<FontDraw>(tmp76);

  // min_byte1
  Read(&min_byte1, &buf);

  // max_byte1
  Read(&max_byte1, &buf);

  // all_chars_exist
  Read(&all_chars_exist, &buf);

  // font_ascent
  Read(&font_ascent, &buf);

  // font_descent
  Read(&font_descent, &buf);

  // char_infos_len
  Read(&char_infos_len, &buf);

  // properties
  properties.resize(properties_len);
  for (auto& properties_elem : properties) {
    // properties_elem
    {
      auto& name = properties_elem.name;
      auto& value = properties_elem.value;

      // name
      Read(&name, &buf);

      // value
      Read(&value, &buf);
    }
  }

  // char_infos
  char_infos.resize(char_infos_len);
  for (auto& char_infos_elem : char_infos) {
    // char_infos_elem
    {
      auto& left_side_bearing = char_infos_elem.left_side_bearing;
      auto& right_side_bearing = char_infos_elem.right_side_bearing;
      auto& character_width = char_infos_elem.character_width;
      auto& ascent = char_infos_elem.ascent;
      auto& descent = char_infos_elem.descent;
      auto& attributes = char_infos_elem.attributes;

      // left_side_bearing
      Read(&left_side_bearing, &buf);

      // right_side_bearing
      Read(&right_side_bearing, &buf);

      // character_width
      Read(&character_width, &buf);

      // ascent
      Read(&ascent, &buf);

      // descent
      Read(&descent, &buf);

      // attributes
      Read(&attributes, &buf);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<QueryTextExtentsReply> XProto::QueryTextExtents(
    const QueryTextExtentsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& font = request.font;
  auto& string = request.string;
  size_t string_len = string.size();

  // major_opcode
  uint8_t major_opcode = 48;
  buf.Write(&major_opcode);

  // odd_length
  uint8_t odd_length = BitAnd(string_len, 1);
  buf.Write(&odd_length);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // font
  buf.Write(&font);

  // string
  CHECK_EQ(static_cast<size_t>(string_len), string.size());
  for (auto& string_elem : string) {
    // string_elem
    {
      auto& byte1 = string_elem.byte1;
      auto& byte2 = string_elem.byte2;

      // byte1
      buf.Write(&byte1);

      // byte2
      buf.Write(&byte2);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<QueryTextExtentsReply>(
      &buf, "QueryTextExtents", false);
}

Future<QueryTextExtentsReply> XProto::QueryTextExtents(
    const Fontable& font,
    const std::vector<Char16>& string) {
  return XProto::QueryTextExtents(QueryTextExtentsRequest{font, string});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<QueryTextExtentsReply> detail::ReadReply<QueryTextExtentsReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<QueryTextExtentsReply>();

  auto& draw_direction = (*reply).draw_direction;
  auto& sequence = (*reply).sequence;
  auto& font_ascent = (*reply).font_ascent;
  auto& font_descent = (*reply).font_descent;
  auto& overall_ascent = (*reply).overall_ascent;
  auto& overall_descent = (*reply).overall_descent;
  auto& overall_width = (*reply).overall_width;
  auto& overall_left = (*reply).overall_left;
  auto& overall_right = (*reply).overall_right;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // draw_direction
  uint8_t tmp77;
  Read(&tmp77, &buf);
  draw_direction = static_cast<FontDraw>(tmp77);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // font_ascent
  Read(&font_ascent, &buf);

  // font_descent
  Read(&font_descent, &buf);

  // overall_ascent
  Read(&overall_ascent, &buf);

  // overall_descent
  Read(&overall_descent, &buf);

  // overall_width
  Read(&overall_width, &buf);

  // overall_left
  Read(&overall_left, &buf);

  // overall_right
  Read(&overall_right, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<ListFontsReply> XProto::ListFonts(const ListFontsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& max_names = request.max_names;
  uint16_t pattern_len{};
  auto& pattern = request.pattern;

  // major_opcode
  uint8_t major_opcode = 49;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // max_names
  buf.Write(&max_names);

  // pattern_len
  pattern_len = pattern.size();
  buf.Write(&pattern_len);

  // pattern
  CHECK_EQ(static_cast<size_t>(pattern_len), pattern.size());
  for (auto& pattern_elem : pattern) {
    // pattern_elem
    buf.Write(&pattern_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<ListFontsReply>(&buf, "ListFonts", false);
}

Future<ListFontsReply> XProto::ListFonts(const uint16_t& max_names,
                                         const std::string& pattern) {
  return XProto::ListFonts(ListFontsRequest{max_names, pattern});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<ListFontsReply> detail::ReadReply<ListFontsReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<ListFontsReply>();

  auto& sequence = (*reply).sequence;
  uint16_t names_len{};
  auto& names = (*reply).names;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // names_len
  Read(&names_len, &buf);

  // pad1
  Pad(&buf, 22);

  // names
  names.resize(names_len);
  for (auto& names_elem : names) {
    // names_elem
    {
      uint8_t name_len{};
      auto& name = names_elem.name;

      // name_len
      Read(&name_len, &buf);

      // name
      name.resize(name_len);
      for (auto& name_elem : name) {
        // name_elem
        Read(&name_elem, &buf);
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<ListFontsWithInfoReply> XProto::ListFontsWithInfo(
    const ListFontsWithInfoRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& max_names = request.max_names;
  uint16_t pattern_len{};
  auto& pattern = request.pattern;

  // major_opcode
  uint8_t major_opcode = 50;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // max_names
  buf.Write(&max_names);

  // pattern_len
  pattern_len = pattern.size();
  buf.Write(&pattern_len);

  // pattern
  CHECK_EQ(static_cast<size_t>(pattern_len), pattern.size());
  for (auto& pattern_elem : pattern) {
    // pattern_elem
    buf.Write(&pattern_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<ListFontsWithInfoReply>(
      &buf, "ListFontsWithInfo", false);
}

Future<ListFontsWithInfoReply> XProto::ListFontsWithInfo(
    const uint16_t& max_names,
    const std::string& pattern) {
  return XProto::ListFontsWithInfo(
      ListFontsWithInfoRequest{max_names, pattern});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<ListFontsWithInfoReply> detail::ReadReply<
    ListFontsWithInfoReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<ListFontsWithInfoReply>();

  uint8_t name_len{};
  auto& sequence = (*reply).sequence;
  auto& min_bounds = (*reply).min_bounds;
  auto& max_bounds = (*reply).max_bounds;
  auto& min_char_or_byte2 = (*reply).min_char_or_byte2;
  auto& max_char_or_byte2 = (*reply).max_char_or_byte2;
  auto& default_char = (*reply).default_char;
  uint16_t properties_len{};
  auto& draw_direction = (*reply).draw_direction;
  auto& min_byte1 = (*reply).min_byte1;
  auto& max_byte1 = (*reply).max_byte1;
  auto& all_chars_exist = (*reply).all_chars_exist;
  auto& font_ascent = (*reply).font_ascent;
  auto& font_descent = (*reply).font_descent;
  auto& replies_hint = (*reply).replies_hint;
  auto& properties = (*reply).properties;
  auto& name = (*reply).name;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // name_len
  Read(&name_len, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // min_bounds
  {
    auto& left_side_bearing = min_bounds.left_side_bearing;
    auto& right_side_bearing = min_bounds.right_side_bearing;
    auto& character_width = min_bounds.character_width;
    auto& ascent = min_bounds.ascent;
    auto& descent = min_bounds.descent;
    auto& attributes = min_bounds.attributes;

    // left_side_bearing
    Read(&left_side_bearing, &buf);

    // right_side_bearing
    Read(&right_side_bearing, &buf);

    // character_width
    Read(&character_width, &buf);

    // ascent
    Read(&ascent, &buf);

    // descent
    Read(&descent, &buf);

    // attributes
    Read(&attributes, &buf);
  }

  // pad0
  Pad(&buf, 4);

  // max_bounds
  {
    auto& left_side_bearing = max_bounds.left_side_bearing;
    auto& right_side_bearing = max_bounds.right_side_bearing;
    auto& character_width = max_bounds.character_width;
    auto& ascent = max_bounds.ascent;
    auto& descent = max_bounds.descent;
    auto& attributes = max_bounds.attributes;

    // left_side_bearing
    Read(&left_side_bearing, &buf);

    // right_side_bearing
    Read(&right_side_bearing, &buf);

    // character_width
    Read(&character_width, &buf);

    // ascent
    Read(&ascent, &buf);

    // descent
    Read(&descent, &buf);

    // attributes
    Read(&attributes, &buf);
  }

  // pad1
  Pad(&buf, 4);

  // min_char_or_byte2
  Read(&min_char_or_byte2, &buf);

  // max_char_or_byte2
  Read(&max_char_or_byte2, &buf);

  // default_char
  Read(&default_char, &buf);

  // properties_len
  Read(&properties_len, &buf);

  // draw_direction
  uint8_t tmp78;
  Read(&tmp78, &buf);
  draw_direction = static_cast<FontDraw>(tmp78);

  // min_byte1
  Read(&min_byte1, &buf);

  // max_byte1
  Read(&max_byte1, &buf);

  // all_chars_exist
  Read(&all_chars_exist, &buf);

  // font_ascent
  Read(&font_ascent, &buf);

  // font_descent
  Read(&font_descent, &buf);

  // replies_hint
  Read(&replies_hint, &buf);

  // properties
  properties.resize(properties_len);
  for (auto& properties_elem : properties) {
    // properties_elem
    {
      auto& name = properties_elem.name;
      auto& value = properties_elem.value;

      // name
      Read(&name, &buf);

      // value
      Read(&value, &buf);
    }
  }

  // name
  name.resize(name_len);
  for (auto& name_elem : name) {
    // name_elem
    Read(&name_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::SetFontPath(const SetFontPathRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  uint16_t font_qty{};
  auto& font = request.font;
  size_t font_len = font.size();

  // major_opcode
  uint8_t major_opcode = 51;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // font_qty
  font_qty = font.size();
  buf.Write(&font_qty);

  // pad1
  Pad(&buf, 2);

  // font
  CHECK_EQ(static_cast<size_t>(font_qty), font.size());
  for (auto& font_elem : font) {
    // font_elem
    {
      uint8_t name_len{};
      auto& name = font_elem.name;

      // name_len
      name_len = name.size();
      buf.Write(&name_len);

      // name
      CHECK_EQ(static_cast<size_t>(name_len), name.size());
      for (auto& name_elem : name) {
        // name_elem
        buf.Write(&name_elem);
      }
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SetFontPath", false);
}

Future<void> XProto::SetFontPath(const std::vector<Str>& font) {
  return XProto::SetFontPath(SetFontPathRequest{font});
}

Future<GetFontPathReply> XProto::GetFontPath(
    const GetFontPathRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 52;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<GetFontPathReply>(&buf, "GetFontPath", false);
}

Future<GetFontPathReply> XProto::GetFontPath() {
  return XProto::GetFontPath(GetFontPathRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetFontPathReply> detail::ReadReply<GetFontPathReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetFontPathReply>();

  auto& sequence = (*reply).sequence;
  uint16_t path_len{};
  auto& path = (*reply).path;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // path_len
  Read(&path_len, &buf);

  // pad1
  Pad(&buf, 22);

  // path
  path.resize(path_len);
  for (auto& path_elem : path) {
    // path_elem
    {
      uint8_t name_len{};
      auto& name = path_elem.name;

      // name_len
      Read(&name_len, &buf);

      // name
      name.resize(name_len);
      for (auto& name_elem : name) {
        // name_elem
        Read(&name_elem, &buf);
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::CreatePixmap(const CreatePixmapRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& depth = request.depth;
  auto& pid = request.pid;
  auto& drawable = request.drawable;
  auto& width = request.width;
  auto& height = request.height;

  // major_opcode
  uint8_t major_opcode = 53;
  buf.Write(&major_opcode);

  // depth
  buf.Write(&depth);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // pid
  buf.Write(&pid);

  // drawable
  buf.Write(&drawable);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "CreatePixmap", false);
}

Future<void> XProto::CreatePixmap(const uint8_t& depth,
                                  const Pixmap& pid,
                                  const Drawable& drawable,
                                  const uint16_t& width,
                                  const uint16_t& height) {
  return XProto::CreatePixmap(
      CreatePixmapRequest{depth, pid, drawable, width, height});
}

Future<void> XProto::FreePixmap(const FreePixmapRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& pixmap = request.pixmap;

  // major_opcode
  uint8_t major_opcode = 54;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // pixmap
  buf.Write(&pixmap);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "FreePixmap", false);
}

Future<void> XProto::FreePixmap(const Pixmap& pixmap) {
  return XProto::FreePixmap(FreePixmapRequest{pixmap});
}

Future<void> XProto::CreateGC(const CreateGCRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cid = request.cid;
  auto& drawable = request.drawable;
  GraphicsContextAttribute value_mask{};
  auto& value_list = request;

  // major_opcode
  uint8_t major_opcode = 55;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cid
  buf.Write(&cid);

  // drawable
  buf.Write(&drawable);

  // value_mask
  SwitchVar(GraphicsContextAttribute::Function, value_list.function.has_value(),
            true, &value_mask);
  SwitchVar(GraphicsContextAttribute::PlaneMask,
            value_list.plane_mask.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::Foreground,
            value_list.foreground.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::Background,
            value_list.background.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::LineWidth,
            value_list.line_width.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::LineStyle,
            value_list.line_style.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::CapStyle,
            value_list.cap_style.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::JoinStyle,
            value_list.join_style.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::FillStyle,
            value_list.fill_style.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::FillRule,
            value_list.fill_rule.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::Tile, value_list.tile.has_value(), true,
            &value_mask);
  SwitchVar(GraphicsContextAttribute::Stipple, value_list.stipple.has_value(),
            true, &value_mask);
  SwitchVar(GraphicsContextAttribute::TileStippleOriginX,
            value_list.tile_stipple_x_origin.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::TileStippleOriginY,
            value_list.tile_stipple_y_origin.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::Font, value_list.font.has_value(), true,
            &value_mask);
  SwitchVar(GraphicsContextAttribute::SubwindowMode,
            value_list.subwindow_mode.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::GraphicsExposures,
            value_list.graphics_exposures.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::ClipOriginX,
            value_list.clip_x_origin.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::ClipOriginY,
            value_list.clip_y_origin.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::ClipMask,
            value_list.clip_mask.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::DashOffset,
            value_list.dash_offset.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::DashList, value_list.dashes.has_value(),
            true, &value_mask);
  SwitchVar(GraphicsContextAttribute::ArcMode, value_list.arc_mode.has_value(),
            true, &value_mask);
  uint32_t tmp79;
  tmp79 = static_cast<uint32_t>(value_mask);
  buf.Write(&tmp79);

  // value_list
  auto value_list_expr = value_mask;
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::Function)) {
    auto& function = *value_list.function;

    // function
    uint32_t tmp80;
    tmp80 = static_cast<uint32_t>(function);
    buf.Write(&tmp80);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::PlaneMask)) {
    auto& plane_mask = *value_list.plane_mask;

    // plane_mask
    buf.Write(&plane_mask);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::Foreground)) {
    auto& foreground = *value_list.foreground;

    // foreground
    buf.Write(&foreground);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::Background)) {
    auto& background = *value_list.background;

    // background
    buf.Write(&background);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::LineWidth)) {
    auto& line_width = *value_list.line_width;

    // line_width
    buf.Write(&line_width);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::LineStyle)) {
    auto& line_style = *value_list.line_style;

    // line_style
    uint32_t tmp81;
    tmp81 = static_cast<uint32_t>(line_style);
    buf.Write(&tmp81);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::CapStyle)) {
    auto& cap_style = *value_list.cap_style;

    // cap_style
    uint32_t tmp82;
    tmp82 = static_cast<uint32_t>(cap_style);
    buf.Write(&tmp82);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::JoinStyle)) {
    auto& join_style = *value_list.join_style;

    // join_style
    uint32_t tmp83;
    tmp83 = static_cast<uint32_t>(join_style);
    buf.Write(&tmp83);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::FillStyle)) {
    auto& fill_style = *value_list.fill_style;

    // fill_style
    uint32_t tmp84;
    tmp84 = static_cast<uint32_t>(fill_style);
    buf.Write(&tmp84);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::FillRule)) {
    auto& fill_rule = *value_list.fill_rule;

    // fill_rule
    uint32_t tmp85;
    tmp85 = static_cast<uint32_t>(fill_rule);
    buf.Write(&tmp85);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::Tile)) {
    auto& tile = *value_list.tile;

    // tile
    buf.Write(&tile);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::Stipple)) {
    auto& stipple = *value_list.stipple;

    // stipple
    buf.Write(&stipple);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::TileStippleOriginX)) {
    auto& tile_stipple_x_origin = *value_list.tile_stipple_x_origin;

    // tile_stipple_x_origin
    buf.Write(&tile_stipple_x_origin);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::TileStippleOriginY)) {
    auto& tile_stipple_y_origin = *value_list.tile_stipple_y_origin;

    // tile_stipple_y_origin
    buf.Write(&tile_stipple_y_origin);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::Font)) {
    auto& font = *value_list.font;

    // font
    buf.Write(&font);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::SubwindowMode)) {
    auto& subwindow_mode = *value_list.subwindow_mode;

    // subwindow_mode
    uint32_t tmp86;
    tmp86 = static_cast<uint32_t>(subwindow_mode);
    buf.Write(&tmp86);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::GraphicsExposures)) {
    auto& graphics_exposures = *value_list.graphics_exposures;

    // graphics_exposures
    buf.Write(&graphics_exposures);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::ClipOriginX)) {
    auto& clip_x_origin = *value_list.clip_x_origin;

    // clip_x_origin
    buf.Write(&clip_x_origin);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::ClipOriginY)) {
    auto& clip_y_origin = *value_list.clip_y_origin;

    // clip_y_origin
    buf.Write(&clip_y_origin);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::ClipMask)) {
    auto& clip_mask = *value_list.clip_mask;

    // clip_mask
    buf.Write(&clip_mask);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::DashOffset)) {
    auto& dash_offset = *value_list.dash_offset;

    // dash_offset
    buf.Write(&dash_offset);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::DashList)) {
    auto& dashes = *value_list.dashes;

    // dashes
    buf.Write(&dashes);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::ArcMode)) {
    auto& arc_mode = *value_list.arc_mode;

    // arc_mode
    uint32_t tmp87;
    tmp87 = static_cast<uint32_t>(arc_mode);
    buf.Write(&tmp87);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "CreateGC", false);
}

Future<void> XProto::CreateGC(
    const GraphicsContext& cid,
    const Drawable& drawable,
    const std::optional<Gx>& function,
    const std::optional<uint32_t>& plane_mask,
    const std::optional<uint32_t>& foreground,
    const std::optional<uint32_t>& background,
    const std::optional<uint32_t>& line_width,
    const std::optional<LineStyle>& line_style,
    const std::optional<CapStyle>& cap_style,
    const std::optional<JoinStyle>& join_style,
    const std::optional<FillStyle>& fill_style,
    const std::optional<FillRule>& fill_rule,
    const std::optional<Pixmap>& tile,
    const std::optional<Pixmap>& stipple,
    const std::optional<int32_t>& tile_stipple_x_origin,
    const std::optional<int32_t>& tile_stipple_y_origin,
    const std::optional<Font>& font,
    const std::optional<SubwindowMode>& subwindow_mode,
    const std::optional<Bool32>& graphics_exposures,
    const std::optional<int32_t>& clip_x_origin,
    const std::optional<int32_t>& clip_y_origin,
    const std::optional<Pixmap>& clip_mask,
    const std::optional<uint32_t>& dash_offset,
    const std::optional<uint32_t>& dashes,
    const std::optional<ArcMode>& arc_mode) {
  return XProto::CreateGC(CreateGCRequest{cid,
                                          drawable,
                                          function,
                                          plane_mask,
                                          foreground,
                                          background,
                                          line_width,
                                          line_style,
                                          cap_style,
                                          join_style,
                                          fill_style,
                                          fill_rule,
                                          tile,
                                          stipple,
                                          tile_stipple_x_origin,
                                          tile_stipple_y_origin,
                                          font,
                                          subwindow_mode,
                                          graphics_exposures,
                                          clip_x_origin,
                                          clip_y_origin,
                                          clip_mask,
                                          dash_offset,
                                          dashes,
                                          arc_mode});
}

Future<void> XProto::ChangeGC(const ChangeGCRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& gc = request.gc;
  GraphicsContextAttribute value_mask{};
  auto& value_list = request;

  // major_opcode
  uint8_t major_opcode = 56;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // gc
  buf.Write(&gc);

  // value_mask
  SwitchVar(GraphicsContextAttribute::Function, value_list.function.has_value(),
            true, &value_mask);
  SwitchVar(GraphicsContextAttribute::PlaneMask,
            value_list.plane_mask.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::Foreground,
            value_list.foreground.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::Background,
            value_list.background.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::LineWidth,
            value_list.line_width.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::LineStyle,
            value_list.line_style.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::CapStyle,
            value_list.cap_style.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::JoinStyle,
            value_list.join_style.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::FillStyle,
            value_list.fill_style.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::FillRule,
            value_list.fill_rule.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::Tile, value_list.tile.has_value(), true,
            &value_mask);
  SwitchVar(GraphicsContextAttribute::Stipple, value_list.stipple.has_value(),
            true, &value_mask);
  SwitchVar(GraphicsContextAttribute::TileStippleOriginX,
            value_list.tile_stipple_x_origin.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::TileStippleOriginY,
            value_list.tile_stipple_y_origin.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::Font, value_list.font.has_value(), true,
            &value_mask);
  SwitchVar(GraphicsContextAttribute::SubwindowMode,
            value_list.subwindow_mode.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::GraphicsExposures,
            value_list.graphics_exposures.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::ClipOriginX,
            value_list.clip_x_origin.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::ClipOriginY,
            value_list.clip_y_origin.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::ClipMask,
            value_list.clip_mask.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::DashOffset,
            value_list.dash_offset.has_value(), true, &value_mask);
  SwitchVar(GraphicsContextAttribute::DashList, value_list.dashes.has_value(),
            true, &value_mask);
  SwitchVar(GraphicsContextAttribute::ArcMode, value_list.arc_mode.has_value(),
            true, &value_mask);
  uint32_t tmp88;
  tmp88 = static_cast<uint32_t>(value_mask);
  buf.Write(&tmp88);

  // value_list
  auto value_list_expr = value_mask;
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::Function)) {
    auto& function = *value_list.function;

    // function
    uint32_t tmp89;
    tmp89 = static_cast<uint32_t>(function);
    buf.Write(&tmp89);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::PlaneMask)) {
    auto& plane_mask = *value_list.plane_mask;

    // plane_mask
    buf.Write(&plane_mask);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::Foreground)) {
    auto& foreground = *value_list.foreground;

    // foreground
    buf.Write(&foreground);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::Background)) {
    auto& background = *value_list.background;

    // background
    buf.Write(&background);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::LineWidth)) {
    auto& line_width = *value_list.line_width;

    // line_width
    buf.Write(&line_width);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::LineStyle)) {
    auto& line_style = *value_list.line_style;

    // line_style
    uint32_t tmp90;
    tmp90 = static_cast<uint32_t>(line_style);
    buf.Write(&tmp90);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::CapStyle)) {
    auto& cap_style = *value_list.cap_style;

    // cap_style
    uint32_t tmp91;
    tmp91 = static_cast<uint32_t>(cap_style);
    buf.Write(&tmp91);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::JoinStyle)) {
    auto& join_style = *value_list.join_style;

    // join_style
    uint32_t tmp92;
    tmp92 = static_cast<uint32_t>(join_style);
    buf.Write(&tmp92);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::FillStyle)) {
    auto& fill_style = *value_list.fill_style;

    // fill_style
    uint32_t tmp93;
    tmp93 = static_cast<uint32_t>(fill_style);
    buf.Write(&tmp93);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::FillRule)) {
    auto& fill_rule = *value_list.fill_rule;

    // fill_rule
    uint32_t tmp94;
    tmp94 = static_cast<uint32_t>(fill_rule);
    buf.Write(&tmp94);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::Tile)) {
    auto& tile = *value_list.tile;

    // tile
    buf.Write(&tile);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::Stipple)) {
    auto& stipple = *value_list.stipple;

    // stipple
    buf.Write(&stipple);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::TileStippleOriginX)) {
    auto& tile_stipple_x_origin = *value_list.tile_stipple_x_origin;

    // tile_stipple_x_origin
    buf.Write(&tile_stipple_x_origin);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::TileStippleOriginY)) {
    auto& tile_stipple_y_origin = *value_list.tile_stipple_y_origin;

    // tile_stipple_y_origin
    buf.Write(&tile_stipple_y_origin);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::Font)) {
    auto& font = *value_list.font;

    // font
    buf.Write(&font);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::SubwindowMode)) {
    auto& subwindow_mode = *value_list.subwindow_mode;

    // subwindow_mode
    uint32_t tmp95;
    tmp95 = static_cast<uint32_t>(subwindow_mode);
    buf.Write(&tmp95);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::GraphicsExposures)) {
    auto& graphics_exposures = *value_list.graphics_exposures;

    // graphics_exposures
    buf.Write(&graphics_exposures);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::ClipOriginX)) {
    auto& clip_x_origin = *value_list.clip_x_origin;

    // clip_x_origin
    buf.Write(&clip_x_origin);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::ClipOriginY)) {
    auto& clip_y_origin = *value_list.clip_y_origin;

    // clip_y_origin
    buf.Write(&clip_y_origin);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::ClipMask)) {
    auto& clip_mask = *value_list.clip_mask;

    // clip_mask
    buf.Write(&clip_mask);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::DashOffset)) {
    auto& dash_offset = *value_list.dash_offset;

    // dash_offset
    buf.Write(&dash_offset);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::DashList)) {
    auto& dashes = *value_list.dashes;

    // dashes
    buf.Write(&dashes);
  }
  if (CaseAnd(value_list_expr, GraphicsContextAttribute::ArcMode)) {
    auto& arc_mode = *value_list.arc_mode;

    // arc_mode
    uint32_t tmp96;
    tmp96 = static_cast<uint32_t>(arc_mode);
    buf.Write(&tmp96);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ChangeGC", false);
}

Future<void> XProto::ChangeGC(
    const GraphicsContext& gc,
    const std::optional<Gx>& function,
    const std::optional<uint32_t>& plane_mask,
    const std::optional<uint32_t>& foreground,
    const std::optional<uint32_t>& background,
    const std::optional<uint32_t>& line_width,
    const std::optional<LineStyle>& line_style,
    const std::optional<CapStyle>& cap_style,
    const std::optional<JoinStyle>& join_style,
    const std::optional<FillStyle>& fill_style,
    const std::optional<FillRule>& fill_rule,
    const std::optional<Pixmap>& tile,
    const std::optional<Pixmap>& stipple,
    const std::optional<int32_t>& tile_stipple_x_origin,
    const std::optional<int32_t>& tile_stipple_y_origin,
    const std::optional<Font>& font,
    const std::optional<SubwindowMode>& subwindow_mode,
    const std::optional<Bool32>& graphics_exposures,
    const std::optional<int32_t>& clip_x_origin,
    const std::optional<int32_t>& clip_y_origin,
    const std::optional<Pixmap>& clip_mask,
    const std::optional<uint32_t>& dash_offset,
    const std::optional<uint32_t>& dashes,
    const std::optional<ArcMode>& arc_mode) {
  return XProto::ChangeGC(ChangeGCRequest{gc,
                                          function,
                                          plane_mask,
                                          foreground,
                                          background,
                                          line_width,
                                          line_style,
                                          cap_style,
                                          join_style,
                                          fill_style,
                                          fill_rule,
                                          tile,
                                          stipple,
                                          tile_stipple_x_origin,
                                          tile_stipple_y_origin,
                                          font,
                                          subwindow_mode,
                                          graphics_exposures,
                                          clip_x_origin,
                                          clip_y_origin,
                                          clip_mask,
                                          dash_offset,
                                          dashes,
                                          arc_mode});
}

Future<void> XProto::CopyGC(const CopyGCRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& src_gc = request.src_gc;
  auto& dst_gc = request.dst_gc;
  auto& value_mask = request.value_mask;

  // major_opcode
  uint8_t major_opcode = 57;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // src_gc
  buf.Write(&src_gc);

  // dst_gc
  buf.Write(&dst_gc);

  // value_mask
  uint32_t tmp97;
  tmp97 = static_cast<uint32_t>(value_mask);
  buf.Write(&tmp97);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "CopyGC", false);
}

Future<void> XProto::CopyGC(const GraphicsContext& src_gc,
                            const GraphicsContext& dst_gc,
                            const GraphicsContextAttribute& value_mask) {
  return XProto::CopyGC(CopyGCRequest{src_gc, dst_gc, value_mask});
}

Future<void> XProto::SetDashes(const SetDashesRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& gc = request.gc;
  auto& dash_offset = request.dash_offset;
  uint16_t dashes_len{};
  auto& dashes = request.dashes;

  // major_opcode
  uint8_t major_opcode = 58;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // gc
  buf.Write(&gc);

  // dash_offset
  buf.Write(&dash_offset);

  // dashes_len
  dashes_len = dashes.size();
  buf.Write(&dashes_len);

  // dashes
  CHECK_EQ(static_cast<size_t>(dashes_len), dashes.size());
  for (auto& dashes_elem : dashes) {
    // dashes_elem
    buf.Write(&dashes_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SetDashes", false);
}

Future<void> XProto::SetDashes(const GraphicsContext& gc,
                               const uint16_t& dash_offset,
                               const std::vector<uint8_t>& dashes) {
  return XProto::SetDashes(SetDashesRequest{gc, dash_offset, dashes});
}

Future<void> XProto::SetClipRectangles(
    const SetClipRectanglesRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& ordering = request.ordering;
  auto& gc = request.gc;
  auto& clip_x_origin = request.clip_x_origin;
  auto& clip_y_origin = request.clip_y_origin;
  auto& rectangles = request.rectangles;
  size_t rectangles_len = rectangles.size();

  // major_opcode
  uint8_t major_opcode = 59;
  buf.Write(&major_opcode);

  // ordering
  uint8_t tmp98;
  tmp98 = static_cast<uint8_t>(ordering);
  buf.Write(&tmp98);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // gc
  buf.Write(&gc);

  // clip_x_origin
  buf.Write(&clip_x_origin);

  // clip_y_origin
  buf.Write(&clip_y_origin);

  // rectangles
  CHECK_EQ(static_cast<size_t>(rectangles_len), rectangles.size());
  for (auto& rectangles_elem : rectangles) {
    // rectangles_elem
    {
      auto& x = rectangles_elem.x;
      auto& y = rectangles_elem.y;
      auto& width = rectangles_elem.width;
      auto& height = rectangles_elem.height;

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);

      // width
      buf.Write(&width);

      // height
      buf.Write(&height);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SetClipRectangles", false);
}

Future<void> XProto::SetClipRectangles(
    const ClipOrdering& ordering,
    const GraphicsContext& gc,
    const int16_t& clip_x_origin,
    const int16_t& clip_y_origin,
    const std::vector<Rectangle>& rectangles) {
  return XProto::SetClipRectangles(SetClipRectanglesRequest{
      ordering, gc, clip_x_origin, clip_y_origin, rectangles});
}

Future<void> XProto::FreeGC(const FreeGCRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& gc = request.gc;

  // major_opcode
  uint8_t major_opcode = 60;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // gc
  buf.Write(&gc);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "FreeGC", false);
}

Future<void> XProto::FreeGC(const GraphicsContext& gc) {
  return XProto::FreeGC(FreeGCRequest{gc});
}

Future<void> XProto::ClearArea(const ClearAreaRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& exposures = request.exposures;
  auto& window = request.window;
  auto& x = request.x;
  auto& y = request.y;
  auto& width = request.width;
  auto& height = request.height;

  // major_opcode
  uint8_t major_opcode = 61;
  buf.Write(&major_opcode);

  // exposures
  buf.Write(&exposures);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ClearArea", false);
}

Future<void> XProto::ClearArea(const uint8_t& exposures,
                               const Window& window,
                               const int16_t& x,
                               const int16_t& y,
                               const uint16_t& width,
                               const uint16_t& height) {
  return XProto::ClearArea(
      ClearAreaRequest{exposures, window, x, y, width, height});
}

Future<void> XProto::CopyArea(const CopyAreaRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& src_drawable = request.src_drawable;
  auto& dst_drawable = request.dst_drawable;
  auto& gc = request.gc;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& dst_x = request.dst_x;
  auto& dst_y = request.dst_y;
  auto& width = request.width;
  auto& height = request.height;

  // major_opcode
  uint8_t major_opcode = 62;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // src_drawable
  buf.Write(&src_drawable);

  // dst_drawable
  buf.Write(&dst_drawable);

  // gc
  buf.Write(&gc);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // dst_x
  buf.Write(&dst_x);

  // dst_y
  buf.Write(&dst_y);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "CopyArea", false);
}

Future<void> XProto::CopyArea(const Drawable& src_drawable,
                              const Drawable& dst_drawable,
                              const GraphicsContext& gc,
                              const int16_t& src_x,
                              const int16_t& src_y,
                              const int16_t& dst_x,
                              const int16_t& dst_y,
                              const uint16_t& width,
                              const uint16_t& height) {
  return XProto::CopyArea(CopyAreaRequest{src_drawable, dst_drawable, gc, src_x,
                                          src_y, dst_x, dst_y, width, height});
}

Future<void> XProto::CopyPlane(const CopyPlaneRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& src_drawable = request.src_drawable;
  auto& dst_drawable = request.dst_drawable;
  auto& gc = request.gc;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& dst_x = request.dst_x;
  auto& dst_y = request.dst_y;
  auto& width = request.width;
  auto& height = request.height;
  auto& bit_plane = request.bit_plane;

  // major_opcode
  uint8_t major_opcode = 63;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // src_drawable
  buf.Write(&src_drawable);

  // dst_drawable
  buf.Write(&dst_drawable);

  // gc
  buf.Write(&gc);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // dst_x
  buf.Write(&dst_x);

  // dst_y
  buf.Write(&dst_y);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // bit_plane
  buf.Write(&bit_plane);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "CopyPlane", false);
}

Future<void> XProto::CopyPlane(const Drawable& src_drawable,
                               const Drawable& dst_drawable,
                               const GraphicsContext& gc,
                               const int16_t& src_x,
                               const int16_t& src_y,
                               const int16_t& dst_x,
                               const int16_t& dst_y,
                               const uint16_t& width,
                               const uint16_t& height,
                               const uint32_t& bit_plane) {
  return XProto::CopyPlane(CopyPlaneRequest{src_drawable, dst_drawable, gc,
                                            src_x, src_y, dst_x, dst_y, width,
                                            height, bit_plane});
}

Future<void> XProto::PolyPoint(const PolyPointRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& coordinate_mode = request.coordinate_mode;
  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& points = request.points;
  size_t points_len = points.size();

  // major_opcode
  uint8_t major_opcode = 64;
  buf.Write(&major_opcode);

  // coordinate_mode
  uint8_t tmp99;
  tmp99 = static_cast<uint8_t>(coordinate_mode);
  buf.Write(&tmp99);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // points
  CHECK_EQ(static_cast<size_t>(points_len), points.size());
  for (auto& points_elem : points) {
    // points_elem
    {
      auto& x = points_elem.x;
      auto& y = points_elem.y;

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "PolyPoint", false);
}

Future<void> XProto::PolyPoint(const CoordMode& coordinate_mode,
                               const Drawable& drawable,
                               const GraphicsContext& gc,
                               const std::vector<Point>& points) {
  return XProto::PolyPoint(
      PolyPointRequest{coordinate_mode, drawable, gc, points});
}

Future<void> XProto::PolyLine(const PolyLineRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& coordinate_mode = request.coordinate_mode;
  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& points = request.points;
  size_t points_len = points.size();

  // major_opcode
  uint8_t major_opcode = 65;
  buf.Write(&major_opcode);

  // coordinate_mode
  uint8_t tmp100;
  tmp100 = static_cast<uint8_t>(coordinate_mode);
  buf.Write(&tmp100);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // points
  CHECK_EQ(static_cast<size_t>(points_len), points.size());
  for (auto& points_elem : points) {
    // points_elem
    {
      auto& x = points_elem.x;
      auto& y = points_elem.y;

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "PolyLine", false);
}

Future<void> XProto::PolyLine(const CoordMode& coordinate_mode,
                              const Drawable& drawable,
                              const GraphicsContext& gc,
                              const std::vector<Point>& points) {
  return XProto::PolyLine(
      PolyLineRequest{coordinate_mode, drawable, gc, points});
}

Future<void> XProto::PolySegment(const PolySegmentRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& segments = request.segments;
  size_t segments_len = segments.size();

  // major_opcode
  uint8_t major_opcode = 66;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // segments
  CHECK_EQ(static_cast<size_t>(segments_len), segments.size());
  for (auto& segments_elem : segments) {
    // segments_elem
    {
      auto& x1 = segments_elem.x1;
      auto& y1 = segments_elem.y1;
      auto& x2 = segments_elem.x2;
      auto& y2 = segments_elem.y2;

      // x1
      buf.Write(&x1);

      // y1
      buf.Write(&y1);

      // x2
      buf.Write(&x2);

      // y2
      buf.Write(&y2);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "PolySegment", false);
}

Future<void> XProto::PolySegment(const Drawable& drawable,
                                 const GraphicsContext& gc,
                                 const std::vector<Segment>& segments) {
  return XProto::PolySegment(PolySegmentRequest{drawable, gc, segments});
}

Future<void> XProto::PolyRectangle(const PolyRectangleRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& rectangles = request.rectangles;
  size_t rectangles_len = rectangles.size();

  // major_opcode
  uint8_t major_opcode = 67;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // rectangles
  CHECK_EQ(static_cast<size_t>(rectangles_len), rectangles.size());
  for (auto& rectangles_elem : rectangles) {
    // rectangles_elem
    {
      auto& x = rectangles_elem.x;
      auto& y = rectangles_elem.y;
      auto& width = rectangles_elem.width;
      auto& height = rectangles_elem.height;

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);

      // width
      buf.Write(&width);

      // height
      buf.Write(&height);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "PolyRectangle", false);
}

Future<void> XProto::PolyRectangle(const Drawable& drawable,
                                   const GraphicsContext& gc,
                                   const std::vector<Rectangle>& rectangles) {
  return XProto::PolyRectangle(PolyRectangleRequest{drawable, gc, rectangles});
}

Future<void> XProto::PolyArc(const PolyArcRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& arcs = request.arcs;
  size_t arcs_len = arcs.size();

  // major_opcode
  uint8_t major_opcode = 68;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // arcs
  CHECK_EQ(static_cast<size_t>(arcs_len), arcs.size());
  for (auto& arcs_elem : arcs) {
    // arcs_elem
    {
      auto& x = arcs_elem.x;
      auto& y = arcs_elem.y;
      auto& width = arcs_elem.width;
      auto& height = arcs_elem.height;
      auto& angle1 = arcs_elem.angle1;
      auto& angle2 = arcs_elem.angle2;

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);

      // width
      buf.Write(&width);

      // height
      buf.Write(&height);

      // angle1
      buf.Write(&angle1);

      // angle2
      buf.Write(&angle2);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "PolyArc", false);
}

Future<void> XProto::PolyArc(const Drawable& drawable,
                             const GraphicsContext& gc,
                             const std::vector<Arc>& arcs) {
  return XProto::PolyArc(PolyArcRequest{drawable, gc, arcs});
}

Future<void> XProto::FillPoly(const FillPolyRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& shape = request.shape;
  auto& coordinate_mode = request.coordinate_mode;
  auto& points = request.points;
  size_t points_len = points.size();

  // major_opcode
  uint8_t major_opcode = 69;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // shape
  uint8_t tmp101;
  tmp101 = static_cast<uint8_t>(shape);
  buf.Write(&tmp101);

  // coordinate_mode
  uint8_t tmp102;
  tmp102 = static_cast<uint8_t>(coordinate_mode);
  buf.Write(&tmp102);

  // pad1
  Pad(&buf, 2);

  // points
  CHECK_EQ(static_cast<size_t>(points_len), points.size());
  for (auto& points_elem : points) {
    // points_elem
    {
      auto& x = points_elem.x;
      auto& y = points_elem.y;

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "FillPoly", false);
}

Future<void> XProto::FillPoly(const Drawable& drawable,
                              const GraphicsContext& gc,
                              const PolyShape& shape,
                              const CoordMode& coordinate_mode,
                              const std::vector<Point>& points) {
  return XProto::FillPoly(
      FillPolyRequest{drawable, gc, shape, coordinate_mode, points});
}

Future<void> XProto::PolyFillRectangle(
    const PolyFillRectangleRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& rectangles = request.rectangles;
  size_t rectangles_len = rectangles.size();

  // major_opcode
  uint8_t major_opcode = 70;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // rectangles
  CHECK_EQ(static_cast<size_t>(rectangles_len), rectangles.size());
  for (auto& rectangles_elem : rectangles) {
    // rectangles_elem
    {
      auto& x = rectangles_elem.x;
      auto& y = rectangles_elem.y;
      auto& width = rectangles_elem.width;
      auto& height = rectangles_elem.height;

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);

      // width
      buf.Write(&width);

      // height
      buf.Write(&height);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "PolyFillRectangle", false);
}

Future<void> XProto::PolyFillRectangle(
    const Drawable& drawable,
    const GraphicsContext& gc,
    const std::vector<Rectangle>& rectangles) {
  return XProto::PolyFillRectangle(
      PolyFillRectangleRequest{drawable, gc, rectangles});
}

Future<void> XProto::PolyFillArc(const PolyFillArcRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& arcs = request.arcs;
  size_t arcs_len = arcs.size();

  // major_opcode
  uint8_t major_opcode = 71;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // arcs
  CHECK_EQ(static_cast<size_t>(arcs_len), arcs.size());
  for (auto& arcs_elem : arcs) {
    // arcs_elem
    {
      auto& x = arcs_elem.x;
      auto& y = arcs_elem.y;
      auto& width = arcs_elem.width;
      auto& height = arcs_elem.height;
      auto& angle1 = arcs_elem.angle1;
      auto& angle2 = arcs_elem.angle2;

      // x
      buf.Write(&x);

      // y
      buf.Write(&y);

      // width
      buf.Write(&width);

      // height
      buf.Write(&height);

      // angle1
      buf.Write(&angle1);

      // angle2
      buf.Write(&angle2);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "PolyFillArc", false);
}

Future<void> XProto::PolyFillArc(const Drawable& drawable,
                                 const GraphicsContext& gc,
                                 const std::vector<Arc>& arcs) {
  return XProto::PolyFillArc(PolyFillArcRequest{drawable, gc, arcs});
}

Future<void> XProto::PutImage(const PutImageRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& format = request.format;
  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& width = request.width;
  auto& height = request.height;
  auto& dst_x = request.dst_x;
  auto& dst_y = request.dst_y;
  auto& left_pad = request.left_pad;
  auto& depth = request.depth;
  auto& data = request.data;
  size_t data_len = data ? data->size() : 0;

  // major_opcode
  uint8_t major_opcode = 72;
  buf.Write(&major_opcode);

  // format
  uint8_t tmp103;
  tmp103 = static_cast<uint8_t>(format);
  buf.Write(&tmp103);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // dst_x
  buf.Write(&dst_x);

  // dst_y
  buf.Write(&dst_y);

  // left_pad
  buf.Write(&left_pad);

  // depth
  buf.Write(&depth);

  // pad0
  Pad(&buf, 2);

  // data
  buf.AppendSizedBuffer(data);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "PutImage", false);
}

Future<void> XProto::PutImage(
    const ImageFormat& format,
    const Drawable& drawable,
    const GraphicsContext& gc,
    const uint16_t& width,
    const uint16_t& height,
    const int16_t& dst_x,
    const int16_t& dst_y,
    const uint8_t& left_pad,
    const uint8_t& depth,
    const scoped_refptr<base::RefCountedMemory>& data) {
  return XProto::PutImage(PutImageRequest{format, drawable, gc, width, height,
                                          dst_x, dst_y, left_pad, depth, data});
}

Future<GetImageReply> XProto::GetImage(const GetImageRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& format = request.format;
  auto& drawable = request.drawable;
  auto& x = request.x;
  auto& y = request.y;
  auto& width = request.width;
  auto& height = request.height;
  auto& plane_mask = request.plane_mask;

  // major_opcode
  uint8_t major_opcode = 73;
  buf.Write(&major_opcode);

  // format
  uint8_t tmp104;
  tmp104 = static_cast<uint8_t>(format);
  buf.Write(&tmp104);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  // plane_mask
  buf.Write(&plane_mask);

  Align(&buf, 4);

  return connection_->SendRequest<GetImageReply>(&buf, "GetImage", false);
}

Future<GetImageReply> XProto::GetImage(const ImageFormat& format,
                                       const Drawable& drawable,
                                       const int16_t& x,
                                       const int16_t& y,
                                       const uint16_t& width,
                                       const uint16_t& height,
                                       const uint32_t& plane_mask) {
  return XProto::GetImage(
      GetImageRequest{format, drawable, x, y, width, height, plane_mask});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetImageReply> detail::ReadReply<GetImageReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetImageReply>();

  auto& depth = (*reply).depth;
  auto& sequence = (*reply).sequence;
  auto& visual = (*reply).visual;
  auto& data = (*reply).data;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // depth
  Read(&depth, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // visual
  Read(&visual, &buf);

  // pad0
  Pad(&buf, 20);

  // data
  data = buffer->ReadAndAdvance((length) * (4));

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::PolyText8(const PolyText8Request& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& x = request.x;
  auto& y = request.y;
  auto& items = request.items;
  size_t items_len = items.size();

  // major_opcode
  uint8_t major_opcode = 74;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  // items
  CHECK_EQ(static_cast<size_t>(items_len), items.size());
  for (auto& items_elem : items) {
    // items_elem
    buf.Write(&items_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "PolyText8", false);
}

Future<void> XProto::PolyText8(const Drawable& drawable,
                               const GraphicsContext& gc,
                               const int16_t& x,
                               const int16_t& y,
                               const std::vector<uint8_t>& items) {
  return XProto::PolyText8(PolyText8Request{drawable, gc, x, y, items});
}

Future<void> XProto::PolyText16(const PolyText16Request& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& x = request.x;
  auto& y = request.y;
  auto& items = request.items;
  size_t items_len = items.size();

  // major_opcode
  uint8_t major_opcode = 75;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  // items
  CHECK_EQ(static_cast<size_t>(items_len), items.size());
  for (auto& items_elem : items) {
    // items_elem
    buf.Write(&items_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "PolyText16", false);
}

Future<void> XProto::PolyText16(const Drawable& drawable,
                                const GraphicsContext& gc,
                                const int16_t& x,
                                const int16_t& y,
                                const std::vector<uint8_t>& items) {
  return XProto::PolyText16(PolyText16Request{drawable, gc, x, y, items});
}

Future<void> XProto::ImageText8(const ImageText8Request& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  uint8_t string_len{};
  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& x = request.x;
  auto& y = request.y;
  auto& string = request.string;

  // major_opcode
  uint8_t major_opcode = 76;
  buf.Write(&major_opcode);

  // string_len
  string_len = string.size();
  buf.Write(&string_len);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  // string
  CHECK_EQ(static_cast<size_t>(string_len), string.size());
  for (auto& string_elem : string) {
    // string_elem
    buf.Write(&string_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ImageText8", false);
}

Future<void> XProto::ImageText8(const Drawable& drawable,
                                const GraphicsContext& gc,
                                const int16_t& x,
                                const int16_t& y,
                                const std::string& string) {
  return XProto::ImageText8(ImageText8Request{drawable, gc, x, y, string});
}

Future<void> XProto::ImageText16(const ImageText16Request& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  uint8_t string_len{};
  auto& drawable = request.drawable;
  auto& gc = request.gc;
  auto& x = request.x;
  auto& y = request.y;
  auto& string = request.string;

  // major_opcode
  uint8_t major_opcode = 77;
  buf.Write(&major_opcode);

  // string_len
  string_len = string.size();
  buf.Write(&string_len);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // gc
  buf.Write(&gc);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  // string
  CHECK_EQ(static_cast<size_t>(string_len), string.size());
  for (auto& string_elem : string) {
    // string_elem
    {
      auto& byte1 = string_elem.byte1;
      auto& byte2 = string_elem.byte2;

      // byte1
      buf.Write(&byte1);

      // byte2
      buf.Write(&byte2);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ImageText16", false);
}

Future<void> XProto::ImageText16(const Drawable& drawable,
                                 const GraphicsContext& gc,
                                 const int16_t& x,
                                 const int16_t& y,
                                 const std::vector<Char16>& string) {
  return XProto::ImageText16(ImageText16Request{drawable, gc, x, y, string});
}

Future<void> XProto::CreateColormap(const CreateColormapRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& alloc = request.alloc;
  auto& mid = request.mid;
  auto& window = request.window;
  auto& visual = request.visual;

  // major_opcode
  uint8_t major_opcode = 78;
  buf.Write(&major_opcode);

  // alloc
  uint8_t tmp105;
  tmp105 = static_cast<uint8_t>(alloc);
  buf.Write(&tmp105);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // mid
  buf.Write(&mid);

  // window
  buf.Write(&window);

  // visual
  buf.Write(&visual);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "CreateColormap", false);
}

Future<void> XProto::CreateColormap(const ColormapAlloc& alloc,
                                    const ColorMap& mid,
                                    const Window& window,
                                    const VisualId& visual) {
  return XProto::CreateColormap(
      CreateColormapRequest{alloc, mid, window, visual});
}

Future<void> XProto::FreeColormap(const FreeColormapRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cmap = request.cmap;

  // major_opcode
  uint8_t major_opcode = 79;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "FreeColormap", false);
}

Future<void> XProto::FreeColormap(const ColorMap& cmap) {
  return XProto::FreeColormap(FreeColormapRequest{cmap});
}

Future<void> XProto::CopyColormapAndFree(
    const CopyColormapAndFreeRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& mid = request.mid;
  auto& src_cmap = request.src_cmap;

  // major_opcode
  uint8_t major_opcode = 80;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // mid
  buf.Write(&mid);

  // src_cmap
  buf.Write(&src_cmap);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "CopyColormapAndFree", false);
}

Future<void> XProto::CopyColormapAndFree(const ColorMap& mid,
                                         const ColorMap& src_cmap) {
  return XProto::CopyColormapAndFree(CopyColormapAndFreeRequest{mid, src_cmap});
}

Future<void> XProto::InstallColormap(const InstallColormapRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cmap = request.cmap;

  // major_opcode
  uint8_t major_opcode = 81;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "InstallColormap", false);
}

Future<void> XProto::InstallColormap(const ColorMap& cmap) {
  return XProto::InstallColormap(InstallColormapRequest{cmap});
}

Future<void> XProto::UninstallColormap(
    const UninstallColormapRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cmap = request.cmap;

  // major_opcode
  uint8_t major_opcode = 82;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "UninstallColormap", false);
}

Future<void> XProto::UninstallColormap(const ColorMap& cmap) {
  return XProto::UninstallColormap(UninstallColormapRequest{cmap});
}

Future<ListInstalledColormapsReply> XProto::ListInstalledColormaps(
    const ListInstalledColormapsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = 83;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<ListInstalledColormapsReply>(
      &buf, "ListInstalledColormaps", false);
}

Future<ListInstalledColormapsReply> XProto::ListInstalledColormaps(
    const Window& window) {
  return XProto::ListInstalledColormaps(ListInstalledColormapsRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<ListInstalledColormapsReply> detail::ReadReply<
    ListInstalledColormapsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<ListInstalledColormapsReply>();

  auto& sequence = (*reply).sequence;
  uint16_t cmaps_len{};
  auto& cmaps = (*reply).cmaps;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // cmaps_len
  Read(&cmaps_len, &buf);

  // pad1
  Pad(&buf, 22);

  // cmaps
  cmaps.resize(cmaps_len);
  for (auto& cmaps_elem : cmaps) {
    // cmaps_elem
    Read(&cmaps_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<AllocColorReply> XProto::AllocColor(const AllocColorRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cmap = request.cmap;
  auto& red = request.red;
  auto& green = request.green;
  auto& blue = request.blue;

  // major_opcode
  uint8_t major_opcode = 84;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  // red
  buf.Write(&red);

  // green
  buf.Write(&green);

  // blue
  buf.Write(&blue);

  // pad1
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<AllocColorReply>(&buf, "AllocColor", false);
}

Future<AllocColorReply> XProto::AllocColor(const ColorMap& cmap,
                                           const uint16_t& red,
                                           const uint16_t& green,
                                           const uint16_t& blue) {
  return XProto::AllocColor(AllocColorRequest{cmap, red, green, blue});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<AllocColorReply> detail::ReadReply<AllocColorReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<AllocColorReply>();

  auto& sequence = (*reply).sequence;
  auto& red = (*reply).red;
  auto& green = (*reply).green;
  auto& blue = (*reply).blue;
  auto& pixel = (*reply).pixel;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // red
  Read(&red, &buf);

  // green
  Read(&green, &buf);

  // blue
  Read(&blue, &buf);

  // pad1
  Pad(&buf, 2);

  // pixel
  Read(&pixel, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<AllocNamedColorReply> XProto::AllocNamedColor(
    const AllocNamedColorRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cmap = request.cmap;
  uint16_t name_len{};
  auto& name = request.name;

  // major_opcode
  uint8_t major_opcode = 85;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  // name_len
  name_len = name.size();
  buf.Write(&name_len);

  // pad1
  Pad(&buf, 2);

  // name
  CHECK_EQ(static_cast<size_t>(name_len), name.size());
  for (auto& name_elem : name) {
    // name_elem
    buf.Write(&name_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<AllocNamedColorReply>(&buf, "AllocNamedColor",
                                                        false);
}

Future<AllocNamedColorReply> XProto::AllocNamedColor(const ColorMap& cmap,
                                                     const std::string& name) {
  return XProto::AllocNamedColor(AllocNamedColorRequest{cmap, name});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<AllocNamedColorReply> detail::ReadReply<AllocNamedColorReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<AllocNamedColorReply>();

  auto& sequence = (*reply).sequence;
  auto& pixel = (*reply).pixel;
  auto& exact_red = (*reply).exact_red;
  auto& exact_green = (*reply).exact_green;
  auto& exact_blue = (*reply).exact_blue;
  auto& visual_red = (*reply).visual_red;
  auto& visual_green = (*reply).visual_green;
  auto& visual_blue = (*reply).visual_blue;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // pixel
  Read(&pixel, &buf);

  // exact_red
  Read(&exact_red, &buf);

  // exact_green
  Read(&exact_green, &buf);

  // exact_blue
  Read(&exact_blue, &buf);

  // visual_red
  Read(&visual_red, &buf);

  // visual_green
  Read(&visual_green, &buf);

  // visual_blue
  Read(&visual_blue, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<AllocColorCellsReply> XProto::AllocColorCells(
    const AllocColorCellsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& contiguous = request.contiguous;
  auto& cmap = request.cmap;
  auto& colors = request.colors;
  auto& planes = request.planes;

  // major_opcode
  uint8_t major_opcode = 86;
  buf.Write(&major_opcode);

  // contiguous
  buf.Write(&contiguous);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  // colors
  buf.Write(&colors);

  // planes
  buf.Write(&planes);

  Align(&buf, 4);

  return connection_->SendRequest<AllocColorCellsReply>(&buf, "AllocColorCells",
                                                        false);
}

Future<AllocColorCellsReply> XProto::AllocColorCells(const uint8_t& contiguous,
                                                     const ColorMap& cmap,
                                                     const uint16_t& colors,
                                                     const uint16_t& planes) {
  return XProto::AllocColorCells(
      AllocColorCellsRequest{contiguous, cmap, colors, planes});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<AllocColorCellsReply> detail::ReadReply<AllocColorCellsReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<AllocColorCellsReply>();

  auto& sequence = (*reply).sequence;
  uint16_t pixels_len{};
  uint16_t masks_len{};
  auto& pixels = (*reply).pixels;
  auto& masks = (*reply).masks;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // pixels_len
  Read(&pixels_len, &buf);

  // masks_len
  Read(&masks_len, &buf);

  // pad1
  Pad(&buf, 20);

  // pixels
  pixels.resize(pixels_len);
  for (auto& pixels_elem : pixels) {
    // pixels_elem
    Read(&pixels_elem, &buf);
  }

  // masks
  masks.resize(masks_len);
  for (auto& masks_elem : masks) {
    // masks_elem
    Read(&masks_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<AllocColorPlanesReply> XProto::AllocColorPlanes(
    const AllocColorPlanesRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& contiguous = request.contiguous;
  auto& cmap = request.cmap;
  auto& colors = request.colors;
  auto& reds = request.reds;
  auto& greens = request.greens;
  auto& blues = request.blues;

  // major_opcode
  uint8_t major_opcode = 87;
  buf.Write(&major_opcode);

  // contiguous
  buf.Write(&contiguous);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  // colors
  buf.Write(&colors);

  // reds
  buf.Write(&reds);

  // greens
  buf.Write(&greens);

  // blues
  buf.Write(&blues);

  Align(&buf, 4);

  return connection_->SendRequest<AllocColorPlanesReply>(
      &buf, "AllocColorPlanes", false);
}

Future<AllocColorPlanesReply> XProto::AllocColorPlanes(
    const uint8_t& contiguous,
    const ColorMap& cmap,
    const uint16_t& colors,
    const uint16_t& reds,
    const uint16_t& greens,
    const uint16_t& blues) {
  return XProto::AllocColorPlanes(
      AllocColorPlanesRequest{contiguous, cmap, colors, reds, greens, blues});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<AllocColorPlanesReply> detail::ReadReply<AllocColorPlanesReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<AllocColorPlanesReply>();

  auto& sequence = (*reply).sequence;
  uint16_t pixels_len{};
  auto& red_mask = (*reply).red_mask;
  auto& green_mask = (*reply).green_mask;
  auto& blue_mask = (*reply).blue_mask;
  auto& pixels = (*reply).pixels;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // pixels_len
  Read(&pixels_len, &buf);

  // pad1
  Pad(&buf, 2);

  // red_mask
  Read(&red_mask, &buf);

  // green_mask
  Read(&green_mask, &buf);

  // blue_mask
  Read(&blue_mask, &buf);

  // pad2
  Pad(&buf, 8);

  // pixels
  pixels.resize(pixels_len);
  for (auto& pixels_elem : pixels) {
    // pixels_elem
    Read(&pixels_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::FreeColors(const FreeColorsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cmap = request.cmap;
  auto& plane_mask = request.plane_mask;
  auto& pixels = request.pixels;
  size_t pixels_len = pixels.size();

  // major_opcode
  uint8_t major_opcode = 88;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  // plane_mask
  buf.Write(&plane_mask);

  // pixels
  CHECK_EQ(static_cast<size_t>(pixels_len), pixels.size());
  for (auto& pixels_elem : pixels) {
    // pixels_elem
    buf.Write(&pixels_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "FreeColors", false);
}

Future<void> XProto::FreeColors(const ColorMap& cmap,
                                const uint32_t& plane_mask,
                                const std::vector<uint32_t>& pixels) {
  return XProto::FreeColors(FreeColorsRequest{cmap, plane_mask, pixels});
}

Future<void> XProto::StoreColors(const StoreColorsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cmap = request.cmap;
  auto& items = request.items;
  size_t items_len = items.size();

  // major_opcode
  uint8_t major_opcode = 89;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  // items
  CHECK_EQ(static_cast<size_t>(items_len), items.size());
  for (auto& items_elem : items) {
    // items_elem
    {
      auto& pixel = items_elem.pixel;
      auto& red = items_elem.red;
      auto& green = items_elem.green;
      auto& blue = items_elem.blue;
      auto& flags = items_elem.flags;

      // pixel
      buf.Write(&pixel);

      // red
      buf.Write(&red);

      // green
      buf.Write(&green);

      // blue
      buf.Write(&blue);

      // flags
      uint8_t tmp106;
      tmp106 = static_cast<uint8_t>(flags);
      buf.Write(&tmp106);

      // pad0
      Pad(&buf, 1);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "StoreColors", false);
}

Future<void> XProto::StoreColors(const ColorMap& cmap,
                                 const std::vector<ColorItem>& items) {
  return XProto::StoreColors(StoreColorsRequest{cmap, items});
}

Future<void> XProto::StoreNamedColor(const StoreNamedColorRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& flags = request.flags;
  auto& cmap = request.cmap;
  auto& pixel = request.pixel;
  uint16_t name_len{};
  auto& name = request.name;

  // major_opcode
  uint8_t major_opcode = 90;
  buf.Write(&major_opcode);

  // flags
  uint8_t tmp107;
  tmp107 = static_cast<uint8_t>(flags);
  buf.Write(&tmp107);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  // pixel
  buf.Write(&pixel);

  // name_len
  name_len = name.size();
  buf.Write(&name_len);

  // pad0
  Pad(&buf, 2);

  // name
  CHECK_EQ(static_cast<size_t>(name_len), name.size());
  for (auto& name_elem : name) {
    // name_elem
    buf.Write(&name_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "StoreNamedColor", false);
}

Future<void> XProto::StoreNamedColor(const ColorFlag& flags,
                                     const ColorMap& cmap,
                                     const uint32_t& pixel,
                                     const std::string& name) {
  return XProto::StoreNamedColor(
      StoreNamedColorRequest{flags, cmap, pixel, name});
}

Future<QueryColorsReply> XProto::QueryColors(
    const QueryColorsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cmap = request.cmap;
  auto& pixels = request.pixels;
  size_t pixels_len = pixels.size();

  // major_opcode
  uint8_t major_opcode = 91;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  // pixels
  CHECK_EQ(static_cast<size_t>(pixels_len), pixels.size());
  for (auto& pixels_elem : pixels) {
    // pixels_elem
    buf.Write(&pixels_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<QueryColorsReply>(&buf, "QueryColors", false);
}

Future<QueryColorsReply> XProto::QueryColors(
    const ColorMap& cmap,
    const std::vector<uint32_t>& pixels) {
  return XProto::QueryColors(QueryColorsRequest{cmap, pixels});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<QueryColorsReply> detail::ReadReply<QueryColorsReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<QueryColorsReply>();

  auto& sequence = (*reply).sequence;
  uint16_t colors_len{};
  auto& colors = (*reply).colors;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // colors_len
  Read(&colors_len, &buf);

  // pad1
  Pad(&buf, 22);

  // colors
  colors.resize(colors_len);
  for (auto& colors_elem : colors) {
    // colors_elem
    {
      auto& red = colors_elem.red;
      auto& green = colors_elem.green;
      auto& blue = colors_elem.blue;

      // red
      Read(&red, &buf);

      // green
      Read(&green, &buf);

      // blue
      Read(&blue, &buf);

      // pad0
      Pad(&buf, 2);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<LookupColorReply> XProto::LookupColor(
    const LookupColorRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cmap = request.cmap;
  uint16_t name_len{};
  auto& name = request.name;

  // major_opcode
  uint8_t major_opcode = 92;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cmap
  buf.Write(&cmap);

  // name_len
  name_len = name.size();
  buf.Write(&name_len);

  // pad1
  Pad(&buf, 2);

  // name
  CHECK_EQ(static_cast<size_t>(name_len), name.size());
  for (auto& name_elem : name) {
    // name_elem
    buf.Write(&name_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<LookupColorReply>(&buf, "LookupColor", false);
}

Future<LookupColorReply> XProto::LookupColor(const ColorMap& cmap,
                                             const std::string& name) {
  return XProto::LookupColor(LookupColorRequest{cmap, name});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<LookupColorReply> detail::ReadReply<LookupColorReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<LookupColorReply>();

  auto& sequence = (*reply).sequence;
  auto& exact_red = (*reply).exact_red;
  auto& exact_green = (*reply).exact_green;
  auto& exact_blue = (*reply).exact_blue;
  auto& visual_red = (*reply).visual_red;
  auto& visual_green = (*reply).visual_green;
  auto& visual_blue = (*reply).visual_blue;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // exact_red
  Read(&exact_red, &buf);

  // exact_green
  Read(&exact_green, &buf);

  // exact_blue
  Read(&exact_blue, &buf);

  // visual_red
  Read(&visual_red, &buf);

  // visual_green
  Read(&visual_green, &buf);

  // visual_blue
  Read(&visual_blue, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::CreateCursor(const CreateCursorRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cid = request.cid;
  auto& source = request.source;
  auto& mask = request.mask;
  auto& fore_red = request.fore_red;
  auto& fore_green = request.fore_green;
  auto& fore_blue = request.fore_blue;
  auto& back_red = request.back_red;
  auto& back_green = request.back_green;
  auto& back_blue = request.back_blue;
  auto& x = request.x;
  auto& y = request.y;

  // major_opcode
  uint8_t major_opcode = 93;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cid
  buf.Write(&cid);

  // source
  buf.Write(&source);

  // mask
  buf.Write(&mask);

  // fore_red
  buf.Write(&fore_red);

  // fore_green
  buf.Write(&fore_green);

  // fore_blue
  buf.Write(&fore_blue);

  // back_red
  buf.Write(&back_red);

  // back_green
  buf.Write(&back_green);

  // back_blue
  buf.Write(&back_blue);

  // x
  buf.Write(&x);

  // y
  buf.Write(&y);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "CreateCursor", false);
}

Future<void> XProto::CreateCursor(const Cursor& cid,
                                  const Pixmap& source,
                                  const Pixmap& mask,
                                  const uint16_t& fore_red,
                                  const uint16_t& fore_green,
                                  const uint16_t& fore_blue,
                                  const uint16_t& back_red,
                                  const uint16_t& back_green,
                                  const uint16_t& back_blue,
                                  const uint16_t& x,
                                  const uint16_t& y) {
  return XProto::CreateCursor(
      CreateCursorRequest{cid, source, mask, fore_red, fore_green, fore_blue,
                          back_red, back_green, back_blue, x, y});
}

Future<void> XProto::CreateGlyphCursor(
    const CreateGlyphCursorRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cid = request.cid;
  auto& source_font = request.source_font;
  auto& mask_font = request.mask_font;
  auto& source_char = request.source_char;
  auto& mask_char = request.mask_char;
  auto& fore_red = request.fore_red;
  auto& fore_green = request.fore_green;
  auto& fore_blue = request.fore_blue;
  auto& back_red = request.back_red;
  auto& back_green = request.back_green;
  auto& back_blue = request.back_blue;

  // major_opcode
  uint8_t major_opcode = 94;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cid
  buf.Write(&cid);

  // source_font
  buf.Write(&source_font);

  // mask_font
  buf.Write(&mask_font);

  // source_char
  buf.Write(&source_char);

  // mask_char
  buf.Write(&mask_char);

  // fore_red
  buf.Write(&fore_red);

  // fore_green
  buf.Write(&fore_green);

  // fore_blue
  buf.Write(&fore_blue);

  // back_red
  buf.Write(&back_red);

  // back_green
  buf.Write(&back_green);

  // back_blue
  buf.Write(&back_blue);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "CreateGlyphCursor", false);
}

Future<void> XProto::CreateGlyphCursor(const Cursor& cid,
                                       const Font& source_font,
                                       const Font& mask_font,
                                       const uint16_t& source_char,
                                       const uint16_t& mask_char,
                                       const uint16_t& fore_red,
                                       const uint16_t& fore_green,
                                       const uint16_t& fore_blue,
                                       const uint16_t& back_red,
                                       const uint16_t& back_green,
                                       const uint16_t& back_blue) {
  return XProto::CreateGlyphCursor(CreateGlyphCursorRequest{
      cid, source_font, mask_font, source_char, mask_char, fore_red, fore_green,
      fore_blue, back_red, back_green, back_blue});
}

Future<void> XProto::FreeCursor(const FreeCursorRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cursor = request.cursor;

  // major_opcode
  uint8_t major_opcode = 95;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cursor
  buf.Write(&cursor);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "FreeCursor", false);
}

Future<void> XProto::FreeCursor(const Cursor& cursor) {
  return XProto::FreeCursor(FreeCursorRequest{cursor});
}

Future<void> XProto::RecolorCursor(const RecolorCursorRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& cursor = request.cursor;
  auto& fore_red = request.fore_red;
  auto& fore_green = request.fore_green;
  auto& fore_blue = request.fore_blue;
  auto& back_red = request.back_red;
  auto& back_green = request.back_green;
  auto& back_blue = request.back_blue;

  // major_opcode
  uint8_t major_opcode = 96;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cursor
  buf.Write(&cursor);

  // fore_red
  buf.Write(&fore_red);

  // fore_green
  buf.Write(&fore_green);

  // fore_blue
  buf.Write(&fore_blue);

  // back_red
  buf.Write(&back_red);

  // back_green
  buf.Write(&back_green);

  // back_blue
  buf.Write(&back_blue);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RecolorCursor", false);
}

Future<void> XProto::RecolorCursor(const Cursor& cursor,
                                   const uint16_t& fore_red,
                                   const uint16_t& fore_green,
                                   const uint16_t& fore_blue,
                                   const uint16_t& back_red,
                                   const uint16_t& back_green,
                                   const uint16_t& back_blue) {
  return XProto::RecolorCursor(
      RecolorCursorRequest{cursor, fore_red, fore_green, fore_blue, back_red,
                           back_green, back_blue});
}

Future<QueryBestSizeReply> XProto::QueryBestSize(
    const QueryBestSizeRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& c_class = request.c_class;
  auto& drawable = request.drawable;
  auto& width = request.width;
  auto& height = request.height;

  // major_opcode
  uint8_t major_opcode = 97;
  buf.Write(&major_opcode);

  // c_class
  uint8_t tmp108;
  tmp108 = static_cast<uint8_t>(c_class);
  buf.Write(&tmp108);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // width
  buf.Write(&width);

  // height
  buf.Write(&height);

  Align(&buf, 4);

  return connection_->SendRequest<QueryBestSizeReply>(&buf, "QueryBestSize",
                                                      false);
}

Future<QueryBestSizeReply> XProto::QueryBestSize(const QueryShapeOf& c_class,
                                                 const Drawable& drawable,
                                                 const uint16_t& width,
                                                 const uint16_t& height) {
  return XProto::QueryBestSize(
      QueryBestSizeRequest{c_class, drawable, width, height});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<QueryBestSizeReply> detail::ReadReply<QueryBestSizeReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<QueryBestSizeReply>();

  auto& sequence = (*reply).sequence;
  auto& width = (*reply).width;
  auto& height = (*reply).height;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<QueryExtensionReply> XProto::QueryExtension(
    const QueryExtensionRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  uint16_t name_len{};
  auto& name = request.name;

  // major_opcode
  uint8_t major_opcode = 98;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // name_len
  name_len = name.size();
  buf.Write(&name_len);

  // pad1
  Pad(&buf, 2);

  // name
  CHECK_EQ(static_cast<size_t>(name_len), name.size());
  for (auto& name_elem : name) {
    // name_elem
    buf.Write(&name_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<QueryExtensionReply>(&buf, "QueryExtension",
                                                       false);
}

Future<QueryExtensionReply> XProto::QueryExtension(const std::string& name) {
  return XProto::QueryExtension(QueryExtensionRequest{name});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<QueryExtensionReply> detail::ReadReply<QueryExtensionReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<QueryExtensionReply>();

  auto& sequence = (*reply).sequence;
  auto& present = (*reply).present;
  auto& major_opcode = (*reply).major_opcode;
  auto& first_event = (*reply).first_event;
  auto& first_error = (*reply).first_error;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // present
  Read(&present, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  // first_event
  Read(&first_event, &buf);

  // first_error
  Read(&first_error, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<ListExtensionsReply> XProto::ListExtensions(
    const ListExtensionsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 99;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<ListExtensionsReply>(&buf, "ListExtensions",
                                                       false);
}

Future<ListExtensionsReply> XProto::ListExtensions() {
  return XProto::ListExtensions(ListExtensionsRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<ListExtensionsReply> detail::ReadReply<ListExtensionsReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<ListExtensionsReply>();

  uint8_t names_len{};
  auto& sequence = (*reply).sequence;
  auto& names = (*reply).names;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // names_len
  Read(&names_len, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // pad0
  Pad(&buf, 24);

  // names
  names.resize(names_len);
  for (auto& names_elem : names) {
    // names_elem
    {
      uint8_t name_len{};
      auto& name = names_elem.name;

      // name_len
      Read(&name_len, &buf);

      // name
      name.resize(name_len);
      for (auto& name_elem : name) {
        // name_elem
        Read(&name_elem, &buf);
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::ChangeKeyboardMapping(
    const ChangeKeyboardMappingRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& keycode_count = request.keycode_count;
  auto& first_keycode = request.first_keycode;
  auto& keysyms_per_keycode = request.keysyms_per_keycode;
  auto& keysyms = request.keysyms;
  size_t keysyms_len = keysyms.size();

  // major_opcode
  uint8_t major_opcode = 100;
  buf.Write(&major_opcode);

  // keycode_count
  buf.Write(&keycode_count);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // first_keycode
  buf.Write(&first_keycode);

  // keysyms_per_keycode
  buf.Write(&keysyms_per_keycode);

  // pad0
  Pad(&buf, 2);

  // keysyms
  CHECK_EQ(static_cast<size_t>((keycode_count) * (keysyms_per_keycode)),
           keysyms.size());
  for (auto& keysyms_elem : keysyms) {
    // keysyms_elem
    buf.Write(&keysyms_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ChangeKeyboardMapping", false);
}

Future<void> XProto::ChangeKeyboardMapping(const uint8_t& keycode_count,
                                           const KeyCode& first_keycode,
                                           const uint8_t& keysyms_per_keycode,
                                           const std::vector<KeySym>& keysyms) {
  return XProto::ChangeKeyboardMapping(ChangeKeyboardMappingRequest{
      keycode_count, first_keycode, keysyms_per_keycode, keysyms});
}

Future<GetKeyboardMappingReply> XProto::GetKeyboardMapping(
    const GetKeyboardMappingRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& first_keycode = request.first_keycode;
  auto& count = request.count;

  // major_opcode
  uint8_t major_opcode = 101;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // first_keycode
  buf.Write(&first_keycode);

  // count
  buf.Write(&count);

  Align(&buf, 4);

  return connection_->SendRequest<GetKeyboardMappingReply>(
      &buf, "GetKeyboardMapping", false);
}

Future<GetKeyboardMappingReply> XProto::GetKeyboardMapping(
    const KeyCode& first_keycode,
    const uint8_t& count) {
  return XProto::GetKeyboardMapping(
      GetKeyboardMappingRequest{first_keycode, count});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetKeyboardMappingReply> detail::ReadReply<
    GetKeyboardMappingReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetKeyboardMappingReply>();

  auto& keysyms_per_keycode = (*reply).keysyms_per_keycode;
  auto& sequence = (*reply).sequence;
  auto& keysyms = (*reply).keysyms;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // keysyms_per_keycode
  Read(&keysyms_per_keycode, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // pad0
  Pad(&buf, 24);

  // keysyms
  keysyms.resize(length);
  for (auto& keysyms_elem : keysyms) {
    // keysyms_elem
    Read(&keysyms_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::ChangeKeyboardControl(
    const ChangeKeyboardControlRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  Keyboard value_mask{};
  auto& value_list = request;

  // major_opcode
  uint8_t major_opcode = 102;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // value_mask
  SwitchVar(Keyboard::KeyClickPercent, value_list.key_click_percent.has_value(),
            true, &value_mask);
  SwitchVar(Keyboard::BellPercent, value_list.bell_percent.has_value(), true,
            &value_mask);
  SwitchVar(Keyboard::BellPitch, value_list.bell_pitch.has_value(), true,
            &value_mask);
  SwitchVar(Keyboard::BellDuration, value_list.bell_duration.has_value(), true,
            &value_mask);
  SwitchVar(Keyboard::Led, value_list.led.has_value(), true, &value_mask);
  SwitchVar(Keyboard::LedMode, value_list.led_mode.has_value(), true,
            &value_mask);
  SwitchVar(Keyboard::Key, value_list.key.has_value(), true, &value_mask);
  SwitchVar(Keyboard::AutoRepeatMode, value_list.auto_repeat_mode.has_value(),
            true, &value_mask);
  uint32_t tmp109;
  tmp109 = static_cast<uint32_t>(value_mask);
  buf.Write(&tmp109);

  // value_list
  auto value_list_expr = value_mask;
  if (CaseAnd(value_list_expr, Keyboard::KeyClickPercent)) {
    auto& key_click_percent = *value_list.key_click_percent;

    // key_click_percent
    buf.Write(&key_click_percent);
  }
  if (CaseAnd(value_list_expr, Keyboard::BellPercent)) {
    auto& bell_percent = *value_list.bell_percent;

    // bell_percent
    buf.Write(&bell_percent);
  }
  if (CaseAnd(value_list_expr, Keyboard::BellPitch)) {
    auto& bell_pitch = *value_list.bell_pitch;

    // bell_pitch
    buf.Write(&bell_pitch);
  }
  if (CaseAnd(value_list_expr, Keyboard::BellDuration)) {
    auto& bell_duration = *value_list.bell_duration;

    // bell_duration
    buf.Write(&bell_duration);
  }
  if (CaseAnd(value_list_expr, Keyboard::Led)) {
    auto& led = *value_list.led;

    // led
    buf.Write(&led);
  }
  if (CaseAnd(value_list_expr, Keyboard::LedMode)) {
    auto& led_mode = *value_list.led_mode;

    // led_mode
    uint32_t tmp110;
    tmp110 = static_cast<uint32_t>(led_mode);
    buf.Write(&tmp110);
  }
  if (CaseAnd(value_list_expr, Keyboard::Key)) {
    auto& key = *value_list.key;

    // key
    buf.Write(&key);
  }
  if (CaseAnd(value_list_expr, Keyboard::AutoRepeatMode)) {
    auto& auto_repeat_mode = *value_list.auto_repeat_mode;

    // auto_repeat_mode
    uint32_t tmp111;
    tmp111 = static_cast<uint32_t>(auto_repeat_mode);
    buf.Write(&tmp111);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ChangeKeyboardControl", false);
}

Future<void> XProto::ChangeKeyboardControl(
    const std::optional<int32_t>& key_click_percent,
    const std::optional<int32_t>& bell_percent,
    const std::optional<int32_t>& bell_pitch,
    const std::optional<int32_t>& bell_duration,
    const std::optional<uint32_t>& led,
    const std::optional<LedMode>& led_mode,
    const std::optional<KeyCode32>& key,
    const std::optional<AutoRepeatMode>& auto_repeat_mode) {
  return XProto::ChangeKeyboardControl(ChangeKeyboardControlRequest{
      key_click_percent, bell_percent, bell_pitch, bell_duration, led, led_mode,
      key, auto_repeat_mode});
}

Future<GetKeyboardControlReply> XProto::GetKeyboardControl(
    const GetKeyboardControlRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 103;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<GetKeyboardControlReply>(
      &buf, "GetKeyboardControl", false);
}

Future<GetKeyboardControlReply> XProto::GetKeyboardControl() {
  return XProto::GetKeyboardControl(GetKeyboardControlRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetKeyboardControlReply> detail::ReadReply<
    GetKeyboardControlReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetKeyboardControlReply>();

  auto& global_auto_repeat = (*reply).global_auto_repeat;
  auto& sequence = (*reply).sequence;
  auto& led_mask = (*reply).led_mask;
  auto& key_click_percent = (*reply).key_click_percent;
  auto& bell_percent = (*reply).bell_percent;
  auto& bell_pitch = (*reply).bell_pitch;
  auto& bell_duration = (*reply).bell_duration;
  auto& auto_repeats = (*reply).auto_repeats;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // global_auto_repeat
  uint8_t tmp112;
  Read(&tmp112, &buf);
  global_auto_repeat = static_cast<AutoRepeatMode>(tmp112);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // led_mask
  Read(&led_mask, &buf);

  // key_click_percent
  Read(&key_click_percent, &buf);

  // bell_percent
  Read(&bell_percent, &buf);

  // bell_pitch
  Read(&bell_pitch, &buf);

  // bell_duration
  Read(&bell_duration, &buf);

  // pad0
  Pad(&buf, 2);

  // auto_repeats
  for (auto& auto_repeats_elem : auto_repeats) {
    // auto_repeats_elem
    Read(&auto_repeats_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::Bell(const BellRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& percent = request.percent;

  // major_opcode
  uint8_t major_opcode = 104;
  buf.Write(&major_opcode);

  // percent
  buf.Write(&percent);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Bell", false);
}

Future<void> XProto::Bell(const int8_t& percent) {
  return XProto::Bell(BellRequest{percent});
}

Future<void> XProto::ChangePointerControl(
    const ChangePointerControlRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& acceleration_numerator = request.acceleration_numerator;
  auto& acceleration_denominator = request.acceleration_denominator;
  auto& threshold = request.threshold;
  auto& do_acceleration = request.do_acceleration;
  auto& do_threshold = request.do_threshold;

  // major_opcode
  uint8_t major_opcode = 105;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // acceleration_numerator
  buf.Write(&acceleration_numerator);

  // acceleration_denominator
  buf.Write(&acceleration_denominator);

  // threshold
  buf.Write(&threshold);

  // do_acceleration
  buf.Write(&do_acceleration);

  // do_threshold
  buf.Write(&do_threshold);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ChangePointerControl", false);
}

Future<void> XProto::ChangePointerControl(
    const int16_t& acceleration_numerator,
    const int16_t& acceleration_denominator,
    const int16_t& threshold,
    const uint8_t& do_acceleration,
    const uint8_t& do_threshold) {
  return XProto::ChangePointerControl(ChangePointerControlRequest{
      acceleration_numerator, acceleration_denominator, threshold,
      do_acceleration, do_threshold});
}

Future<GetPointerControlReply> XProto::GetPointerControl(
    const GetPointerControlRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 106;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<GetPointerControlReply>(
      &buf, "GetPointerControl", false);
}

Future<GetPointerControlReply> XProto::GetPointerControl() {
  return XProto::GetPointerControl(GetPointerControlRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetPointerControlReply> detail::ReadReply<
    GetPointerControlReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetPointerControlReply>();

  auto& sequence = (*reply).sequence;
  auto& acceleration_numerator = (*reply).acceleration_numerator;
  auto& acceleration_denominator = (*reply).acceleration_denominator;
  auto& threshold = (*reply).threshold;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // acceleration_numerator
  Read(&acceleration_numerator, &buf);

  // acceleration_denominator
  Read(&acceleration_denominator, &buf);

  // threshold
  Read(&threshold, &buf);

  // pad1
  Pad(&buf, 18);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::SetScreenSaver(const SetScreenSaverRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& timeout = request.timeout;
  auto& interval = request.interval;
  auto& prefer_blanking = request.prefer_blanking;
  auto& allow_exposures = request.allow_exposures;

  // major_opcode
  uint8_t major_opcode = 107;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // timeout
  buf.Write(&timeout);

  // interval
  buf.Write(&interval);

  // prefer_blanking
  uint8_t tmp113;
  tmp113 = static_cast<uint8_t>(prefer_blanking);
  buf.Write(&tmp113);

  // allow_exposures
  uint8_t tmp114;
  tmp114 = static_cast<uint8_t>(allow_exposures);
  buf.Write(&tmp114);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SetScreenSaver", false);
}

Future<void> XProto::SetScreenSaver(const int16_t& timeout,
                                    const int16_t& interval,
                                    const Blanking& prefer_blanking,
                                    const Exposures& allow_exposures) {
  return XProto::SetScreenSaver(SetScreenSaverRequest{
      timeout, interval, prefer_blanking, allow_exposures});
}

Future<GetScreenSaverReply> XProto::GetScreenSaver(
    const GetScreenSaverRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 108;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<GetScreenSaverReply>(&buf, "GetScreenSaver",
                                                       false);
}

Future<GetScreenSaverReply> XProto::GetScreenSaver() {
  return XProto::GetScreenSaver(GetScreenSaverRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetScreenSaverReply> detail::ReadReply<GetScreenSaverReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetScreenSaverReply>();

  auto& sequence = (*reply).sequence;
  auto& timeout = (*reply).timeout;
  auto& interval = (*reply).interval;
  auto& prefer_blanking = (*reply).prefer_blanking;
  auto& allow_exposures = (*reply).allow_exposures;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // timeout
  Read(&timeout, &buf);

  // interval
  Read(&interval, &buf);

  // prefer_blanking
  uint8_t tmp115;
  Read(&tmp115, &buf);
  prefer_blanking = static_cast<Blanking>(tmp115);

  // allow_exposures
  uint8_t tmp116;
  Read(&tmp116, &buf);
  allow_exposures = static_cast<Exposures>(tmp116);

  // pad1
  Pad(&buf, 18);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::ChangeHosts(const ChangeHostsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& mode = request.mode;
  auto& family = request.family;
  uint16_t address_len{};
  auto& address = request.address;

  // major_opcode
  uint8_t major_opcode = 109;
  buf.Write(&major_opcode);

  // mode
  uint8_t tmp117;
  tmp117 = static_cast<uint8_t>(mode);
  buf.Write(&tmp117);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // family
  uint8_t tmp118;
  tmp118 = static_cast<uint8_t>(family);
  buf.Write(&tmp118);

  // pad0
  Pad(&buf, 1);

  // address_len
  address_len = address.size();
  buf.Write(&address_len);

  // address
  CHECK_EQ(static_cast<size_t>(address_len), address.size());
  for (auto& address_elem : address) {
    // address_elem
    buf.Write(&address_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ChangeHosts", false);
}

Future<void> XProto::ChangeHosts(const HostMode& mode,
                                 const Family& family,
                                 const std::vector<uint8_t>& address) {
  return XProto::ChangeHosts(ChangeHostsRequest{mode, family, address});
}

Future<ListHostsReply> XProto::ListHosts(const ListHostsRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 110;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<ListHostsReply>(&buf, "ListHosts", false);
}

Future<ListHostsReply> XProto::ListHosts() {
  return XProto::ListHosts(ListHostsRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<ListHostsReply> detail::ReadReply<ListHostsReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<ListHostsReply>();

  auto& mode = (*reply).mode;
  auto& sequence = (*reply).sequence;
  uint16_t hosts_len{};
  auto& hosts = (*reply).hosts;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // mode
  uint8_t tmp119;
  Read(&tmp119, &buf);
  mode = static_cast<AccessControl>(tmp119);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // hosts_len
  Read(&hosts_len, &buf);

  // pad0
  Pad(&buf, 22);

  // hosts
  hosts.resize(hosts_len);
  for (auto& hosts_elem : hosts) {
    // hosts_elem
    {
      auto& family = hosts_elem.family;
      uint16_t address_len{};
      auto& address = hosts_elem.address;

      // family
      uint8_t tmp120;
      Read(&tmp120, &buf);
      family = static_cast<Family>(tmp120);

      // pad0
      Pad(&buf, 1);

      // address_len
      Read(&address_len, &buf);

      // address
      address.resize(address_len);
      for (auto& address_elem : address) {
        // address_elem
        Read(&address_elem, &buf);
      }

      // pad1
      Align(&buf, 4);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::SetAccessControl(const SetAccessControlRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& mode = request.mode;

  // major_opcode
  uint8_t major_opcode = 111;
  buf.Write(&major_opcode);

  // mode
  uint8_t tmp121;
  tmp121 = static_cast<uint8_t>(mode);
  buf.Write(&tmp121);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SetAccessControl", false);
}

Future<void> XProto::SetAccessControl(const AccessControl& mode) {
  return XProto::SetAccessControl(SetAccessControlRequest{mode});
}

Future<void> XProto::SetCloseDownMode(const SetCloseDownModeRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& mode = request.mode;

  // major_opcode
  uint8_t major_opcode = 112;
  buf.Write(&major_opcode);

  // mode
  uint8_t tmp122;
  tmp122 = static_cast<uint8_t>(mode);
  buf.Write(&tmp122);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "SetCloseDownMode", false);
}

Future<void> XProto::SetCloseDownMode(const CloseDown& mode) {
  return XProto::SetCloseDownMode(SetCloseDownModeRequest{mode});
}

Future<void> XProto::KillClient(const KillClientRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& resource = request.resource;

  // major_opcode
  uint8_t major_opcode = 113;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // resource
  buf.Write(&resource);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "KillClient", false);
}

Future<void> XProto::KillClient(const uint32_t& resource) {
  return XProto::KillClient(KillClientRequest{resource});
}

Future<void> XProto::RotateProperties(const RotatePropertiesRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  uint16_t atoms_len{};
  auto& delta = request.delta;
  auto& atoms = request.atoms;

  // major_opcode
  uint8_t major_opcode = 114;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // atoms_len
  atoms_len = atoms.size();
  buf.Write(&atoms_len);

  // delta
  buf.Write(&delta);

  // atoms
  CHECK_EQ(static_cast<size_t>(atoms_len), atoms.size());
  for (auto& atoms_elem : atoms) {
    // atoms_elem
    buf.Write(&atoms_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "RotateProperties", false);
}

Future<void> XProto::RotateProperties(const Window& window,
                                      const int16_t& delta,
                                      const std::vector<Atom>& atoms) {
  return XProto::RotateProperties(
      RotatePropertiesRequest{window, delta, atoms});
}

Future<void> XProto::ForceScreenSaver(const ForceScreenSaverRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& mode = request.mode;

  // major_opcode
  uint8_t major_opcode = 115;
  buf.Write(&major_opcode);

  // mode
  uint8_t tmp123;
  tmp123 = static_cast<uint8_t>(mode);
  buf.Write(&tmp123);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "ForceScreenSaver", false);
}

Future<void> XProto::ForceScreenSaver(const ScreenSaverMode& mode) {
  return XProto::ForceScreenSaver(ForceScreenSaverRequest{mode});
}

Future<SetPointerMappingReply> XProto::SetPointerMapping(
    const SetPointerMappingRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  uint8_t map_len{};
  auto& map = request.map;

  // major_opcode
  uint8_t major_opcode = 116;
  buf.Write(&major_opcode);

  // map_len
  map_len = map.size();
  buf.Write(&map_len);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // map
  CHECK_EQ(static_cast<size_t>(map_len), map.size());
  for (auto& map_elem : map) {
    // map_elem
    buf.Write(&map_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<SetPointerMappingReply>(
      &buf, "SetPointerMapping", false);
}

Future<SetPointerMappingReply> XProto::SetPointerMapping(
    const std::vector<uint8_t>& map) {
  return XProto::SetPointerMapping(SetPointerMappingRequest{map});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SetPointerMappingReply> detail::ReadReply<
    SetPointerMappingReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SetPointerMappingReply>();

  auto& status = (*reply).status;
  auto& sequence = (*reply).sequence;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // status
  uint8_t tmp124;
  Read(&tmp124, &buf);
  status = static_cast<MappingStatus>(tmp124);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<GetPointerMappingReply> XProto::GetPointerMapping(
    const GetPointerMappingRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 117;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<GetPointerMappingReply>(
      &buf, "GetPointerMapping", false);
}

Future<GetPointerMappingReply> XProto::GetPointerMapping() {
  return XProto::GetPointerMapping(GetPointerMappingRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetPointerMappingReply> detail::ReadReply<
    GetPointerMappingReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetPointerMappingReply>();

  uint8_t map_len{};
  auto& sequence = (*reply).sequence;
  auto& map = (*reply).map;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // map_len
  Read(&map_len, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // pad0
  Pad(&buf, 24);

  // map
  map.resize(map_len);
  for (auto& map_elem : map) {
    // map_elem
    Read(&map_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<SetModifierMappingReply> XProto::SetModifierMapping(
    const SetModifierMappingRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  auto& keycodes_per_modifier = request.keycodes_per_modifier;
  auto& keycodes = request.keycodes;
  size_t keycodes_len = keycodes.size();

  // major_opcode
  uint8_t major_opcode = 118;
  buf.Write(&major_opcode);

  // keycodes_per_modifier
  buf.Write(&keycodes_per_modifier);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // keycodes
  CHECK_EQ(static_cast<size_t>((keycodes_per_modifier) * (8)), keycodes.size());
  for (auto& keycodes_elem : keycodes) {
    // keycodes_elem
    buf.Write(&keycodes_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<SetModifierMappingReply>(
      &buf, "SetModifierMapping", false);
}

Future<SetModifierMappingReply> XProto::SetModifierMapping(
    const uint8_t& keycodes_per_modifier,
    const std::vector<KeyCode>& keycodes) {
  return XProto::SetModifierMapping(
      SetModifierMappingRequest{keycodes_per_modifier, keycodes});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<SetModifierMappingReply> detail::ReadReply<
    SetModifierMappingReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<SetModifierMappingReply>();

  auto& status = (*reply).status;
  auto& sequence = (*reply).sequence;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // status
  uint8_t tmp125;
  Read(&tmp125, &buf);
  status = static_cast<MappingStatus>(tmp125);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<GetModifierMappingReply> XProto::GetModifierMapping(
    const GetModifierMappingRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 119;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<GetModifierMappingReply>(
      &buf, "GetModifierMapping", false);
}

Future<GetModifierMappingReply> XProto::GetModifierMapping() {
  return XProto::GetModifierMapping(GetModifierMappingRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<GetModifierMappingReply> detail::ReadReply<
    GetModifierMappingReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<GetModifierMappingReply>();

  auto& keycodes_per_modifier = (*reply).keycodes_per_modifier;
  auto& sequence = (*reply).sequence;
  auto& keycodes = (*reply).keycodes;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // keycodes_per_modifier
  Read(&keycodes_per_modifier, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // pad0
  Pad(&buf, 24);

  // keycodes
  keycodes.resize((keycodes_per_modifier) * (8));
  for (auto& keycodes_elem : keycodes) {
    // keycodes_elem
    Read(&keycodes_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XProto::NoOperation(const NoOperationRequest& request) {
  if (!connection_->Ready())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = 127;
  buf.Write(&major_opcode);

  // pad0
  Pad(&buf, 1);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "NoOperation", false);
}

Future<void> XProto::NoOperation() {
  return XProto::NoOperation(NoOperationRequest{});
}

}  // namespace x11
