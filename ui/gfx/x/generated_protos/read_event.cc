// Copyright 2021 The Chromium Authors. All rights reserved.
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

#include "ui/gfx/x/event.h"

#include <xcb/xcb.h>

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
#include "ui/gfx/x/xproto_types.h"
#include "ui/gfx/x/xselinux.h"
#include "ui/gfx/x/xtest.h"
#include "ui/gfx/x/xv.h"
#include "ui/gfx/x/xvmc.h"

namespace x11 {

void ReadEvent(Event* event, Connection* conn, ReadBuffer* buffer) {
  auto* buf = buffer->data->data();
  auto* ev = reinterpret_cast<const xcb_generic_event_t*>(buf);
  auto* ge = reinterpret_cast<const xcb_ge_generic_event_t*>(buf);
  auto evtype = ev->response_type & ~kSendEventMask;
  bool send_event = ev->response_type & kSendEventMask;

  if (conn->damage().present() &&
      evtype - conn->damage().first_event() == Damage::NotifyEvent::opcode) {
    event->type_id_ = 1;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Damage::NotifyEvent*>(event);
    };
    auto* event_ = new Damage::NotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->dri2().present() && evtype - conn->dri2().first_event() ==
                                    Dri2::BufferSwapCompleteEvent::opcode) {
    event->type_id_ = 2;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Dri2::BufferSwapCompleteEvent*>(event);
    };
    auto* event_ = new Dri2::BufferSwapCompleteEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->dri2().present() && evtype - conn->dri2().first_event() ==
                                    Dri2::InvalidateBuffersEvent::opcode) {
    event->type_id_ = 3;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Dri2::InvalidateBuffersEvent*>(event);
    };
    auto* event_ = new Dri2::InvalidateBuffersEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->glx().present() &&
      evtype - conn->glx().first_event() == Glx::PbufferClobberEvent::opcode) {
    event->type_id_ = 4;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Glx::PbufferClobberEvent*>(event);
    };
    auto* event_ = new Glx::PbufferClobberEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->glx().present() && evtype - conn->glx().first_event() ==
                                   Glx::BufferSwapCompleteEvent::opcode) {
    event->type_id_ = 5;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Glx::BufferSwapCompleteEvent*>(event);
    };
    auto* event_ = new Glx::BufferSwapCompleteEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->present().present() &&
      evtype - conn->present().first_event() == Present::GenericEvent::opcode) {
    event->type_id_ = 6;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Present::GenericEvent*>(event);
    };
    auto* event_ = new Present::GenericEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GeGenericEvent::opcode && conn->present().present() &&
      ge->extension == conn->present().major_opcode() &&
      ge->event_type == Present::ConfigureNotifyEvent::opcode) {
    event->type_id_ = 7;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Present::ConfigureNotifyEvent*>(event);
    };
    auto* event_ = new Present::ConfigureNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GeGenericEvent::opcode && conn->present().present() &&
      ge->extension == conn->present().major_opcode() &&
      ge->event_type == Present::CompleteNotifyEvent::opcode) {
    event->type_id_ = 8;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Present::CompleteNotifyEvent*>(event);
    };
    auto* event_ = new Present::CompleteNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GeGenericEvent::opcode && conn->present().present() &&
      ge->extension == conn->present().major_opcode() &&
      ge->event_type == Present::IdleNotifyEvent::opcode) {
    event->type_id_ = 9;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Present::IdleNotifyEvent*>(event);
    };
    auto* event_ = new Present::IdleNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GeGenericEvent::opcode && conn->present().present() &&
      ge->extension == conn->present().major_opcode() &&
      ge->event_type == Present::RedirectNotifyEvent::opcode) {
    event->type_id_ = 10;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Present::RedirectNotifyEvent*>(event);
    };
    auto* event_ = new Present::RedirectNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->randr().present() && evtype - conn->randr().first_event() ==
                                     RandR::ScreenChangeNotifyEvent::opcode) {
    event->type_id_ = 11;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<RandR::ScreenChangeNotifyEvent*>(event);
    };
    auto* event_ = new RandR::ScreenChangeNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->randr().present() &&
      evtype - conn->randr().first_event() == RandR::NotifyEvent::opcode) {
    event->type_id_ = 12;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<RandR::NotifyEvent*>(event);
    };
    auto* event_ = new RandR::NotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->screensaver().present() &&
      evtype - conn->screensaver().first_event() ==
          ScreenSaver::NotifyEvent::opcode) {
    event->type_id_ = 13;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<ScreenSaver::NotifyEvent*>(event);
    };
    auto* event_ = new ScreenSaver::NotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->shape().present() &&
      evtype - conn->shape().first_event() == Shape::NotifyEvent::opcode) {
    event->type_id_ = 14;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Shape::NotifyEvent*>(event);
    };
    auto* event_ = new Shape::NotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->shm().present() &&
      evtype - conn->shm().first_event() == Shm::CompletionEvent::opcode) {
    event->type_id_ = 15;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Shm::CompletionEvent*>(event);
    };
    auto* event_ = new Shm::CompletionEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->sync().present() &&
      evtype - conn->sync().first_event() == Sync::CounterNotifyEvent::opcode) {
    event->type_id_ = 16;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Sync::CounterNotifyEvent*>(event);
    };
    auto* event_ = new Sync::CounterNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->sync().present() &&
      evtype - conn->sync().first_event() == Sync::AlarmNotifyEvent::opcode) {
    event->type_id_ = 17;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Sync::AlarmNotifyEvent*>(event);
    };
    auto* event_ = new Sync::AlarmNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xfixes().present() && evtype - conn->xfixes().first_event() ==
                                      XFixes::SelectionNotifyEvent::opcode) {
    event->type_id_ = 18;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<XFixes::SelectionNotifyEvent*>(event);
    };
    auto* event_ = new XFixes::SelectionNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xfixes().present() && evtype - conn->xfixes().first_event() ==
                                      XFixes::CursorNotifyEvent::opcode) {
    event->type_id_ = 19;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<XFixes::CursorNotifyEvent*>(event);
    };
    auto* event_ = new XFixes::CursorNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xinput().present() && evtype - conn->xinput().first_event() ==
                                      Input::DeviceValuatorEvent::opcode) {
    event->type_id_ = 20;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::DeviceValuatorEvent*>(event);
    };
    auto* event_ = new Input::DeviceValuatorEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xinput().present() &&
      (evtype - conn->xinput().first_event() ==
           Input::LegacyDeviceEvent::DeviceButtonRelease ||
       evtype - conn->xinput().first_event() ==
           Input::LegacyDeviceEvent::ProximityIn ||
       evtype - conn->xinput().first_event() ==
           Input::LegacyDeviceEvent::DeviceKeyRelease ||
       evtype - conn->xinput().first_event() ==
           Input::LegacyDeviceEvent::DeviceButtonPress ||
       evtype - conn->xinput().first_event() ==
           Input::LegacyDeviceEvent::ProximityOut ||
       evtype - conn->xinput().first_event() ==
           Input::LegacyDeviceEvent::DeviceKeyPress ||
       evtype - conn->xinput().first_event() ==
           Input::LegacyDeviceEvent::DeviceMotionNotify)) {
    event->type_id_ = 21;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::LegacyDeviceEvent*>(event);
    };
    auto* event_ = new Input::LegacyDeviceEvent;
    ReadEvent(event_, buffer);
    event_->opcode = static_cast<decltype(event_->opcode)>(
        evtype - conn->xinput().first_event());
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xinput().present() &&
      (evtype - conn->xinput().first_event() == Input::DeviceFocusEvent::In ||
       evtype - conn->xinput().first_event() == Input::DeviceFocusEvent::Out)) {
    event->type_id_ = 22;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::DeviceFocusEvent*>(event);
    };
    auto* event_ = new Input::DeviceFocusEvent;
    ReadEvent(event_, buffer);
    event_->opcode = static_cast<decltype(event_->opcode)>(
        evtype - conn->xinput().first_event());
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xinput().present() && evtype - conn->xinput().first_event() ==
                                      Input::DeviceStateNotifyEvent::opcode) {
    event->type_id_ = 23;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::DeviceStateNotifyEvent*>(event);
    };
    auto* event_ = new Input::DeviceStateNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xinput().present() && evtype - conn->xinput().first_event() ==
                                      Input::DeviceMappingNotifyEvent::opcode) {
    event->type_id_ = 24;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::DeviceMappingNotifyEvent*>(event);
    };
    auto* event_ = new Input::DeviceMappingNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xinput().present() && evtype - conn->xinput().first_event() ==
                                      Input::ChangeDeviceNotifyEvent::opcode) {
    event->type_id_ = 25;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::ChangeDeviceNotifyEvent*>(event);
    };
    auto* event_ = new Input::ChangeDeviceNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xinput().present() &&
      evtype - conn->xinput().first_event() ==
          Input::DeviceKeyStateNotifyEvent::opcode) {
    event->type_id_ = 26;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::DeviceKeyStateNotifyEvent*>(event);
    };
    auto* event_ = new Input::DeviceKeyStateNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xinput().present() &&
      evtype - conn->xinput().first_event() ==
          Input::DeviceButtonStateNotifyEvent::opcode) {
    event->type_id_ = 27;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::DeviceButtonStateNotifyEvent*>(event);
    };
    auto* event_ = new Input::DeviceButtonStateNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xinput().present() &&
      evtype - conn->xinput().first_event() ==
          Input::DevicePresenceNotifyEvent::opcode) {
    event->type_id_ = 28;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::DevicePresenceNotifyEvent*>(event);
    };
    auto* event_ = new Input::DevicePresenceNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xinput().present() &&
      evtype - conn->xinput().first_event() ==
          Input::DevicePropertyNotifyEvent::opcode) {
    event->type_id_ = 29;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::DevicePropertyNotifyEvent*>(event);
    };
    auto* event_ = new Input::DevicePropertyNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GeGenericEvent::opcode && conn->xinput().present() &&
      ge->extension == conn->xinput().major_opcode() &&
      ge->event_type == Input::DeviceChangedEvent::opcode) {
    event->type_id_ = 30;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::DeviceChangedEvent*>(event);
    };
    auto* event_ = new Input::DeviceChangedEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GeGenericEvent::opcode && conn->xinput().present() &&
      ge->extension == conn->xinput().major_opcode() &&
      (ge->event_type == Input::DeviceEvent::ButtonPress ||
       ge->event_type == Input::DeviceEvent::TouchEnd ||
       ge->event_type == Input::DeviceEvent::KeyRelease ||
       ge->event_type == Input::DeviceEvent::TouchUpdate ||
       ge->event_type == Input::DeviceEvent::KeyPress ||
       ge->event_type == Input::DeviceEvent::Motion ||
       ge->event_type == Input::DeviceEvent::TouchBegin ||
       ge->event_type == Input::DeviceEvent::ButtonRelease)) {
    event->type_id_ = 31;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::DeviceEvent*>(event);
    };
    auto* event_ = new Input::DeviceEvent;
    ReadEvent(event_, buffer);
    event_->opcode = static_cast<decltype(event_->opcode)>(ge->event_type);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GeGenericEvent::opcode && conn->xinput().present() &&
      ge->extension == conn->xinput().major_opcode() &&
      (ge->event_type == Input::CrossingEvent::Leave ||
       ge->event_type == Input::CrossingEvent::FocusIn ||
       ge->event_type == Input::CrossingEvent::FocusOut ||
       ge->event_type == Input::CrossingEvent::Enter)) {
    event->type_id_ = 32;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::CrossingEvent*>(event);
    };
    auto* event_ = new Input::CrossingEvent;
    ReadEvent(event_, buffer);
    event_->opcode = static_cast<decltype(event_->opcode)>(ge->event_type);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GeGenericEvent::opcode && conn->xinput().present() &&
      ge->extension == conn->xinput().major_opcode() &&
      ge->event_type == Input::HierarchyEvent::opcode) {
    event->type_id_ = 33;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::HierarchyEvent*>(event);
    };
    auto* event_ = new Input::HierarchyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GeGenericEvent::opcode && conn->xinput().present() &&
      ge->extension == conn->xinput().major_opcode() &&
      ge->event_type == Input::PropertyEvent::opcode) {
    event->type_id_ = 34;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::PropertyEvent*>(event);
    };
    auto* event_ = new Input::PropertyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GeGenericEvent::opcode && conn->xinput().present() &&
      ge->extension == conn->xinput().major_opcode() &&
      (ge->event_type == Input::RawDeviceEvent::RawTouchEnd ||
       ge->event_type == Input::RawDeviceEvent::RawTouchUpdate ||
       ge->event_type == Input::RawDeviceEvent::RawTouchBegin ||
       ge->event_type == Input::RawDeviceEvent::RawButtonRelease ||
       ge->event_type == Input::RawDeviceEvent::RawMotion ||
       ge->event_type == Input::RawDeviceEvent::RawButtonPress ||
       ge->event_type == Input::RawDeviceEvent::RawKeyRelease ||
       ge->event_type == Input::RawDeviceEvent::RawKeyPress)) {
    event->type_id_ = 35;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::RawDeviceEvent*>(event);
    };
    auto* event_ = new Input::RawDeviceEvent;
    ReadEvent(event_, buffer);
    event_->opcode = static_cast<decltype(event_->opcode)>(ge->event_type);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GeGenericEvent::opcode && conn->xinput().present() &&
      ge->extension == conn->xinput().major_opcode() &&
      ge->event_type == Input::TouchOwnershipEvent::opcode) {
    event->type_id_ = 36;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::TouchOwnershipEvent*>(event);
    };
    auto* event_ = new Input::TouchOwnershipEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GeGenericEvent::opcode && conn->xinput().present() &&
      ge->extension == conn->xinput().major_opcode() &&
      (ge->event_type == Input::BarrierEvent::Leave ||
       ge->event_type == Input::BarrierEvent::Hit)) {
    event->type_id_ = 37;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Input::BarrierEvent*>(event);
    };
    auto* event_ = new Input::BarrierEvent;
    ReadEvent(event_, buffer);
    event_->opcode = static_cast<decltype(event_->opcode)>(ge->event_type);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xkb().present() && evtype - conn->xkb().first_event() ==
                                   Xkb::NewKeyboardNotifyEvent::opcode) {
    event->type_id_ = 38;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xkb::NewKeyboardNotifyEvent*>(event);
    };
    auto* event_ = new Xkb::NewKeyboardNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xkb().present() &&
      evtype - conn->xkb().first_event() == Xkb::MapNotifyEvent::opcode) {
    event->type_id_ = 39;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xkb::MapNotifyEvent*>(event);
    };
    auto* event_ = new Xkb::MapNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xkb().present() &&
      evtype - conn->xkb().first_event() == Xkb::StateNotifyEvent::opcode) {
    event->type_id_ = 40;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xkb::StateNotifyEvent*>(event);
    };
    auto* event_ = new Xkb::StateNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xkb().present() &&
      evtype - conn->xkb().first_event() == Xkb::ControlsNotifyEvent::opcode) {
    event->type_id_ = 41;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xkb::ControlsNotifyEvent*>(event);
    };
    auto* event_ = new Xkb::ControlsNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xkb().present() && evtype - conn->xkb().first_event() ==
                                   Xkb::IndicatorStateNotifyEvent::opcode) {
    event->type_id_ = 42;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xkb::IndicatorStateNotifyEvent*>(event);
    };
    auto* event_ = new Xkb::IndicatorStateNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xkb().present() && evtype - conn->xkb().first_event() ==
                                   Xkb::IndicatorMapNotifyEvent::opcode) {
    event->type_id_ = 43;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xkb::IndicatorMapNotifyEvent*>(event);
    };
    auto* event_ = new Xkb::IndicatorMapNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xkb().present() &&
      evtype - conn->xkb().first_event() == Xkb::NamesNotifyEvent::opcode) {
    event->type_id_ = 44;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xkb::NamesNotifyEvent*>(event);
    };
    auto* event_ = new Xkb::NamesNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xkb().present() &&
      evtype - conn->xkb().first_event() == Xkb::CompatMapNotifyEvent::opcode) {
    event->type_id_ = 45;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xkb::CompatMapNotifyEvent*>(event);
    };
    auto* event_ = new Xkb::CompatMapNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xkb().present() &&
      evtype - conn->xkb().first_event() == Xkb::BellNotifyEvent::opcode) {
    event->type_id_ = 46;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xkb::BellNotifyEvent*>(event);
    };
    auto* event_ = new Xkb::BellNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xkb().present() &&
      evtype - conn->xkb().first_event() == Xkb::ActionMessageEvent::opcode) {
    event->type_id_ = 47;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xkb::ActionMessageEvent*>(event);
    };
    auto* event_ = new Xkb::ActionMessageEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xkb().present() &&
      evtype - conn->xkb().first_event() == Xkb::AccessXNotifyEvent::opcode) {
    event->type_id_ = 48;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xkb::AccessXNotifyEvent*>(event);
    };
    auto* event_ = new Xkb::AccessXNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xkb().present() && evtype - conn->xkb().first_event() ==
                                   Xkb::ExtensionDeviceNotifyEvent::opcode) {
    event->type_id_ = 49;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xkb::ExtensionDeviceNotifyEvent*>(event);
    };
    auto* event_ = new Xkb::ExtensionDeviceNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xprint().present() &&
      evtype - conn->xprint().first_event() == XPrint::NotifyEvent::opcode) {
    event->type_id_ = 50;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<XPrint::NotifyEvent*>(event);
    };
    auto* event_ = new XPrint::NotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xprint().present() && evtype - conn->xprint().first_event() ==
                                      XPrint::AttributNotifyEvent::opcode) {
    event->type_id_ = 51;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<XPrint::AttributNotifyEvent*>(event);
    };
    auto* event_ = new XPrint::AttributNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if ((evtype == KeyEvent::Release || evtype == KeyEvent::Press)) {
    event->type_id_ = 52;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<KeyEvent*>(event);
    };
    auto* event_ = new KeyEvent;
    ReadEvent(event_, buffer);
    event_->opcode = static_cast<decltype(event_->opcode)>(evtype);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if ((evtype == ButtonEvent::Release || evtype == ButtonEvent::Press)) {
    event->type_id_ = 53;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<ButtonEvent*>(event);
    };
    auto* event_ = new ButtonEvent;
    ReadEvent(event_, buffer);
    event_->opcode = static_cast<decltype(event_->opcode)>(evtype);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == MotionNotifyEvent::opcode) {
    event->type_id_ = 54;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<MotionNotifyEvent*>(event);
    };
    auto* event_ = new MotionNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if ((evtype == CrossingEvent::EnterNotify ||
       evtype == CrossingEvent::LeaveNotify)) {
    event->type_id_ = 55;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<CrossingEvent*>(event);
    };
    auto* event_ = new CrossingEvent;
    ReadEvent(event_, buffer);
    event_->opcode = static_cast<decltype(event_->opcode)>(evtype);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if ((evtype == FocusEvent::Out || evtype == FocusEvent::In)) {
    event->type_id_ = 56;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<FocusEvent*>(event);
    };
    auto* event_ = new FocusEvent;
    ReadEvent(event_, buffer);
    event_->opcode = static_cast<decltype(event_->opcode)>(evtype);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == KeymapNotifyEvent::opcode) {
    event->type_id_ = 57;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<KeymapNotifyEvent*>(event);
    };
    auto* event_ = new KeymapNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == ExposeEvent::opcode) {
    event->type_id_ = 58;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<ExposeEvent*>(event);
    };
    auto* event_ = new ExposeEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GraphicsExposureEvent::opcode) {
    event->type_id_ = 59;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<GraphicsExposureEvent*>(event);
    };
    auto* event_ = new GraphicsExposureEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == NoExposureEvent::opcode) {
    event->type_id_ = 60;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<NoExposureEvent*>(event);
    };
    auto* event_ = new NoExposureEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == VisibilityNotifyEvent::opcode) {
    event->type_id_ = 61;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<VisibilityNotifyEvent*>(event);
    };
    auto* event_ = new VisibilityNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == CreateNotifyEvent::opcode) {
    event->type_id_ = 62;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<CreateNotifyEvent*>(event);
    };
    auto* event_ = new CreateNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == DestroyNotifyEvent::opcode) {
    event->type_id_ = 63;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<DestroyNotifyEvent*>(event);
    };
    auto* event_ = new DestroyNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == UnmapNotifyEvent::opcode) {
    event->type_id_ = 64;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<UnmapNotifyEvent*>(event);
    };
    auto* event_ = new UnmapNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == MapNotifyEvent::opcode) {
    event->type_id_ = 65;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<MapNotifyEvent*>(event);
    };
    auto* event_ = new MapNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == MapRequestEvent::opcode) {
    event->type_id_ = 66;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<MapRequestEvent*>(event);
    };
    auto* event_ = new MapRequestEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == ReparentNotifyEvent::opcode) {
    event->type_id_ = 67;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<ReparentNotifyEvent*>(event);
    };
    auto* event_ = new ReparentNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == ConfigureNotifyEvent::opcode) {
    event->type_id_ = 68;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<ConfigureNotifyEvent*>(event);
    };
    auto* event_ = new ConfigureNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == ConfigureRequestEvent::opcode) {
    event->type_id_ = 69;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<ConfigureRequestEvent*>(event);
    };
    auto* event_ = new ConfigureRequestEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == GravityNotifyEvent::opcode) {
    event->type_id_ = 70;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<GravityNotifyEvent*>(event);
    };
    auto* event_ = new GravityNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == ResizeRequestEvent::opcode) {
    event->type_id_ = 71;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<ResizeRequestEvent*>(event);
    };
    auto* event_ = new ResizeRequestEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if ((evtype == CirculateEvent::Request || evtype == CirculateEvent::Notify)) {
    event->type_id_ = 72;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<CirculateEvent*>(event);
    };
    auto* event_ = new CirculateEvent;
    ReadEvent(event_, buffer);
    event_->opcode = static_cast<decltype(event_->opcode)>(evtype);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == PropertyNotifyEvent::opcode) {
    event->type_id_ = 73;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<PropertyNotifyEvent*>(event);
    };
    auto* event_ = new PropertyNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == SelectionClearEvent::opcode) {
    event->type_id_ = 74;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<SelectionClearEvent*>(event);
    };
    auto* event_ = new SelectionClearEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == SelectionRequestEvent::opcode) {
    event->type_id_ = 75;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<SelectionRequestEvent*>(event);
    };
    auto* event_ = new SelectionRequestEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == SelectionNotifyEvent::opcode) {
    event->type_id_ = 76;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<SelectionNotifyEvent*>(event);
    };
    auto* event_ = new SelectionNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == ColormapNotifyEvent::opcode) {
    event->type_id_ = 77;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<ColormapNotifyEvent*>(event);
    };
    auto* event_ = new ColormapNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == ClientMessageEvent::opcode) {
    event->type_id_ = 78;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<ClientMessageEvent*>(event);
    };
    auto* event_ = new ClientMessageEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (evtype == MappingNotifyEvent::opcode) {
    event->type_id_ = 79;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<MappingNotifyEvent*>(event);
    };
    auto* event_ = new MappingNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xv().present() &&
      evtype - conn->xv().first_event() == Xv::VideoNotifyEvent::opcode) {
    event->type_id_ = 81;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xv::VideoNotifyEvent*>(event);
    };
    auto* event_ = new Xv::VideoNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  if (conn->xv().present() &&
      evtype - conn->xv().first_event() == Xv::PortNotifyEvent::opcode) {
    event->type_id_ = 82;
    event->deleter_ = [](void* event) {
      delete reinterpret_cast<Xv::PortNotifyEvent*>(event);
    };
    auto* event_ = new Xv::PortNotifyEvent;
    ReadEvent(event_, buffer);
    event_->send_event = send_event;
    event->event_ = event_;
    event->window_ = event_->GetWindow();
    return;
  }

  NOTREACHED();
}

}  // namespace x11
