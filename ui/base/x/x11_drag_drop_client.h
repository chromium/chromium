// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_DRAG_DROP_CLIENT_H_
#define UI_BASE_X_X11_DRAG_DROP_CLIENT_H_

#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_drag_context.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

class OSExchangeData;
class XOSExchangeDataProvider;

extern const char kXdndDirectSave0[];

// Converts the current set of X masks into the set of ui::EventFlags.
COMPONENT_EXPORT(UI_BASE_X) int XGetMaskAsEventFlags();

// Works for both incoming and outgoing drags.  For the incoming drags, receives
// XDND events via HandleXdndEvent() and routes them to OnXdnd...() handlers;
// outgoing drags are handled via Handle...() methods.  Both ways end up in
// SendXdnd...() calls that target the window that is currently at the drag
// location.  If the target is another Chrome window, the event is delivered
// directly to its XdndHandler, bypassing the X server.
//
// The owner is notified about the ongoing drag through the Delegate interface.
class COMPONENT_EXPORT(UI_BASE_X) XDragDropClient {
 public:
  // Handlers and callbacks that should be implemented at the consumer side.
  class Delegate {
   public:
    // Get the window being dragged. This window should be ignored when finding
    // the topmost window.
    virtual std::optional<gfx::AcceleratedWidget> GetDragWidget() = 0;

    // Updates the drag status by the new position. Returns the drag operations
    // possible at that position.
    //
    // Handling XdndPosition can be paused while waiting for more data; this is
    // called either synchronously from OnXdndPosition, or asynchronously after
    // we've received data requested from the other window.
    virtual int UpdateDrag(const gfx::Point& screen_point) = 0;

    // Updates the mouse cursor shape.
    virtual void UpdateCursor(mojom::DragOperation negotiated_operation) = 0;

    // Called when data from another application (not Chrome) enters the window.
    virtual void OnBeginForeignDrag(x11::Window window) = 0;

    // Called when data from another application (not Chrome) is about to leave
    // the window.
    virtual void OnEndForeignDrag() = 0;

    // Called just before the drag leaves the window.
    virtual void OnBeforeDragLeave() = 0;

    // Drops data at the current location and returns the resulting operation.
    virtual mojom::DragOperation PerformDrop() = 0;

    // Called to end the drag loop that is maintained by the subclass.
    virtual void EndDragLoop() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  XDragDropClient(Delegate* delegate, x11::Window xwindow);
  virtual ~XDragDropClient();
  XDragDropClient(const XDragDropClient&) = delete;
  XDragDropClient& operator=(const XDragDropClient&) = delete;

  // We maintain a mapping of live XDragDropClient objects to their X11 windows,
  // so that we'd able to short circuit sending X11 messages to windows in our
  // process.
  static XDragDropClient* GetForWindow(x11::Window window);

  x11::Window xwindow() const { return xwindow_; }
  XDragContext* target_current_context() {
    return target_current_context_.get();
  }
  const XOSExchangeDataProvider* source_provider() const {
    return source_provider_;
  }
  int current_modifier_state() const { return current_modifier_state_; }

  // Handling XdndPosition can be paused while waiting for more data; this is
  // called by XDragContext either synchronously or asynchronously, depending on
  // whether the context has data requested from the other window.
  void CompleteXdndPosition(x11::Window source_window,
                            const gfx::Point& screen_point);

  void ProcessMouseMove(const gfx::Point& screen_point,
                        unsigned long event_time);

  // During the blocking StartDragAndDrop() call, this converts the views-style
  // |drag_operation_| bitfield into a vector of Atoms to offer to other
  // processes.
  std::vector<x11::Atom> GetOfferedDragOperations() const;

  // Tries to handle the XDND event.  Returns true for all known event types:
  // XdndEnter, XdndLeave, XdndPosition, XdndStatus, XdndDrop, and XdndFinished;
  // returns false if an event of an unexpected type has been passed.
  bool HandleXdndEvent(const x11::ClientMessageEvent& event);

  // These |Handle...| methods essentially implement the
  // views::X11MoveLoopDelegate interface.
  void HandleMouseMovement(const gfx::Point& screen_point,
                           int flags,
                           base::TimeTicks event_time);
  void HandleMouseReleased();
  void HandleMoveLoopEnded();

  // Called when XSelection data has been copied to our process.
  void OnSelectionNotify(const x11::SelectionNotifyEvent& xselection);

  // Resets the drag state so the object is ready to handle the drag.  Sets
  // X window properties so that the desktop environment is aware of available
  // actions.  Sets |source_provider_| so properties of dragged data can be
  // queried afterwards.  Should be called before entering the main drag loop.
  void InitDrag(int operation, const OSExchangeData* data);

