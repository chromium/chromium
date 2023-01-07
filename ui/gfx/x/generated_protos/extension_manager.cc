// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    composite \
//    damage \
//    dpms \
//    dri2 \
//    dri3 \
//    ge \
//    glx \
//    present \
//    randr \
//    record \
//    render \
//    res \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xc_misc \
//    xevie \
//    xf86dri \
//    xf86vidmode \
//    xfixes \
//    xinerama \
//    xinput \
//    xkb \
//    xprint \
//    xproto \
//    xselinux \
//    xtest \
//    xv \
//    xvmc

#include "ui/gfx/x/extension_manager.h"

#include "ui/gfx/x/bigreq.h"
#include "ui/gfx/x/composite.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/damage.h"
#include "ui/gfx/x/dpms.h"
#include "ui/gfx/x/dri2.h"
#include "ui/gfx/x/dri3.h"
#include "ui/gfx/x/ge.h"
#include "ui/gfx/x/glx.h"
#include "ui/gfx/x/present.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/record.h"
#include "ui/gfx/x/render.h"
#include "ui/gfx/x/res.h"
#include "ui/gfx/x/screensaver.h"
#include "ui/gfx/x/shape.h"
#include "ui/gfx/x/shm.h"
#include "ui/gfx/x/sync.h"
#include "ui/gfx/x/xc_misc.h"
#include "ui/gfx/x/xevie.h"
#include "ui/gfx/x/xf86dri.h"
#include "ui/gfx/x/xf86vidmode.h"
#include "ui/gfx/x/xfixes.h"
#include "ui/gfx/x/xinerama.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xprint.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_internal.h"
#include "ui/gfx/x/xselinux.h"
#include "ui/gfx/x/xtest.h"
#include "ui/gfx/x/xv.h"
#include "ui/gfx/x/xvmc.h"

namespace x11 {

void ExtensionManager::Init(Connection* conn) {
  auto bigreq_future = conn->QueryExtension("BIG-REQUESTS");
  auto composite_future = conn->QueryExtension("Composite");
  auto damage_future = conn->QueryExtension("DAMAGE");
  auto dpms_future = conn->QueryExtension("DPMS");
  auto dri2_future = conn->QueryExtension("DRI2");
  auto dri3_future = conn->QueryExtension("DRI3");
  auto ge_future = conn->QueryExtension("Generic Event Extension");
  auto glx_future = conn->QueryExtension("GLX");
  auto present_future = conn->QueryExtension("Present");
  auto randr_future = conn->QueryExtension("RANDR");
  auto record_future = conn->QueryExtension("RECORD");
  auto render_future = conn->QueryExtension("RENDER");
  auto res_future = conn->QueryExtension("X-Resource");
  auto screensaver_future = conn->QueryExtension("MIT-SCREEN-SAVER");
  auto shape_future = conn->QueryExtension("SHAPE");
  auto shm_future = conn->QueryExtension("MIT-SHM");
  auto sync_future = conn->QueryExtension("SYNC");
  auto xc_misc_future = conn->QueryExtension("XC-MISC");
  auto xevie_future = conn->QueryExtension("XEVIE");
  auto xf86dri_future = conn->QueryExtension("XFree86-DRI");
  auto xf86vidmode_future = conn->QueryExtension("XFree86-VidModeExtension");
  auto xfixes_future = conn->QueryExtension("XFIXES");
  auto xinerama_future = conn->QueryExtension("XINERAMA");
  auto xinput_future = conn->QueryExtension("XInputExtension");
  auto xkb_future = conn->QueryExtension("XKEYBOARD");
  auto xprint_future = conn->QueryExtension("XpExtension");
  auto xselinux_future = conn->QueryExtension("SELinux");
  auto xtest_future = conn->QueryExtension("XTEST");
  auto xv_future = conn->QueryExtension("XVideo");
  auto xvmc_future = conn->QueryExtension("XVideo-MotionCompensation");
  conn->Flush();

  bigreq_ = MakeExtension<BigRequests>(conn, std::move(bigreq_future));
  composite_ = MakeExtension<Composite>(conn, std::move(composite_future));
  damage_ = MakeExtension<Damage>(conn, std::move(damage_future));
  dpms_ = MakeExtension<Dpms>(conn, std::move(dpms_future));
  dri2_ = MakeExtension<Dri2>(conn, std::move(dri2_future));
  dri3_ = MakeExtension<Dri3>(conn, std::move(dri3_future));
  ge_ = MakeExtension<GenericEvent>(conn, std::move(ge_future));
  glx_ = MakeExtension<Glx>(conn, std::move(glx_future));
  present_ = MakeExtension<Present>(conn, std::move(present_future));
  randr_ = MakeExtension<RandR>(conn, std::move(randr_future));
  record_ = MakeExtension<Record>(conn, std::move(record_future));
  render_ = MakeExtension<Render>(conn, std::move(render_future));
  res_ = MakeExtension<Res>(conn, std::move(res_future));
  screensaver_ =
      MakeExtension<ScreenSaver>(conn, std::move(screensaver_future));
  shape_ = MakeExtension<Shape>(conn, std::move(shape_future));
  shm_ = MakeExtension<Shm>(conn, std::move(shm_future));
  sync_ = MakeExtension<Sync>(conn, std::move(sync_future));
  xc_misc_ = MakeExtension<XCMisc>(conn, std::move(xc_misc_future));
  xevie_ = MakeExtension<Xevie>(conn, std::move(xevie_future));
  xf86dri_ = MakeExtension<XF86Dri>(conn, std::move(xf86dri_future));
  xf86vidmode_ =
      MakeExtension<XF86VidMode>(conn, std::move(xf86vidmode_future));
  xfixes_ = MakeExtension<XFixes>(conn, std::move(xfixes_future));
  xinerama_ = MakeExtension<Xinerama>(conn, std::move(xinerama_future));
  xinput_ = MakeExtension<Input>(conn, std::move(xinput_future));
  xkb_ = MakeExtension<Xkb>(conn, std::move(xkb_future));
  xprint_ = MakeExtension<XPrint>(conn, std::move(xprint_future));
  xselinux_ = MakeExtension<SELinux>(conn, std::move(xselinux_future));
  xtest_ = MakeExtension<Test>(conn, std::move(xtest_future));
  xv_ = MakeExtension<Xv>(conn, std::move(xv_future));
  xvmc_ = MakeExtension<XvMC>(conn, std::move(xvmc_future));
}

ExtensionManager::ExtensionManager() = default;
ExtensionManager::~ExtensionManager() = default;

}  // namespace x11
