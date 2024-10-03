// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/connection.h"

#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include <algorithm>
#include <string>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/bigreq.h"
#include "ui/gfx/x/dri3.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/glx.h"
#include "ui/gfx/x/keyboard_state.h"
#include "ui/gfx/x/property_cache.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/render.h"
#include "ui/gfx/x/screensaver.h"
#include "ui/gfx/x/shape.h"
#include "ui/gfx/x/shm.h"
#include "ui/gfx/x/sync.h"
#include "ui/gfx/x/visual_manager.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/wm_sync.h"
#include "ui/gfx/x/xfixes.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_internal.h"
#include "ui/gfx/x/xproto_types.h"
#include "ui/gfx/x/xtest.h"

namespace x11 {

namespace {

base::ThreadLocalOwnedPointer<Connection>& GetConnectionTLS() {
  static base::NoDestructor<base::ThreadLocalOwnedPointer<Connection>> tls;
  return *tls;
}

void DefaultErrorHandler(const Error* error, const char* request_name) {
  LOG(WARNING) << "X error received.  Request: " << request_name
               << "Request, Error: " << error->ToString();
}

void DefaultIOErrorHandler() {
  LOG(ERROR) << "X connection error received.";
}

class UnknownError : public Error {
 public:
  explicit UnknownError(RawError error_bytes) : error_bytes_(error_bytes) {}

  ~UnknownError() override = default;

  std::string ToString() const override {
    std::string out = "UnknownError{";
    // xcb promises that there are at least kMinimumErrorSize bytes in any
    // error, so it's safe to construct a span of at least that much memory
    // here.
    UNSAFE_BUFFERS(base::span<const uint8_t> bytes(error_bytes_->bytes(),
                                                   kMinimumErrorSize));
    for (size_t i = 0; i < bytes.size(); ++i) {
      if (i > 0) {
        out += ", ";
      }
      base::AppendHexEncodedByte(bytes[i], out, false);
    }
    out += "}";
    return out;
  }

 private:
  RawError error_bytes_;
};

Window GetWindowPropertyAsWindow(const GetPropertyResponse& value) {
  if (const Window* wm_window = PropertyCache::GetAs<Window>(value)) {
    return *wm_window;
  }
  return Window::None;
}

}  // namespace

// static
Connection* Connection::Get() {
  auto& tls = GetConnectionTLS();
  if (Connection* connection = tls.Get()) {
    return connection;
  }
  auto connection = std::make_unique<Connection>();
  auto* p_connection = connection.get();
  tls.Set(std::move(connection));
  return p_connection;
}

// static
void Connection::Set(std::unique_ptr<Connection> connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(connection->sequence_checker_);
  auto& tls = GetConnectionTLS();
  CHECK(!tls.Get());
  tls.Set(std::move(connection));
}

Connection::Connection(const std::string& address)
    : XProto(this),
      display_string_(
          address.empty()
              ? base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                    switches::kX11Display)
              : address),
      connection_(xcb_connect(display_string_.empty() ? nullptr
                                                      : display_string_.c_str(),
                              &default_screen_id_),
                  xcb_disconnect),
      io_error_handler_(base::BindOnce(DefaultIOErrorHandler)),
      window_event_manager_(this) {
  CHECK(connection_);
  if (Ready()) {
    auto buf = ReadBuffer(
        base::MakeRefCounted<UnretainedRefCountedMemory>(
            // ReadBuffer doesn't use write access but we don't have a const
            // UnsizedRefCountedMemory type for ReadBuffer to use.
            const_cast<xcb_setup_t*>(xcb_get_setup(XcbConnection()))),
        true);
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
  InitializeExtensions();

  // We build an array mapping bit depths back to the last pixmap format that
  // supports that depth; we make room in the array for any depth that could be
  // expressed by Format::depth. If Format::depth gets wider at some point in
  // the future this array might get too big and we'll need to switch to a
  // sparse map.
  std::array<const Format*, std::numeric_limits<decltype(Format::depth)>::max()>
      formats{};
  for (const auto& format : setup_.pixmap_formats) {
    formats[format.depth] = &format;
  }

  std::vector<std::pair<VisualId, VisualInfo>> default_screen_visuals;
  for (const auto& depth : default_screen().allowed_depths) {
    const Format* format = formats[depth.depth];
    for (const auto& visual : depth.visuals) {
      default_screen_visuals.emplace_back(visual.visual_id,
                                          VisualInfo{format, &visual});
    }
  }
  default_screen_visuals_ =
      base::flat_map<VisualId, VisualInfo>(std::move(default_screen_visuals));

  keyboard_state_ = CreateKeyboardState(this);

  InitErrorParsers();

  atom_cache_ = std::make_unique<AtomCache>(this);

  root_props_ = std::make_unique<PropertyCache>(
      this, default_root(),
      std::vector<Atom>{GetAtom("_NET_SUPPORTING_WM_CHECK"),
                        GetAtom("_NET_SUPPORTED")},
      base::BindRepeating(&Connection::OnRootPropertyChanged,
                          base::Unretained(this)));
}

Connection::~Connection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  window_event_manager_.Reset();
  platform_event_source.reset();
}

