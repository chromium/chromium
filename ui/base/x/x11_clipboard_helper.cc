// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_clipboard_helper.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "base/compiler_specific.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/selection_requestor.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xfixes.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

const char kClipboard[] = "CLIPBOARD";

// Uses the XFixes API to notify about selection changes.
class SelectionChangeObserver : public x11::EventObserver {
 public:
  using SelectionChangeCallback = XClipboardHelper::SelectionChangeCallback;

  SelectionChangeObserver(const SelectionChangeObserver&) = delete;
  SelectionChangeObserver& operator=(const SelectionChangeObserver&) = delete;

  static SelectionChangeObserver* Get();

  void set_callback(SelectionChangeCallback callback) {
    callback_ = std::move(callback);
  }

 private:
  friend struct base::DefaultSingletonTraits<SelectionChangeObserver>;

  SelectionChangeObserver();
  ~SelectionChangeObserver() override = default;

  // x11::EventObserver:
  void OnEvent(const x11::Event& xev) override;

  const x11::Atom clipboard_atom_{};
  SelectionChangeCallback callback_;
};

SelectionChangeObserver::SelectionChangeObserver()
    : clipboard_atom_(x11::GetAtom(kClipboard)) {
  auto* connection = x11::Connection::Get();
  auto& xfixes = connection->xfixes();
  if (!xfixes.present()) {
    return;
  }

  auto mask = x11::XFixes::SelectionEventMask::SetSelectionOwner |
              x11::XFixes::SelectionEventMask::SelectionWindowDestroy |
              x11::XFixes::SelectionEventMask::SelectionClientClose;
  xfixes.SelectSelectionInput({GetX11RootWindow(), clipboard_atom_, mask});
  // This seems to be semi-optional. For some reason, registering for any
  // selection notify events seems to subscribe us to events for both the
  // primary and the clipboard buffers. Register anyway just to be safe.
  xfixes.SelectSelectionInput({GetX11RootWindow(), x11::Atom::PRIMARY, mask});

  connection->AddEventObserver(this);
}

SelectionChangeObserver* SelectionChangeObserver::Get() {
  return base::Singleton<SelectionChangeObserver>::get();
}

void SelectionChangeObserver::OnEvent(const x11::Event& xev) {
  if (auto* ev = xev.As<x11::XFixes::SelectionNotifyEvent>()) {
    if ((ev->selection == x11::Atom::PRIMARY ||
         ev->selection == clipboard_atom_) &&
        callback_) {
      callback_.Run(ev->selection == x11::Atom::PRIMARY
                        ? ClipboardBuffer::kSelection
                        : ClipboardBuffer::kCopyPaste);
    }
  }
}

x11::Window GetSelectionOwner(x11::Atom selection) {
  auto response = x11::Connection::Get()->GetSelectionOwner({selection}).Sync();
  return response ? response->owner : x11::Window::None;
}

}  // namespace

class XClipboardHelper::TargetList {
 public:
  explicit TargetList(const std::vector<x11::Atom>& target_list)
      : target_list_(target_list) {}
  TargetList(const TargetList&) = default;
  TargetList& operator=(const TargetList&) = default;
  ~TargetList() = default;

  const std::vector<x11::Atom>& target_list() const { return target_list_; }

  bool ContainsText() const {
    for (const auto& atom : GetTextAtomsFrom()) {
      if (std::ranges::contains(target_list_, atom)) {
        return true;
      }
    }
    return false;
  }

  bool ContainsFormat(const ClipboardFormatType& format_type) const {
    x11::Atom atom = x11::GetAtom(format_type.GetName().c_str());
    return std::ranges::contains(target_list_, atom);
  }

 private:
  std::vector<x11::Atom> target_list_;
};

XClipboardHelper::XClipboardHelper(
    SelectionChangeCallback selection_change_callback)
    : connection_(*x11::Connection::Get()),
      x_root_window_(ui::GetX11RootWindow()),
      x_window_(connection_->CreateDummyWindow("Chromium Clipboard Window")),
      selection_requestor_(
          std::make_unique<SelectionRequestor>(x_window_, this)),
      clipboard_owner_(connection_.get(), x_window_, x11::GetAtom(kClipboard)),
      primary_owner_(connection_.get(), x_window_, x11::Atom::PRIMARY) {
  DCHECK(selection_requestor_);

  connection_->SetStringProperty(x_window_, x11::Atom::WM_NAME,
                                 x11::Atom::STRING, "Chromium clipboard");
  x_window_events_ =
      connection_->ScopedSelectEvent(x_window_, x11::EventMask::PropertyChange);
  connection_->AddEventObserver(this);

  SelectionChangeObserver::Get()->set_callback(
      std::move(selection_change_callback));
}

