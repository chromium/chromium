// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_aurax11.h"

#include <stddef.h>
#include <stdint.h>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aurax11.h"
#include "ui/base/layout.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_window_event_manager.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/x11_topmost_window_finder.h"
#include "ui/views/widget/desktop_aura/x11_whole_screen_move_loop.h"
#include "ui/views/widget/widget.h"

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

using aura::client::DragDropDelegate;
using ui::OSExchangeData;

namespace {

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

constexpr int kWillAcceptDrop = 1;
constexpr int kWantFurtherPosEvents = 2;

// These actions have the same meaning as in the W3C Drag and Drop spec.
const char kXdndActionCopy[] = "XdndActionCopy";
const char kXdndActionMove[] = "XdndActionMove";
const char kXdndActionLink[] = "XdndActionLink";

// Triggers the XDS protocol.
const char kXdndActionDirectSave[] = "XdndActionDirectSave";

// Window property that will receive the drag and drop selection data.
const char kChromiumDragReciever[] = "_CHROMIUM_DRAG_RECEIVER";

// Window property that contains the possible actions that will be presented to
// the user when the drag and drop action is kXdndActionAsk.
const char kXdndActionList[] = "XdndActionList";

// Window property that tells other applications the window understands XDND.
const char kXdndAware[] = "XdndAware";

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

// Window property pointing to a proxy window to receive XDND target messages.
// The XDND source must check the proxy window must for the XdndAware property,
// and must send all XDND messages to the proxy instead of the target. However,
// the target field in the messages must still represent the original target
// window (the window pointed to by the cursor).
const char kXdndProxy[] = "XdndProxy";

// Window property that holds the supported drag and drop data types.
// This property is set on the XDND source window when the drag and drop data
// can be converted to more than 3 types.
const char kXdndTypeList[] = "XdndTypeList";

// Selection used by the XDND protocol to transfer data between applications.
const char kXdndSelection[] = "XdndSelection";

// Message sent from an XDND source to the target when the user confirms the
// drag and drop operation.
const char kXdndDrop[] = "XdndDrop";

// Message sent from an XDND source to the target to start the XDND protocol.
// The target must wait for an XDndPosition event before querying the data.
const char kXdndEnter[] = "XdndEnter";

// Message sent from an XDND target to the source in respose to an XdndDrop.
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

int XGetModifiers() {
  XDisplay* display = gfx::GetXDisplay();

  XID root, child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;
  XQueryPointer(display,
                DefaultRootWindow(display),
                &root,
                &child,
                &root_x,
                &root_y,
                &win_x,
                &win_y,
                &mask);
  int modifiers = ui::EF_NONE;
  if (mask & ShiftMask)
    modifiers |= ui::EF_SHIFT_DOWN;
  if (mask & ControlMask)
    modifiers |= ui::EF_CONTROL_DOWN;
  if (mask & Mod1Mask)
    modifiers |= ui::EF_ALT_DOWN;
  if (mask & Mod4Mask)
    modifiers |= ui::EF_COMMAND_DOWN;
  if (mask & Button1Mask)
    modifiers |= ui::EF_LEFT_MOUSE_BUTTON;
  if (mask & Button2Mask)
    modifiers |= ui::EF_MIDDLE_MOUSE_BUTTON;
  if (mask & Button3Mask)
    modifiers |= ui::EF_RIGHT_MOUSE_BUTTON;
  return modifiers;
}

// The time to wait for the target to respond after the user has released the
// mouse button before ending the move loop.
const int kEndMoveLoopTimeoutMs = 1000;

// The time to wait since sending the last XdndPosition message before
// reprocessing the most recent mouse move event in case that the window
// stacking order has changed and |source_current_window_| needs to be updated.
const int kRepeatMouseMoveTimeoutMs = 350;

// The minimum alpha before we declare a pixel transparent when searching in
// our source image.
const uint32_t kMinAlpha = 32;

// |drag_widget_|'s opacity.
const float kDragWidgetOpacity = .75f;

static base::LazyInstance<
    std::map<::Window, views::DesktopDragDropClientAuraX11*> >::Leaky
        g_live_client_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace views {

DesktopDragDropClientAuraX11*
DesktopDragDropClientAuraX11::g_current_drag_drop_client = NULL;

class DesktopDragDropClientAuraX11::X11DragContext
    : public ui::PlatformEventDispatcher {
 public:
  X11DragContext(::Window local_window, const XClientMessageEvent& event);
  ~X11DragContext() override;

  // When we receive an XdndPosition message, we need to have all the data
  // copied from the other window before we process the XdndPosition
  // message. If we have that data already, dispatch immediately. Otherwise,
  // delay dispatching until we do.
  void OnXdndPositionMessage(DesktopDragDropClientAuraX11* client,
                                  ::Atom suggested_action,
                                  ::Window source_window,
                                  ::Time time_stamp,
                                  const gfx::Point& screen_point);

  // Called when XSelection data has been copied to our process.
  void OnSelectionNotify(const XSelectionEvent& xselection);

  // Clones the fetched targets.
  const ui::SelectionFormatMap& fetched_targets() { return fetched_targets_; }

  // Reads the kXdndActionList property from |source_window| and copies it
  // into |actions|.
  void ReadActions();

  // Creates a ui::DragDropTypes::DragOperation representation of the current
  // action list.
  int GetDragOperation() const;

  DesktopDragDropClientAuraX11* source_client() { return source_client_; }

 private:
  // Called to request the next target from the source window. This is only
  // done on the first XdndPosition; after that, we cache the data offered by
  // the source window.
  void RequestNextTarget();

  // Masks the X11 atom |xdnd_operation|'s views representation onto
  // |drag_operation|.
  void MaskOperation(::Atom xdnd_operation, int* drag_operation) const;

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  // The XID of our chrome local aura window handling our events.
  ::Window local_window_;

  // The XID of the window that's initiated the drag.
  unsigned long source_window_;

  // Events that we have selected on |source_window_|.
  std::unique_ptr<ui::XScopedEventSelector> source_window_events_;

  // The DesktopDragDropClientAuraX11 for |source_window_| if |source_window_|
  // belongs to a Chrome window.
  DesktopDragDropClientAuraX11* source_client_;

  // The client we inform once we're done with requesting data.
  DesktopDragDropClientAuraX11* drag_drop_client_;

  // Whether we're blocking the handling of an XdndPosition message by waiting
  // for |unfetched_targets_| to be fetched.
  bool waiting_to_handle_position_;

  // Where the cursor is on screen.
  gfx::Point screen_point_;

  // The time stamp of the last XdndPosition event we received. The XDND
  // specification mandates that we use this time stamp when querying the source
  // about the drag and drop data.
  ::Time position_time_stamp_;

  // A SelectionFormatMap of data that we have in our process.
  ui::SelectionFormatMap fetched_targets_;

  // The names of various data types offered by the other window that we
  // haven't fetched and put in |fetched_targets_| yet.
  std::vector<::Atom> unfetched_targets_;

  // XdndPosition messages have a suggested action. Qt applications exclusively
  // use this, instead of the XdndActionList which is backed by |actions_|.
  ::Atom suggested_action_;

  // Possible actions.
  std::vector<::Atom> actions_;

  DISALLOW_COPY_AND_ASSIGN(X11DragContext);
};

DesktopDragDropClientAuraX11::X11DragContext::X11DragContext(
    ::Window local_window,
    const XClientMessageEvent& event)
    : local_window_(local_window),
      source_window_(event.data.l[0]),
      source_client_(
          DesktopDragDropClientAuraX11::GetForWindow(source_window_)),
      drag_drop_client_(NULL),
      waiting_to_handle_position_(false),
      suggested_action_(x11::None) {
  if (!source_client_) {
    bool get_types_from_property = ((event.data.l[1] & 1) != 0);

    if (get_types_from_property) {
      if (!ui::GetAtomArrayProperty(source_window_, kXdndTypeList,
                                    &unfetched_targets_)) {
        return;
      }
    } else {
      // data.l[2,3,4] contain the first three types. Unused slots can be None.
      for (int i = 0; i < 3; ++i) {
        if (event.data.l[2 + i] != x11::None) {
          unfetched_targets_.push_back(event.data.l[2 + i]);
        }
      }
    }

#if DCHECK_IS_ON()
    DVLOG(1) << "XdndEnter has " << unfetched_targets_.size() << " data types";
    for (::Atom target : unfetched_targets_) {
      DVLOG(1) << "XdndEnter data type: " << target;
    }
#endif  // DCHECK_IS_ON()

    // The window doesn't have a DesktopDragDropClientAuraX11, that means it's
    // created by some other process. Listen for messages on it.
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
    source_window_events_.reset(
        new ui::XScopedEventSelector(source_window_, PropertyChangeMask));

    // We must perform a full sync here because we could be racing
    // |source_window_|.
    XSync(gfx::GetXDisplay(), x11::False);
  } else {
    // This drag originates from an aura window within our process. This means
    // that we can shortcut the X11 server and ask the owning SelectionOwner
    // for the data it's offering.
    fetched_targets_ = source_client_->GetFormatMap();
  }

  ReadActions();
}

DesktopDragDropClientAuraX11::X11DragContext::~X11DragContext() {
  if (!source_client_) {
    // Unsubscribe from message events.
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  }
}

void DesktopDragDropClientAuraX11::X11DragContext::OnXdndPositionMessage(
    DesktopDragDropClientAuraX11* client,
    ::Atom suggested_action,
    ::Window source_window,
    ::Time time_stamp,
    const gfx::Point& screen_point) {
  DCHECK_EQ(source_window_, source_window);
  suggested_action_ = suggested_action;

  if (!unfetched_targets_.empty()) {
    // We have unfetched targets. That means we need to pause the handling of
    // the position message and ask the other window for its data.
    screen_point_ = screen_point;
    drag_drop_client_ = client;
    position_time_stamp_ = time_stamp;
    waiting_to_handle_position_ = true;

    fetched_targets_ = ui::SelectionFormatMap();
    RequestNextTarget();
  } else {
    client->CompleteXdndPosition(source_window, screen_point);
  }
}

void DesktopDragDropClientAuraX11::X11DragContext::RequestNextTarget() {
  DCHECK(!unfetched_targets_.empty());
  DCHECK(drag_drop_client_);
  DCHECK(waiting_to_handle_position_);

  ::Atom target = unfetched_targets_.back();
  unfetched_targets_.pop_back();

  XConvertSelection(gfx::GetXDisplay(), gfx::GetAtom(kXdndSelection), target,
                    gfx::GetAtom(kChromiumDragReciever), local_window_,
                    position_time_stamp_);
}

void DesktopDragDropClientAuraX11::X11DragContext::OnSelectionNotify(
    const XSelectionEvent& event) {
  if (!waiting_to_handle_position_) {
    // A misbehaved window may send SelectionNotify without us requesting data
    // via XConvertSelection().
    return;
  }
  DCHECK(drag_drop_client_);

  DVLOG(1) << "SelectionNotify, format " << event.target;

  if (event.property != x11::None) {
    DCHECK_EQ(event.property, gfx::GetAtom(kChromiumDragReciever));

    scoped_refptr<base::RefCountedMemory> data;
    ::Atom type = x11::None;
    if (ui::GetRawBytesOfProperty(local_window_, event.property,
                                  &data, NULL, &type)) {
      fetched_targets_.Insert(event.target, data);
    }
  } else {
    // The source failed to convert the drop data to the format (target in X11
    // parlance) that we asked for. This happens, even though we only ask for
    // the formats advertised by the source. http://crbug.com/628099
    LOG(ERROR) << "XConvertSelection failed for source-advertised target "
               << event.target;
  }

  if (!unfetched_targets_.empty()) {
    RequestNextTarget();
  } else {
    waiting_to_handle_position_ = false;
    drag_drop_client_->CompleteXdndPosition(source_window_, screen_point_);
    drag_drop_client_ = NULL;
  }
}

void DesktopDragDropClientAuraX11::X11DragContext::ReadActions() {
  if (!source_client_) {
    std::vector<::Atom> atom_array;
    if (!ui::GetAtomArrayProperty(source_window_, kXdndActionList,
                                  &atom_array)) {
      actions_.clear();
    } else {
      actions_.swap(atom_array);
    }
  } else {
    // We have a property notify set up for other windows in case they change
    // their action list. Thankfully, the views interface is static and you
    // can't change the action list after you enter StartDragAndDrop().
    actions_ = source_client_->GetOfferedDragOperations();
  }
}

int DesktopDragDropClientAuraX11::X11DragContext::GetDragOperation() const {
  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  for (auto it = actions_.begin(); it != actions_.end(); ++it) {
    MaskOperation(*it, &drag_operation);
  }

  MaskOperation(suggested_action_, &drag_operation);

  return drag_operation;
}

void DesktopDragDropClientAuraX11::X11DragContext::MaskOperation(
    ::Atom xdnd_operation,
    int* drag_operation) const {
  if (xdnd_operation == gfx::GetAtom(kXdndActionCopy))
    *drag_operation |= ui::DragDropTypes::DRAG_COPY;
  else if (xdnd_operation == gfx::GetAtom(kXdndActionMove))
    *drag_operation |= ui::DragDropTypes::DRAG_MOVE;
  else if (xdnd_operation == gfx::GetAtom(kXdndActionLink))
    *drag_operation |= ui::DragDropTypes::DRAG_LINK;
}

bool DesktopDragDropClientAuraX11::X11DragContext::CanDispatchEvent(
    const ui::PlatformEvent& event) {
  return event->xany.window == source_window_;
}

uint32_t DesktopDragDropClientAuraX11::X11DragContext::DispatchEvent(
    const ui::PlatformEvent& event) {
  if (event->type == PropertyNotify &&
      event->xproperty.atom == gfx::GetAtom(kXdndActionList)) {
    ReadActions();
    return ui::POST_DISPATCH_STOP_PROPAGATION;
  }
  return ui::POST_DISPATCH_NONE;
}

///////////////////////////////////////////////////////////////////////////////

DesktopDragDropClientAuraX11::DesktopDragDropClientAuraX11(
    aura::Window* root_window,
    views::DesktopNativeCursorManager* cursor_manager,
    ::Display* xdisplay,
    ::Window xwindow)
    : root_window_(root_window),
      cursor_manager_(cursor_manager),
      xdisplay_(xdisplay),
      xwindow_(xwindow),
      current_modifier_state_(ui::EF_NONE),
      target_window_(NULL),
      waiting_on_status_(false),
      status_received_since_enter_(false),
      source_provider_(NULL),
      source_current_window_(x11::None),
      source_state_(SOURCE_STATE_OTHER),
      drag_operation_(0),
      negotiated_operation_(ui::DragDropTypes::DRAG_NONE),
      weak_ptr_factory_(this) {
  // Some tests change the DesktopDragDropClientAuraX11 associated with an
  // |xwindow|.
  g_live_client_map.Get()[xwindow] = this;

  // Mark that we are aware of drag and drop concepts.
  unsigned long xdnd_version = kMaxXdndVersion;
  XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom(kXdndAware), XA_ATOM, 32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&xdnd_version), 1);
}

