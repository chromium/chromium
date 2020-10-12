// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/connection.h"

#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include <algorithm>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/string16.h"
#include "base/threading/thread_local.h"
#include "ui/gfx/x/bigreq.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/keysyms/keysyms.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_switches.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_internal.h"
#include "ui/gfx/x/xproto_types.h"

extern "C" {
typedef struct {
  int type;
  unsigned long serial;
  Bool send_event;
  Display* display;
  Window window;
  Window root;
  Window subwindow;
  Time time;
  int x, y;
  int x_root, y_root;
  unsigned int state;
  unsigned int keycode;
  Bool same_screen;
} XKeyEvent;

// This is temporarily required to fix XKB key event processing (bugs 1125886,
// 1136265, 1136248, 1136206).  It should be removed and replaced with an
// XProto equivalent.
int XLookupString(XKeyEvent* event_struct,
                  char* buffer_return,
                  int bytes_buffer,
                  ::KeySym* keysym_return,
                  void* status_in_out);
}

namespace x11 {

namespace {

constexpr KeySym kNoSymbol = static_cast<KeySym>(0);

// On the wire, sequence IDs are 16 bits.  In xcb, they're usually extended to
// 32 and sometimes 64 bits.  In Xlib, they're extended to unsigned long, which
// may be 32 or 64 bits depending on the platform.  This function is intended to
// prevent bugs caused by comparing two differently sized sequences.  Also
// handles rollover.  To use, compare the result of this function with 0.  For
// example, to compare seq1 <= seq2, use CompareSequenceIds(seq1, seq2) <= 0.
template <typename T, typename U>
auto CompareSequenceIds(T t, U u) {
  static_assert(std::is_unsigned<T>::value, "");
  static_assert(std::is_unsigned<U>::value, "");
  // Cast to the smaller of the two types so that comparisons will always work.
  // If we casted to the larger type, then the smaller type will be zero-padded
  // and may incorrectly compare less than the other value.
  using SmallerType =
      typename std::conditional<sizeof(T) <= sizeof(U), T, U>::type;
  SmallerType t0 = static_cast<SmallerType>(t);
  SmallerType u0 = static_cast<SmallerType>(u);
  using SignedType = typename std::make_signed<SmallerType>::type;
  return static_cast<SignedType>(t0 - u0);
}

XDisplay* OpenNewXDisplay(const std::string& address) {
  if (!XInitThreads())
    return nullptr;
  std::string display_str =
      address.empty()
          ? base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                switches::kX11Display)
          : address;
  return XOpenDisplay(display_str.empty() ? nullptr : display_str.c_str());
}

// Ported from XConvertCase:
// https://gitlab.freedesktop.org/xorg/lib/libx11/-/blob/2b7598221d87049d03e9a95fcb541c37c8728184/src/KeyBind.c#L645
void ConvertCaseImpl(uint32_t sym, uint32_t* lower, uint32_t* upper) {
  // Unicode keysym
  if ((sym & 0xff000000) == 0x01000000) {
    base::string16 string({sym & 0x00ffffff});
    auto lower_string = base::i18n::ToLower(string);
    auto upper_string = base::i18n::ToUpper(string);
    *lower = lower_string[0] | 0x01000000;
    *upper = upper_string[0] | 0x01000000;
    return;
  }

  *lower = sym;
  *upper = sym;

  switch (sym >> 8) {
    // Latin 1
    case 0:
      if ((sym >= XK_A) && (sym <= XK_Z))
        *lower += (XK_a - XK_A);
      else if ((sym >= XK_a) && (sym <= XK_z))
        *upper -= (XK_a - XK_A);
      else if ((sym >= XK_Agrave) && (sym <= XK_Odiaeresis))
        *lower += (XK_agrave - XK_Agrave);
      else if ((sym >= XK_agrave) && (sym <= XK_odiaeresis))
        *upper -= (XK_agrave - XK_Agrave);
      else if ((sym >= XK_Ooblique) && (sym <= XK_Thorn))
        *lower += (XK_oslash - XK_Ooblique);
      else if ((sym >= XK_oslash) && (sym <= XK_thorn))
        *upper -= (XK_oslash - XK_Ooblique);
      break;
    // Latin 2
    case 1:
      if (sym == XK_Aogonek)
        *lower = XK_aogonek;
      else if (sym >= XK_Lstroke && sym <= XK_Sacute)
        *lower += (XK_lstroke - XK_Lstroke);
      else if (sym >= XK_Scaron && sym <= XK_Zacute)
        *lower += (XK_scaron - XK_Scaron);
      else if (sym >= XK_Zcaron && sym <= XK_Zabovedot)
        *lower += (XK_zcaron - XK_Zcaron);
      else if (sym == XK_aogonek)
        *upper = XK_Aogonek;
      else if (sym >= XK_lstroke && sym <= XK_sacute)
        *upper -= (XK_lstroke - XK_Lstroke);
      else if (sym >= XK_scaron && sym <= XK_zacute)
        *upper -= (XK_scaron - XK_Scaron);
      else if (sym >= XK_zcaron && sym <= XK_zabovedot)
        *upper -= (XK_zcaron - XK_Zcaron);
      else if (sym >= XK_Racute && sym <= XK_Tcedilla)
        *lower += (XK_racute - XK_Racute);
      else if (sym >= XK_racute && sym <= XK_tcedilla)
        *upper -= (XK_racute - XK_Racute);
      break;
    // Latin 3
    case 2:
      if (sym >= XK_Hstroke && sym <= XK_Hcircumflex)
        *lower += (XK_hstroke - XK_Hstroke);
      else if (sym >= XK_Gbreve && sym <= XK_Jcircumflex)
        *lower += (XK_gbreve - XK_Gbreve);
      else if (sym >= XK_hstroke && sym <= XK_hcircumflex)
        *upper -= (XK_hstroke - XK_Hstroke);
      else if (sym >= XK_gbreve && sym <= XK_jcircumflex)
        *upper -= (XK_gbreve - XK_Gbreve);
      else if (sym >= XK_Cabovedot && sym <= XK_Scircumflex)
        *lower += (XK_cabovedot - XK_Cabovedot);
      else if (sym >= XK_cabovedot && sym <= XK_scircumflex)
        *upper -= (XK_cabovedot - XK_Cabovedot);
      break;
    // Latin 4
    case 3:
      if (sym >= XK_Rcedilla && sym <= XK_Tslash)
        *lower += (XK_rcedilla - XK_Rcedilla);
      else if (sym >= XK_rcedilla && sym <= XK_tslash)
        *upper -= (XK_rcedilla - XK_Rcedilla);
      else if (sym == XK_ENG)
        *lower = XK_eng;
      else if (sym == XK_eng)
        *upper = XK_ENG;
      else if (sym >= XK_Amacron && sym <= XK_Umacron)
        *lower += (XK_amacron - XK_Amacron);
      else if (sym >= XK_amacron && sym <= XK_umacron)
        *upper -= (XK_amacron - XK_Amacron);
      break;
    // Cyrillic
    case 6:
      if (sym >= XK_Serbian_DJE && sym <= XK_Serbian_DZE)
        *lower -= (XK_Serbian_DJE - XK_Serbian_dje);
      else if (sym >= XK_Serbian_dje && sym <= XK_Serbian_dze)
        *upper += (XK_Serbian_DJE - XK_Serbian_dje);
      else if (sym >= XK_Cyrillic_YU && sym <= XK_Cyrillic_HARDSIGN)
        *lower -= (XK_Cyrillic_YU - XK_Cyrillic_yu);
      else if (sym >= XK_Cyrillic_yu && sym <= XK_Cyrillic_hardsign)
        *upper += (XK_Cyrillic_YU - XK_Cyrillic_yu);
      break;
    // Greek
    case 7:
      if (sym >= XK_Greek_ALPHAaccent && sym <= XK_Greek_OMEGAaccent)
        *lower += (XK_Greek_alphaaccent - XK_Greek_ALPHAaccent);
      else if (sym >= XK_Greek_alphaaccent && sym <= XK_Greek_omegaaccent &&
               sym != XK_Greek_iotaaccentdieresis &&
               sym != XK_Greek_upsilonaccentdieresis)
        *upper -= (XK_Greek_alphaaccent - XK_Greek_ALPHAaccent);
      else if (sym >= XK_Greek_ALPHA && sym <= XK_Greek_OMEGA)
        *lower += (XK_Greek_alpha - XK_Greek_ALPHA);
      else if (sym >= XK_Greek_alpha && sym <= XK_Greek_omega &&
               sym != XK_Greek_finalsmallsigma)
        *upper -= (XK_Greek_alpha - XK_Greek_ALPHA);
      break;
    // Latin 9
    case 0x13:
      if (sym == XK_OE)
        *lower = XK_oe;
      else if (sym == XK_oe)
        *upper = XK_OE;
      else if (sym == XK_Ydiaeresis)
        *lower = XK_ydiaeresis;
      break;
  }
}

void ConvertCase(KeySym sym, KeySym* lower, KeySym* upper) {
  uint32_t lower32;
  uint32_t upper32;
  ConvertCaseImpl(static_cast<uint32_t>(sym), &lower32, &upper32);
  *lower = static_cast<KeySym>(lower32);
  *upper = static_cast<KeySym>(upper32);
}

bool IsXKeypadKey(KeySym keysym) {
  auto key = static_cast<uint32_t>(keysym);
  return key >= XK_KP_Space && key <= XK_KP_Equal;
}

bool IsPrivateXKeypadKey(KeySym keysym) {
  auto key = static_cast<uint32_t>(keysym);
  return key >= 0x11000000 && key <= 0x1100FFFF;
}

base::ThreadLocalOwnedPointer<Connection>& GetConnectionTLS() {
  static base::NoDestructor<base::ThreadLocalOwnedPointer<Connection>> tls;
  return *tls;
}

}  // namespace

