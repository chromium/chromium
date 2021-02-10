// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_CONNECTION_H_
#define UI_GFX_X_CONNECTION_H_

#include <queue>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/sequence_checker.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/xlib_support.h"
#include "ui/gfx/x/xproto.h"

typedef struct xcb_connection_t xcb_connection_t;

namespace x11 {

class KeyboardState;

// Represents a socket to the X11 server.
class COMPONENT_EXPORT(X11) Connection : public XProto,
                                         public ExtensionManager {
 public:
  using ErrorHandler = base::RepeatingCallback<void(const Error*, const char*)>;
  using IOErrorHandler = base::OnceClosure;

  class Delegate {
   public:
    virtual bool ShouldContinueStream() const = 0;
    virtual void DispatchXEvent(x11::Event* event) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  struct VisualInfo {
    const Format* format;
    const VisualType* visual_type;
  };

  // Gets or creates the thread local connection instance.
  static Connection* Get();

  // Sets the thread local connection instance.
  static void Set(std::unique_ptr<x11::Connection> connection);

  explicit Connection(const std::string& address = "");
  ~Connection();

  Connection(const Connection&) = delete;
  Connection(Connection&&) = delete;

  xcb_connection_t* XcbConnection();

  // Obtain an Xlib display that's connected to the same server as |this|.  This
  // is meant to be used only for compatibility with components like GLX,
  // Vulkan, and VAAPI.  The underlying socket is not shared, so synchronization
  // with |this| may be necessary.  The |type| parameter can be used to achieve
  // synchronization.  The returned wrapper should not be saved.
  XlibDisplayWrapper GetXlibDisplay(
      XlibDisplayType type = XlibDisplayType::kNormal);

  uint32_t extended_max_request_length() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return extended_max_request_length_;
  }

  const Setup& setup() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return setup_;
  }
  const Screen& default_screen() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return *default_screen_;
  }
  x11::Window default_root() const {
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

  // Returns the underlying socket's FD if the connection is valid, or -1
  // otherwise.
  int GetFd();

  const std::string& DisplayString() const;

  std::string GetConnectionHostname() const;

  int DefaultScreenId() const;

  template <typename T>
  T GenerateId() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return static_cast<T>(GenerateIdImpl());
  }

  // Is the connection up and error-free?
  bool Ready() const;

  // Write all requests to the socket.
  void Flush();

  // Flush and block until the server has responded to all requests.
  void Sync();

  // If |synchronous| is true, this makes all requests Sync().
  void SynchronizeForTest(bool synchronous);

  bool synchronous() const { return synchronous_; }

  // Read all responses from the socket without blocking.
  void ReadResponses();

  Event WaitForNextEvent();

  // Are there any events, errors, or replies already buffered?
  bool HasPendingResponses();

  // Dispatch any buffered events, errors, or replies.
  void Dispatch(Delegate* delegate);

  // Returns the old error handler.
  ErrorHandler SetErrorHandler(ErrorHandler new_handler);

  void SetIOErrorHandler(IOErrorHandler new_handler);

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

  struct Request {
    Request(unsigned int sequence, FutureBase::ResponseCallback callback);
    Request(Request&& other);
    ~Request();

    const unsigned int sequence;
    FutureBase::ResponseCallback callback;
    bool have_response = false;
    FutureBase::RawReply reply;
    FutureBase::RawError error;
  };

  void InitRootDepthAndVisual();

  void AddRequest(unsigned int sequence, FutureBase::ResponseCallback callback);

  bool HasNextResponse();

  bool HasNextEvent();

  void PreDispatchEvent(const Event& event);

  int ScreenIndexFromRootWindow(x11::Window root) const;

  // This function is implemented in the generated read_error.cc.
  void InitErrorParsers();

  std::unique_ptr<Error> ParseError(FutureBase::RawError error_bytes);

  uint32_t GenerateIdImpl();

  xcb_connection_t* connection_ = nullptr;
  std::unique_ptr<XlibDisplay> xlib_display_;

  bool synchronous_ = false;
  bool syncing_ = false;

  uint32_t extended_max_request_length_ = 0;

  std::string display_string_;
  int default_screen_id_ = 0;
  Setup setup_;
  Screen* default_screen_ = nullptr;
  Depth* default_root_depth_ = nullptr;
  VisualType* default_root_visual_ = nullptr;

  std::unordered_map<VisualId, VisualInfo> default_screen_visuals_;

  std::unique_ptr<KeyboardState> keyboard_state_;

  base::circular_deque<Event> events_;

  std::queue<Request> requests_;

  using ErrorParser =
      std::unique_ptr<Error> (*)(FutureBase::RawError error_bytes);
  std::array<ErrorParser, 256> error_parsers_{};

  ErrorHandler error_handler_;
  IOErrorHandler io_error_handler_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace x11

#endif  // UI_GFX_X_CONNECTION_H_