DesktopDragDropClientAuraX11::~DesktopDragDropClientAuraX11() {
  // This is necessary when the parent native widget gets destroyed while a drag
  // operation is in progress.
  move_loop_->EndMoveLoop();
  NotifyDragLeave();

  g_live_client_map.Get().erase(xwindow_);
}

// static
DesktopDragDropClientAuraX11* DesktopDragDropClientAuraX11::GetForWindow(
    ::Window window) {
  std::map<::Window, DesktopDragDropClientAuraX11*>::const_iterator it =
      g_live_client_map.Get().find(window);
  if (it == g_live_client_map.Get().end())
    return NULL;
  return it->second;
}

void DesktopDragDropClientAuraX11::Init() {
  move_loop_ = CreateMoveLoop(this);
}

void DesktopDragDropClientAuraX11::OnXdndEnter(
    const XClientMessageEvent& event) {
  int version = (event.data.l[1] & 0xff000000) >> 24;
  DVLOG(1) << "OnXdndEnter, version " << version;

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
  target_current_context_.reset();
  target_current_context_.reset(new X11DragContext(xwindow_, event));

  // In the Windows implementation, we immediately call DesktopDropTargetWin::
  // Translate(). The XDND specification demands that we wait until we receive
  // an XdndPosition message before we use XConvertSelection or send an
  // XdndStatus message.
}