// static
Connection* Connection::Get() {
  auto& tls = GetConnectionTLS();
  if (Connection* connection = tls.Get())
    return connection;
  auto connection = std::make_unique<Connection>();
  auto* p_connection = connection.get();
  tls.Set(std::move(connection));
  return p_connection;
}

// static
void Connection::Set(std::unique_ptr<x11::Connection> connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(connection->sequence_checker_);
  auto& tls = GetConnectionTLS();
  DCHECK(!tls.Get());
  tls.Set(std::move(connection));
}

Connection::Connection(const std::string& address)
    : XProto(this),
      display_(OpenNewXDisplay(address)),
      display_string_(address) {
  char* host = nullptr;
  int display = 0;
  xcb_parse_display(address.c_str(), &host, &display, &default_screen_id_);
  if (host)
    free(host);
  if (display_) {
    XSetEventQueueOwner(display_, XCBOwnsEventQueue);

    auto buf = ReadBuffer(base::MakeRefCounted<UnretainedRefCountedMemory>(
        xcb_get_setup(XcbConnection())));
    setup_ = Read<Setup>(&buf);
    default_screen_ = &setup_.roots[DefaultScreenId()];
    InitRootDepthAndVisual();
  } else {
    // Default-initialize the setup data so we always have something to return.
    setup_.roots.emplace_back();
    default_screen_ = &setup_.roots[0];
    default_screen_->allowed_depths.emplace_back();
    default_root_depth_ = &default_screen_->allowed_depths[0];
    default_root_depth_->visuals.emplace_back();
    default_root_visual_ = &default_root_depth_->visuals[0];
  }

  ExtensionManager::Init(this);
  if (auto response = bigreq().Enable({}).Sync())
    extended_max_request_length_ = response->maximum_request_length;

  const Format* formats[256];
  memset(formats, 0, sizeof(formats));
  for (const auto& format : setup_.pixmap_formats)
    formats[format.depth] = &format;

  for (const auto& depth : default_screen().allowed_depths) {
    const Format* format = formats[depth.depth];
    for (const auto& visual : depth.visuals)
      default_screen_visuals_[visual.visual_id] = VisualInfo{format, &visual};
  }

  ResetKeyboardState();
}

