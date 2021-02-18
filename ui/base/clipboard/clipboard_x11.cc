// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_x11.h"

#include <stdint.h>

#include <limits>
#include <list>
#include <memory>
#include <set>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/selection_requestor.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_window_event_manager.h"
#include "ui/gfx/x/xfixes.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_util.h"

namespace ui {

namespace {

const char kClipboard[] = "CLIPBOARD";
const char kClipboardManager[] = "CLIPBOARD_MANAGER";

///////////////////////////////////////////////////////////////////////////////

// Uses the XFixes API to provide sequence numbers for GetSequenceNumber().
class SelectionChangeObserver : public x11::EventObserver {
 public:
  static SelectionChangeObserver* GetInstance();

  uint64_t clipboard_sequence_number() const {
    return clipboard_sequence_number_;
  }
  uint64_t primary_sequence_number() const { return primary_sequence_number_; }

 private:
  friend struct base::DefaultSingletonTraits<SelectionChangeObserver>;

  SelectionChangeObserver();
  ~SelectionChangeObserver() override;

  // x11::EventObserver:
  void OnEvent(const x11::Event& xev) override;

  x11::Atom clipboard_atom_{};
  uint64_t clipboard_sequence_number_{};
  uint64_t primary_sequence_number_{};

  DISALLOW_COPY_AND_ASSIGN(SelectionChangeObserver);
};

SelectionChangeObserver::SelectionChangeObserver() {
  auto* connection = x11::Connection::Get();
  auto& xfixes = connection->xfixes();
  // Let the server know the client version.  No need to sync since we don't
  // care what version is running on the server.
  xfixes.QueryVersion({x11::XFixes::major_version, x11::XFixes::minor_version});
  if (!xfixes.present())
    return;

  clipboard_atom_ = x11::GetAtom(kClipboard);
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

// We are a singleton; we will outlive the event source.
SelectionChangeObserver::~SelectionChangeObserver() = default;

SelectionChangeObserver* SelectionChangeObserver::GetInstance() {
  return base::Singleton<SelectionChangeObserver>::get();
}

void SelectionChangeObserver::OnEvent(const x11::Event& xev) {
  auto* ev = xev.As<x11::XFixes::SelectionNotifyEvent>();
  if (!ev)
    return;

  if (static_cast<x11::Atom>(ev->selection) == clipboard_atom_) {
    clipboard_sequence_number_++;
    ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  } else if (ev->selection == x11::Atom::PRIMARY) {
    primary_sequence_number_++;
  } else {
    DLOG(ERROR) << "Unexpected selection atom: "
                << static_cast<uint32_t>(ev->selection);
  }
}

///////////////////////////////////////////////////////////////////////////////

// Represents a list of possible return types. Copy constructable.
class TargetList {
 public:
  using AtomVector = std::vector<x11::Atom>;

  explicit TargetList(const AtomVector& target_list);

  const AtomVector& target_list() { return target_list_; }

  bool ContainsText() const;
  bool ContainsFormat(const ClipboardFormatType& format_type) const;
  bool ContainsAtom(x11::Atom atom) const;

 private:
  AtomVector target_list_;
};

TargetList::TargetList(const AtomVector& target_list)
    : target_list_(target_list) {}

bool TargetList::ContainsText() const {
  std::vector<x11::Atom> atoms = GetTextAtomsFrom();
  for (const auto& atom : atoms) {
    if (ContainsAtom(atom))
      return true;
  }

  return false;
}

bool TargetList::ContainsFormat(const ClipboardFormatType& format_type) const {
  x11::Atom atom = x11::GetAtom(format_type.GetName().c_str());
  return ContainsAtom(atom);
}

bool TargetList::ContainsAtom(x11::Atom atom) const {
  return base::Contains(target_list_, atom);
}

x11::Window GetSelectionOwner(x11::Atom selection) {
  auto response = x11::Connection::Get()->GetSelectionOwner({selection}).Sync();
  return response ? response->owner : x11::Window::None;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ClipboardX11::X11Details

// Private implementation of our X11 integration. Keeps X11 headers out of the
// majority of chrome, which break badly.
class ClipboardX11::X11Details : public x11::EventObserver {
 public:
  X11Details();
  ~X11Details() override;

  // Returns the X11 selection atom that we pass to various XSelection functions
  // for the given buffer.
  x11::Atom LookupSelectionForClipboardBuffer(ClipboardBuffer buffer) const;

  // Returns the X11 selection atom that we pass to various XSelection functions
  // for ClipboardBuffer::kCopyPaste.
  x11::Atom GetCopyPasteSelection() const;

  // Finds the SelectionFormatMap for the incoming selection atom.
  const SelectionFormatMap& LookupStorageForAtom(x11::Atom atom);

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

  // Returns the first of |types| offered by the current selection holder in
  // |data_out|, or returns nullptr if none of those types are available.
  //
  // If the selection holder is us, this call is synchronous and we pull
  // the data out of |clipboard_selection_| or |primary_selection_|. If the
  // selection holder is some other window, we spin up a nested run loop
  // and do the asynchronous dance with whatever application is holding the
  // selection.
  SelectionData RequestAndWaitForTypes(ClipboardBuffer buffer,
                                       const std::vector<x11::Atom>& types);

  // Retrieves the list of possible data types the current clipboard owner has.
  //
  // If the selection holder is us, this is synchronous, otherwise this runs a
  // blocking message loop.
  TargetList WaitAndGetTargetsList(ClipboardBuffer buffer);

  // Returns a list of all text atoms that we handle.
  std::vector<x11::Atom> GetTextAtoms() const;

  // Returns a vector with a |format| converted to an X11 atom.
  std::vector<x11::Atom> GetAtomsForFormat(const ClipboardFormatType& format);

  // Clears a certain clipboard buffer, whether we own it or not.
  void Clear(ClipboardBuffer buffer);

  // If we own the CLIPBOARD selection, requests the clipboard manager to take
  // ownership of it.
  void StoreCopyPasteDataAndWait();

 private:
  // x11::EventObserver:
  void OnEvent(const x11::Event& xev) override;

  // Our X11 state.
  x11::Connection* connection_;
  x11::Window x_root_window_;

  // Input-only window used as a selection owner.
  x11::Window x_window_;

  // Events selected on |x_window_|.
  std::unique_ptr<x11::XScopedEventSelector> x_window_events_;

  // Object which requests and receives selection data.
  SelectionRequestor selection_requestor_;

  // Temporary target map that we write to during DispatchObects.
  SelectionFormatMap clipboard_data_;

  // Objects which offer selection data to other windows.
  SelectionOwner clipboard_owner_;
  SelectionOwner primary_owner_;

  DISALLOW_COPY_AND_ASSIGN(X11Details);
};

ClipboardX11::X11Details::X11Details()
    : connection_(x11::Connection::Get()),
      x_root_window_(ui::GetX11RootWindow()),
      x_window_(x11::CreateDummyWindow("Chromium Clipboard Window")),
      selection_requestor_(x_window_, this),
      clipboard_owner_(connection_, x_window_, x11::GetAtom(kClipboard)),
      primary_owner_(connection_, x_window_, x11::Atom::PRIMARY) {
  x11::SetStringProperty(x_window_, x11::Atom::WM_NAME, x11::Atom::STRING,
                         "Chromium clipboard");
  x_window_events_ = std::make_unique<x11::XScopedEventSelector>(
      x_window_, x11::EventMask::PropertyChange);

  connection_->AddEventObserver(this);
}

ClipboardX11::X11Details::~X11Details() {
  connection_->RemoveEventObserver(this);
  connection_->DestroyWindow({x_window_});
}

x11::Atom ClipboardX11::X11Details::LookupSelectionForClipboardBuffer(
    ClipboardBuffer buffer) const {
  if (buffer == ClipboardBuffer::kCopyPaste)
    return GetCopyPasteSelection();

  return x11::Atom::PRIMARY;
}

x11::Atom ClipboardX11::X11Details::GetCopyPasteSelection() const {
  return x11::GetAtom(kClipboard);
}

const SelectionFormatMap& ClipboardX11::X11Details::LookupStorageForAtom(
    x11::Atom atom) {
  if (atom == x11::Atom::PRIMARY)
    return primary_owner_.selection_format_map();

  DCHECK_EQ(GetCopyPasteSelection(), atom);
  return clipboard_owner_.selection_format_map();
}

void ClipboardX11::X11Details::CreateNewClipboardData() {
  clipboard_data_ = SelectionFormatMap();
}

void ClipboardX11::X11Details::InsertMapping(
    const std::string& key,
    const scoped_refptr<base::RefCountedMemory>& memory) {
  x11::Atom atom_key = x11::GetAtom(key.c_str());
  clipboard_data_.Insert(atom_key, memory);
}

void ClipboardX11::X11Details::TakeOwnershipOfSelection(
    ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste)
    return clipboard_owner_.TakeOwnershipOfSelection(clipboard_data_);
  else
    return primary_owner_.TakeOwnershipOfSelection(clipboard_data_);
}

SelectionData ClipboardX11::X11Details::RequestAndWaitForTypes(
    ClipboardBuffer buffer,
    const std::vector<x11::Atom>& types) {
  x11::Atom selection_name = LookupSelectionForClipboardBuffer(buffer);
  if (GetSelectionOwner(selection_name) == x_window_) {
    // We can local fastpath instead of playing the nested run loop game
    // with the X server.
    const SelectionFormatMap& format_map = LookupStorageForAtom(selection_name);

    for (const auto& type : types) {
      auto format_map_it = format_map.find(type);
      if (format_map_it != format_map.end())
        return SelectionData(format_map_it->first, format_map_it->second);
    }
  } else {
    TargetList targets = WaitAndGetTargetsList(buffer);

    x11::Atom selection_name = LookupSelectionForClipboardBuffer(buffer);
    std::vector<x11::Atom> intersection;
    GetAtomIntersection(types, targets.target_list(), &intersection);
    return selection_requestor_.RequestAndWaitForTypes(selection_name,
                                                       intersection);
  }

  return SelectionData();
}

TargetList ClipboardX11::X11Details::WaitAndGetTargetsList(
    ClipboardBuffer buffer) {
  x11::Atom selection_name = LookupSelectionForClipboardBuffer(buffer);
  std::vector<x11::Atom> out;
  if (GetSelectionOwner(selection_name) == x_window_) {
    // We can local fastpath and return the list of local targets.
    const SelectionFormatMap& format_map = LookupStorageForAtom(selection_name);

    for (const auto& format : format_map)
      out.push_back(format.first);
  } else {
    std::vector<uint8_t> data;
    x11::Atom out_type = x11::Atom::None;

    if (selection_requestor_.PerformBlockingConvertSelection(
            selection_name, x11::GetAtom(kTargets), &data, &out_type)) {
      // Some apps return an |out_type| of "TARGETS". (crbug.com/377893)
      if (out_type == x11::Atom::ATOM || out_type == x11::GetAtom(kTargets)) {
        const x11::Atom* atom_array =
            reinterpret_cast<const x11::Atom*>(data.data());
        for (size_t i = 0; i < data.size() / sizeof(x11::Atom); ++i)
          out.push_back(atom_array[i]);
      }
    } else {
      // There was no target list. Most Java apps doesn't offer a TARGETS list,
      // even though they AWT to. They will offer individual text types if you
      // ask. If this is the case we attempt to make sense of the contents as
      // text. This is pretty unfortunate since it means we have to actually
      // copy the data to see if it is available, but at least this path
      // shouldn't be hit for conforming programs.
      std::vector<x11::Atom> types = GetTextAtoms();
      for (const auto& text_atom : types) {
        x11::Atom type = x11::Atom::None;
        if (selection_requestor_.PerformBlockingConvertSelection(
                selection_name, text_atom, nullptr, &type) &&
            type == text_atom) {
          out.push_back(text_atom);
        }
      }
    }
  }

  return TargetList(out);
}

std::vector<x11::Atom> ClipboardX11::X11Details::GetTextAtoms() const {
  return GetTextAtomsFrom();
}

std::vector<x11::Atom> ClipboardX11::X11Details::GetAtomsForFormat(
    const ClipboardFormatType& format) {
  return {x11::GetAtom(format.GetName().c_str())};
}

void ClipboardX11::X11Details::Clear(ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste)
    clipboard_owner_.ClearSelectionOwner();
  else
    primary_owner_.ClearSelectionOwner();
}

void ClipboardX11::X11Details::StoreCopyPasteDataAndWait() {
  x11::Atom selection = GetCopyPasteSelection();
  if (GetSelectionOwner(selection) != x_window_)
    return;

  x11::Atom clipboard_manager_atom = x11::GetAtom(kClipboardManager);
  if (GetSelectionOwner(clipboard_manager_atom) == x11::Window::None)
    return;

  const SelectionFormatMap& format_map = LookupStorageForAtom(selection);
  if (format_map.size() == 0)
    return;
  std::vector<x11::Atom> targets = format_map.GetTypes();

  base::TimeTicks start = base::TimeTicks::Now();
  selection_requestor_.PerformBlockingConvertSelectionWithParameter(
      x11::GetAtom(kClipboardManager), x11::GetAtom(kSaveTargets), targets);
  UMA_HISTOGRAM_TIMES("Clipboard.X11StoreCopyPasteDuration",
                      base::TimeTicks::Now() - start);
}

void ClipboardX11::X11Details::OnEvent(const x11::Event& xev) {
  if (auto* request = xev.As<x11::SelectionRequestEvent>()) {
    if (request->owner != x_window_)
      return;
    if (request->selection == x11::Atom::PRIMARY) {
      primary_owner_.OnSelectionRequest(*request);
    } else {
      // We should not get requests for the CLIPBOARD_MANAGER selection
      // because we never take ownership of it.
      DCHECK_EQ(GetCopyPasteSelection(), request->selection);
      clipboard_owner_.OnSelectionRequest(*request);
    }
  } else if (auto* notify = xev.As<x11::SelectionNotifyEvent>()) {
    if (notify->requestor == x_window_)
      selection_requestor_.OnSelectionNotify(*notify);
  } else if (auto* clear = xev.As<x11::SelectionClearEvent>()) {
    if (clear->owner != x_window_)
      return;
    if (clear->selection == x11::Atom::PRIMARY) {
      primary_owner_.OnSelectionClear(*clear);
    } else {
      // We should not get requests for the CLIPBOARD_MANAGER selection
      // because we never take ownership of it.
      DCHECK_EQ(GetCopyPasteSelection(), clear->selection);
      clipboard_owner_.OnSelectionClear(*clear);
    }
  } else if (auto* prop = xev.As<x11::PropertyNotifyEvent>()) {
    if (primary_owner_.CanDispatchPropertyEvent(*prop))
      primary_owner_.OnPropertyEvent(*prop);
    if (clipboard_owner_.CanDispatchPropertyEvent(*prop))
      clipboard_owner_.OnPropertyEvent(*prop);
    if (selection_requestor_.CanDispatchPropertyEvent(*prop))
      selection_requestor_.OnPropertyEvent(*prop);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ClipboardX11

ClipboardX11::ClipboardX11() : x11_details_(new X11Details) {
  DCHECK(CalledOnValidThread());
}

ClipboardX11::~ClipboardX11() {
  DCHECK(CalledOnValidThread());
}

void ClipboardX11::OnPreShutdown() {
  DCHECK(CalledOnValidThread());
  x11_details_->StoreCopyPasteDataAndWait();
}

DataTransferEndpoint* ClipboardX11::GetSource(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  auto it = data_src_.find(buffer);
  return it == data_src_.end() ? nullptr : it->second.get();
}

uint64_t ClipboardX11::GetSequenceNumber(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  if (buffer == ClipboardBuffer::kCopyPaste)
    return SelectionChangeObserver::GetInstance()->clipboard_sequence_number();
  else
    return SelectionChangeObserver::GetInstance()->primary_sequence_number();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
bool ClipboardX11::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  TargetList target_list = x11_details_->WaitAndGetTargetsList(buffer);
  if (format == ClipboardFormatType::GetPlainTextType() ||
      format == ClipboardFormatType::GetUrlType()) {
    return target_list.ContainsText();
  }
  return target_list.ContainsFormat(format);
}

void ClipboardX11::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));
  x11_details_->Clear(buffer);
  data_src_[buffer].reset();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<base::string16>* types) const {
  DCHECK(CalledOnValidThread());
  DCHECK(types);

  TargetList target_list = x11_details_->WaitAndGetTargetsList(buffer);

  types->clear();

  if (target_list.ContainsText())
    types->push_back(base::UTF8ToUTF16(kMimeTypeText));
  if (target_list.ContainsFormat(ClipboardFormatType::GetHtmlType()))
    types->push_back(base::UTF8ToUTF16(kMimeTypeHTML));
  if (target_list.ContainsFormat(ClipboardFormatType::GetRtfType()))
    types->push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  if (target_list.ContainsFormat(ClipboardFormatType::GetBitmapType()))
    types->push_back(base::UTF8ToUTF16(kMimeTypePNG));
  // Only support filenames if chrome://flags#clipboard-filenames is enabled.
  if (target_list.ContainsFormat(ClipboardFormatType::GetFilenamesType()) &&
      base::FeatureList::IsEnabled(features::kClipboardFilenames))
    types->push_back(base::UTF8ToUTF16(kMimeTypeURIList));

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer, x11_details_->GetAtomsForFormat(
                  ClipboardFormatType::GetWebCustomDataType())));
  if (data.IsValid())
    ReadCustomDataTypes(data.GetData(), data.GetSize(), types);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