void DesktopDragDropClientAuraX11::OnXdndLeave(
    const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndLeave";
  NotifyDragLeave();
  target_current_context_.reset();
}

void DesktopDragDropClientAuraX11::OnXdndPosition(
    const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndPosition";

  unsigned long source_window = event.data.l[0];
  int x_root_window = event.data.l[2] >> 16;
  int y_root_window = event.data.l[2] & 0xffff;
  ::Time time_stamp = event.data.l[3];
  ::Atom suggested_action = event.data.l[4];

  if (!target_current_context_.get()) {
    NOTREACHED();
    return;
  }

  target_current_context_->OnXdndPositionMessage(
      this, suggested_action, source_window, time_stamp,
      gfx::Point(x_root_window, y_root_window));
}

void DesktopDragDropClientAuraX11::OnXdndStatus(
    const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndStatus";

  unsigned long source_window = event.data.l[0];

  if (source_window != source_current_window_)
    return;

  if (source_state_ != SOURCE_STATE_PENDING_DROP &&
      source_state_ != SOURCE_STATE_OTHER) {
    return;
  }

  waiting_on_status_ = false;
  status_received_since_enter_ = true;

  if (event.data.l[1] & 1) {
    ::Atom atom_operation = event.data.l[4];
    negotiated_operation_ = AtomToDragOperation(atom_operation);
  } else {
    negotiated_operation_ = ui::DragDropTypes::DRAG_NONE;
  }

  if (source_state_ == SOURCE_STATE_PENDING_DROP) {
    // We were waiting on the status message so we could send the XdndDrop.
    if (negotiated_operation_ == ui::DragDropTypes::DRAG_NONE) {
      move_loop_->EndMoveLoop();
      return;
    }
    source_state_ = SOURCE_STATE_DROPPED;
    SendXdndDrop(source_window);
    return;
  }

  ui::CursorType cursor_type = ui::CursorType::kNull;
  switch (negotiated_operation_) {
    case ui::DragDropTypes::DRAG_NONE:
      cursor_type = ui::CursorType::kDndNone;
      break;
    case ui::DragDropTypes::DRAG_MOVE:
      cursor_type = ui::CursorType::kDndMove;
      break;
    case ui::DragDropTypes::DRAG_COPY:
      cursor_type = ui::CursorType::kDndCopy;
      break;
    case ui::DragDropTypes::DRAG_LINK:
      cursor_type = ui::CursorType::kDndLink;
      break;
  }
  move_loop_->UpdateCursor(cursor_manager_->GetInitializedCursor(cursor_type));

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

void DesktopDragDropClientAuraX11::OnXdndFinished(
    const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndFinished";
  unsigned long source_window = event.data.l[0];
  if (source_current_window_ != source_window)
    return;

  // Clear |negotiated_operation_| if the drag was rejected.
  if ((event.data.l[1] & 1) == 0)
    negotiated_operation_ = ui::DragDropTypes::DRAG_NONE;

  // Clear |source_current_window_| to avoid sending XdndLeave upon ending the
  // move loop.
  source_current_window_ = x11::None;
  move_loop_->EndMoveLoop();
}

void DesktopDragDropClientAuraX11::OnXdndDrop(
    const XClientMessageEvent& event) {
  DVLOG(1) << "OnXdndDrop";

  unsigned long source_window = event.data.l[0];

  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  if (target_window_) {
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(target_window_);
    if (delegate) {
      ui::OSExchangeData data(
          std::make_unique<ui::OSExchangeDataProviderAuraX11>(
              xwindow_, target_current_context_->fetched_targets()));

      ui::DropTargetEvent event(data,
                                gfx::PointF(target_window_location_),
                                gfx::PointF(target_window_root_location_),
                                target_current_context_->GetDragOperation());
      if (target_current_context_->source_client()) {
        event.set_flags(target_current_context_->source_client()
                            ->current_modifier_state());
      } else {
        event.set_flags(XGetModifiers());
      }

      if (!IsDragDropInProgress()) {
        UMA_HISTOGRAM_COUNTS_1M("Event.DragDrop.ExternalOriginDrop", 1);
      }

      drag_operation = delegate->OnPerformDrop(event);
    }

    target_window_->RemoveObserver(this);
    target_window_ = NULL;
  }

  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(kXdndFinished);
  xev.xclient.format = 32;
  xev.xclient.window = source_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = (drag_operation != 0) ? 1 : 0;
  xev.xclient.data.l[2] = DragOperationToAtom(drag_operation);

  SendXClientEvent(source_window, &xev);
}

void DesktopDragDropClientAuraX11::OnSelectionNotify(
    const XSelectionEvent& xselection) {
  DVLOG(1) << "OnSelectionNotify";
  if (target_current_context_)
    target_current_context_->OnSelectionNotify(xselection);

  // ICCCM requires us to delete the property passed into SelectionNotify.
  if (xselection.property != x11::None)
    XDeleteProperty(xdisplay_, xwindow_, xselection.property);
}

int DesktopDragDropClientAuraX11::StartDragAndDrop(
    const ui::OSExchangeData& data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  UMA_HISTOGRAM_ENUMERATION("Event.DragDrop.Start", source,
                            ui::DragDropTypes::DRAG_EVENT_SOURCE_COUNT);

  source_current_window_ = x11::None;
  DCHECK(!g_current_drag_drop_client);
  g_current_drag_drop_client = this;
  waiting_on_status_ = false;
  next_position_message_.reset();
  status_received_since_enter_ = false;
  source_state_ = SOURCE_STATE_OTHER;
  drag_operation_ = operation;
  negotiated_operation_ = ui::DragDropTypes::DRAG_NONE;

  const ui::OSExchangeData::Provider* provider = &data.provider();
  source_provider_ = static_cast<const ui::OSExchangeDataProviderAuraX11*>(
      provider);

  source_provider_->TakeOwnershipOfSelection();

  std::vector<::Atom> actions = GetOfferedDragOperations();
  if (!source_provider_->file_contents_name().empty()) {
    actions.push_back(gfx::GetAtom(kXdndActionDirectSave));
    ui::SetStringProperty(
        xwindow_, gfx::GetAtom(kXdndDirectSave0),
        gfx::GetAtom(ui::Clipboard::kMimeTypeText),
        source_provider_->file_contents_name().AsUTF8Unsafe());
  }
  ui::SetAtomArrayProperty(xwindow_, kXdndActionList, "ATOM", actions);

  gfx::ImageSkia drag_image = source_provider_->GetDragImage();
  if (IsValidDragImage(drag_image)) {
    CreateDragWidget(drag_image);
    drag_widget_offset_ = source_provider_->GetDragImageOffset();
  }

  // Chrome expects starting drag and drop to release capture.
  aura::Window* capture_window =
      aura::client::GetCaptureClient(root_window)->GetGlobalCaptureWindow();
  if (capture_window)
    capture_window->ReleaseCapture();

  // It is possible for the DesktopWindowTreeHostX11 to be destroyed during the
  // move loop, which would also destroy this drag-client. So keep track of
  // whether it is alive after the drag ends.
  base::WeakPtr<DesktopDragDropClientAuraX11> alive(
      weak_ptr_factory_.GetWeakPtr());

  // Windows has a specific method, DoDragDrop(), which performs the entire
  // drag. We have to emulate this, so we spin off a nested runloop which will
  // track all cursor movement and reroute events to a specific handler.
  move_loop_->RunMoveLoop(source_window, cursor_manager_->GetInitializedCursor(
                                             ui::CursorType::kGrabbing));

  if (alive) {
    if (negotiated_operation_ == ui::DragDropTypes::DRAG_NONE) {
      UMA_HISTOGRAM_ENUMERATION("Event.DragDrop.Cancel", source,
                                ui::DragDropTypes::DRAG_EVENT_SOURCE_COUNT);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Event.DragDrop.Drop", source,
                                ui::DragDropTypes::DRAG_EVENT_SOURCE_COUNT);
    }
    drag_widget_.reset();

    source_provider_ = NULL;
    g_current_drag_drop_client = NULL;
    drag_operation_ = 0;
    XDeleteProperty(xdisplay_, xwindow_, gfx::GetAtom(kXdndActionList));
    XDeleteProperty(xdisplay_, xwindow_, gfx::GetAtom(kXdndDirectSave0));

    return negotiated_operation_;
  }
  UMA_HISTOGRAM_ENUMERATION("Event.DragDrop.Cancel", source,
                            ui::DragDropTypes::DRAG_EVENT_SOURCE_COUNT);
  return ui::DragDropTypes::DRAG_NONE;
}