Connection::~Connection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  platform_event_source.reset();
  if (display_)
    XCloseDisplay(display_);
}

xcb_connection_t* Connection::XcbConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!display())
    return nullptr;
  return XGetXCBConnection(display());
}

Connection::Request::Request(unsigned int sequence,
                             FutureBase::ResponseCallback callback)
    : sequence(sequence), callback(std::move(callback)) {}

Connection::Request::Request(Request&& other)
    : sequence(other.sequence), callback(std::move(other.callback)) {}

Connection::Request::~Request() = default;

bool Connection::HasNextResponse() const {
  return !requests_.empty() &&
         CompareSequenceIds(XLastKnownRequestProcessed(display_),
                            requests_.front().sequence) >= 0;
}

int Connection::GetFd() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Ready() ? xcb_get_file_descriptor(XcbConnection()) : -1;
}

const std::string& Connection::DisplayString() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return display_string_;
}

int Connection::DefaultScreenId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This is not part of the setup data as the server has no concept of a
  // default screen. Instead, it's part of the display name. Eg in
  // "localhost:0.0", the screen ID is the second "0".
  return default_screen_id_;
}

bool Connection::Ready() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return display_ && !xcb_connection_has_error(XGetXCBConnection(display_));
}

void Connection::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (display_)
    XFlush(display_);
}

