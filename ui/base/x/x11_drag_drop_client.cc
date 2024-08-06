// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_drag_drop_client.h"

#include "base/containers/flat_set.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/x/x11_os_exchange_data_provider.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/window_cache.h"
#include "ui/gfx/x/xproto.h"

// Reading recommended for understanding the implementation in this file:
//
// * The X Window System Concepts section in The X New Developer’s Guide
// * The X Selection Mechanism paper by Keith Packard
// * The Peer-to-Peer Communication by Means of Selections section in the
//   ICCCM (X Consortium's Inter-Client Communication Conventions Manual)
// * The XDND specification, Drag-and-Drop Protocol for the X Window System
// * The XDS specification, The Direct Save Protocol for the X Window System
//
// All the readings are freely available online.

namespace ui {

// Window property on the source window and message used by the XDS protocol.
// This atom name intentionally includes the XDS protocol version (0).
// After the source sends the XdndDrop message, this property stores the
// (path-less) name of the file to be saved, and has the type text/plain, with
// an optional charset attribute.
// When receiving an XdndDrop event, the target needs to check for the
// XdndDirectSave property on the source window. The target then modifies the
// XdndDirectSave on the source window, and sends an XdndDirectSave message to
// the source.
// After the target sends the XdndDirectSave message, this property stores an
// URL indicating the location where the source should save the file.
const char kXdndDirectSave0[] = "XdndDirectSave0";

namespace {

using mojom::DragOperation;

constexpr int kWillAcceptDrop = 1;
constexpr int kWantFurtherPosEvents = 2;

// The lowest XDND protocol version that we understand.
//
// The XDND protocol specification says that we must support all versions
// between 3 and the version we advertise in the XDndAware property.
constexpr int kMinXdndVersion = 3;

// The value used in the XdndAware property.
//
// The XDND protocol version used between two windows will be the minimum
// between the two versions advertised in the XDndAware property.
constexpr int kMaxXdndVersion = 5;

// Window property that tells other applications the window understands XDND.
const char kXdndAware[] = "XdndAware";

// Window property that holds the supported drag and drop data types.
// This property is set on the XDND source window when the drag and drop data
// can be converted to more than 3 types.
const char kXdndTypeList[] = "XdndTypeList";

// These actions have the same meaning as in the W3C Drag and Drop spec.
const char kXdndActionCopy[] = "XdndActionCopy";
const char kXdndActionMove[] = "XdndActionMove";
const char kXdndActionLink[] = "XdndActionLink";

// Triggers the XDS protocol.
const char kXdndActionDirectSave[] = "XdndActionDirectSave";

// Window property that contains the possible actions that will be presented to
// the user when the drag and drop action is kXdndActionAsk.
const char kXdndActionList[] = "XdndActionList";

// Window property pointing to a proxy window to receive XDND target messages.
// The XDND source must check the proxy window must for the XdndAware property,
// and must send all XDND messages to the proxy instead of the target. However,
// the target field in the messages must still represent the original target
// window (the window pointed to by the cursor).
const char kXdndProxy[] = "XdndProxy";

// Message sent from an XDND source to the target when the user confirms the
// drag and drop operation.
const char kXdndDrop[] = "XdndDrop";

// Message sent from an XDND source to the target to start the XDND protocol.
// The target must wait for an XDndPosition event before querying the data.
const char kXdndEnter[] = "XdndEnter";

// Message sent from an XDND target to the source in response to an XdndDrop.
// The message must be sent whether the target acceepts the drop or not.
const char kXdndFinished[] = "XdndFinished";

// Message sent from an XDND source to the target when the user cancels the drag
// and drop operation.
const char kXdndLeave[] = "XdndLeave";

// Message sent by the XDND source when the cursor position changes.
// The source will also send an XdndPosition event right after the XdndEnter
// event, to tell the target about the initial cursor position and the desired
// drop action.
// The time stamp in the XdndPosition must be used when requesting selection
// information.
// After the target optionally acquires selection information, it must tell the
// source if it can accept the drop via an XdndStatus message.
const char kXdndPosition[] = "XdndPosition";

// Message sent by the XDND target in response to an XdndPosition message.
// The message informs the source if the target will accept the drop, and what
// action will be taken if the drop is accepted.
const char kXdndStatus[] = "XdndStatus";

static base::LazyInstance<std::map<x11::Window, XDragDropClient*>>::Leaky
    g_live_client_map = LAZY_INSTANCE_INITIALIZER;

x11::Atom DragOperationToAtom(DragOperation operation) {
  switch (operation) {
    case DragOperation::kNone:
      return x11::Atom::None;
    case DragOperation::kCopy:
      return x11::GetAtom(kXdndActionCopy);
    case DragOperation::kMove:
      return x11::GetAtom(kXdndActionMove);
    case DragOperation::kLink:
      return x11::GetAtom(kXdndActionLink);
  }
  NOTREACHED();
}

DragOperation AtomToDragOperation(x11::Atom atom) {
  if (atom == x11::GetAtom(kXdndActionCopy)) {
    return DragOperation::kCopy;
  }
  if (atom == x11::GetAtom(kXdndActionMove)) {
    return DragOperation::kMove;
  }
  if (atom == x11::GetAtom(kXdndActionLink)) {
    return DragOperation::kLink;
  }
  return DragOperation::kNone;
}

}  // namespace

int XGetMaskAsEventFlags() {
  x11::KeyButMask mask{};
  auto* connection = x11::Connection::Get();
  if (auto reply =
          connection->QueryPointer({connection->default_root()}).Sync()) {
    mask = reply->mask;
  }

  int modifiers = ui::EF_NONE;
  if (static_cast<bool>(mask & x11::KeyButMask::Shift)) {
    modifiers |= ui::EF_SHIFT_DOWN;
  }
  if (static_cast<bool>(mask & x11::KeyButMask::Control)) {
    modifiers |= ui::EF_CONTROL_DOWN;
  }
  if (static_cast<bool>(mask & x11::KeyButMask::Mod1)) {
    modifiers |= ui::EF_ALT_DOWN;
  }
  if (static_cast<bool>(mask & x11::KeyButMask::Mod4)) {
    modifiers |= ui::EF_COMMAND_DOWN;
  }
  if (static_cast<bool>(mask & x11::KeyButMask::Button1)) {
    modifiers |= ui::EF_LEFT_MOUSE_BUTTON;
  }
  if (static_cast<bool>(mask & x11::KeyButMask::Button2)) {
    modifiers |= ui::EF_MIDDLE_MOUSE_BUTTON;
  }
  if (static_cast<bool>(mask & x11::KeyButMask::Button3)) {
    modifiers |= ui::EF_RIGHT_MOUSE_BUTTON;
  }
  return modifiers;
}

// static
XDragDropClient* XDragDropClient::GetForWindow(x11::Window window) {
  std::map<x11::Window, XDragDropClient*>::const_iterator it =
      g_live_client_map.Get().find(window);
  if (it == g_live_client_map.Get().end()) {
    return nullptr;
  }
  return it->second;
}

XDragDropClient::XDragDropClient(XDragDropClient::Delegate* delegate,
                                 x11::Window xwindow)
    : delegate_(delegate), xwindow_(xwindow) {
  DCHECK(delegate_);

  // Mark that we are aware of drag and drop concepts.
  uint32_t xdnd_version = kMaxXdndVersion;
  x11::Connection::Get()->SetProperty(xwindow_, x11::GetAtom(kXdndAware),
                                      x11::Atom::ATOM, xdnd_version);

  // Some tests change the XDragDropClient associated with an |xwindow|.
  g_live_client_map.Get()[xwindow] = this;
}

XDragDropClient::~XDragDropClient() {
  g_live_client_map.Get().erase(xwindow());
}

std::vector<x11::Atom> XDragDropClient::GetOfferedDragOperations() const {
  std::vector<x11::Atom> operations;
  if (allowed_operations_ & DragDropTypes::DRAG_COPY) {
    operations.push_back(x11::GetAtom(kXdndActionCopy));
  }
  if (allowed_operations_ & DragDropTypes::DRAG_MOVE) {
    operations.push_back(x11::GetAtom(kXdndActionMove));
  }
  if (allowed_operations_ & DragDropTypes::DRAG_LINK) {
    operations.push_back(x11::GetAtom(kXdndActionLink));
  }
  return operations;
}

void XDragDropClient::CompleteXdndPosition(x11::Window source_window,
                                           const gfx::Point& screen_point) {
  DragOperation drag_operation =
      PreferredDragOperation(delegate_->UpdateDrag(screen_point));

  // Sends an XdndStatus message back to the source_window. l[2,3]
  // theoretically represent an area in the window where the current action is
  // the same as what we're returning, but I can't find any implementation that
  // actually making use of this. A client can return (0, 0) and/or set the
  // first bit of l[1] to disable the feature, and it appears that gtk neither
  // sets this nor respects it if set.
  auto xev = PrepareXdndClientMessage(kXdndStatus, source_window);
  xev.data.data32[1] = (drag_operation != DragOperation::kNone)
                           ? (kWantFurtherPosEvents | kWillAcceptDrop)
                           : 0;
  xev.data.data32[4] =
      static_cast<uint32_t>(DragOperationToAtom(drag_operation));
  SendXClientEvent(source_window, xev);
}

void XDragDropClient::ProcessMouseMove(const gfx::Point& screen_point,
                                       unsigned long event_time) {
  if (source_state_ != SourceState::kOther) {
    return;
  }

  // Find the current window the cursor is over.
  x11::Window dest_window = FindWindowFor(screen_point);

  if (target_current_window_ != dest_window) {
    if (target_current_window_ != x11::Window::None) {
      SendXdndLeave(target_current_window_);
    }

    target_current_window_ = dest_window;
    waiting_on_status_ = false;
    next_position_message_.reset();
    status_received_since_enter_ = false;
    negotiated_operation_ = DragOperation::kNone;

    if (target_current_window_ != x11::Window::None) {
      std::vector<x11::Atom> targets;
      source_provider_->RetrieveTargets(&targets);
      SendXdndEnter(target_current_window_, targets);
    }
  }

  if (target_current_window_ != x11::Window::None) {
    if (waiting_on_status_) {
      next_position_message_ =
          std::make_unique<std::pair<gfx::Point, unsigned long>>(screen_point,
                                                                 event_time);
    } else {
      SendXdndPosition(dest_window, screen_point, event_time);
    }
  }
}

bool XDragDropClient::HandleXdndEvent(const x11::ClientMessageEvent& event) {
  x11::Atom message_type = event.type;
  if (message_type == x11::GetAtom("XdndEnter")) {
    OnXdndEnter(event);
  } else if (message_type == x11::GetAtom("XdndLeave")) {
    OnXdndLeave(event);
  } else if (message_type == x11::GetAtom("XdndPosition")) {
    OnXdndPosition(event);
  } else if (message_type == x11::GetAtom("XdndStatus")) {
    OnXdndStatus(event);
  } else if (message_type == x11::GetAtom("XdndFinished")) {
    OnXdndFinished(event);
  } else if (message_type == x11::GetAtom("XdndDrop")) {
    OnXdndDrop(event);
  } else {
    return false;
  }
  return true;
}

void XDragDropClient::OnXdndEnter(const x11::ClientMessageEvent& event) {
  DVLOG(1) << "OnXdndEnter, version "
           << ((event.data.data32[1] & 0xff000000) >> 24);

  int version = (event.data.data32[1] & 0xff000000) >> 24;
  if (version < kMinXdndVersion) {
    // This protocol version is not documented in the XDND standard (last
    // revised in 1999), so we don't support it. Since don't understand the
    // protocol spoken by the source, we can't tell it that we can't talk to it.
    LOG(ERROR) << "XdndEnter message discarded because its version is too old.";
    return;
  }
  if (version > kMaxXdndVersion) {
    // The XDND version used should be the minimum between the versions
    // advertised by the source and the target. We advertise kMaxXdndVersion, so
    // this should never happen when talking to an XDND-compliant application.
    LOG(ERROR) << "XdndEnter message discarded because its version is too new.";
    return;
  }

  // Make sure that we've run ~X11DragContext() before creating another one.
  ResetDragContext();
  auto* source_client =
      GetForWindow(static_cast<x11::Window>(event.data.data32[0]));
  DCHECK(!source_client || source_client->source_provider_);
  target_current_context_ = std::make_unique<XDragContext>(
      xwindow_, event,
      (source_client ? source_client->source_provider_->GetFormatMap()
                     : SelectionFormatMap()));

  if (!source_client) {
    // The window doesn't have a XDragDropClient, which means it's
    // created by some other process, i.e: incoming drag session. Thus,
    // start listening for messages on it.
    delegate_->OnBeginForeignDrag(
        static_cast<x11::Window>(event.data.data32[0]));
  }

  // In the Windows implementation, we immediately call DesktopDropTargetWin::
  // Translate().  The XDND specification demands that we wait until we receive
  // an XdndPosition message before we use XConvertSelection or send an
  // XdndStatus message.
}

void XDragDropClient::OnXdndPosition(const x11::ClientMessageEvent& event) {
  DVLOG(1) << "OnXdndPosition";

  auto source_window = static_cast<x11::Window>(event.data.data32[0]);
  int x_root_window = event.data.data32[2] >> 16;
  int y_root_window = event.data.data32[2] & 0xffff;
  x11::Time time_stamp = static_cast<x11::Time>(event.data.data32[3]);
  x11::Atom suggested_action = static_cast<x11::Atom>(event.data.data32[4]);

  CHECK(target_current_context());

  target_current_context()->OnXdndPositionMessage(
      this, suggested_action, source_window, time_stamp,
      gfx::Point(x_root_window, y_root_window));
}

void XDragDropClient::OnXdndStatus(const x11::ClientMessageEvent& event) {
  DVLOG(1) << "OnXdndStatus";

  auto source_window = static_cast<x11::Window>(event.data.data32[0]);

  if (source_window != target_current_window_) {
    return;
  }

  if (source_state_ != SourceState::kPendingDrop &&
      source_state_ != SourceState::kOther) {
    return;
  }

  waiting_on_status_ = false;
  status_received_since_enter_ = true;

  if (event.data.data32[1] & 1) {
    x11::Atom atom_operation = static_cast<x11::Atom>(event.data.data32[4]);
    negotiated_operation_ = AtomToDragOperation(atom_operation);
  } else {
    negotiated_operation_ = DragOperation::kNone;
  }

  if (source_state_ == SourceState::kPendingDrop) {
    // We were waiting on the status message so we could send the XdndDrop.
    if (negotiated_operation_ == DragOperation::kNone) {
      EndMoveLoop();
      return;
    }
    source_state_ = SourceState::kDropped;
    SendXdndDrop(source_window);
    return;
  }

  delegate_->UpdateCursor(negotiated_operation_);

  // Note: event.data.[2,3] specify a rectangle. It is a request by the other
  // window to not send further XdndPosition messages while the cursor is
  // within it. However, it is considered advisory and (at least according to
  // the spec) the other side must handle further position messages within
  // it. GTK+ doesn't bother with this, so neither should we.

  if (next_position_message_.get()) {
    // We were waiting on the status message so we could send off the next
    // position message we queued up.
    gfx::Point p = next_position_message_->first;
    unsigned long event_time = next_position_message_->second;
    next_position_message_.reset();

    SendXdndPosition(source_window, p, event_time);
  }
}

void XDragDropClient::OnXdndLeave(const x11::ClientMessageEvent& event) {
  DVLOG(1) << "OnXdndLeave";
  delegate_->OnBeforeDragLeave();
  ResetDragContext();
}

void XDragDropClient::OnXdndDrop(const x11::ClientMessageEvent& event) {
  DVLOG(1) << "OnXdndDrop";

  auto source_window = static_cast<x11::Window>(event.data.data32[0]);

  DragOperation drag_operation = delegate_->PerformDrop();

  auto xev = PrepareXdndClientMessage(kXdndFinished, source_window);
  xev.data.data32[1] = (drag_operation != DragOperation::kNone) ? 1 : 0;
  xev.data.data32[2] =
      static_cast<uint32_t>(DragOperationToAtom(drag_operation));
  SendXClientEvent(source_window, xev);
}

void XDragDropClient::OnXdndFinished(const x11::ClientMessageEvent& event) {
  DVLOG(1) << "OnXdndFinished";
  auto source_window = static_cast<x11::Window>(event.data.data32[0]);
  if (target_current_window_ != source_window) {
    return;
  }

  // Clear |negotiated_operation_| if the drag was rejected.
  if ((event.data.data32[1] & 1) == 0) {
    negotiated_operation_ = DragOperation::kNone;
  }

  // Clear |target_current_window_| to avoid sending XdndLeave upon ending the
  // move loop.
  target_current_window_ = x11::Window::None;
  EndMoveLoop();
}

void XDragDropClient::OnSelectionNotify(
    const x11::SelectionNotifyEvent& xselection) {
  DVLOG(1) << "OnSelectionNotify";
  if (target_current_context_) {
    target_current_context_->OnSelectionNotify(xselection);
  }

  // ICCCM requires us to delete the property passed into SelectionNotify.
  if (xselection.property != x11::Atom::None) {
    x11::Connection::Get()->DeleteProperty(xwindow_, xselection.property);
  }
}

void XDragDropClient::InitDrag(int allowed_operations,
                               const OSExchangeData* data) {
  target_current_window_ = x11::Window::None;
  source_state_ = SourceState::kOther;
  waiting_on_status_ = false;
  next_position_message_.reset();
  status_received_since_enter_ = false;
  allowed_operations_ = allowed_operations;
  negotiated_operation_ = DragOperation::kNone;

  source_provider_ =
      static_cast<const XOSExchangeDataProvider*>(&data->provider());
  source_provider_->TakeOwnershipOfSelection();

  std::vector<x11::Atom> actions = GetOfferedDragOperations();
  if (!source_provider_->file_contents_name().empty()) {
    actions.push_back(x11::GetAtom(kXdndActionDirectSave));
    x11::Connection::Get()->SetStringProperty(
        xwindow_, x11::GetAtom(kXdndDirectSave0), x11::GetAtom(kMimeTypeText),
        source_provider_->file_contents_name().AsUTF8Unsafe());
  }
  x11::Connection::Get()->SetArrayProperty(
      xwindow_, x11::GetAtom(kXdndActionList), x11::Atom::ATOM, actions);
}

void XDragDropClient::CleanupDrag() {
  source_provider_ = nullptr;
  x11::Connection::Get()->DeleteProperty(xwindow_,
                                         x11::GetAtom(kXdndActionList));
  x11::Connection::Get()->DeleteProperty(xwindow_,
                                         x11::GetAtom(kXdndDirectSave0));
}

void XDragDropClient::UpdateModifierState(int flags) {
  const int kModifiers = EF_SHIFT_DOWN | EF_CONTROL_DOWN | EF_ALT_DOWN |
                         EF_COMMAND_DOWN | EF_LEFT_MOUSE_BUTTON |
                         EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON;
  current_modifier_state_ = flags & kModifiers;
}

void XDragDropClient::ResetDragContext() {
  if (!target_current_context()) {
    return;
  }
  XDragDropClient* source_client =
      GetForWindow(target_current_context()->source_window());
  if (!source_client) {
    delegate_->OnEndForeignDrag();
  }

  target_current_context_.reset();
}

void XDragDropClient::StopRepeatMouseMoveTimer() {
  repeat_mouse_move_timer_.Stop();
}

void XDragDropClient::StartEndMoveLoopTimer() {
  end_move_loop_timer_.Start(FROM_HERE, base::Milliseconds(1000), this,
                             &XDragDropClient::EndMoveLoop);
}

void XDragDropClient::StopEndMoveLoopTimer() {
  end_move_loop_timer_.Stop();
}

void XDragDropClient::HandleMouseMovement(const gfx::Point& screen_point,
                                          int flags,
                                          base::TimeTicks event_time) {
  UpdateModifierState(flags);
  StopRepeatMouseMoveTimer();
  ProcessMouseMove(screen_point,
                   (event_time - base::TimeTicks()).InMilliseconds());
}

void XDragDropClient::HandleMouseReleased() {
  StopRepeatMouseMoveTimer();

  if (source_state_ != SourceState::kOther) {
    // The user has previously released the mouse and is clicking in
    // frustration.
    EndMoveLoop();
    return;
  }

  if (target_current_window_ != x11::Window::None) {
    if (waiting_on_status_) {
      if (status_received_since_enter_) {
        // If we are waiting for an XdndStatus message, we need to wait for it
        // to complete.
        source_state_ = SourceState::kPendingDrop;

        // Start timer to end the move loop if the target takes too long to send
        // the XdndStatus and XdndFinished messages.
        StartEndMoveLoopTimer();
        return;
      }

      EndMoveLoop();
      return;
    }

    if (negotiated_operation() != DragOperation::kNone) {
      // Start timer to end the move loop if the target takes too long to send
      // an XdndFinished message. It is important that StartEndMoveLoopTimer()
      // is called before SendXdndDrop() because SendXdndDrop()
      // sends XdndFinished synchronously if the drop target is a Chrome
      // window.
      StartEndMoveLoopTimer();

      // We have negotiated an action with the other end.
      source_state_ = SourceState::kDropped;
      SendXdndDrop(target_current_window_);
      return;
    } else {
      // No transfer is negotiated.  We need to tell the target window that we
      // are leaving.
      SendXdndLeave(target_current_window_);
    }
  }

  EndMoveLoop();
}

void XDragDropClient::HandleMoveLoopEnded() {
  if (target_current_window_ != x11::Window::None) {
    SendXdndLeave(target_current_window_);
    target_current_window_ = x11::Window::None;
  }
  ResetDragContext();
  StopRepeatMouseMoveTimer();
  StopEndMoveLoopTimer();
}

x11::ClientMessageEvent XDragDropClient::PrepareXdndClientMessage(
    const char* message,
    x11::Window recipient) const {
  x11::ClientMessageEvent xev;
  xev.type = x11::GetAtom(message);
  xev.window = recipient;
  xev.format = 32;
  xev.data.data32.fill(0);
  xev.data.data32[0] = static_cast<uint32_t>(xwindow_);
  return xev;
}

x11::Window XDragDropClient::FindWindowFor(const gfx::Point& screen_point) {
  base::flat_set<x11::Window> ignore;
  if (auto dragged_window = delegate_->GetDragWidget()) {
    ignore.insert(static_cast<x11::Window>(*dragged_window));
  }
  auto target = x11::GetWindowAtPoint(screen_point, &ignore);

  if (target == x11::Window::None) {
    return x11::Window::None;
  }

  // TODO(crbug.com/41278320): The proxy window should be reported separately
  // from the
  //     target window. XDND messages should be sent to the proxy, and their
  //     window field should point to the target.

  // Figure out which window we should test as XdndAware. If |target| has
  // XdndProxy, it will set that proxy on target, and if not, |target|'s
  // original value will remain.
  x11::Connection::Get()->GetPropertyAs(target, x11::GetAtom(kXdndProxy),
                                        &target);

  uint32_t version;
  if (x11::Connection::Get()->GetPropertyAs(target, x11::GetAtom(kXdndAware),
                                            &version) &&
      version >= kMaxXdndVersion) {
    return target;
  }
  return x11::Window::None;
}

void XDragDropClient::SendXClientEvent(x11::Window window,
                                       const x11::ClientMessageEvent& xev) {
  // Don't send messages to the X11 message queue if we can help it.
  XDragDropClient* short_circuit = GetForWindow(window);
  if (short_circuit && short_circuit->HandleXdndEvent(xev)) {
    return;
  }

  // I don't understand why the GTK+ code is doing what it's doing here. It
  // goes out of its way to send the XEvent so that it receives a callback on
  // success or failure, and when it fails, it then sends an internal
  // GdkEvent about the failed drag. (And sending this message doesn't appear
  // to go through normal xlib machinery, but instead passes through the low
  // level xProto (the x11 wire format) that I don't understand.
  //
  // I'm unsure if I have to jump through those hoops, or if XSendEvent is
  // sufficient.
  x11::Connection::Get()->SendEvent(xev, window, x11::EventMask::NoEvent);
}

void XDragDropClient::SendXdndEnter(x11::Window dest_window,
                                    const std::vector<x11::Atom>& targets) {
  auto xev = PrepareXdndClientMessage(kXdndEnter, dest_window);
  xev.data.data32[1] = (kMaxXdndVersion << 24);  // The version number.

  if (targets.size() > 3) {
    xev.data.data32[1] |= 1;
    x11::Connection::Get()->SetArrayProperty(
        xwindow(), x11::GetAtom(kXdndTypeList), x11::Atom::ATOM, targets);
  } else {
    // Pack the targets into the enter message.
    for (size_t i = 0; i < targets.size(); ++i) {
      xev.data.data32[2 + i] = static_cast<uint32_t>(targets[i]);
    }
  }

  SendXClientEvent(dest_window, xev);
}

void XDragDropClient::SendXdndPosition(x11::Window dest_window,
                                       const gfx::Point& screen_point,
                                       unsigned long event_time) {
  waiting_on_status_ = true;

  auto xev = PrepareXdndClientMessage(kXdndPosition, dest_window);
  xev.data.data32[2] = (screen_point.x() << 16) | screen_point.y();
  xev.data.data32[3] = event_time;
  xev.data.data32[4] = static_cast<uint32_t>(
      DragOperationToAtom(PreferredDragOperation(allowed_operations_)));
  SendXClientEvent(dest_window, xev);

  // http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html and
  // the Xdnd protocol both recommend that drag events should be sent
  // periodically.
  repeat_mouse_move_timer_.Start(
      FROM_HERE, base::Milliseconds(350),
      base::BindOnce(&XDragDropClient::ProcessMouseMove, base::Unretained(this),
                     screen_point, event_time));
}

void XDragDropClient::SendXdndLeave(x11::Window dest_window) {
  auto xev = PrepareXdndClientMessage(kXdndLeave, dest_window);
  SendXClientEvent(dest_window, xev);
}

void XDragDropClient::SendXdndDrop(x11::Window dest_window) {
  auto xev = PrepareXdndClientMessage(kXdndDrop, dest_window);
  xev.data.data32[2] = static_cast<uint32_t>(x11::Time::CurrentTime);
  SendXClientEvent(dest_window, xev);
}

void XDragDropClient::EndMoveLoop() {
  StopEndMoveLoopTimer();
  delegate_->EndDragLoop();
}

}  // namespace ui
