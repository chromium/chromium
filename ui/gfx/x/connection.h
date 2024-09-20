// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_CONNECTION_H_
#define UI_GFX_X_CONNECTION_H_

#include <optional>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "base/sequence_checker.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/event_observer.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xlib_support.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_types.h"

typedef struct xcb_connection_t xcb_connection_t;

namespace x11 {

class AtomCache;
class Event;
class KeyboardState;
class PropertyCache;
class VisualManager;
class WmSync;
class WriteBuffer;

enum WmState : uint32_t {
  WM_STATE_WITHDRAWN = 0,
  WM_STATE_NORMAL = 1,
  WM_STATE_ICONIC = 3,
};

enum SizeHintsFlags : int32_t {
  SIZE_HINT_US_POSITION = 1 << 0,
  SIZE_HINT_US_SIZE = 1 << 1,
  SIZE_HINT_P_POSITION = 1 << 2,
  SIZE_HINT_P_SIZE = 1 << 3,
  SIZE_HINT_P_MIN_SIZE = 1 << 4,
  SIZE_HINT_P_MAX_SIZE = 1 << 5,
  SIZE_HINT_P_RESIZE_INC = 1 << 6,
  SIZE_HINT_P_ASPECT = 1 << 7,
  SIZE_HINT_BASE_SIZE = 1 << 8,
  SIZE_HINT_P_WIN_GRAVITY = 1 << 9,
};

struct SizeHints {
  // User specified flags
  int32_t flags;
  // User-specified position
  int32_t x, y;
  // User-specified size
  int32_t width, height;
  // Program-specified minimum size
  int32_t min_width, min_height;
  // Program-specified maximum size
  int32_t max_width, max_height;
  // Program-specified resize increments
  int32_t width_inc, height_inc;
  // Program-specified minimum aspect ratios
  int32_t min_aspect_num, min_aspect_den;
  // Program-specified maximum aspect ratios
  int32_t max_aspect_num, max_aspect_den;
  // Program-specified base size
  int32_t base_width, base_height;
  // Program-specified window gravity
  uint32_t win_gravity;
};

enum WinGravityHint : int32_t {
  WIN_GRAVITY_HINT_UNMAP_GRAVITY = 0,
  WIN_GRAVITY_HINT_NORTHWEST_GRAVITY = 1,
  WIN_GRAVITY_HINT_NORTH_GRAVITY = 2,
  WIN_GRAVITY_HINT_NORTHEAST_GRAVITY = 3,
  WIN_GRAVITY_HINT_WEST_GRAVITY = 4,
  WIN_GRAVITY_HINT_CENTER_GRAVITY = 5,
  WIN_GRAVITY_HINT_EAST_GRAVITY = 6,
  WIN_GRAVITY_HINT_SOUTHWEST_GRAVITY = 7,
  WIN_GRAVITY_HINT_SOUTH_GRAVITY = 8,
  WIN_GRAVITY_HINT_SOUTHEAST_GRAVITY = 9,
  WIN_GRAVITY_HINT_STATIC_GRAVITY = 10,
};

enum WmHintsFlags : uint32_t {
  WM_HINT_INPUT = 1L << 0,
  WM_HINT_STATE = 1L << 1,
  WM_HINT_ICON_PIXMAP = 1L << 2,
  WM_HINT_ICON_WINDOW = 1L << 3,
  WM_HINT_ICON_POSITION = 1L << 4,
  WM_HINT_ICON_MASK = 1L << 5,
  WM_HINT_WINDOW_GROUP = 1L << 6,
  // 1L << 7 doesn't have any defined meaning
  WM_HINT_X_URGENCY = 1L << 8
};

struct WmHints {
  // Marks which fields in this structure are defined
  int32_t flags;
  // Does this application rely on the window manager to get keyboard input?
  uint32_t input;
  // See below
  int32_t initial_state;
  // Pixmap to be used as icon
  Pixmap icon_pixmap;
  // Window to be used as icon
  Window icon_window;
  // Initial position of icon
  int32_t icon_x, icon_y;
  // Icon mask bitmap
  Pixmap icon_mask;
  // Identifier of related window group
  Window window_group;
};

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

// Represents a socket to the X11 server.
class COMPONENT_EXPORT(X11) Connection final : public XProto,
                                               public ExtensionManager {
 public:
  using IOErrorHandler = base::OnceClosure;

  using ExtensionVersion = std::pair<uint32_t, uint32_t>;

  struct VisualInfo {
    raw_ptr<const Format> format;
    raw_ptr<const VisualType> visual_type;
  };

  // Gets or creates the thread local connection instance.
  static Connection* Get();

  // Sets the thread local connection instance.
  static void Set(std::unique_ptr<Connection> connection);

  template <typename T>
  T GenerateId() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return static_cast<T>(GenerateIdImpl());
  }