std::vector<base::string16>
ClipboardX11::ReadAvailablePlatformSpecificFormatNames(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());

  // Copy target_list(), so that XGetAtomNames can get a non-const Atom*.
  TargetList::AtomVector target_list =
      x11_details_->WaitAndGetTargetsList(buffer).target_list();
  if (target_list.empty())
    return {};

  std::vector<x11::Future<x11::GetAtomNameReply>> futures;
  for (x11::Atom target : target_list)
    futures.push_back(x11::Connection::Get()->GetAtomName({target}));
  std::vector<base::string16> types;
  types.reserve(target_list.size());
  for (auto& future : futures) {
    if (auto response = future.Sync())
      types.push_back(base::UTF8ToUTF16(response->name));
    else
      types.emplace_back();
  }

  return types;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadText(ClipboardBuffer buffer,
                            const DataTransferEndpoint* data_dst,
                            base::string16* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kText);

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer, x11_details_->GetTextAtoms()));
  if (data.IsValid()) {
    std::string text = data.GetText();
    *result = base::UTF8ToUTF16(text);
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadAsciiText(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kText);

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer, x11_details_->GetTextAtoms()));
  if (data.IsValid())
    result->assign(data.GetText());
}

// TODO(estade): handle different charsets.
// TODO(port): set *src_url.
// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadHTML(ClipboardBuffer buffer,
                            const DataTransferEndpoint* data_dst,
                            base::string16* markup,
                            std::string* src_url,
                            uint32_t* fragment_start,
                            uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kHtml);
  markup->clear();
  if (src_url)
    src_url->clear();
  *fragment_start = 0;
  *fragment_end = 0;

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer,
      x11_details_->GetAtomsForFormat(ClipboardFormatType::GetHtmlType())));
  if (data.IsValid()) {
    *markup = data.GetHtml();

    *fragment_start = 0;
    DCHECK_LE(markup->length(), std::numeric_limits<uint32_t>::max());
    *fragment_end = static_cast<uint32_t>(markup->length());
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadSvg(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           base::string16* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kSvg);

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer,
      x11_details_->GetAtomsForFormat(ClipboardFormatType::GetSvgType())));
  if (data.IsValid()) {
    std::string markup;
    data.AssignTo(&markup);
    *result = base::UTF8ToUTF16(markup);
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadRTF(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kRtf);

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer,
      x11_details_->GetAtomsForFormat(ClipboardFormatType::GetRtfType())));
  if (data.IsValid())
    data.AssignTo(result);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadImage(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             ReadImageCallback callback) const {
  DCHECK(IsSupportedClipboardBuffer(buffer));
  RecordRead(ClipboardFormatMetric::kImage);
  std::move(callback).Run(ReadImageInternal(buffer));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadCustomData(ClipboardBuffer buffer,
                                  const base::string16& type,
                                  const DataTransferEndpoint* data_dst,
                                  base::string16* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kCustomData);

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer, x11_details_->GetAtomsForFormat(
                  ClipboardFormatType::GetWebCustomDataType())));
  if (data.IsValid())
    ReadCustomDataForType(data.GetData(), data.GetSize(), type, result);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadFilenames(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::vector<ui::FileInfo>* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kFilenames);

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer, x11_details_->GetAtomsForFormat(
                  ClipboardFormatType::GetFilenamesType())));
  std::string uri_list;
  if (data.IsValid())
    data.AssignTo(&uri_list);
  *result = ui::URIListToFileInfos(uri_list);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadBookmark(const DataTransferEndpoint* data_dst,
                                base::string16* title,
                                std::string* url) const {
  DCHECK(CalledOnValidThread());
  // TODO(erg): This was left NOTIMPLEMENTED() in the gtk port too.
  NOTIMPLEMENTED();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadData(const ClipboardFormatType& format,
                            const DataTransferEndpoint* data_dst,
                            std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kData);

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      ClipboardBuffer::kCopyPaste, x11_details_->GetAtomsForFormat(format)));
  if (data.IsValid())
    data.AssignTo(result);
}

