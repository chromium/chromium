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

#ifndef UI_GFX_X_GENERATED_PROTOS_EXTENSION_MANAGER_H_
#define UI_GFX_X_GENERATED_PROTOS_EXTENSION_MANAGER_H_

#include <memory>

#include "base/component_export.h"

namespace x11 {

class Connection;

class BigRequests;
class Dri3;
class Glx;
class RandR;
class Render;
class ScreenSaver;
class Shape;
class Shm;
class Sync;
class XFixes;
class Input;
class Xkb;
class XProto;
class Test;

class COMPONENT_EXPORT(X11) ExtensionManager {
 public:
  ExtensionManager();
  ~ExtensionManager();

  void GetEventTypeAndOp(const void* raw_event,
                         uint8_t* type_id,
                         uint8_t* opcode) const;

  BigRequests& bigreq() { return *bigreq_; }
  Dri3& dri3() { return *dri3_; }
  Glx& glx() { return *glx_; }
  RandR& randr() { return *randr_; }
  Render& render() { return *render_; }
  ScreenSaver& screensaver() { return *screensaver_; }
  Shape& shape() { return *shape_; }
  Shm& shm() { return *shm_; }
  Sync& sync() { return *sync_; }
  XFixes& xfixes() { return *xfixes_; }
  Input& xinput() { return *xinput_; }
  Xkb& xkb() { return *xkb_; }
  Test& xtest() { return *xtest_; }

 protected:
  void Init(Connection* conn);

 private:
  struct ExtensionGeMap {
    // The extension ID provided by the server.
    uint8_t extension_id = 0;
    // The count of generic events for this extension.
    uint8_t ge_count = 0;
    // The index in `ge_type_ids_` for this extension.
    uint16_t offset = 0;
  };

  std::unique_ptr<BigRequests> bigreq_;
  std::unique_ptr<Dri3> dri3_;
  std::unique_ptr<Glx> glx_;
  std::unique_ptr<RandR> randr_;
  std::unique_ptr<Render> render_;
  std::unique_ptr<ScreenSaver> screensaver_;
  std::unique_ptr<Shape> shape_;
  std::unique_ptr<Shm> shm_;
  std::unique_ptr<Sync> sync_;
  std::unique_ptr<XFixes> xfixes_;
  std::unique_ptr<Input> xinput_;
  std::unique_ptr<Xkb> xkb_;
  std::unique_ptr<Test> xtest_;

  // Event opcodes indexed by response ID.
  uint8_t opcodes_[128] = {0};
  // Event type IDs indexed by response ID.
  uint8_t event_type_ids_[128] = {0};
  // Generic event type IDs for all extensions.
  uint8_t ge_type_ids_[33] = {0};
  ExtensionGeMap ge_extensions_[1] = {};
};

}  // namespace x11

#endif  // UI_GFX_X_GENERATED_PROTOS_EXTENSION_MANAGER_H_