  template <typename Reply>
  Future<Reply> SendRequest(WriteBuffer* buf,
                            const char* request_name,
                            bool reply_has_fds) {
    bool generates_reply = !std::is_void<Reply>::value;
    return Future<Reply>(
        SendRequestImpl(buf, request_name, generates_reply, reply_has_fds));
  }

  explicit Connection(const std::string& address = "");
  ~Connection();

  Connection(const Connection&) = delete;
  Connection(Connection&&) = delete;

  // Obtain an Xlib display that's connected to the same server as |this|.  This
  // is meant to be used only for compatibility with components like GLX,
  // Vulkan, and VAAPI.  The underlying socket is not shared, so synchronization
  // with |this| may be necessary.
  XlibDisplay& GetXlibDisplay();

  size_t MaxRequestSizeInBytes() const;

  const Setup& setup() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return setup_;
  }
  const Screen& default_screen() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return *default_screen_;
  }
  Window default_root() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return default_screen().root;
  }
  const Depth& default_root_depth() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return *default_root_depth_;
  }
  const VisualType& default_root_visual() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return *default_root_visual_;
  }

  const Event* dispatching_event() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return dispatching_event_;
  }

  ExtensionVersion randr_version() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return randr_version_;
  }

  ExtensionVersion render_version() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return render_version_;
  }

  ExtensionVersion screensaver_version() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return screensaver_version_;
  }

  ExtensionVersion shm_version() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return shm_version_;
  }

  ExtensionVersion sync_version() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return sync_version_;
  }

  ExtensionVersion xinput_version() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return xinput_version_;
  }

  WindowEventManager& window_event_manager() { return window_event_manager_; }

  // Indicates if the connection was able to successfully sync with the
  // window manager.
  bool synced_with_wm() const { return synced_with_wm_; }

  // Returns the underlying socket's FD if the connection is valid, or -1
  // otherwise.
  int GetFd();

  const std::string& DisplayString() const;

  std::string GetConnectionHostname() const;

  int DefaultScreenId() const;

  // Is the connection up and error-free?
  bool Ready() const;

  // Write all requests to the socket.
  void Flush();

  // Flush and block until the server has responded to all requests.
  void Sync();

  // If |synchronous| is true, this makes all requests Sync().
  void SynchronizeForTest(bool synchronous);

  // Read all responses from the socket without blocking.  This function will
  // make non-blocking read() syscalls.
  void ReadResponses();

  // Read a single response.  If |queued| is true, no read() will be done; a
  // response may only be translated from buffered socket data.  If |queued| is
  // false, a non-blocking read() will only be done if no response is buffered.
  // Returns true if an event was read.
  bool ReadResponse(bool queued);

  // Are there any events, errors, or replies already buffered?
  bool HasPendingResponses();

  // Dispatches one event, reply, or error from the server; or returns false
  // if there's none available.  This function doesn't read or write any data on
  // the socket.
  bool Dispatch();

  // Dispatches all available events, replies, and errors.  This function
  // ensures the read and write buffers on the socket are empty upon returning.
  void DispatchAll();

  // Directly dispatch an event, bypassing the event queue.
  void DispatchEvent(const Event& event);

  void SetIOErrorHandler(IOErrorHandler new_handler);

  void AddEventObserver(EventObserver* observer);

  void RemoveEventObserver(EventObserver* observer);

  // Returns the visual data for |id|, or nullptr if the visual with that ID
  // doesn't exist or only exists on a non-default screen.
  const VisualInfo* GetVisualInfoFromId(VisualId id) const;

  KeyCode KeysymToKeycode(uint32_t keysym) const;

  uint32_t KeycodeToKeysym(KeyCode keycode, uint32_t modifiers) const;

  // Access the event buffer.  Clients may modify the queue, including
  // "deleting" events by setting events[i] = Event(), which will
  // guarantee all calls to Event::As() will return nullptr.
  base::circular_deque<Event>& events() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return events_;
  }

  std::unique_ptr<Connection> Clone() const;

  // Releases ownership of this connection to a different thread.
  void DetachFromSequence();

  ////////////////////////////////////////
  // Utilities
  ////////////////////////////////////////

  template <typename T>
  Future<void> SendEvent(const T& event, Window target, EventMask mask) {
    static_assert(T::type_id > 0, "T must be an *Event type");
    auto write_buffer = Write(event);
    CHECK_EQ(write_buffer.GetBuffers().size(), 1ul);
    base::span<uint8_t> first_buffer = write_buffer.GetBuffers()[0];
    char event_bytes[kMinimumEventSize] = {};
    base::span(event_bytes).copy_prefix_from(base::as_chars(first_buffer));

    SendEventRequest send_event{false, target, mask};
    base::span(send_event.event).copy_from(event_bytes);
    base::ranges::copy(event_bytes, send_event.event.begin());
    return XProto::SendEvent(send_event);
  }

  template <typename T>
  bool GetArrayProperty(Window window,
                        Atom name,
                        std::vector<T>* value,
                        Atom* out_type = nullptr,
                        size_t amount = 0) {
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "");

    size_t bytes = amount * sizeof(T);
    // The length field specifies the maximum amount of data we would like the
    // server to give us.  It's specified in units of 4 bytes, so divide by 4.
    // Add 3 before division to round up.
    size_t length = (bytes + 3) / 4;
    using lentype = decltype(GetPropertyRequest::long_length);
    auto response =
        GetProperty(
            GetPropertyRequest{
                .window = static_cast<Window>(window),
                .property = name,
                .long_length = static_cast<uint32_t>(
                    amount ? length : std::numeric_limits<lentype>::max())})
            .Sync();
    if (!response || response->format / 8u != sizeof(T)) {
      return false;
    }

    size_t byte_len = response->value_len * response->format / 8u;
    value->resize(response->value_len);
    if (byte_len > 0u) {
      memcpy(value->data(), response->value->bytes(), byte_len);
    }
    if (out_type) {
      *out_type = response->type;
    }
    return true;
  }

  template <typename T>
  bool GetPropertyAs(Window window, const Atom name, T* value) {
    std::vector<T> values;
    if (!GetArrayProperty(window, name, &values, nullptr, 1) ||
        values.empty()) {
      return false;
    }
    *value = values[0];
    return true;
  }

  template <typename T>
  Future<void> SetArrayProperty(Window window,
                                Atom name,
                                Atom type,
                                const std::vector<T>& values) {
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "");
    return SetArrayPropertyImpl(window, name, type, 8u * sizeof(T),
                                base::as_byte_span(values));
  }

  template <typename T>
  Future<void> SetProperty(Window window,
                           Atom name,
                           Atom type,
                           const T& value) {
    return SetArrayProperty(window, name, type, std::vector<T>{value});
  }

  void DeleteProperty(Window window, Atom name);

  void SetStringProperty(Window window,
                         Atom property,
                         Atom type,
                         const std::string& value);

  Window CreateDummyWindow(const std::string& name = std::string());

  VisualManager& GetOrCreateVisualManager();

  bool GetWmNormalHints(Window window, SizeHints* hints);

  void SetWmNormalHints(Window window, const SizeHints& hints);

  bool GetWmHints(Window window, WmHints* hints);

  void SetWmHints(Window window, const WmHints& hints);

  void WithdrawWindow(Window window);

  void RaiseWindow(Window window);

  void LowerWindow(Window window);

  void DefineCursor(Window window, Cursor cursor);

  ScopedEventSelector ScopedSelectEvent(Window window, EventMask event_mask);

  Atom GetAtom(const char* name) const;

  // Returns an empty string if there is no window manager or the WM is unnamed.
  std::string GetWmName() const;

  bool WmSupportsHint(Atom atom) const;

  // The viz compositor thread hangs a PlatformEventSource off the connection so
  // that it gets destroyed at the appropriate time.
  // TODO(thomasanderson): This is a layering violation and this should be moved
  // somewhere else.
  std::unique_ptr<ui::PlatformEventSource> platform_event_source;

 private:
  friend class FutureBase;
  friend class FutureImpl;
  template <typename Reply>
  friend class Future;

  struct Request {
    explicit Request(ResponseCallback callback);
    Request(Request&& other);
    ~Request();

    // Takes ownership of |reply| and |error|.
    // Note that raw_error is an xcb_generic_error_t, but that type is not used
    // here to avoid having this header file pull in xcb.h.
    void SetResponse(Connection* connection, void* raw_reply, void* raw_error);

    // If |callback| is nullptr, then this request has already been processed
    // out-of-order.
    ResponseCallback callback;

    // Indicates if |reply| and |error| are available.  A separate
    // |have_response| flag is necessary to distinguish the case where a request
    // hasn't finished yet from the case where a request finished but didn't
    // generate a reply or error.
    bool have_response = false;
    RawReply reply;
    std::unique_ptr<Error> error;
  };

  xcb_connection_t* XcbConnection();

  void InitRootDepthAndVisual();

  void InitializeExtensions();

  void ProcessNextEvent();

  void ProcessNextResponse();

  bool HasNextResponse();

  bool HasNextEvent();

  Future<void> SetArrayPropertyImpl(Window window,
                                    Atom name,
                                    Atom type,
                                    uint8_t format,
                                    base::span<const uint8_t> values);

  // Creates a new Request and adds it to the end of the queue.
  // |request_name_for_tracing| must be valid until the response is
  // dispatched; currently the string values are only stored in .rodata, so
  // this constraint is satisfied.
  std::unique_ptr<FutureImpl> SendRequestImpl(
      WriteBuffer* buf,
      const char* request_name_for_tracing,
      bool generates_reply,
      bool reply_has_fds);

  // Block until the reply or error for request |sequence| is received.
  void WaitForResponse(FutureImpl* future);

  Request* GetRequestForFuture(FutureImpl* future);

  void PreDispatchEvent(const Event& event);

  int ScreenIndexFromRootWindow(Window root) const;

  // This function is implemented in the generated read_error.cc.
  void InitErrorParsers();

  std::unique_ptr<Error> ParseError(RawError error_bytes);

  uint32_t GenerateIdImpl();

  void OnRootPropertyChanged(Atom property, const GetPropertyResponse& value);

  bool WmSupportsEwmh() const;

  void AttemptSyncWithWm();

  void OnWmSynced();

  std::string display_string_;
  int default_screen_id_ = 0;
  std::unique_ptr<xcb_connection_t, void (*)(xcb_connection_t*)> connection_ = {
      nullptr, nullptr};
  std::unique_ptr<XlibDisplay> xlib_display_;

  bool synchronous_ = false;
  bool syncing_ = false;

  // Extension data.
  uint32_t extended_max_request_length_ = 0;
  ExtensionVersion randr_version_ = {0, 0};
  ExtensionVersion render_version_ = {0, 0};
  ExtensionVersion screensaver_version_ = {0, 0};
  ExtensionVersion shm_version_ = {0, 0};
  ExtensionVersion sync_version_ = {0, 0};
  ExtensionVersion xinput_version_ = {0, 0};

  Setup setup_;
  raw_ptr<Screen> default_screen_ = nullptr;
  raw_ptr<Depth> default_root_depth_ = nullptr;
  raw_ptr<VisualType> default_root_visual_ = nullptr;

  base::flat_map<VisualId, VisualInfo> default_screen_visuals_;

  std::unique_ptr<KeyboardState> keyboard_state_;

  base::circular_deque<Event> events_;

  base::ObserverList<EventObserver>::UncheckedAndDanglingUntriaged
      event_observers_;

  // The Event currently being dispatched, or nullptr if there is none.
  raw_ptr<const Event> dispatching_event_ = nullptr;

  base::circular_deque<Request> requests_;
  // The sequence ID of requests_.front(), or if |requests_| is empty, then the
  // ID of the next request that will go in the queue.  This starts at 1 because
  // the 0'th request is handled internally by XCB when opening the connection.
  SequenceType first_request_id_ = 1;
  // If any request in |requests_| will generate a reply, this is the ID of the
  // latest one, otherwise this is std::nullopt.
  std::optional<SequenceType> last_non_void_request_id_;

  using ErrorParser = std::unique_ptr<Error> (*)(RawError error_bytes);
  std::array<ErrorParser, 256> error_parsers_{};

  IOErrorHandler io_error_handler_;

  WindowEventManager window_event_manager_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be after `sequence_checker_`.
  std::unique_ptr<VisualManager> visual_manager_;

  std::unique_ptr<AtomCache> atom_cache_;

  std::unique_ptr<WmSync> wm_sync_;
  bool synced_with_wm_ = false;

  std::unique_ptr<PropertyCache> root_props_;
  std::unique_ptr<PropertyCache> wm_props_;
};

// Grab/release the X server connection within a scope. This can help avoid race
// conditions that would otherwise lead to X errors.
class COMPONENT_EXPORT(X11) ScopedXGrabServer {
 public:
  explicit ScopedXGrabServer(x11::Connection* connection);

  ScopedXGrabServer(const ScopedXGrabServer&) = delete;
  ScopedXGrabServer& operator=(const ScopedXGrabServer&) = delete;

  ~ScopedXGrabServer();

 private:
  raw_ptr<x11::Connection> connection_;
};

}  // namespace x11

namespace base {

template <>
struct ScopedObservationTraits<x11::Connection, x11::EventObserver> {
  static void AddObserver(x11::Connection* connection,
                          x11::EventObserver* observer) {
    connection->AddEventObserver(observer);
  }
  static void RemoveObserver(x11::Connection* connection,
                             x11::EventObserver* observer) {
    connection->RemoveEventObserver(observer);
  }
};

}  // namespace base

#endif  // UI_GFX_X_CONNECTION_H_