void Connection::Sync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (syncing_)
    return;
  {
    base::AutoReset<bool> auto_reset(&syncing_, true);
    GetInputFocus({}).Sync();
  }
}

void Connection::SynchronizeForTest(bool synchronous) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  XSynchronize(display(), synchronous);
  synchronous_ = synchronous;
  if (synchronous_)
    Sync();
}

void Connection::ReadResponses() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (auto* event = xcb_poll_for_event(XcbConnection())) {
    events_.emplace_back(base::MakeRefCounted<MallocedRefCountedMemory>(event),
                         this, true);
  }
}

Event Connection::WaitForNextEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!events_.empty()) {
    Event event = std::move(events_.front());
    events_.pop_front();
    return event;
  }
  if (auto* xcb_event = xcb_wait_for_event(XcbConnection())) {
    return Event(base::MakeRefCounted<MallocedRefCountedMemory>(xcb_event),
                 this, true);
  }
  return Event();
}

bool Connection::HasPendingResponses() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !events_.empty() || HasNextResponse();
}

const Connection::VisualInfo* Connection::GetVisualInfoFromId(
    VisualId id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = default_screen_visuals_.find(id);
  if (it != default_screen_visuals_.end())
    return &it->second;
  return nullptr;
}

KeyCode Connection::KeysymToKeycode(KeySym keysym) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint8_t min_keycode = static_cast<uint8_t>(setup_.min_keycode);
  uint8_t max_keycode = static_cast<uint8_t>(setup_.max_keycode);
  uint8_t count = max_keycode - min_keycode + 1;
  DCHECK_EQ(count * keyboard_mapping_.keysyms_per_keycode,
            static_cast<int>(keyboard_mapping_.keysyms.size()));
  for (size_t i = 0; i < keyboard_mapping_.keysyms.size(); i++) {
    if (keyboard_mapping_.keysyms[i] == keysym) {
      return static_cast<KeyCode>(min_keycode +
                                  i / keyboard_mapping_.keysyms_per_keycode);
    }
  }
  return {};
}

KeySym Connection::KeycodeToKeysym(uint32_t keycode, unsigned int modifiers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  XKeyEvent key_event{
      .type = KeyEvent::Press,
      .display = display_,
      .state = modifiers,
      .keycode = keycode,
  };
  ::KeySym keysym;
  XLookupString(&key_event, nullptr, 0, &keysym, nullptr);
  return static_cast<x11::KeySym>(keysym);
}

std::unique_ptr<Connection> Connection::Clone() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<Connection>(display_string_);
}

void Connection::DetachFromSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void Connection::Dispatch(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(display_);

  auto process_next_response = [&] {
    xcb_connection_t* connection = XGetXCBConnection(display_);
    auto request = std::move(requests_.front());
    requests_.pop();

    void* raw_reply = nullptr;
    xcb_generic_error_t* raw_error = nullptr;
    xcb_poll_for_reply(connection, request.sequence, &raw_reply, &raw_error);

    scoped_refptr<MallocedRefCountedMemory> reply;
    if (raw_reply)
      reply = base::MakeRefCounted<MallocedRefCountedMemory>(raw_reply);
    std::move(request.callback).Run(reply, FutureBase::RawError{raw_error});
  };

  auto process_next_event = [&] {
    DCHECK(!events_.empty());

    Event event = std::move(events_.front());
    events_.pop_front();
    PreDispatchEvent(event);
    delegate->DispatchXEvent(&event);
  };

  // Handle all pending events.
  while (delegate->ShouldContinueStream()) {
    Flush();
    ReadResponses();

    if (HasNextResponse() && !events_.empty()) {
      if (!events_.front().sequence_valid()) {
        process_next_event();
        continue;
      }

      auto next_response_sequence = requests_.front().sequence;
      auto next_event_sequence = events_.front().sequence();

      // All events have the sequence number of the last processed request
      // included in them.  So if a reply and an event have the same sequence,
      // the reply must have been received first.
      if (CompareSequenceIds(next_event_sequence, next_response_sequence) <= 0)
        process_next_response();
      else
        process_next_event();
    } else if (HasNextResponse()) {
      process_next_response();
    } else if (!events_.empty()) {
      process_next_event();
    } else {
      break;
    }
  }
}

void Connection::InitRootDepthAndVisual() {
  for (auto& depth : default_screen_->allowed_depths) {
    for (auto& visual : depth.visuals) {
      if (visual.visual_id == default_screen_->root_visual) {
        default_root_depth_ = &depth;
        default_root_visual_ = &visual;
        return;
      }
    }
  }
  NOTREACHED();
}