  // Cleans up the drag state after the drag is done.  Removes the X window
  // properties related to the drag operation.  Should be called after exiting
  // the main drag loop.
  void CleanupDrag();

 protected:
  enum class SourceState {
    // |source_current_window_| will receive a drop once we receive an
    // XdndStatus from it.
    kPendingDrop,

    // The move looped will be ended once we receive XdndFinished from
    // |source_current_window_|. We should not send XdndPosition to
    // |source_current_window_| while in this state.
    kDropped,

    // There is no drag in progress or there is a drag in progress and the
    // user has not yet released the mouse.
    kOther,
  };

  mojom::DragOperation negotiated_operation() const {
    return negotiated_operation_;
  }

  // Updates |current_modifier_state_| with the given set of flags.
  void UpdateModifierState(int flags);

  // Resets the drag context.  Calls |OnEndForeignDrag()| if necessary.
  void ResetDragContext();

  void StopRepeatMouseMoveTimer();

  // Start timer to end the move loop if the target is too slow to respond after
  // the mouse is released.
  void StartEndMoveLoopTimer();
  void StopEndMoveLoopTimer();

 private:
  // These methods handle the various X11 client messages from the platform.
  void OnXdndEnter(const x11::ClientMessageEvent& event);
  void OnXdndPosition(const x11::ClientMessageEvent& event);
  void OnXdndStatus(const x11::ClientMessageEvent& event);
  void OnXdndLeave(const x11::ClientMessageEvent& event);
  void OnXdndDrop(const x11::ClientMessageEvent& event);
  void OnXdndFinished(const x11::ClientMessageEvent& event);

  // Creates an XEvent and fills it in with values typical for XDND messages:
  // the type of event is set to ClientMessage, the format is set to 32 (longs),
  // and the zero member of data payload is set to |xwindow_|.  All other data
  // members are zeroed, as per XDND specification.
  x11::ClientMessageEvent PrepareXdndClientMessage(const char* message,
                                                   x11::Window recipient) const;

  // Finds the topmost X11 window at |screen_point| and returns it if it is
  // Xdnd aware.  Returns x11::None otherwise.
  // Virtual for testing.
  virtual x11::Window FindWindowFor(const gfx::Point& screen_point);

  // Sends |xev| to |window|, optionally short circuiting the round trip to the
  // X server. Virtual for testing.
  virtual void SendXClientEvent(x11::Window window,
                                const x11::ClientMessageEvent& xev);

  void SendXdndEnter(x11::Window dest_window,
                     const std::vector<x11::Atom>& targets);
  void SendXdndPosition(x11::Window dest_window,
                        const gfx::Point& screen_point,
                        unsigned long event_time);
  void SendXdndLeave(x11::Window dest_window);
  void SendXdndDrop(x11::Window dest_window);

  void EndMoveLoop();

  const raw_ptr<Delegate> delegate_;

  const x11::Window xwindow_;

  // Target side information.
  x11::Window target_current_window_ = x11::Window::None;
  std::unique_ptr<XDragContext> target_current_context_;

  // Source side information.
  SourceState source_state_ = SourceState::kOther;
  raw_ptr<const XOSExchangeDataProvider> source_provider_ = nullptr;

  // The operation bitfield as requested by StartDragAndDrop.
  int allowed_operations_ = 0;

  // The modifier state for the most recent mouse move.  Used to bypass an
  // asynchronous roundtrip through the X11 server.
  int current_modifier_state_ = 0;

  // We offer the other window a list of possible operations, XdndActionsList.
  // This is the requested action from the other window. This is
  // DragOperation::kNone if we haven't sent out an XdndPosition message yet,
  // haven't yet received an XdndStatus or if the other window has told us that
  // there's no action that we can agree on.
  mojom::DragOperation negotiated_operation_ = mojom::DragOperation::kNone;

  // In the Xdnd protocol, we aren't supposed to send another XdndPosition
  // message until we have received a confirming XdndStatus message.
  bool waiting_on_status_ = false;

  // If we would send an XdndPosition message while we're waiting for an
  // XdndStatus response, we need to cache the latest details we'd send.
  std::unique_ptr<std::pair<gfx::Point, unsigned long>> next_position_message_;

  // Reprocesses the most recent mouse move event if the mouse has not moved
  // in a while in case the window stacking order has changed and
  // |source_current_window_| needs to be updated.
  base::OneShotTimer repeat_mouse_move_timer_;

  // Ends the move loop if the target is too slow to respond after the mouse is
  // released.
  base::OneShotTimer end_move_loop_timer_;

  // When the mouse is released, we need to wait for the last XdndStatus message
  // only if we have previously received a status message from
  // |source_current_window_|.
  bool status_received_since_enter_ = false;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_DRAG_DROP_CLIENT_H_