size_t Connection::MaxRequestSizeInBytes() const {
  return 4 * std::max<size_t>(extended_max_request_length_,
                              setup_.maximum_request_length);
}

XlibDisplay& Connection::GetXlibDisplay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!xlib_display_) {
    xlib_display_ = base::WrapUnique(new XlibDisplay(display_string_));
  }
  return *xlib_display_;
}

void Connection::DeleteProperty(Window window, Atom name) {
  XProto::DeleteProperty({
      .window = static_cast<Window>(window),
      .property = name,
  });
}

void Connection::SetStringProperty(Window window,
                                   Atom property,
                                   Atom type,
                                   const std::string& value) {
  std::vector<char> str(value.begin(), value.end());
  SetArrayProperty(window, property, type, str);
}

Window Connection::CreateDummyWindow(const std::string& name) {
  auto window = GenerateId<Window>();
  CreateWindow(CreateWindowRequest{
      .wid = window,
      .parent = default_root(),
      .x = -100,
      .y = -100,
      .width = 10,
      .height = 10,
      .c_class = WindowClass::InputOnly,
      .override_redirect = Bool32(true),
  });
  if (!name.empty()) {
    SetStringProperty(window, Atom::WM_NAME, Atom::STRING, name);
  }
  return window;
}

VisualManager& Connection::GetOrCreateVisualManager() {
  if (!visual_manager_) {
    visual_manager_ = std::make_unique<VisualManager>(this);
  }
  return *visual_manager_;
}

bool Connection::GetWmNormalHints(Window window, SizeHints* hints) {
  std::vector<uint32_t> hints32;
  if (!GetArrayProperty(window, Atom::WM_NORMAL_HINTS, &hints32)) {
    return false;
  }
  if (hints32.size() != sizeof(SizeHints) / 4) {
    return false;
  }
  memcpy(hints, hints32.data(), sizeof(*hints));
  return true;
}

void Connection::SetWmNormalHints(Window window, const SizeHints& hints) {
  std::vector<uint32_t> hints32(sizeof(SizeHints) / 4);
  memcpy(hints32.data(), &hints, sizeof(SizeHints));
  SetArrayProperty(window, Atom::WM_NORMAL_HINTS, Atom::WM_SIZE_HINTS, hints32);
}

bool Connection::GetWmHints(Window window, WmHints* hints) {
  std::vector<uint32_t> hints32;
  if (!GetArrayProperty(window, Atom::WM_HINTS, &hints32)) {
    return false;
  }
  if (hints32.size() != sizeof(WmHints) / 4) {
    return false;
  }
  memcpy(hints, hints32.data(), sizeof(*hints));
  return true;
}

void Connection::SetWmHints(Window window, const WmHints& hints) {
  std::vector<uint32_t> hints32(sizeof(WmHints) / 4);
  memcpy(hints32.data(), &hints, sizeof(WmHints));
  SetArrayProperty(window, Atom::WM_HINTS, Atom::WM_HINTS, hints32);
}

void Connection::WithdrawWindow(Window window) {
  UnmapWindow({window});

  auto root = default_root();
  UnmapNotifyEvent event{.event = root, .window = window};
  auto mask = EventMask::SubstructureNotify | EventMask::SubstructureRedirect;
  SendEvent(event, root, mask);
}

void Connection::RaiseWindow(Window window) {
  ConfigureWindow(
      ConfigureWindowRequest{.window = window, .stack_mode = StackMode::Above});
}

void Connection::LowerWindow(Window window) {
  ConfigureWindow(
      ConfigureWindowRequest{.window = window, .stack_mode = StackMode::Below});
}

void Connection::DefineCursor(Window window, Cursor cursor) {
  ChangeWindowAttributes(
      ChangeWindowAttributesRequest{.window = window, .cursor = cursor});
}

