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
}

ExtensionManager::ExtensionManager() = default;
ExtensionManager::~ExtensionManager() = default;

}  // namespace x11
