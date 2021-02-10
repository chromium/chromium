// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/connection.h"

#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include <algorithm>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/string16.h"
#include "base/threading/thread_local.h"
#include "ui/gfx/x/bigreq.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/keyboard_state.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/x11_switches.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_internal.h"
#include "ui/gfx/x/xproto_types.h"

namespace x11 {

namespace {

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

base::ThreadLocalOwnedPointer<Connection>& GetConnectionTLS() {
  static base::NoDestructor<base::ThreadLocalOwnedPointer<Connection>> tls;
  return *tls;
}

void DefaultErrorHandler(const x11::Error* error, const char* request_name) {
  LOG(WARNING) << "X error received.  Request: x11::" << request_name
               << "Request, Error: " << error->ToString();
}

void DefaultIOErrorHandler() {
  LOG(ERROR) << "X connection error received.";
}

class UnknownError : public Error {
 public:
  explicit UnknownError(FutureBase::RawError error_bytes)
      : error_bytes_(error_bytes) {}

  ~UnknownError() override = default;

  std::string ToString() const override {
    std::stringstream ss;
    ss << "x11::UnknownError{";
    // Errors are always a fixed 32 bytes.
    for (size_t i = 0; i < 32; i++) {
      char buf[3];
      sprintf(buf, "%02x", error_bytes_->data()[i]);
      ss << "0x" << buf;
      if (i != 31)
        ss << ", ";
    }
    ss << "}";
    return ss.str();
  }

 private:
  FutureBase::RawError error_bytes_;
};

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
      display_string_(
          address.empty()
              ? base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                    switches::kX11Display)
              : address),
      error_handler_(base::BindRepeating(DefaultErrorHandler)),
      io_error_handler_(base::BindOnce(DefaultIOErrorHandler)) {
  connection_ =
      xcb_connect(display_string_.empty() ? nullptr : display_string_.c_str(),
                  &default_screen_id_);
  DCHECK(connection_);
  if (Ready()) {
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
  auto enable_bigreq = bigreq().Enable({});
  // Xlib enables XKB on display creation, so we do that here to maintain
  // compatibility.
  xkb()
      .UseExtension({x11::Xkb::major_version, x11::Xkb::minor_version})
      .OnResponse(base::BindOnce([](x11::Xkb::UseExtensionResponse response) {
        if (!response || !response->supported)
          DVLOG(1) << "Xkb extension not available.";
      }));
  Flush();
  if (auto response = enable_bigreq.Sync())
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

  keyboard_state_ = CreateKeyboardState(this);

  InitErrorParsers();
}

Connection::~Connection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  platform_event_source.reset();
  xcb_disconnect(connection_);
}

xcb_connection_t* Connection::XcbConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (io_error_handler_ && xcb_connection_has_error(connection_))
    std::move(io_error_handler_).Run();
  return connection_;
}

XlibDisplayWrapper Connection::GetXlibDisplay(XlibDisplayType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!xlib_display_)
    xlib_display_ = base::WrapUnique(new XlibDisplay(display_string_));
  return XlibDisplayWrapper(xlib_display_->display_, type);
}

Connection::Request::Request(unsigned int sequence,
                             FutureBase::ResponseCallback callback)
    : sequence(sequence), callback(std::move(callback)) {}

Connection::Request::Request(Request&& other)
    : sequence(other.sequence),
      callback(std::move(other.callback)),
      have_response(other.have_response),
      reply(std::move(other.reply)),
      error(std::move(other.error)) {}

Connection::Request::~Request() = default;

bool Connection::HasNextResponse() {
  if (requests_.empty())
    return false;
  auto& request = requests_.front();
  if (request.have_response)
    return true;

  void* reply = nullptr;
  xcb_generic_error_t* error = nullptr;
  request.have_response =
      xcb_poll_for_reply(XcbConnection(), request.sequence, &reply, &error);
  if (reply)
    request.reply = base::MakeRefCounted<MallocedRefCountedMemory>(reply);
  if (error)
    request.error = base::MakeRefCounted<MallocedRefCountedMemory>(error);
  return request.have_response;
}

bool Connection::HasNextEvent() {
  while (!events_.empty()) {
    if (events_.front().Initialized())
      return true;
    events_.pop_front();
  }
  return false;
}

int Connection::GetFd() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Ready() ? xcb_get_file_descriptor(XcbConnection()) : -1;
}

const std::string& Connection::DisplayString() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return display_string_;
}

std::string Connection::GetConnectionHostname() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  char* host = nullptr;
  int display_id = 0;
  int screen = 0;
  if (xcb_parse_display(display_string_.c_str(), &host, &display_id, &screen)) {
    std::string name = host;
    free(host);
    return name;
  }
  return std::string();
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
  return !xcb_connection_has_error(connection_);
}

void Connection::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  xcb_flush(connection_);
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
  if (HasNextEvent()) {
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

bool Connection::HasPendingResponses() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return HasNextEvent() || HasNextResponse();
}

const Connection::VisualInfo* Connection::GetVisualInfoFromId(
    VisualId id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = default_screen_visuals_.find(id);
  if (it != default_screen_visuals_.end())
    return &it->second;
  return nullptr;
}

KeyCode Connection::KeysymToKeycode(uint32_t keysym) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return keyboard_state_->KeysymToKeycode(keysym);
}

uint32_t Connection::KeycodeToKeysym(KeyCode keycode,
                                     uint32_t modifiers) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return keyboard_state_->KeycodeToKeysym(keycode, modifiers);
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

  auto process_next_response = [&] {
    DCHECK(!requests_.empty());
    DCHECK(requests_.front().have_response);

    Request request = std::move(requests_.front());
    requests_.pop();
    std::move(request.callback).Run(request.reply, request.error);
  };

  auto process_next_event = [&] {
    DCHECK(HasNextEvent());

    Event event = std::move(events_.front());
    events_.pop_front();
    PreDispatchEvent(event);
    delegate->DispatchXEvent(&event);
  };

  // Handle all pending events.
  while (delegate->ShouldContinueStream()) {
    Flush();
    ReadResponses();

    if (HasNextResponse() && HasNextEvent()) {
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
    } else if (HasNextEvent()) {
      process_next_event();
    } else {
      break;
    }
  }
}

Connection::ErrorHandler Connection::SetErrorHandler(ErrorHandler new_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::exchange(error_handler_, new_handler);
}

void Connection::SetIOErrorHandler(IOErrorHandler new_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  io_error_handler_ = std::move(new_handler);
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
      keyboard_state_->UpdateMapping();
    }
  }
  if (auto* notify = event.As<x11::Xkb::NewKeyboardNotifyEvent>()) {
    setup_.min_keycode = notify->minKeyCode;
    setup_.max_keycode = notify->maxKeyCode;
    keyboard_state_->UpdateMapping();
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

std::unique_ptr<Error> Connection::ParseError(
    FutureBase::RawError error_bytes) {
  if (!error_bytes)
    return nullptr;
  struct ErrorHeader {
    uint8_t response_type;
    uint8_t error_code;
    uint16_t sequence;
  };
  auto error_code = error_bytes->front_as<ErrorHeader>()->error_code;
  if (auto parser = error_parsers_[error_code])
    return parser(error_bytes);
  return std::make_unique<UnknownError>(error_bytes);
}

uint32_t Connection::GenerateIdImpl() {
  return xcb_generate_id(connection_);
}

}  // namespace x11