void DesktopDragDropClientAuraX11::DragCancel() {
  move_loop_->EndMoveLoop();
}

bool DesktopDragDropClientAuraX11::IsDragDropInProgress() {
  return !!g_current_drag_drop_client;
}

void DesktopDragDropClientAuraX11::AddObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED();
}

void DesktopDragDropClientAuraX11::RemoveObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED();
}

void DesktopDragDropClientAuraX11::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(target_window_, window);
  target_window_ = NULL;
}

void DesktopDragDropClientAuraX11::OnMouseMovement(
    const gfx::Point& screen_point,
    int flags,
    base::TimeTicks event_time) {
  if (drag_widget_.get()) {
    float scale_factor =
        ui::GetScaleFactorForNativeView(drag_widget_->GetNativeWindow());
    gfx::Point scaled_point =
        gfx::ScaleToRoundedPoint(screen_point, 1.f / scale_factor);
    drag_widget_->SetBounds(
        gfx::Rect(scaled_point - drag_widget_offset_, drag_image_size_));
    drag_widget_->StackAtTop();
  }

  const int kModifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                         ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                         ui::EF_LEFT_MOUSE_BUTTON |
                         ui::EF_MIDDLE_MOUSE_BUTTON |
                         ui::EF_RIGHT_MOUSE_BUTTON;
  current_modifier_state_ = flags & kModifiers;

  repeat_mouse_move_timer_.Stop();
  ProcessMouseMove(screen_point,
                   (event_time - base::TimeTicks()).InMilliseconds());
}

