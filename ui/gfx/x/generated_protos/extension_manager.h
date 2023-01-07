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

#ifndef UI_GFX_X_GENERATED_PROTOS_EXTENSION_MANAGER_H_
#define UI_GFX_X_GENERATED_PROTOS_EXTENSION_MANAGER_H_

#include <memory>

#include "base/component_export.h"

namespace x11 {

class Connection;

class BigRequests;
class Composite;
class Damage;
class Dpms;
class Dri2;
class Dri3;
class GenericEvent;
class Glx;
class Present;
class RandR;
class Record;
class Render;
class Res;
class ScreenSaver;
class Shape;
class Shm;
class Sync;
class XCMisc;
class Xevie;
class XF86Dri;
class XF86VidMode;
class XFixes;
class Xinerama;
class Input;
class Xkb;
class XPrint;
class XProto;
class SELinux;
class Test;
class Xv;
class XvMC;

class COMPONENT_EXPORT(X11) ExtensionManager {
 public:
  ExtensionManager();
  ~ExtensionManager();

  BigRequests& bigreq() { return *bigreq_; }
  Composite& composite() { return *composite_; }
  Damage& damage() { return *damage_; }
  Dpms& dpms() { return *dpms_; }
  Dri2& dri2() { return *dri2_; }
  Dri3& dri3() { return *dri3_; }
  GenericEvent& ge() { return *ge_; }
  Glx& glx() { return *glx_; }
  Present& present() { return *present_; }
  RandR& randr() { return *randr_; }
  Record& record() { return *record_; }
  Render& render() { return *render_; }
  Res& res() { return *res_; }
  ScreenSaver& screensaver() { return *screensaver_; }
  Shape& shape() { return *shape_; }
  Shm& shm() { return *shm_; }
  Sync& sync() { return *sync_; }
  XCMisc& xc_misc() { return *xc_misc_; }
  Xevie& xevie() { return *xevie_; }
  XF86Dri& xf86dri() { return *xf86dri_; }
  XF86VidMode& xf86vidmode() { return *xf86vidmode_; }
  XFixes& xfixes() { return *xfixes_; }
  Xinerama& xinerama() { return *xinerama_; }
  Input& xinput() { return *xinput_; }
  Xkb& xkb() { return *xkb_; }
  XPrint& xprint() { return *xprint_; }
  SELinux& xselinux() { return *xselinux_; }
  Test& xtest() { return *xtest_; }
  Xv& xv() { return *xv_; }
  XvMC& xvmc() { return *xvmc_; }

 protected:
  void Init(Connection* conn);

 private:
  std::unique_ptr<BigRequests> bigreq_;
  std::unique_ptr<Composite> composite_;
  std::unique_ptr<Damage> damage_;
  std::unique_ptr<Dpms> dpms_;
  std::unique_ptr<Dri2> dri2_;
  std::unique_ptr<Dri3> dri3_;
  std::unique_ptr<GenericEvent> ge_;
  std::unique_ptr<Glx> glx_;
  std::unique_ptr<Present> present_;
  std::unique_ptr<RandR> randr_;
  std::unique_ptr<Record> record_;
  std::unique_ptr<Render> render_;
  std::unique_ptr<Res> res_;
  std::unique_ptr<ScreenSaver> screensaver_;
  std::unique_ptr<Shape> shape_;
  std::unique_ptr<Shm> shm_;
  std::unique_ptr<Sync> sync_;
  std::unique_ptr<XCMisc> xc_misc_;
  std::unique_ptr<Xevie> xevie_;
  std::unique_ptr<XF86Dri> xf86dri_;
  std::unique_ptr<XF86VidMode> xf86vidmode_;
  std::unique_ptr<XFixes> xfixes_;
  std::unique_ptr<Xinerama> xinerama_;
  std::unique_ptr<Input> xinput_;
  std::unique_ptr<Xkb> xkb_;
  std::unique_ptr<XPrint> xprint_;
  std::unique_ptr<SELinux> xselinux_;
  std::unique_ptr<Test> xtest_;
  std::unique_ptr<Xv> xv_;
  std::unique_ptr<XvMC> xvmc_;
};

}  // namespace x11

#endif  // UI_GFX_X_GENERATED_PROTOS_EXTENSION_MANAGER_H_