void Connection::AddRequest(unsigned int sequence,
                            FutureBase::ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(requests_.empty() ||
         CompareSequenceIds(requests_.back().sequence, sequence) < 0);

  requests_.emplace(sequence, std::move(callback));
}

void Connection::PreDispatchEvent(const Event& event) {
  if (auto* mapping = event.As<MappingNotifyEvent>()) {
    if (mapping->request == Mapping::Modifier ||
        mapping->request == Mapping::Keyboard) {
      setup_.min_keycode = mapping->first_keycode;
      setup_.max_keycode = static_cast<x11::KeyCode>(
          static_cast<int>(mapping->first_keycode) + mapping->count - 1);
      ResetKeyboardState();
    }
  }
  if (auto* notify = event.As<x11::Xkb::NewKeyboardNotifyEvent>()) {
    setup_.min_keycode = notify->minKeyCode;
    setup_.max_keycode = notify->maxKeyCode;
    ResetKeyboardState();
  }

  // This is adapted from XRRUpdateConfiguration.
  if (auto* configure = event.As<ConfigureNotifyEvent>()) {
    int index = ScreenIndexFromRootWindow(configure->window);
    if (index != -1) {
      setup_.roots[index].width_in_pixels = configure->width;
      setup_.roots[index].height_in_pixels = configure->height;
    }
  } else if (auto* screen = event.As<RandR::ScreenChangeNotifyEvent>()) {
    int index = ScreenIndexFromRootWindow(screen->root);
    DCHECK_GE(index, 0);
    bool portrait =
        static_cast<bool>(screen->rotation & (RandR::Rotation::Rotate_90 |
                                              RandR::Rotation::Rotate_270));
    if (portrait) {
      setup_.roots[index].width_in_pixels = screen->height;
      setup_.roots[index].height_in_pixels = screen->width;
      setup_.roots[index].width_in_millimeters = screen->mheight;
      setup_.roots[index].height_in_millimeters = screen->mwidth;
    } else {
      setup_.roots[index].width_in_pixels = screen->width;
      setup_.roots[index].height_in_pixels = screen->height;
      setup_.roots[index].width_in_millimeters = screen->mwidth;
      setup_.roots[index].height_in_millimeters = screen->mheight;
    }
  }
}

int Connection::ScreenIndexFromRootWindow(Window root) const {
  for (size_t i = 0; i < setup_.roots.size(); i++) {
    if (setup_.roots[i].root == root)
      return i;
  }
  return -1;
}

void Connection::ResetKeyboardState() {
  uint8_t min_keycode = static_cast<uint8_t>(setup_.min_keycode);
  uint8_t max_keycode = static_cast<uint8_t>(setup_.max_keycode);
  uint8_t count = max_keycode - min_keycode + 1;
  auto keyboard_future = GetKeyboardMapping({setup_.min_keycode, count});
  auto modifier_future = GetModifierMapping({});
  Flush();
  if (auto reply = keyboard_future.Sync())
    keyboard_mapping_ = std::move(*reply.reply);
  if (auto reply = modifier_future.Sync())
    modifier_mapping_ = std::move(*reply.reply);

  for (uint8_t i = 0; i < modifier_mapping_.keycodes_per_modifier; i++) {
    // Lock modifiers are in the second row of the matrix
    size_t index = 2 * modifier_mapping_.keycodes_per_modifier + i;
    for (uint8_t j = 0; j < keyboard_mapping_.keysyms_per_keycode; j++) {
      auto sym = static_cast<uint32_t>(
          KeyCodetoKeySym(modifier_mapping_.keycodes[index], j));
      if (sym == XK_Caps_Lock || sym == XK_ISO_Lock) {
        lock_meaning_ = XK_Caps_Lock;
        break;
      }
      if (sym == XK_Shift_Lock)
        lock_meaning_ = XK_Shift_Lock;
    }
  }

  // Mod<n> is at row (n + 2) of the matrix.  This iterates from Mod1 to Mod5.
  for (int mod = 3; mod < 8; mod++) {
    for (size_t i = 0; i < modifier_mapping_.keycodes_per_modifier; i++) {
      size_t index = mod * modifier_mapping_.keycodes_per_modifier + i;
      for (uint8_t j = 0; j < keyboard_mapping_.keysyms_per_keycode; j++) {
        auto sym = static_cast<uint32_t>(
            KeyCodetoKeySym(modifier_mapping_.keycodes[index], j));
        if (sym == XK_Mode_switch)
          mode_switch_ |= 1 << mod;
        if (sym == XK_Num_Lock)
          num_lock_ |= 1 << mod;
      }
    }
  }
}