ScopedEventSelector Connection::ScopedSelectEvent(Window window,
                                                  EventMask event_mask) {
  return ScopedEventSelector(this, window, event_mask);
}

Atom Connection::GetAtom(const char* name) const {
  return atom_cache_->GetAtom(name);
}

std::string Connection::GetWmName() const {
  if (WmSupportsEwmh()) {
    size_t size;
    if (const char* name =
            wm_props_->GetAs<char>(GetAtom("_NET_WM_NAME"), &size)) {
      std::string wm_name;
      wm_name.assign(name, size);
      return wm_name;
    }
  }
  return std::string();
}

bool Connection::WmSupportsHint(Atom atom) const {
  if (WmSupportsEwmh()) {
    auto supported = root_props_->GetAsSpan<Atom>(GetAtom("_NET_SUPPORTED"));
    return base::Contains(supported, atom);
  }
  return false;
}

Connection::Request::Request(ResponseCallback callback)
    : callback(std::move(callback)) {}

Connection::Request::Request(Request&& other) = default;

Connection::Request::~Request() = default;

void Connection::Request::SetResponse(Connection* connection,
                                      void* raw_reply,
                                      void* raw_error) {
  have_response = true;
  if (raw_reply) {
    reply = base::MakeRefCounted<MallocedRefCountedMemory>(raw_reply);
  }
  if (raw_error) {
    error = connection->ParseError(
        base::MakeRefCounted<MallocedRefCountedMemory>(raw_error));
  }
}

bool Connection::HasNextResponse() {
  if (requests_.empty()) {
    return false;
  }
  auto& request = requests_.front();
  if (request.have_response) {
    return true;
  }

  void* reply = nullptr;
  xcb_generic_error_t* error = nullptr;
  if (!xcb_poll_for_reply(XcbConnection(), first_request_id_, &reply, &error)) {
    return false;
  }

  request.SetResponse(this, reply, error);
  return true;
}

bool Connection::HasNextEvent() {
  while (!events_.empty()) {
    if (events_.front().Initialized()) {
      return true;
    }
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
  return !xcb_connection_has_error(connection_.get());
}

void Connection::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  xcb_flush(connection_.get());
}

void Connection::Sync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (syncing_) {
    return;
  }
  {
    base::AutoReset<bool> auto_reset(&syncing_, true);
    GetInputFocus().Sync();
  }
}

void Connection::SynchronizeForTest(bool synchronous) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  synchronous_ = synchronous;
  if (synchronous_) {
    Sync();
  }
}

void Connection::ReadResponses() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (ReadResponse(false)) {
  }
}

bool Connection::ReadResponse(bool queued) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* event = queued ? xcb_poll_for_queued_event(XcbConnection())
                       : xcb_poll_for_event(XcbConnection());
  if (event) {
    events_.emplace_back(base::MakeRefCounted<MallocedRefCountedMemory>(event),
                         this);
  }
  return event;
}

bool Connection::HasPendingResponses() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return HasNextEvent() || HasNextResponse();
}

const Connection::VisualInfo* Connection::GetVisualInfoFromId(
    VisualId id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = default_screen_visuals_.find(id);
  if (it != default_screen_visuals_.end()) {
    return &it->second;
  }
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

bool Connection::Dispatch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (HasNextResponse() && HasNextEvent()) {
    auto next_response_sequence = first_request_id_;
    auto next_event_sequence = events_.front().sequence();

    // All events have the sequence number of the last processed request
    // included in them.  So if a reply and an event have the same sequence,
    // the reply must have been received first.
    if (CompareSequenceIds(next_event_sequence, next_response_sequence) <= 0) {
      ProcessNextResponse();
    } else {
      ProcessNextEvent();
    }
  } else if (HasNextResponse()) {
    ProcessNextResponse();
  } else if (HasNextEvent()) {
    ProcessNextEvent();
  } else {
    return false;
  }
  return true;
}

void Connection::DispatchAll() {
  do {
    Flush();
    ReadResponses();
  } while (Dispatch());
}

void Connection::DispatchEvent(const Event& event) {
  PreDispatchEvent(event);

  // NB: The event should be reset to nullptr when this function
  // returns, not to its initial value, otherwise nested message loops
  // will incorrectly think that the current event being dispatched is
  // an old event.  This means base::AutoReset should not be used.
  dispatching_event_ = &event;
  event_observers_.Notify(&EventObserver::OnEvent, event);
  dispatching_event_ = nullptr;
}

void Connection::SetIOErrorHandler(IOErrorHandler new_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  io_error_handler_ = std::move(new_handler);
}