void DesktopDragDropClientAuraX11::OnMouseReleased() {
  repeat_mouse_move_timer_.Stop();

  if (source_state_ != SOURCE_STATE_OTHER) {
    // The user has previously released the mouse and is clicking in
    // frustration.
    move_loop_->EndMoveLoop();
    return;
  }

  if (source_current_window_ != x11::None) {
    if (waiting_on_status_) {
      if (status_received_since_enter_) {
        // If we are waiting for an XdndStatus message, we need to wait for it
        // to complete.
        source_state_ = SOURCE_STATE_PENDING_DROP;

        // Start timer to end the move loop if the target takes too long to send
        // the XdndStatus and XdndFinished messages.
        StartEndMoveLoopTimer();
        return;
      }

      move_loop_->EndMoveLoop();
      return;
    }

    if (negotiated_operation_ != ui::DragDropTypes::DRAG_NONE) {
      // Start timer to end the move loop if the target takes too long to send
      // an XdndFinished message. It is important that StartEndMoveLoopTimer()
      // is called before SendXdndDrop() because SendXdndDrop()
      // sends XdndFinished synchronously if the drop target is a Chrome
      // window.
      StartEndMoveLoopTimer();

      // We have negotiated an action with the other end.
      source_state_ = SOURCE_STATE_DROPPED;
      SendXdndDrop(source_current_window_);
      return;
    }
  }

  move_loop_->EndMoveLoop();
}

