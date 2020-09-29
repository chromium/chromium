// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_CONNECTION_H_
#define UI_GFX_X_CONNECTION_H_

#include <list>
#include <queue>

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

// Represents a socket to the X11 server.
class COMPONENT_EXPORT(X11) Connection : public XProto,
                                         public ExtensionManager {
 public:
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

  XDisplay* display() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return display_;
  }
  xcb_connection_t* XcbConnection();

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

  int DefaultScreenId() const;

  template <typename T>
  T GenerateId() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return static_cast<T>(xcb_generate_id(XcbConnection()));
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
  bool HasPendingResponses() const;

  // Dispatch any buffered events, errors, or replies.
  void Dispatch(Delegate* delegate);

  // Returns the visual data for |id|, or nullptr if the visual with that ID
  // doesn't exist or only exists on a non-default screen.
  const VisualInfo* GetVisualInfoFromId(VisualId id) const;

  KeyCode KeysymToKeycode(KeySym keysym);

  KeySym KeycodeToKeysym(uint32_t keycode, unsigned int modifiers);

  // Access the event buffer.  Clients can add, delete, or modify events.
  std::list<Event>& events() {
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
  };

  void InitRootDepthAndVisual();

  void AddRequest(unsigned int sequence, FutureBase::ResponseCallback callback);

  bool HasNextResponse() const;

  void PreDispatchEvent(const Event& event);

  int ScreenIndexFromRootWindow(x11::Window root) const;

  void ResetKeyboardState();

  KeySym KeyCodetoKeySym(KeyCode keycode, int column) const;

  KeySym TranslateKey(uint32_t keycode, unsigned int modifiers) const;

  XDisplay* const display_;

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

  // Keyboard state.
  GetKeyboardMappingReply keyboard_mapping_;
  GetModifierMappingReply modifier_mapping_;
  uint16_t lock_meaning_ = 0;
  uint8_t mode_switch_ = 0;
  uint8_t num_lock_ = 0;

  std::list<Event> events_;

  std::queue<Request> requests_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace x11

#endif  // UI_GFX_X_CONNECTION_H_