void Connection::AddEventObserver(EventObserver* observer) {
  event_observers_.AddObserver(observer);
}

void Connection::RemoveEventObserver(EventObserver* observer) {
  event_observers_.RemoveObserver(observer);
}

xcb_connection_t* Connection::XcbConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (io_error_handler_ && xcb_connection_has_error(connection_.get())) {
    std::move(io_error_handler_).Run();
  }
  return connection_.get();
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
  NOTREACHED_IN_MIGRATION();
}

void Connection::InitializeExtensions() {
  auto bigreq_future = bigreq().Enable();
  dri3().QueryVersion(Dri3::major_version, Dri3::minor_version);
  glx().QueryVersion(Glx::major_version, Glx::minor_version);
  auto randr_future =
      randr().QueryVersion(RandR::major_version, RandR::minor_version);
  auto render_future =
      render().QueryVersion(Render::major_version, Render::minor_version);
  auto screensaver_future = screensaver().QueryVersion(
      ScreenSaver::major_version, ScreenSaver::minor_version);
  shape().QueryVersion();
  auto shm_future = shm().QueryVersion();
  auto sync_future =
      sync().Initialize(Sync::major_version, Sync::minor_version);
  xfixes().QueryVersion(XFixes::major_version, XFixes::minor_version);
  auto xinput_future =
      xinput().XIQueryVersion(Input::major_version, Input::minor_version);
  xkb().UseExtension({Xkb::major_version, Xkb::minor_version});
  xtest().GetVersion(Test::major_version, Test::minor_version);

  Flush();

  if (auto response = bigreq_future.Sync()) {
    extended_max_request_length_ = response->maximum_request_length;
  }
  if (auto response = randr_future.Sync()) {
    randr_version_ = {response->major_version, response->minor_version};
  }
  if (auto response = render_future.Sync()) {
    render_version_ = {response->major_version, response->minor_version};
  }
  if (auto response = screensaver_future.Sync()) {
    screensaver_version_ = {response->server_major_version,
                            response->server_minor_version};
  }
  if (auto response = shm_future.Sync()) {
    shm_version_ = {response->major_version, response->minor_version};
  }
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Chrome for ChromeOS can be run with X11 on a Linux desktop. In this case,
  // NotifySwapAfterResize is never called as the compositor does not notify
  // about swaps after resize. Thus, simply disable usage of XSyncCounter on
  // ChromeOS builds.
  if (auto response = sync_future.Sync()) {
    sync_version_ = {response->major_version, response->minor_version};
  }
#endif
  if (auto response = xinput_future.Sync()) {
    xinput_version_ = {response->major_version, response->minor_version};
  }
}

void Connection::ProcessNextEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(HasNextEvent());

  Event event = std::move(events_.front());
  events_.pop_front();

  DispatchEvent(event);
}

void Connection::ProcessNextResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!requests_.empty());
  CHECK(requests_.front().have_response);

  Request request = std::move(requests_.front());
  requests_.pop_front();
  if (last_non_void_request_id_.has_value() &&
      last_non_void_request_id_.value() == first_request_id_) {
    last_non_void_request_id_ = std::nullopt;
  }
  first_request_id_++;
  if (request.callback) {
    std::move(request.callback)
        .Run(std::move(request.reply), std::move(request.error));
  }
}

Future<void> Connection::SetArrayPropertyImpl(
    Window window,
    Atom name,
    Atom type,
    uint8_t format,
    base::span<const uint8_t> values) {
  return ChangeProperty(ChangePropertyRequest{
      .window = static_cast<Window>(window),
      .property = name,
      .type = type,
      .format = format,
      .data_len = static_cast<uint32_t>(values.size()) / (format / 8u),
      .data = base::MakeRefCounted<base::RefCountedBytes>(values)});
}

