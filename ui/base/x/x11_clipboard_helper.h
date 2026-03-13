// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_CLIPBOARD_HELPER_H_
#define UI_BASE_X_X11_CLIPBOARD_HELPER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/selection_utils.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

class SelectionRequester;

// Helper class that provides core X11 clipboard integration code. Shared by
// both legacy and Ozone X11 backends.
//
// TODO(crbug.com/40551833): Merge into X11ClipboardOzone class once ozone
// migration is complete and legacy backend gets removed.
class COMPONENT_EXPORT(UI_BASE_X) XClipboardHelper : public x11::EventObserver {
 public:
  using SelectionChangeCallback =
      base::RepeatingCallback<void(ClipboardBuffer)>;

  explicit XClipboardHelper(SelectionChangeCallback selection_change_callback);
  XClipboardHelper(const XClipboardHelper&) = delete;
  XClipboardHelper& operator=(const XClipboardHelper&) = delete;
  ~XClipboardHelper() override;

  // As we need to collect all the data types before we tell X11 that we own a
  // particular selection, we create a temporary clipboard mapping that
  // InsertMapping writes to. Then we commit it in TakeOwnershipOfSelection,
  // where we save it in one of the clipboard data slots.
  void CreateNewClipboardData();

  // Inserts a mapping into clipboard_data_.
  void InsertMapping(const std::string& key,
                     const scoped_refptr<base::RefCountedMemory>& memory);

  // Moves the temporary |clipboard_data_| to the long term data storage for
  // |buffer|.
  void TakeOwnershipOfSelection(ClipboardBuffer buffer);

  // Runs `callback` with the first of `types` offered by the current
  // selection holder, or an empty SelectionData if none of those types
  // are available.
  void ReadAsync(ClipboardBuffer buffer,
                 const std::vector<x11::Atom>& types,
                 base::OnceCallback<void(SelectionData)> callback);

  // Runs `callback` with the list of possible data types the current clipboard
  // owner has, for a given `buffer`.
  void GetAvailableMimeTypesAsync(
      ClipboardBuffer buffer,
      base::OnceCallback<void(const std::vector<std::string>&)> callback);

  // Runs `callback` with true if the current clipboard owner for `buffer`
  // offers `format` and false otherwise.
  void IsFormatAvailableAsync(ClipboardBuffer buffer,
                              const ClipboardFormatType& format,
                              base::OnceCallback<void(bool)> callback);

  // Runs `callback` with whether we currently own the selection for a given
  // clipboard `buffer`.
  void IsSelectionOwnerAsync(ClipboardBuffer buffer,
                             base::OnceCallback<void(bool)> callback) const;

  // Returns a list of all text atoms that we handle.
  std::vector<x11::Atom> GetTextAtoms() const;

  // Returns a vector with a |format| converted to an X11 atom.
  std::vector<x11::Atom> GetAtomsForFormat(const ClipboardFormatType& format);

  // Clears a certain clipboard buffer, whether we own it or not.
  void Clear(ClipboardBuffer buffer);

  // Returns true if the event was handled.
  bool DispatchEvent(const x11::Event& xev);

  SelectionRequester* GetSelectionRequesterForTest();

  base::WeakPtr<XClipboardHelper> GetWeakPtr();

 private:
  class TargetList;

  // x11::EventObserver:
  void OnEvent(const x11::Event& xev) override;

  void GetTargetListAsync(ClipboardBuffer buffer,
                          base::OnceCallback<void(TargetList)> callback);

  void OnGetTargetList(ClipboardBuffer buffer,
                       base::OnceCallback<void(TargetList)> callback,
                       x11::Window owner);

  void OnReadAsync(ClipboardBuffer buffer,
                   const std::vector<x11::Atom>& types,
                   base::OnceCallback<void(SelectionData)> callback,
                   x11::Window owner);

  void OnReadAsyncTargetList(x11::Atom selection_name,
                             const std::vector<x11::Atom>& types,
                             base::OnceCallback<void(SelectionData)> callback,
                             TargetList targets);

  void OnGetAvailableMimeTypesTargetList(
      base::OnceCallback<void(const std::vector<std::string>&)> final_callback,
      TargetList target_list);

  void OnGetTargetListResponse(
      x11::Atom selection_name,
      base::OnceCallback<void(TargetList)> final_callback,
      bool success,
      std::vector<uint8_t> data,
      x11::Atom out_type);

  // Returns the X11 selection atom that we pass to various XSelection functions
  // for the given buffer.
  x11::Atom LookupSelectionForClipboardBuffer(ClipboardBuffer buffer) const;

  // Returns the X11 selection atom that we pass to various XSelection functions
  // for ClipboardBuffer::kCopyPaste.
  x11::Atom GetCopyPasteSelection() const;

  // Finds the SelectionFormatMap for the incoming selection atom.
  const SelectionFormatMap& LookupStorageForAtom(x11::Atom atom);

  // Our X11 state.
  raw_ref<x11::Connection> connection_;
  const x11::Window x_root_window_;

  // Input-only window used as a selection owner.
  x11::Window x_window_;

  // Events selected on |x_window_|.
  x11::ScopedEventSelector x_window_events_;

  // Object which requests and receives selection data.
  const std::unique_ptr<SelectionRequester> selection_requester_;

  // Temporary target map that we write to during DispatchObects.
  SelectionFormatMap clipboard_data_;

  // Objects which offer selection data to other windows.
  SelectionOwner clipboard_owner_;
  SelectionOwner primary_owner_;

  base::WeakPtrFactory<XClipboardHelper> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  //  UI_BASE_X_X11_CLIPBOARD_HELPER_H_