XClipboardHelper::~XClipboardHelper() {
  connection_->RemoveEventObserver(this);
  connection_->DestroyWindow({x_window_});
  SelectionChangeObserver::Get()->set_callback(SelectionChangeCallback());
}

void XClipboardHelper::CreateNewClipboardData() {
  clipboard_data_ = SelectionFormatMap();
}

void XClipboardHelper::InsertMapping(
    const std::string& key,
    const scoped_refptr<base::RefCountedMemory>& memory) {
  x11::Atom atom_key = x11::GetAtom(key.c_str());
  clipboard_data_.Insert(atom_key, memory);
}

void XClipboardHelper::TakeOwnershipOfSelection(ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste) {
    return clipboard_owner_.TakeOwnershipOfSelection(clipboard_data_);
  } else {
    return primary_owner_.TakeOwnershipOfSelection(clipboard_data_);
  }
}

void XClipboardHelper::ReadAsync(
    ClipboardBuffer buffer,
    const std::vector<x11::Atom>& types,
    base::OnceCallback<void(SelectionData)> callback) {
  x11::Atom selection_name = LookupSelectionForClipboardBuffer(buffer);
  if (GetSelectionOwner(selection_name) == x_window_) {
    // We can local fastpath instead of playing the nested run loop game
    // with the X server.
    const SelectionFormatMap& format_map = LookupStorageForAtom(selection_name);

    for (const auto& type : types) {
      auto format_map_it = format_map.find(type);
      if (format_map_it != format_map.end()) {
        std::move(callback).Run(
            SelectionData(format_map_it->first, format_map_it->second));
        return;
      }
    }
    std::move(callback).Run(SelectionData());
    return;
  }

  GetTargetListAsync(
      buffer,
      base::BindOnce(&XClipboardHelper::OnReadAsyncTargetList, GetWeakPtr(),
                     selection_name, types, std::move(callback)));
}

void XClipboardHelper::OnReadAsyncTargetList(
    x11::Atom selection_name,
    const std::vector<x11::Atom>& types,
    base::OnceCallback<void(SelectionData)> callback,
    TargetList targets) {
  std::vector<x11::Atom> intersection;
  GetAtomIntersection(types, targets.target_list(), &intersection);
  selection_requestor_->RequestTypesAsync(selection_name, intersection,
                                          std::move(callback));
}

void XClipboardHelper::GetAvailableMimeTypesAsync(
    ClipboardBuffer buffer,
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  GetTargetListAsync(
      buffer,
      base::BindOnce(&XClipboardHelper::OnGetAvailableMimeTypesTargetList,
                     GetWeakPtr(), std::move(callback)));
}

void XClipboardHelper::OnGetAvailableMimeTypesTargetList(
    base::OnceCallback<void(const std::vector<std::string>&)> final_callback,
    TargetList target_list) {
  std::vector<std::string> available_types;
  if (target_list.ContainsText()) {
    available_types.push_back(kMimeTypePlainText);
  }
  if (target_list.ContainsFormat(ClipboardFormatType::HtmlType())) {
    available_types.push_back(kMimeTypeHtml);
  }
  if (target_list.ContainsFormat(ClipboardFormatType::SvgType())) {
    available_types.push_back(kMimeTypeSvg);
  }
  if (target_list.ContainsFormat(ClipboardFormatType::RtfType())) {
    available_types.push_back(kMimeTypeRtf);
  }
  if (target_list.ContainsFormat(ClipboardFormatType::PngType())) {
    available_types.push_back(kMimeTypePng);
  }
  if (target_list.ContainsFormat(ClipboardFormatType::FilenamesType())) {
    available_types.push_back(kMimeTypeUriList);
  }
  if (target_list.ContainsFormat(
          ClipboardFormatType::DataTransferCustomType())) {
    available_types.push_back(kMimeTypeDataTransferCustomData);
  }

  const auto& targets = target_list.target_list();
  if (targets.empty()) {
    std::move(final_callback).Run(available_types);
    return;
  }

  auto barrier = base::BarrierCallback<std::string>(
      targets.size(),
      base::BindOnce(
          [](std::vector<std::string> common_types,
             base::OnceCallback<void(const std::vector<std::string>&)>
                 final_callback,
             std::vector<std::string> atom_names) {
            for (const auto& name : atom_names) {
              if (!name.empty()) {
                common_types.push_back(name);
              }
            }
            std::ranges::sort(common_types);
            auto [first, last] = std::ranges::unique(common_types);
            common_types.erase(first, last);
            std::move(final_callback).Run(common_types);
          },
          std::move(available_types), std::move(final_callback)));

  for (x11::Atom target : targets) {
    x11::Connection::Get()->GetAtomName({target}).OnResponse(base::BindOnce(
        [](base::OnceCallback<void(std::string)> callback,
           x11::GetAtomNameResponse response) {
          std::move(callback).Run(response ? std::string(response->name)
                                           : std::string());
        },
        barrier));
  }
}