std::unique_ptr<FutureImpl> Connection::SendRequestImpl(
    WriteBuffer* buf,
    const char* request_name_for_tracing,
    bool generates_reply,
    bool reply_has_fds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  xcb_protocol_request_t xpr{
      .ext = nullptr,
      .isvoid = !generates_reply,
  };

  struct RequestHeader {
    uint8_t major_opcode;
    uint8_t minor_opcode;
    uint16_t length;
  };

  struct ExtendedRequestHeader {
    RequestHeader header;
    uint32_t long_length;
  };
  static_assert(sizeof(ExtendedRequestHeader) == 8, "");

  base::span<uint8_t> first_buffer = buf->GetBuffers()[0];
  CHECK_GE(first_buffer.size(), sizeof(RequestHeader));
  auto* old_header = reinterpret_cast<RequestHeader*>(first_buffer.data());
  ExtendedRequestHeader new_header{*old_header, 0};

  // Requests are always a multiple of 4 bytes on the wire.  Because of this,
  // the length field represents the size in chunks of 4 bytes.
  CHECK_EQ(buf->offset() % 4, 0UL);
  size_t size32 = buf->offset() / 4;

  // XCB requires 2 iovecs for its own internal usage.
  std::vector<struct iovec> io{{nullptr, 0}, {nullptr, 0}};
  if (size32 < setup_.maximum_request_length) {
    // Regular request
    old_header->length = size32;
  } else if (size32 < extended_max_request_length_) {
    // BigRequests extension request
    CHECK_EQ(new_header.header.length, 0U);
    new_header.long_length = size32 + 1;

    io.push_back({&new_header, sizeof(ExtendedRequestHeader)});
    buf->OffsetFirstBuffer(sizeof(RequestHeader));
  } else {
    LOG(ERROR) << "Cannot send request of length " << buf->offset();
    return nullptr;
  }

  for (base::span<uint8_t> buffer : buf->GetBuffers()) {
    io.push_back({buffer.data(), buffer.size()});
  }
  xpr.count = io.size() - 2;

  xcb_connection_t* conn = XcbConnection();
  auto flags = XCB_REQUEST_CHECKED | XCB_REQUEST_RAW;
  if (reply_has_fds) {
    flags |= XCB_REQUEST_REPLY_FDS;
  }

  for (int fd : buf->fds()) {
    xcb_send_fd(conn, fd);
  }
  SequenceType sequence = xcb_send_request(conn, flags, &io[2], &xpr);

  if (xcb_connection_has_error(conn)) {
    return nullptr;
  }

  SequenceType next_request_id = first_request_id_ + requests_.size();
  // XCB inserts requests every 2^32 requests (or every 2^16 requests if
  // all outstanding requests don't generate a reply).  Because it's difficult
  // to track these, increment the sequence counter until ours matches XCB's.
  CHECK_LT(CompareSequenceIds(sequence, next_request_id), 10);
  while (CompareSequenceIds(sequence, next_request_id) > 0) {
    requests_.emplace_back(ResponseCallback());
    requests_.back().have_response = true;
    next_request_id++;
    // If we ever reach 2^32 outstanding requests, then bail because sequence
    // IDs would no longer be unique.
    CHECK_NE(next_request_id, first_request_id_);
  }
  next_request_id++;
  CHECK_NE(next_request_id, first_request_id_);

  // Install a default response-handler that throws away the reply and prints
  // the error if there is one.  This handler may be overridden by clients.
  auto callback = base::BindOnce(
      [](const char* request_name, RawReply raw_reply,
         std::unique_ptr<Error> error) {
        if (error) {
          DefaultErrorHandler(error.get(), request_name);
        }
      },
      request_name_for_tracing);
  requests_.emplace_back(std::move(callback));
  if (generates_reply) {
    last_non_void_request_id_ = sequence;
  }
  if (synchronous_) {
    Sync();
  }

  return std::make_unique<FutureImpl>(this, sequence, generates_reply,
                                      request_name_for_tracing);
}

void Connection::WaitForResponse(FutureImpl* future) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* request = GetRequestForFuture(future);
  CHECK(request->callback);
  if (request->have_response) {
    return;
  }

  xcb_generic_error_t* error = nullptr;
  void* reply = nullptr;
  if (future->generates_reply()) {
    if (!xcb_poll_for_reply(XcbConnection(), future->sequence(), &reply,
                            &error)) {
      TRACE_EVENT1("ui", "xcb_wait_for_reply", "request",
                   future->request_name_for_tracing());
      reply = xcb_wait_for_reply(XcbConnection(), future->sequence(), &error);
    }
  } else {
    // There's a special case here.  This request doesn't generate a reply, and
    // may not generate an error, so the only way to know if it finished is to
    // send another request that we know will generate a reply or error.  Once
    // the new request finishes, we know this request has finished, since the
    // server is guaranteed to process requests in order.  Normally, the
    // xcb_request_check() below would do this for us automatically, but we need
    // to keep track of the sequence count ourselves, so we explicitly make a
    // GetInputFocus request if necessary (which is the request xcb would have
    // made -- GetInputFocus is chosen since it has the minimum size request and
    // reply, and can be made at any time).
    bool needs_extra_request_for_check = false;
    if (!last_non_void_request_id_.has_value()) {
      needs_extra_request_for_check = true;
    } else {
      SequenceType last_non_void_offset =
          last_non_void_request_id_.value() - first_request_id_;
      SequenceType sequence_offset = future->sequence() - first_request_id_;
      needs_extra_request_for_check = sequence_offset > last_non_void_offset;
    }
    if (needs_extra_request_for_check) {
      GetInputFocus().IgnoreError();
      // The circular_deque may have swapped buffers, so we need to get a fresh
      // pointer to the request.
      request = GetRequestForFuture(future);
    }

    // libxcb has a bug where it doesn't flush in xcb_request_check() under some
    // circumstances, leading to deadlock [1], so always perform a manual flush.
    // [1] https://gitlab.freedesktop.org/xorg/lib/libxcb/-/issues/53
    Flush();

    {
      TRACE_EVENT1("ui", "xcb_request_check", "request",
                   future->request_name_for_tracing());
      error = xcb_request_check(XcbConnection(), {future->sequence()});
    }
  }
  request->SetResponse(this, reply, error);
}