#if defined(USE_OZONE)
bool ClipboardX11::IsSelectionBufferAvailable() const {
  return true;
}
#endif  // defined(USE_OZONE)

// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::WritePortableRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  x11_details_->CreateNewClipboardData();
  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);
  x11_details_->TakeOwnershipOfSelection(buffer);

  if (buffer == ClipboardBuffer::kCopyPaste) {
    auto text_iter = objects.find(PortableFormat::kText);
    if (text_iter != objects.end()) {
      x11_details_->CreateNewClipboardData();
      const ObjectMapParams& params_vector = text_iter->second;
      if (params_vector.size()) {
        const ObjectMapParam& char_vector = params_vector[0];
        if (char_vector.size())
          WriteText(&char_vector.front(), char_vector.size());
      }
      x11_details_->TakeOwnershipOfSelection(ClipboardBuffer::kSelection);
    }
  }

  data_src_[buffer] = std::move(data_src);
}

// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::WritePlatformRepresentations(
    ClipboardBuffer buffer,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  x11_details_->CreateNewClipboardData();
  DispatchPlatformRepresentations(std::move(platform_representations));
  x11_details_->TakeOwnershipOfSelection(buffer);
  data_src_[buffer] = std::move(data_src);
}

void ClipboardX11::WriteText(const char* text_data, size_t text_len) {
  std::string str(text_data, text_len);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&str));

  x11_details_->InsertMapping(kMimeTypeText, mem);
  x11_details_->InsertMapping(kMimeTypeLinuxText, mem);
  x11_details_->InsertMapping(kMimeTypeLinuxString, mem);
  x11_details_->InsertMapping(kMimeTypeLinuxUtf8String, mem);
}