void XClipboardHelper::IsFormatAvailableAsync(
    ClipboardBuffer buffer,
    const ClipboardFormatType& format,
    base::OnceCallback<void(bool)> callback) {
  GetTargetListAsync(
      buffer,
      base::BindOnce(
          [](const ClipboardFormatType& format,
             base::OnceCallback<void(bool)> callback, TargetList target_list) {
            if (format == ClipboardFormatType::PlainTextType() ||
                format == ClipboardFormatType::UrlType()) {
              std::move(callback).Run(target_list.ContainsText());
            } else {
              std::move(callback).Run(target_list.ContainsFormat(format));
            }
          },
          format, std::move(callback)));
}

bool XClipboardHelper::IsSelectionOwner(ClipboardBuffer buffer) const {
  x11::Atom selection = LookupSelectionForClipboardBuffer(buffer);
  return GetSelectionOwner(selection) == x_window_;
}

std::vector<x11::Atom> XClipboardHelper::GetTextAtoms() const {
  return GetTextAtomsFrom();
}

std::vector<x11::Atom> XClipboardHelper::GetAtomsForFormat(
    const ClipboardFormatType& format) {
  return {x11::GetAtom(format.GetName().c_str())};
}

void XClipboardHelper::Clear(ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste) {
    clipboard_owner_.ClearSelectionOwner();
  } else {
    primary_owner_.ClearSelectionOwner();
  }
}

void XClipboardHelper::GetTargetListAsync(
    ClipboardBuffer buffer,
    base::OnceCallback<void(TargetList)> callback) {
  x11::Atom selection_name = LookupSelectionForClipboardBuffer(buffer);
  if (GetSelectionOwner(selection_name) == x_window_) {
    // We can local fastpath and return the list of local targets.
    const SelectionFormatMap& format_map = LookupStorageForAtom(selection_name);
    std::vector<x11::Atom> out;
    out.reserve(format_map.size());
    for (const auto& format : format_map) {
      out.push_back(format.first);
    }
    std::move(callback).Run(XClipboardHelper::TargetList(out));
    return;
  }

  selection_requestor_->PerformConvertSelectionAsync(
      selection_name, x11::GetAtom(kTargets),
      base::BindOnce(&XClipboardHelper::OnGetTargetListResponse, GetWeakPtr(),
                     selection_name, std::move(callback)));
}

