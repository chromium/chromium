// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_drag_context.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/x/x11_drag_drop_client.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_util_linux.h"
#include "ui/base/x/selection_utils.h"
#endif

namespace ui {

namespace {

// Window property that holds the supported drag and drop data types.
// This property is set on the XDND source window when the drag and drop data
// can be converted to more than 3 types.
const char kXdndTypeList[] = "XdndTypeList";

// Selection used by the XDND protocol to transfer data between applications.
const char kXdndSelection[] = "XdndSelection";

// Window property that contains the possible actions that will be presented to
// the user when the drag and drop action is kXdndActionAsk.
const char kXdndActionList[] = "XdndActionList";

// These actions have the same meaning as in the W3C Drag and Drop spec.
const char kXdndActionCopy[] = "XdndActionCopy";
const char kXdndActionMove[] = "XdndActionMove";
const char kXdndActionLink[] = "XdndActionLink";

// Window property that will receive the drag and drop selection data.
const char kChromiumDragReciever[] = "_CHROMIUM_DRAG_RECEIVER";

}  // namespace

XDragContext::XDragContext(x11::Window local_window,
                           const x11::ClientMessageEvent& event,
                           const SelectionFormatMap& data)
    : local_window_(local_window),
      source_window_(static_cast<x11::Window>(event.data.data32[0])) {
  XDragDropClient* source_client =
      XDragDropClient::GetForWindow(source_window_);
  if (!source_client) {
    auto* connection = x11::Connection::Get();
    bool get_types_from_property = ((event.data.data32[1] & 1) != 0);

    if (get_types_from_property) {
      if (!connection->GetArrayProperty(source_window_,
                                        connection->GetAtom(kXdndTypeList),
                                        &unfetched_targets_)) {
        return;
      }
    } else {
      // data.l[2,3,4] contain the first three types. Unused slots can be None.
      for (size_t i = 2; i < 5; ++i) {
        if (event.data.data32[i]) {
          unfetched_targets_.push_back(
              static_cast<x11::Atom>(event.data.data32[i]));
        }
      }
    }

#if DCHECK_IS_ON()
    DVLOG(1) << "XdndEnter has " << unfetched_targets_.size() << " data types";
    for (x11::Atom target : unfetched_targets_) {
      DVLOG(1) << "XdndEnter data type: " << static_cast<uint32_t>(target);
    }
#endif  // DCHECK_IS_ON()

    // We must perform a full sync here because we could be racing
    // |source_window_|.
    connection->Sync();
  } else {
    // This drag originates from an aura window within our process. This means
    // that we can shortcut the X11 server and ask the owning SelectionOwner
    // for the data it's offering.
    fetched_targets_ = data;
  }

  ReadActions();
}

XDragContext::~XDragContext() = default;

void XDragContext::OnXdndPositionMessage(XDragDropClient* client,
                                         x11::Atom suggested_action,
                                         x11::Window source_window,
                                         x11::Time time_stamp,
                                         const gfx::Point& screen_point) {
  CHECK_EQ(source_window_, source_window, base::NotFatalUntil::M163);
  suggested_action_ = suggested_action;

  if (!unfetched_targets_.empty()) {
    // We have unfetched targets. That means we need to pause the handling of
    // the position message and ask the other window for its data.
    screen_point_ = screen_point;
    drag_drop_client_ = client;
    position_time_stamp_ = time_stamp;
    waiting_to_handle_position_ = true;

    fetched_targets_ = SelectionFormatMap();
    RequestNextTarget();
  } else {
    client->CompleteXdndPosition(source_window, screen_point);
  }
}

void XDragContext::RequestNextTarget() {
  CHECK(!unfetched_targets_.empty(), base::NotFatalUntil::M163);
  CHECK(drag_drop_client_, base::NotFatalUntil::M163);
  CHECK(waiting_to_handle_position_, base::NotFatalUntil::M163);

  x11::Atom target = unfetched_targets_.back();
  unfetched_targets_.pop_back();

  auto* connection = x11::Connection::Get();
  connection->ConvertSelection(
      {local_window_, connection->GetAtom(kXdndSelection), target,
       connection->GetAtom(kChromiumDragReciever), position_time_stamp_});
}

void XDragContext::OnSelectionNotify(const x11::SelectionNotifyEvent& event) {
  if (!waiting_to_handle_position_) {
    // A misbehaved window may send SelectionNotify without us requesting data
    // via XConvertSelection().
    return;
  }
  CHECK(drag_drop_client_, base::NotFatalUntil::M163);

  DVLOG(1) << "SelectionNotify, format " << static_cast<uint32_t>(event.target);

  auto property = static_cast<x11::Atom>(event.property);
  auto target = static_cast<x11::Atom>(event.target);

  if (event.property != x11::Atom::None) {
    auto* connection = x11::Connection::Get();
    CHECK_EQ(property, connection->GetAtom(kChromiumDragReciever),
             base::NotFatalUntil::M163);

    scoped_refptr<base::RefCountedMemory> data;
    x11::Atom type = x11::Atom::None;
    if (GetRawBytesOfProperty(local_window_, property, &data, &type)) {
#if BUILDFLAG(IS_LINUX)
      // If the source provided a portal key, retrieve the files now.
      if (target == connection->GetAtom(kMimeTypePortalFileTransfer) ||
          target == connection->GetAtom(kMimeTypePortalFiles)) {
        if (fetched_targets_.contains(connection->GetAtom(kMimeTypeUriList))) {
          RequestNextTargetOrComplete();
          return;
        }
        ui::clipboard_util::ExtractPathsFromPortalKey(
            base::as_byte_span(*data),
            base::BindOnce(&XDragContext::OnPortalPathsExtracted,
                           weak_factory_.GetWeakPtr()));
        return;
      }
      if (target == connection->GetAtom(kMimeTypeUriList) &&
          fetched_targets_.contains(target)) {
        RequestNextTargetOrComplete();
        return;
      }
#endif  // BUILDFLAG(IS_LINUX)
      fetched_targets_.Insert(target, data);
    }
  } else {
    // The source failed to convert the drop data to the format (target in X11
    // parlance) that we asked for. This happens, even though we only ask for
    // the formats advertised by the source. http://crbug.com/628099
    LOG(ERROR) << "XConvertSelection failed for source-advertised target "
               << static_cast<uint32_t>(event.target);
  }

  RequestNextTargetOrComplete();
}

void XDragContext::RequestNextTargetOrComplete() {
  if (!unfetched_targets_.empty()) {
    RequestNextTarget();
  } else {
    waiting_to_handle_position_ = false;
    XDragDropClient* client = drag_drop_client_;
    drag_drop_client_ = nullptr;
    if (client) {
      client->CompleteXdndPosition(source_window_, screen_point_);
    }
  }
}

#if BUILDFLAG(IS_LINUX)
void XDragContext::OnPortalPathsExtracted(std::vector<std::string> paths) {
  if (!paths.empty()) {
    auto data = base::MakeRefCounted<base::RefCountedString>(
        ui::clipboard_util::GetUriListFromPaths(paths));
    // Store as text/uri-list so the rest of Chrome understands it.
    fetched_targets_.Insert(x11::Connection::Get()->GetAtom(kMimeTypeUriList),
                            data);
  }
  RequestNextTargetOrComplete();
}
#endif

void XDragContext::ReadActions() {
  XDragDropClient* source_client =
      XDragDropClient::GetForWindow(source_window_);
  if (!source_client) {
    std::vector<x11::Atom> atom_array;
    auto* connection = x11::Connection::Get();
    if (!connection->GetArrayProperty(source_window_,
                                      connection->GetAtom(kXdndActionList),
                                      &atom_array)) {
      actions_.clear();
    } else {
      actions_.swap(atom_array);
    }
  } else {
    // We have a property notify set up for other windows in case they change
    // their action list. Thankfully, the views interface is static and you
    // can't change the action list after you enter StartDragAndDrop().
    actions_ = source_client->GetOfferedDragOperations();
  }
}

int XDragContext::GetDragOperation() const {
  int drag_operation = DragDropTypes::DRAG_NONE;
  for (const auto& action : actions_) {
    MaskOperation(action, &drag_operation);
  }

  MaskOperation(suggested_action_, &drag_operation);

  return drag_operation;
}

void XDragContext::MaskOperation(x11::Atom xdnd_operation,
                                 int* drag_operation) const {
  auto* connection = x11::Connection::Get();
  if (xdnd_operation == connection->GetAtom(kXdndActionCopy)) {
    *drag_operation |= DragDropTypes::DRAG_COPY;
  } else if (xdnd_operation == connection->GetAtom(kXdndActionMove)) {
    *drag_operation |= DragDropTypes::DRAG_MOVE;
  } else if (xdnd_operation == connection->GetAtom(kXdndActionLink)) {
    *drag_operation |= DragDropTypes::DRAG_LINK;
  }
}

bool XDragContext::DispatchPropertyNotifyEvent(
    const x11::PropertyNotifyEvent& prop) {
  if (prop.atom == x11::Connection::Get()->GetAtom(kXdndActionList)) {
    ReadActions();
    return true;
  }
  return false;
}

}  // namespace ui
