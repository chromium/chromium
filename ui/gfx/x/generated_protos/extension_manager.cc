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

#include "ui/gfx/x/extension_manager.h"

#include <xcb/xcb.h>

#include "ui/gfx/x/bigreq.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/dri3.h"
#include "ui/gfx/x/glx.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/render.h"
#include "ui/gfx/x/screensaver.h"
#include "ui/gfx/x/shape.h"
#include "ui/gfx/x/shm.h"
#include "ui/gfx/x/sync.h"
#include "ui/gfx/x/xfixes.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_internal.h"
#include "ui/gfx/x/xproto_types.h"
#include "ui/gfx/x/xtest.h"

namespace x11 {

void ExtensionManager::Init(Connection* conn) {
  auto bigreq_future = conn->QueryExtension("BIG-REQUESTS");
  auto dri3_future = conn->QueryExtension("DRI3");
  auto glx_future = conn->QueryExtension("GLX");
  auto randr_future = conn->QueryExtension("RANDR");
  auto render_future = conn->QueryExtension("RENDER");
  auto screensaver_future = conn->QueryExtension("MIT-SCREEN-SAVER");
  auto shape_future = conn->QueryExtension("SHAPE");
  auto shm_future = conn->QueryExtension("MIT-SHM");
  auto sync_future = conn->QueryExtension("SYNC");
  auto xfixes_future = conn->QueryExtension("XFIXES");
  auto xinput_future = conn->QueryExtension("XInputExtension");
  auto xkb_future = conn->QueryExtension("XKEYBOARD");
  auto xtest_future = conn->QueryExtension("XTEST");
  conn->Flush();
  bigreq_ = MakeExtension<BigRequests>(conn, std::move(bigreq_future));
  dri3_ = MakeExtension<Dri3>(conn, std::move(dri3_future));
  glx_ = MakeExtension<Glx>(conn, std::move(glx_future));
  randr_ = MakeExtension<RandR>(conn, std::move(randr_future));
  render_ = MakeExtension<Render>(conn, std::move(render_future));
  screensaver_ =
      MakeExtension<ScreenSaver>(conn, std::move(screensaver_future));
  shape_ = MakeExtension<Shape>(conn, std::move(shape_future));
  shm_ = MakeExtension<Shm>(conn, std::move(shm_future));
  sync_ = MakeExtension<Sync>(conn, std::move(sync_future));
  xfixes_ = MakeExtension<XFixes>(conn, std::move(xfixes_future));
  xinput_ = MakeExtension<Input>(conn, std::move(xinput_future));
  xkb_ = MakeExtension<Xkb>(conn, std::move(xkb_future));
  xtest_ = MakeExtension<Test>(conn, std::move(xtest_future));

  // XProto may know about more events than the server
  // if the server extension is an earlier version.
  // Always take the event with the later `first_event`
  // to prevent conflicts.
  uint8_t first_events[128] = {0};
  auto set_type = [&](uint8_t first_event, uint8_t op, uint8_t type_id) {
    const uint8_t id = first_event + op;
    if (first_events[id] <= first_event) {
      first_events[id] = first_event;
      event_type_ids_[id] = type_id;
      opcodes_[id] = op;
    }
  };

  set_type(0, 2, 44);
  set_type(0, 3, 44);
  set_type(0, 4, 45);
  set_type(0, 5, 45);
  set_type(0, 6, 46);
  set_type(0, 7, 47);
  set_type(0, 8, 47);
  set_type(0, 9, 48);
  set_type(0, 10, 48);
  set_type(0, 11, 49);
  set_type(0, 12, 50);
  set_type(0, 13, 51);
  set_type(0, 14, 52);
  set_type(0, 15, 53);
  set_type(0, 16, 54);
  set_type(0, 17, 55);
  set_type(0, 18, 56);
  set_type(0, 19, 57);
  set_type(0, 20, 58);
  set_type(0, 21, 59);
  set_type(0, 22, 60);
  set_type(0, 23, 61);
  set_type(0, 24, 62);
  set_type(0, 25, 63);
  set_type(0, 26, 64);
  set_type(0, 27, 64);
  set_type(0, 28, 65);
  set_type(0, 29, 66);
  set_type(0, 30, 67);
  set_type(0, 31, 68);
  set_type(0, 32, 69);
  set_type(0, 33, 70);
  set_type(0, 34, 71);
  uint16_t ge_offset = 0;
  uint8_t ge_extension = 0;
  if (glx_->present()) {
    auto first_event = glx_->first_event();
    set_type(first_event, 0, 1);
    set_type(first_event, 1, 2);
  }
  if (randr_->present()) {
    auto first_event = randr_->first_event();
    set_type(first_event, 0, 3);
    set_type(first_event, 1, 4);
  }
  if (screensaver_->present()) {
    auto first_event = screensaver_->first_event();
    set_type(first_event, 0, 5);
  }
  if (shape_->present()) {
    auto first_event = shape_->first_event();
    set_type(first_event, 0, 6);
  }
  if (shm_->present()) {
    auto first_event = shm_->first_event();
    set_type(first_event, 0, 7);
  }
  if (sync_->present()) {
    auto first_event = sync_->first_event();
    set_type(first_event, 0, 8);
    set_type(first_event, 1, 9);
  }
  if (xfixes_->present()) {
    auto first_event = xfixes_->first_event();
    set_type(first_event, 0, 10);
    set_type(first_event, 1, 11);
  }
  if (xinput_->present()) {
    auto first_event = xinput_->first_event();
    set_type(first_event, 0, 12);
    set_type(first_event, 1, 13);
    set_type(first_event, 2, 13);
    set_type(first_event, 3, 13);
    set_type(first_event, 4, 13);
    set_type(first_event, 5, 13);
    set_type(first_event, 8, 13);
    set_type(first_event, 9, 13);
    set_type(first_event, 6, 14);
    set_type(first_event, 7, 14);
    set_type(first_event, 10, 15);
    set_type(first_event, 11, 16);
    set_type(first_event, 12, 17);
    set_type(first_event, 13, 18);
    set_type(first_event, 14, 19);
    set_type(first_event, 15, 20);
    set_type(first_event, 16, 21);
    ge_type_ids_[ge_offset + 1] = 22;
    ge_type_ids_[ge_offset + 2] = 23;
    ge_type_ids_[ge_offset + 3] = 23;
    ge_type_ids_[ge_offset + 4] = 23;
    ge_type_ids_[ge_offset + 5] = 23;
    ge_type_ids_[ge_offset + 6] = 23;
    ge_type_ids_[ge_offset + 18] = 23;
    ge_type_ids_[ge_offset + 19] = 23;
    ge_type_ids_[ge_offset + 20] = 23;
    ge_type_ids_[ge_offset + 7] = 24;
    ge_type_ids_[ge_offset + 8] = 24;
    ge_type_ids_[ge_offset + 9] = 24;
    ge_type_ids_[ge_offset + 10] = 24;
    ge_type_ids_[ge_offset + 11] = 25;
    ge_type_ids_[ge_offset + 12] = 26;
    ge_type_ids_[ge_offset + 13] = 27;
    ge_type_ids_[ge_offset + 14] = 27;
    ge_type_ids_[ge_offset + 15] = 27;
    ge_type_ids_[ge_offset + 16] = 27;
    ge_type_ids_[ge_offset + 17] = 27;
    ge_type_ids_[ge_offset + 22] = 27;
    ge_type_ids_[ge_offset + 23] = 27;
    ge_type_ids_[ge_offset + 24] = 27;
    ge_type_ids_[ge_offset + 21] = 28;
    ge_type_ids_[ge_offset + 25] = 29;
    ge_type_ids_[ge_offset + 26] = 29;
    ge_type_ids_[ge_offset + 27] = 30;
    ge_type_ids_[ge_offset + 28] = 30;
    ge_type_ids_[ge_offset + 29] = 30;
    ge_type_ids_[ge_offset + 30] = 31;
    ge_type_ids_[ge_offset + 31] = 31;
    ge_type_ids_[ge_offset + 32] = 31;
    ge_extensions_[ge_extension] = {xinput_->major_opcode(), 33, ge_offset};
    ge_offset += 33;
    ge_extension++;
  }
  if (xkb_->present()) {
    auto first_event = xkb_->first_event();
    set_type(first_event, 0, 32);
    set_type(first_event, 1, 33);
    set_type(first_event, 2, 34);
    set_type(first_event, 3, 35);
    set_type(first_event, 4, 36);
    set_type(first_event, 5, 37);
    set_type(first_event, 6, 38);
    set_type(first_event, 7, 39);
    set_type(first_event, 8, 40);
    set_type(first_event, 9, 41);
    set_type(first_event, 10, 42);
    set_type(first_event, 11, 43);
  }
}

void ExtensionManager::GetEventTypeAndOp(const void* raw_event,
                                         uint8_t* type_id,
                                         uint8_t* opcode) const {
  const auto* event = static_cast<const xcb_generic_event_t*>(raw_event);
  auto event_id = event->response_type & ~kSendEventMask;
  if (event_id != GeGenericEvent::opcode) {
    *type_id = event_type_ids_[event_id];
    *opcode = opcodes_[event_id];
    return;
  }

  const auto* ge = static_cast<const xcb_ge_generic_event_t*>(raw_event);
  *type_id = 0;
  *opcode = ge->event_type;
  for (const auto& ext : ge_extensions_) {
    if (ext.extension_id == ge->extension) {
      if (ge->event_type < ext.ge_count) {
        *type_id = ge_type_ids_[ext.offset + ge->event_type];
      }
      return;
    }
  }
}

ExtensionManager::ExtensionManager() = default;
ExtensionManager::~ExtensionManager() = default;

}  // namespace x11