void DesktopDragDropClientAuraX11::OnMoveLoopEnded() {
  if (source_current_window_ != x11::None) {
    SendXdndLeave(source_current_window_);
    source_current_window_ = x11::None;
  }
  target_current_context_.reset();
  repeat_mouse_move_timer_.Stop();
  end_move_loop_timer_.Stop();
}

std::unique_ptr<X11MoveLoop> DesktopDragDropClientAuraX11::CreateMoveLoop(
    X11MoveLoopDelegate* delegate) {
  return base::WrapUnique(new X11WholeScreenMoveLoop(this));
}

XID DesktopDragDropClientAuraX11::FindWindowFor(
    const gfx::Point& screen_point) {
  views::X11TopmostWindowFinder finder;
  ::Window target = finder.FindWindowAt(screen_point);

  if (target == x11::None)
    return x11::None;

  // TODO(crbug/651775): The proxy window should be reported separately from the
  //     target window. XDND messages should be sent to the proxy, and their
  //     window field should point to the target.

  // Figure out which window we should test as XdndAware. If |target| has
  // XdndProxy, it will set that proxy on target, and if not, |target|'s
  // original value will remain.
  ui::GetXIDProperty(target, kXdndProxy, &target);

  int version;
  if (ui::GetIntProperty(target, kXdndAware, &version) &&
      version >= kMaxXdndVersion) {
    return target;
  }
  return x11::None;
}