void XClipboardHelper::OnGetTargetListResponse(
    x11::Atom selection_name,
    base::OnceCallback<void(TargetList)> final_callback,
    bool success,
    std::vector<uint8_t> data,
    x11::Atom out_type) {
  if (success) {
    // Some apps return an |out_type| of "TARGETS". (crbug.com/377893)
    if (out_type == x11::Atom::ATOM || out_type == x11::GetAtom(kTargets)) {
      // SAFETY: `data` is populated by the X11 server and is expected
      // to be a series of atoms since we already checked `out_type`
      // is an ATOM or GetAtom(kTargets).
      CHECK_EQ(data.size() % sizeof(x11::Atom), 0u);
      base::span<const x11::Atom> atom_array = UNSAFE_BUFFERS(
          base::span(reinterpret_cast<const x11::Atom*>(data.data()),
                     data.size() / sizeof(x11::Atom)));
      std::move(final_callback).Run(TargetList(base::ToVector(atom_array)));
      return;
    }
  }

  // There was no target list. Most Java apps doesn't offer a TARGETS
  // list, even though they AWT to. They will offer individual text
  // types if you ask. If this is the case we attempt to make sense of
  // the contents as text. This is pretty unfortunate since it means
  // we have to actually copy the data to see if it is available, but
  // at least this path shouldn't be hit for conforming programs.
  std::vector<x11::Atom> types = GetTextAtoms();
  auto barrier = base::BarrierCallback<x11::Atom>(
      types.size(), base::BindOnce(
                        [](base::OnceCallback<void(TargetList)> final_callback,
                           std::vector<x11::Atom> atoms) {
                          std::erase(atoms, x11::Atom::None);
                          std::move(final_callback).Run(TargetList(atoms));
                        },
                        std::move(final_callback)));

  for (const auto& text_atom : types) {
    selection_requestor_->PerformConvertSelectionAsync(
        selection_name, text_atom,
        base::BindOnce(
            [](x11::Atom text_atom,
               base::OnceCallback<void(x11::Atom)> callback, bool success,
               std::vector<uint8_t> data, x11::Atom type) {
              std::move(callback).Run(
                  (success && type == text_atom) ? text_atom : x11::Atom::None);
            },
            text_atom, barrier));
  }
}

bool XClipboardHelper::DispatchEvent(const x11::Event& xev) {
  if (auto* request = xev.As<x11::SelectionRequestEvent>()) {
    if (request->owner != x_window_) {
      return false;
    }
    if (request->selection == x11::Atom::PRIMARY) {
      primary_owner_.OnSelectionRequest(*request);
    } else {
      // We should not get requests for the CLIPBOARD_MANAGER selection
      // because we never take ownership of it.
      DCHECK_EQ(GetCopyPasteSelection(), request->selection);
      clipboard_owner_.OnSelectionRequest(*request);
    }
  } else if (auto* notify = xev.As<x11::SelectionNotifyEvent>()) {
    if (notify->requestor == x_window_) {
      selection_requestor_->OnSelectionNotify(*notify);
    } else {
      return false;
    }
  } else if (auto* clear = xev.As<x11::SelectionClearEvent>()) {
    if (clear->owner != x_window_) {
      return false;
    }
    if (clear->selection == x11::Atom::PRIMARY) {
      primary_owner_.OnSelectionClear(*clear);
    } else {
      // We should not get requests for the CLIPBOARD_MANAGER selection
      // because we never take ownership of it.
      DCHECK_EQ(GetCopyPasteSelection(), clear->selection);
      clipboard_owner_.OnSelectionClear(*clear);
    }
  } else if (auto* prop = xev.As<x11::PropertyNotifyEvent>()) {
    if (primary_owner_.CanDispatchPropertyEvent(*prop)) {
      primary_owner_.OnPropertyEvent(*prop);
    } else if (clipboard_owner_.CanDispatchPropertyEvent(*prop)) {
      clipboard_owner_.OnPropertyEvent(*prop);
    } else if (selection_requestor_->CanDispatchPropertyEvent(*prop)) {
      selection_requestor_->OnPropertyEvent(*prop);
    } else {
      return false;
    }
  } else {
    return false;
  }
  return true;
}

SelectionRequestor* XClipboardHelper::GetSelectionRequestorForTest() {
  return selection_requestor_.get();
}

base::WeakPtr<XClipboardHelper> XClipboardHelper::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void XClipboardHelper::OnEvent(const x11::Event& xev) {
  DispatchEvent(xev);
}

x11::Atom XClipboardHelper::LookupSelectionForClipboardBuffer(
    ClipboardBuffer buffer) const {
  if (buffer == ClipboardBuffer::kCopyPaste) {
    return GetCopyPasteSelection();
  }

  return x11::Atom::PRIMARY;
}

x11::Atom XClipboardHelper::GetCopyPasteSelection() const {
  return x11::GetAtom(kClipboard);
}

const SelectionFormatMap& XClipboardHelper::LookupStorageForAtom(
    x11::Atom atom) {
  if (atom == x11::Atom::PRIMARY) {
    return primary_owner_.selection_format_map();
  }

  DCHECK_EQ(GetCopyPasteSelection(), atom);
  return clipboard_owner_.selection_format_map();
}

}  //  namespace ui