void ClipboardX11::WriteHTML(const char* markup_data,
                             size_t markup_len,
                             const char* url_data,
                             size_t url_len) {
  // TODO(estade): We need to expand relative links with |url_data|.
  static const char* html_prefix =
      "<meta http-equiv=\"content-type\" "
      "content=\"text/html; charset=utf-8\">";
  std::string data = html_prefix;
  data += std::string(markup_data, markup_len);
  // Some programs expect '\0'-terminated data. See http://crbug.com/42624
  data += '\0';

  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&data));
  x11_details_->InsertMapping(kMimeTypeHTML, mem);
}

void ClipboardX11::WriteSvg(const char* markup_data, size_t markup_len) {
  std::string str(markup_data, markup_len);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&str));

  x11_details_->InsertMapping(kMimeTypeSvg, mem);
}

void ClipboardX11::WriteRTF(const char* rtf_data, size_t data_len) {
  WriteData(ClipboardFormatType::GetRtfType(), rtf_data, data_len);
}

void ClipboardX11::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  std::string uri_list = ui::FileInfosToURIList(filenames);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&uri_list));
  x11_details_->InsertMapping(ClipboardFormatType::GetFilenamesType().GetName(),
                              mem);
}

void ClipboardX11::WriteBookmark(const char* title_data,
                                 size_t title_len,
                                 const char* url_data,
                                 size_t url_len) {
  // Write as a mozilla url (UTF16: URL, newline, title).
  base::string16 url = base::UTF8ToUTF16(std::string(url_data, url_len) + "\n");
  base::string16 title =
      base::UTF8ToUTF16(base::StringPiece(title_data, title_len));

  std::vector<unsigned char> data;
  AddString16ToVector(url, &data);
  AddString16ToVector(title, &data);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedBytes::TakeVector(&data));

  x11_details_->InsertMapping(kMimeTypeMozillaURL, mem);
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void ClipboardX11::WriteWebSmartPaste() {
  std::string empty;
  x11_details_->InsertMapping(kMimeTypeWebkitSmartPaste,
                              scoped_refptr<base::RefCountedMemory>(
                                  base::RefCountedString::TakeString(&empty)));
}

void ClipboardX11::WriteBitmap(const SkBitmap& bitmap) {
  // Encode the bitmap as a PNG for transport.
  std::vector<unsigned char> output;
  if (gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, false, &output)) {
    x11_details_->InsertMapping(kMimeTypePNG,
                                base::RefCountedBytes::TakeVector(&output));
  }
}

void ClipboardX11::WriteData(const ClipboardFormatType& format,
                             const char* data_data,
                             size_t data_len) {
  std::vector<unsigned char> bytes(data_data, data_data + data_len);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedBytes::TakeVector(&bytes));
  x11_details_->InsertMapping(format.GetName(), mem);
}

SkBitmap ClipboardX11::ReadImageInternal(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());

  // TODO(https://crbug.com/443355): Since now that ReadImage() is async,
  // refactor the code to keep a callback with the request, and invoke the
  // callback when the request is satisfied.
  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer,
      x11_details_->GetAtomsForFormat(ClipboardFormatType::GetBitmapType())));
  if (data.IsValid()) {
    SkBitmap bitmap;
    if (gfx::PNGCodec::Decode(data.GetData(), data.GetSize(), &bitmap))
      return SkBitmap(bitmap);
  }

  return SkBitmap();
}

}  // namespace ui