void DesktopDragDropClientAuraX11::SendXClientEvent(::Window xid,
                                                    XEvent* xev) {
  DCHECK_EQ(ClientMessage, xev->type);

  // Don't send messages to the X11 message queue if we can help it.
  DesktopDragDropClientAuraX11* short_circuit = GetForWindow(xid);
  if (short_circuit) {
    Atom message_type = xev->xclient.message_type;
    if (message_type == gfx::GetAtom(kXdndEnter)) {
      short_circuit->OnXdndEnter(xev->xclient);
      return;
    } else if (message_type == gfx::GetAtom(kXdndLeave)) {
      short_circuit->OnXdndLeave(xev->xclient);
      return;
    } else if (message_type == gfx::GetAtom(kXdndPosition)) {
      short_circuit->OnXdndPosition(xev->xclient);
      return;
    } else if (message_type == gfx::GetAtom(kXdndStatus)) {
      short_circuit->OnXdndStatus(xev->xclient);
      return;
    } else if (message_type == gfx::GetAtom(kXdndFinished)) {
      short_circuit->OnXdndFinished(xev->xclient);
      return;
    } else if (message_type == gfx::GetAtom(kXdndDrop)) {
      short_circuit->OnXdndDrop(xev->xclient);
      return;
    }
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
  XSendEvent(xdisplay_, xid, x11::False, 0, xev);
}

void DesktopDragDropClientAuraX11::ProcessMouseMove(
    const gfx::Point& screen_point,
    unsigned long event_time) {
  if (source_state_ != SOURCE_STATE_OTHER)
    return;

  // Find the current window the cursor is over.
  ::Window dest_window = FindWindowFor(screen_point);

  if (source_current_window_ != dest_window) {
    if (source_current_window_ != x11::None)
      SendXdndLeave(source_current_window_);

    source_current_window_ = dest_window;
    waiting_on_status_ = false;
    next_position_message_.reset();
    status_received_since_enter_ = false;
    negotiated_operation_ = ui::DragDropTypes::DRAG_NONE;

    if (source_current_window_ != x11::None)
      SendXdndEnter(source_current_window_);
  }

  if (source_current_window_ != x11::None) {
    if (waiting_on_status_) {
      next_position_message_.reset(
          new std::pair<gfx::Point, unsigned long>(screen_point, event_time));
    } else {
      SendXdndPosition(dest_window, screen_point, event_time);
    }
  }
}

void DesktopDragDropClientAuraX11::StartEndMoveLoopTimer() {
  end_move_loop_timer_.Start(FROM_HERE,
                             base::TimeDelta::FromMilliseconds(
                                 kEndMoveLoopTimeoutMs),
                             this,
                             &DesktopDragDropClientAuraX11::EndMoveLoop);
}

void DesktopDragDropClientAuraX11::EndMoveLoop() {
  move_loop_->EndMoveLoop();
}

void DesktopDragDropClientAuraX11::DragTranslate(
    const gfx::Point& root_window_location,
    std::unique_ptr<ui::OSExchangeData>* data,
    std::unique_ptr<ui::DropTargetEvent>* event,
    aura::client::DragDropDelegate** delegate) {
  gfx::Point root_location = root_window_location;
  root_window_->GetHost()->ConvertScreenInPixelsToDIP(&root_location);
  aura::Window* target_window =
      root_window_->GetEventHandlerForPoint(root_location);
  bool target_window_changed = false;
  if (target_window != target_window_) {
    if (target_window_)
      NotifyDragLeave();
    target_window_ = target_window;
    if (target_window_)
      target_window_->AddObserver(this);
    target_window_changed = true;
  }
  *delegate = NULL;
  if (!target_window_)
    return;
  *delegate = aura::client::GetDragDropDelegate(target_window_);
  if (!*delegate)
    return;

  data->reset(
      new OSExchangeData(std::make_unique<ui::OSExchangeDataProviderAuraX11>(
          xwindow_, target_current_context_->fetched_targets())));
  gfx::Point location = root_location;
  aura::Window::ConvertPointToTarget(root_window_, target_window_, &location);

  target_window_location_ = location;
  target_window_root_location_ = root_location;

  int drag_op = target_current_context_->GetDragOperation();
  // KDE-based file browsers such as Dolphin change the drag operation depending
  // on whether alt/ctrl/shift was pressed. However once Chromium gets control
  // over the X11 events, the source application does no longer receive X11
  // events for key modifier changes, so the dnd operation gets stuck in an
  // incorrect state. Blink can only dnd-open files of type DRAG_COPY, so the
  // DRAG_COPY mask is added if the dnd object is a file.
  if (drag_op & (ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_LINK) &&
      data->get()->HasFile()) {
    drag_op |= ui::DragDropTypes::DRAG_COPY;
  }

  event->reset(new ui::DropTargetEvent(
      *(data->get()),
      gfx::PointF(location),
      gfx::PointF(root_location),
      drag_op));
  if (target_current_context_->source_client()) {
    (*event)->set_flags(
        target_current_context_->source_client()->current_modifier_state());
  } else {
    (*event)->set_flags(XGetModifiers());
  }
  if (target_window_changed)
    (*delegate)->OnDragEntered(*event->get());
}

void DesktopDragDropClientAuraX11::NotifyDragLeave() {
  if (!target_window_)
    return;
  DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(target_window_);
  if (delegate)
    delegate->OnDragExited();
  target_window_->RemoveObserver(this);
  target_window_ = NULL;
}

::Atom DesktopDragDropClientAuraX11::DragOperationToAtom(
    int drag_operation) {
  if (drag_operation & ui::DragDropTypes::DRAG_COPY)
    return gfx::GetAtom(kXdndActionCopy);
  if (drag_operation & ui::DragDropTypes::DRAG_MOVE)
    return gfx::GetAtom(kXdndActionMove);
  if (drag_operation & ui::DragDropTypes::DRAG_LINK)
    return gfx::GetAtom(kXdndActionLink);

  return x11::None;
}

ui::DragDropTypes::DragOperation
DesktopDragDropClientAuraX11::AtomToDragOperation(::Atom atom) {
  if (atom == gfx::GetAtom(kXdndActionCopy))
    return ui::DragDropTypes::DRAG_COPY;
  if (atom == gfx::GetAtom(kXdndActionMove))
    return ui::DragDropTypes::DRAG_MOVE;
  if (atom == gfx::GetAtom(kXdndActionLink))
    return ui::DragDropTypes::DRAG_LINK;

  return ui::DragDropTypes::DRAG_NONE;
}

std::vector<::Atom> DesktopDragDropClientAuraX11::GetOfferedDragOperations() {
  std::vector<::Atom> operations;
  if (drag_operation_ & ui::DragDropTypes::DRAG_COPY)
    operations.push_back(gfx::GetAtom(kXdndActionCopy));
  if (drag_operation_ & ui::DragDropTypes::DRAG_MOVE)
    operations.push_back(gfx::GetAtom(kXdndActionMove));
  if (drag_operation_ & ui::DragDropTypes::DRAG_LINK)
    operations.push_back(gfx::GetAtom(kXdndActionLink));
  return operations;
}

ui::SelectionFormatMap DesktopDragDropClientAuraX11::GetFormatMap() const {
  return source_provider_ ? source_provider_->GetFormatMap() :
      ui::SelectionFormatMap();
}

void DesktopDragDropClientAuraX11::CompleteXdndPosition(
    ::Window source_window,
    const gfx::Point& screen_point) {
  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  std::unique_ptr<ui::OSExchangeData> data;
  std::unique_ptr<ui::DropTargetEvent> drop_target_event;
  DragDropDelegate* delegate = NULL;
  DragTranslate(screen_point, &data, &drop_target_event, &delegate);
  if (delegate)
    drag_operation = delegate->OnDragUpdated(*drop_target_event);

  // Sends an XdndStatus message back to the source_window. l[2,3]
  // theoretically represent an area in the window where the current action is
  // the same as what we're returning, but I can't find any implementation that
  // actually making use of this. A client can return (0, 0) and/or set the
  // first bit of l[1] to disable the feature, and it appears that gtk neither
  // sets this nor respects it if set.
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(kXdndStatus);
  xev.xclient.format = 32;
  xev.xclient.window = source_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = (drag_operation != 0) ?
      (kWantFurtherPosEvents | kWillAcceptDrop) : 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = DragOperationToAtom(drag_operation);

  SendXClientEvent(source_window, &xev);
}

void DesktopDragDropClientAuraX11::SendXdndEnter(::Window dest_window) {
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(kXdndEnter);
  xev.xclient.format = 32;
  xev.xclient.window = dest_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = (kMaxXdndVersion << 24);  // The version number.
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  std::vector<Atom> targets;
  source_provider_->RetrieveTargets(&targets);

  if (targets.size() > 3) {
    xev.xclient.data.l[1] |= 1;
    ui::SetAtomArrayProperty(xwindow_, kXdndTypeList, "ATOM", targets);
  } else {
    // Pack the targets into the enter message.
    for (size_t i = 0; i < targets.size(); ++i)
      xev.xclient.data.l[2 + i] = targets[i];
  }

  SendXClientEvent(dest_window, &xev);
}

void DesktopDragDropClientAuraX11::SendXdndLeave(::Window dest_window) {
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(kXdndLeave);
  xev.xclient.format = 32;
  xev.xclient.window = dest_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;
  SendXClientEvent(dest_window, &xev);
}

void DesktopDragDropClientAuraX11::SendXdndPosition(
    ::Window dest_window,
    const gfx::Point& screen_point,
    unsigned long event_time) {
  waiting_on_status_ = true;

  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(kXdndPosition);
  xev.xclient.format = 32;
  xev.xclient.window = dest_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = (screen_point.x() << 16) | screen_point.y();
  xev.xclient.data.l[3] = event_time;
  xev.xclient.data.l[4] = DragOperationToAtom(drag_operation_);
  SendXClientEvent(dest_window, &xev);

  // http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html and
  // the Xdnd protocol both recommend that drag events should be sent
  // periodically.
  repeat_mouse_move_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kRepeatMouseMoveTimeoutMs),
      base::Bind(&DesktopDragDropClientAuraX11::ProcessMouseMove,
                 base::Unretained(this),
                 screen_point,
                 event_time));
}