// Ported from xcb_key_symbols_get_keysym
// https://gitlab.freedesktop.org/xorg/lib/libxcb-keysyms/-/blob/691515491a4a3c119adc6c769c29de264b3f3806/keysyms/keysyms.c#L189
KeySym Connection::KeyCodetoKeySym(KeyCode keycode, int column) const {
  uint8_t key = static_cast<uint8_t>(keycode);
  uint8_t n_keysyms = keyboard_mapping_.keysyms_per_keycode;

  uint8_t min_key = static_cast<uint8_t>(setup_.min_keycode);
  uint8_t max_key = static_cast<uint8_t>(setup_.max_keycode);
  if (column < 0 || (column >= n_keysyms && column > 3) || key < min_key ||
      key > max_key) {
    return kNoSymbol;
  }

  const auto* syms = &keyboard_mapping_.keysyms[(key - min_key) * n_keysyms];
  if (column < 4) {
    if (column > 1) {
      while ((n_keysyms > 2) && (syms[n_keysyms - 1] == kNoSymbol))
        n_keysyms--;
      if (n_keysyms < 3)
        column -= 2;
    }
    if ((n_keysyms <= (column | 1)) || (syms[column | 1] == kNoSymbol)) {
      KeySym lsym, usym;
      ConvertCase(syms[column & ~1], &lsym, &usym);
      if (!(column & 1))
        return lsym;
      if (usym == lsym)
        return kNoSymbol;
      return usym;
    }
  }
  return syms[column];
}

// Ported from _XTranslateKey:
// https://gitlab.freedesktop.org/xorg/lib/libx11/-/blob/2b7598221d87049d03e9a95fcb541c37c8728184/src/KeyBind.c#L761
KeySym Connection::TranslateKey(uint32_t key, unsigned int modifiers) const {
  constexpr auto kShiftMask = static_cast<unsigned int>(x11::ModMask::Shift);
  constexpr auto kLockMask = static_cast<unsigned int>(x11::ModMask::Lock);

  uint8_t min_key = static_cast<uint8_t>(setup_.min_keycode);
  uint8_t max_key = static_cast<uint8_t>(setup_.max_keycode);
  if (key < min_key || key > max_key)
    return kNoSymbol;

  uint8_t n_keysyms = keyboard_mapping_.keysyms_per_keycode;
  if (!n_keysyms)
    return {};
  const auto* syms = &keyboard_mapping_.keysyms[(key - min_key) * n_keysyms];
  while ((n_keysyms > 2) && (syms[n_keysyms - 1] == kNoSymbol))
    n_keysyms--;
  if ((n_keysyms > 2) && (modifiers & mode_switch_)) {
    syms += 2;
    n_keysyms -= 2;
  }

  if ((modifiers & num_lock_) &&
      (n_keysyms > 1 &&
       (IsXKeypadKey(syms[1]) || IsPrivateXKeypadKey(syms[1])))) {
    if ((modifiers & kShiftMask) ||
        ((modifiers & kLockMask) && (lock_meaning_ == XK_Shift_Lock))) {
      return syms[0];
    }
    return syms[1];
  }

  KeySym lower;
  KeySym upper;
  if (!(modifiers & kShiftMask) &&
      (!(modifiers & kLockMask) ||
       (static_cast<x11::KeySym>(lock_meaning_) == kNoSymbol))) {
    if ((n_keysyms == 1) || (syms[1] == kNoSymbol)) {
      ConvertCase(syms[0], &lower, &upper);
      return lower;
    }
    return syms[0];
  }

  if (!(modifiers & kLockMask) || (lock_meaning_ != XK_Caps_Lock)) {
    if ((n_keysyms == 1) || ((upper = syms[1]) == kNoSymbol))
      ConvertCase(syms[0], &lower, &upper);
    return upper;
  }

  KeySym sym;
  if ((n_keysyms == 1) || ((sym = syms[1]) == kNoSymbol))
    sym = syms[0];
  ConvertCase(sym, &lower, &upper);
  if (!(modifiers & kShiftMask) && (sym != syms[0]) &&
      ((sym != upper) || (lower == upper)))
    ConvertCase(syms[0], &lower, &upper);
  return upper;
}

}  // namespace x11
