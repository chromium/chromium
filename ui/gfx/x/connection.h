// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_CONNECTION_H_
#define UI_GFX_X_CONNECTION_H_

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/xlib_support.h"
#include "ui/gfx/x/xproto.h"

typedef struct xcb_connection_t xcb_connection_t;

namespace x11 {

class Event;
class KeyboardState;
class WriteBuffer;

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

// This interface is used by classes wanting to receive
// Events directly.  For input events (mouse, keyboard, touch), a
// PlatformEventObserver should be used instead.
class EVENTS_EXPORT EventObserver {
 public:
  virtual void OnEvent(const Event& xevent) = 0;

 protected:
  virtual ~EventObserver() = default;
};

// Represents a socket to the X11 server.
class COMPONENT_EXPORT(X11) Connection : public XProto,
                                         public ExtensionManager {
 public:
  using IOErrorHandler = base::OnceClosure;
  using RawReply = scoped_refptr<base::RefCountedMemory>;
  using RawError = scoped_refptr<base::RefCountedMemory>;
  using ResponseCallback =
      base::OnceCallback<void(RawReply reply, std::unique_ptr<Error> error)>;

  // xcb returns unsigned int when making requests.  This may be updated to
  // uint16_t if/when we stop using xcb for socket IO.
  using SequenceType = unsigned int;

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
        SendRequest(buf, request_name, generates_reply, reply_has_fds));
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
  // "deleting" events by setting events[i] = x11::Event(), which will
  // guarantee all calls to x11::Event::As() will return nullptr.
  base::circular_deque<Event>& events() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return events_;
  }

  std::unique_ptr<Connection> Clone() const;

  // Releases ownership of this connection to a different thread.
  void DetachFromSequence();

  // The viz compositor thread hangs a PlatformEventSource off the connection so
  // that it gets destroyed at the appropriate time.
  // TODO(thomasanderson): This is a layering violation and this should be moved
  // somewhere else.
  std::unique_ptr<ui::PlatformEventSource> platform_event_source;

 private:
  friend class FutureBase;
  template <typename Reply>
  friend class Future;

  class COMPONENT_EXPORT(X11) FutureImpl {
   public:
    FutureImpl(Connection* connection,
               SequenceType sequence,
               bool generates_reply,
               const char* request_name_for_tracing);

    void Wait();

    void DispatchNow();

    bool AfterEvent(const Event& event) const;

    void Sync(RawReply* raw_reply, std::unique_ptr<Error>* error);

    void OnResponse(ResponseCallback callback);

    // Update an existing Request with a new handler.  |sequence| must
    // correspond to a request in the queue that has not already been processed
    // out-of-order.
    void UpdateRequestHandler(ResponseCallback callback);

    // Call the response handler for request |sequence| now (out-of-order).  The
    // response must already have been obtained from a call to
    // WaitForResponse().
    void ProcessResponse();

    // Clear the response handler for request |sequence| and take the response.
    // The response must already have been obtained using WaitForResponse().
    void TakeResponse(RawReply* reply, std::unique_ptr<Error>* error);

    raw_ptr<Connection, DanglingUntriaged> connection = nullptr;
    SequenceType sequence = 0;
    bool generates_reply = false;
    const char* request_name_for_tracing = nullptr;
  };

  struct Request {
    explicit Request(ResponseCallback callback);
    Request(Request&& other);
    ~Request();

    // Takes ownership of |reply| and |error|.
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

  void ProcessNextEvent();

  void ProcessNextResponse();

  bool HasNextResponse();

  bool HasNextEvent();

  // Creates a new Request and adds it to the end of the queue.
  // |request_name_for_tracing| must be valid until the response is
  // dispatched; currently the string values are only stored in .rodata, so
  // this constraint is satisfied.
  std::unique_ptr<FutureImpl> SendRequest(WriteBuffer* buf,
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

  std::string display_string_;
  int default_screen_id_ = 0;
  std::unique_ptr<xcb_connection_t, void (*)(xcb_connection_t*)> connection_ = {
      nullptr, nullptr};
  std::unique_ptr<XlibDisplay> xlib_display_;

  bool synchronous_ = false;
  bool syncing_ = false;

  uint32_t extended_max_request_length_ = 0;

  Setup setup_;
  raw_ptr<Screen> default_screen_ = nullptr;
  raw_ptr<Depth> default_root_depth_ = nullptr;
  raw_ptr<VisualType> default_root_visual_ = nullptr;

  base::flat_map<VisualId, VisualInfo> default_screen_visuals_;

  std::unique_ptr<KeyboardState> keyboard_state_;

  base::circular_deque<Event> events_;

  base::ObserverList<EventObserver>::Unchecked event_observers_;

  // The Event currently being dispatched, or nullptr if there is none.
  raw_ptr<const Event> dispatching_event_ = nullptr;

  base::circular_deque<Request> requests_;
  // The sequence ID of requests_.front(), or if |requests_| is empty, then the
  // ID of the next request that will go in the queue.  This starts at 1 because
  // the 0'th request is handled internally by XCB when opening the connection.
  SequenceType first_request_id_ = 1;
  // If any request in |requests_| will generate a reply, this is the ID of the
  // latest one, otherwise this is absl::nullopt.
  absl::optional<SequenceType> last_non_void_request_id_;

  using ErrorParser = std::unique_ptr<Error> (*)(RawError error_bytes);
  std::array<ErrorParser, 256> error_parsers_{};

  IOErrorHandler io_error_handler_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace x11

#endif  // UI_GFX_X_CONNECTION_H_