void DesktopDragDropClientAuraX11::SendXdndDrop(::Window dest_window) {
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = gfx::GetAtom(kXdndDrop);
  xev.xclient.format = 32;
  xev.xclient.window = dest_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = x11::CurrentTime;
  xev.xclient.data.l[3] = x11::None;
  xev.xclient.data.l[4] = x11::None;
  SendXClientEvent(dest_window, &xev);
}

void DesktopDragDropClientAuraX11::CreateDragWidget(
    const gfx::ImageSkia& image) {
  Widget* widget = new Widget;
  Widget::InitParams params(Widget::InitParams::TYPE_DRAG);
  if (ui::IsCompositingManagerPresent())
    params.opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
  else
    params.opacity = Widget::InitParams::OPAQUE_WINDOW;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.accept_events = false;

  gfx::Point location = display::Screen::GetScreen()->GetCursorScreenPoint() -
                        drag_widget_offset_;
  params.bounds = gfx::Rect(location, image.size());
  widget->set_focus_on_creation(false);
  widget->set_frame_type(Widget::FRAME_TYPE_FORCE_NATIVE);
  widget->Init(params);
  if (params.opacity == Widget::InitParams::TRANSLUCENT_WINDOW)
    widget->SetOpacity(kDragWidgetOpacity);
  widget->GetNativeWindow()->SetName("DragWindow");

  drag_image_size_ = image.size();
  ImageView* image_view = new ImageView();
  image_view->SetImage(image);
  image_view->SetBoundsRect(gfx::Rect(drag_image_size_));
  widget->SetContentsView(image_view);
  widget->Show();
  widget->GetNativeWindow()->layer()->SetFillsBoundsOpaquely(false);

  drag_widget_.reset(widget);
}

bool DesktopDragDropClientAuraX11::IsValidDragImage(
    const gfx::ImageSkia& image) {
  if (image.isNull())
    return false;

  // Because we need a GL context per window, we do a quick check so that we
  // don't make another context if the window would just be displaying a mostly
  // transparent image.
  const SkBitmap* in_bitmap = image.bitmap();
  for (int y = 0; y < in_bitmap->height(); ++y) {
    uint32_t* in_row = in_bitmap->getAddr32(0, y);

    for (int x = 0; x < in_bitmap->width(); ++x) {
      if (SkColorGetA(in_row[x]) > kMinAlpha)
        return true;
    }
  }

  return false;
}

}  // namespace views