Connection::Request* Connection::GetRequestForFuture(FutureImpl* future) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SequenceType offset = future->sequence() - first_request_id_;
  CHECK_LT(offset, requests_.size());
  return &requests_[offset];
}

void Connection::PreDispatchEvent(const Event& event) {
  if (auto* mapping = event.As<MappingNotifyEvent>()) {
    if (mapping->request == Mapping::Modifier ||
        mapping->request == Mapping::Keyboard) {
      setup_.min_keycode = mapping->first_keycode;
      setup_.max_keycode = static_cast<KeyCode>(
          static_cast<int>(mapping->first_keycode) + mapping->count - 1);
      keyboard_state_->UpdateMapping();
    }
  }
  if (auto* notify = event.As<Xkb::NewKeyboardNotifyEvent>()) {
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
    CHECK_GE(index, 0);
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
    if (setup_.roots[i].root == root) {
      return i;
    }
  }
  return -1;
}

std::unique_ptr<Error> Connection::ParseError(RawError error_bytes) {
  if (!error_bytes) {
    return nullptr;
  }
  struct ErrorHeader {
    uint8_t response_type;
    uint8_t error_code;
    uint16_t sequence;
  };
  auto error_code = error_bytes->cast_to<ErrorHeader>()->error_code;
  if (auto parser = error_parsers_[error_code]) {
    return parser(error_bytes);
  }
  return std::make_unique<UnknownError>(error_bytes);
}

uint32_t Connection::GenerateIdImpl() {
  return xcb_generate_id(connection_.get());
}

void Connection::OnRootPropertyChanged(Atom property,
                                       const GetPropertyResponse& value) {
  Atom check_atom = GetAtom("_NET_SUPPORTING_WM_CHECK");
  if (property == check_atom) {
    // We've detected a new window manager, which may have different behavior
    // when attempting to use WmSync.  Attempt to sync with the window manager
    // so we know which behavior WmSync should use.
    AttemptSyncWithWm();
    wm_props_.reset();
    Window wm_window = GetWindowPropertyAsWindow(value);
    if (wm_window != Window::None) {
      wm_props_ = std::make_unique<PropertyCache>(
          this, wm_window,
          std::vector<Atom>{check_atom, GetAtom("_NET_WM_NAME")});
    }
  }
}

bool Connection::WmSupportsEwmh() const {
  Atom check_atom = GetAtom("_NET_SUPPORTING_WM_CHECK");
  Window wm_window = GetWindowPropertyAsWindow(root_props_->Get(check_atom));

  if (!wm_props_) {
    return false;
  }
  if (const x11::Window* wm_check = wm_props_->GetAs<Window>(check_atom)) {
    return *wm_check == wm_window;
  }
  return false;
}

void Connection::AttemptSyncWithWm() {
  synced_with_wm_ = false;
  wm_sync_ = std::make_unique<WmSync>(
      this, base::BindOnce(&Connection::OnWmSynced, base::Unretained(this)),
      true);
}

void Connection::OnWmSynced() {
  synced_with_wm_ = true;
}

ScopedXGrabServer::ScopedXGrabServer(x11::Connection* connection)
    : connection_(connection) {
  connection_->GrabServer();
}

ScopedXGrabServer::~ScopedXGrabServer() {
  connection_->UngrabServer();
  connection_->Flush();
}

}  // namespace x11
