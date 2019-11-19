// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_x11.h"

#include <stdint.h>

#include <limits>
#include <list>
#include <memory>
#include <set>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/selection_requestor.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {

namespace {

const char kClipboard[] = "CLIPBOARD";
const char kClipboardManager[] = "CLIPBOARD_MANAGER";

///////////////////////////////////////////////////////////////////////////////

// Uses the XFixes API to provide sequence numbers for GetSequenceNumber().
class SelectionChangeObserver : public PlatformEventObserver {
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

  // PlatformEventObserver:
  void WillProcessEvent(const PlatformEvent& event) override;
  void DidProcessEvent(const PlatformEvent& event) override {}

  int event_base_;
  Atom clipboard_atom_;
  uint64_t clipboard_sequence_number_;
  uint64_t primary_sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(SelectionChangeObserver);
};

SelectionChangeObserver::SelectionChangeObserver()
    : event_base_(-1),
      clipboard_atom_(x11::None),
      clipboard_sequence_number_(0),
      primary_sequence_number_(0) {
  int ignored;
  if (XFixesQueryExtension(gfx::GetXDisplay(), &event_base_, &ignored)) {
    clipboard_atom_ = gfx::GetAtom(kClipboard);
    XFixesSelectSelectionInput(gfx::GetXDisplay(), GetX11RootWindow(),
                               clipboard_atom_,
                               XFixesSetSelectionOwnerNotifyMask |
                                   XFixesSelectionWindowDestroyNotifyMask |
                                   XFixesSelectionClientCloseNotifyMask);
    // This seems to be semi-optional. For some reason, registering for any
    // selection notify events seems to subscribe us to events for both the
    // primary and the clipboard buffers. Register anyway just to be safe.
    XFixesSelectSelectionInput(gfx::GetXDisplay(), GetX11RootWindow(),
                               XA_PRIMARY,
                               XFixesSetSelectionOwnerNotifyMask |
                                   XFixesSelectionWindowDestroyNotifyMask |
                                   XFixesSelectionClientCloseNotifyMask);

    PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
  }
}

SelectionChangeObserver::~SelectionChangeObserver() {
  // We are a singleton; we will outlive the event source.
}

SelectionChangeObserver* SelectionChangeObserver::GetInstance() {
  return base::Singleton<SelectionChangeObserver>::get();
}

void SelectionChangeObserver::WillProcessEvent(const PlatformEvent& event) {
  if (event->type == event_base_ + XFixesSelectionNotify) {
    XFixesSelectionNotifyEvent* ev =
        reinterpret_cast<XFixesSelectionNotifyEvent*>(event);
    if (ev->selection == clipboard_atom_) {
      clipboard_sequence_number_++;
      ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
    } else if (ev->selection == XA_PRIMARY) {
      primary_sequence_number_++;
    } else {
      DLOG(ERROR) << "Unexpected selection atom: " << ev->selection;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

// Represents a list of possible return types. Copy constructable.
class TargetList {
 public:
  using AtomVector = std::vector<::Atom>;

  explicit TargetList(const AtomVector& target_list);

  const AtomVector& target_list() { return target_list_; }

  bool ContainsText() const;
  bool ContainsFormat(const ClipboardFormatType& format_type) const;
  bool ContainsAtom(::Atom atom) const;

 private:
  AtomVector target_list_;
};

TargetList::TargetList(const AtomVector& target_list)
    : target_list_(target_list) {}

bool TargetList::ContainsText() const {
  std::vector<::Atom> atoms = GetTextAtomsFrom();
  for (const auto& atom : atoms) {
    if (ContainsAtom(atom))
      return true;
  }

  return false;
}

bool TargetList::ContainsFormat(const ClipboardFormatType& format_type) const {
  ::Atom atom = gfx::GetAtom(format_type.ToString().c_str());
  return ContainsAtom(atom);
}

bool TargetList::ContainsAtom(::Atom atom) const {
  return base::Contains(target_list_, atom);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ClipboardX11::X11Details

// Private implementation of our X11 integration. Keeps X11 headers out of the
// majority of chrome, which break badly.
class ClipboardX11::X11Details : public PlatformEventDispatcher {
 public:
  X11Details();
  ~X11Details() override;

  // Returns the X11 selection atom that we pass to various XSelection functions
  // for the given buffer.
  ::Atom LookupSelectionForClipboardBuffer(ClipboardBuffer buffer) const;

  // Returns the X11 selection atom that we pass to various XSelection functions
  // for ClipboardBuffer::kCopyPaste.
  ::Atom GetCopyPasteSelection() const;

  // Finds the SelectionFormatMap for the incoming selection atom.
  const SelectionFormatMap& LookupStorageForAtom(::Atom atom);

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
  ui::SelectionData RequestAndWaitForTypes(ClipboardBuffer buffer,
                                           const std::vector<::Atom>& types);

  // Retrieves the list of possible data types the current clipboard owner has.
  //
  // If the selection holder is us, this is synchronous, otherwise this runs a
  // blocking message loop.
  TargetList WaitAndGetTargetsList(ClipboardBuffer buffer);

  // Returns a list of all text atoms that we handle.
  std::vector<::Atom> GetTextAtoms() const;

  // Returns a vector with a |format| converted to an X11 atom.
  std::vector<::Atom> GetAtomsForFormat(const ClipboardFormatType& format);

  // Clears a certain clipboard buffer, whether we own it or not.
  void Clear(ClipboardBuffer buffer);

  // If we own the CLIPBOARD selection, requests the clipboard manager to take
  // ownership of it.
  void StoreCopyPasteDataAndWait();

 private:
  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // Our X11 state.
  Display* x_display_;
  ::Window x_root_window_;

  // Input-only window used as a selection owner.
  ::Window x_window_;

  // Events selected on |x_window_|.
  std::unique_ptr<XScopedEventSelector> x_window_events_;

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
    : x_display_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(x_display_)),
      x_window_(XCreateWindow(x_display_,
                              x_root_window_,
                              -100,
                              -100,
                              10,
                              10,              // x, y, width, height
                              0,               // border width
                              CopyFromParent,  // depth
                              InputOnly,
                              CopyFromParent,  // visual
                              0,
                              nullptr)),
      selection_requestor_(x_display_, x_window_, this),
      clipboard_owner_(x_display_, x_window_, gfx::GetAtom(kClipboard)),
      primary_owner_(x_display_, x_window_, XA_PRIMARY) {
  XStoreName(x_display_, x_window_, "Chromium clipboard");
  x_window_events_.reset(
      new XScopedEventSelector(x_window_, PropertyChangeMask));

  if (PlatformEventSource::GetInstance())
    PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

ClipboardX11::X11Details::~X11Details() {
  if (PlatformEventSource::GetInstance())
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);

  XDestroyWindow(x_display_, x_window_);
}

::Atom ClipboardX11::X11Details::LookupSelectionForClipboardBuffer(
    ClipboardBuffer buffer) const {
  if (buffer == ClipboardBuffer::kCopyPaste)
    return GetCopyPasteSelection();

  return XA_PRIMARY;
}

::Atom ClipboardX11::X11Details::GetCopyPasteSelection() const {
  return gfx::GetAtom(kClipboard);
}

const SelectionFormatMap& ClipboardX11::X11Details::LookupStorageForAtom(
    ::Atom atom) {
  if (atom == XA_PRIMARY)
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
  ::Atom atom_key = gfx::GetAtom(key.c_str());
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
    const std::vector<::Atom>& types) {
  ::Atom selection_name = LookupSelectionForClipboardBuffer(buffer);
  if (XGetSelectionOwner(x_display_, selection_name) == x_window_) {
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

    ::Atom selection_name = LookupSelectionForClipboardBuffer(buffer);
    std::vector<::Atom> intersection;
    GetAtomIntersection(types, targets.target_list(), &intersection);
    return selection_requestor_.RequestAndWaitForTypes(selection_name,
                                                       intersection);
  }

  return SelectionData();
}

TargetList ClipboardX11::X11Details::WaitAndGetTargetsList(
    ClipboardBuffer buffer) {
  ::Atom selection_name = LookupSelectionForClipboardBuffer(buffer);
  std::vector<::Atom> out;
  if (XGetSelectionOwner(x_display_, selection_name) == x_window_) {
    // We can local fastpath and return the list of local targets.
    const SelectionFormatMap& format_map = LookupStorageForAtom(selection_name);

    for (const auto& format : format_map)
      out.push_back(format.first);
  } else {
    scoped_refptr<base::RefCountedMemory> data;
    size_t out_data_items = 0;
    ::Atom out_type = x11::None;

    if (selection_requestor_.PerformBlockingConvertSelection(
            selection_name, gfx::GetAtom(kTargets), &data, &out_data_items,
            &out_type)) {
      // Some apps return an |out_type| of "TARGETS". (crbug.com/377893)
      if (out_type == XA_ATOM || out_type == gfx::GetAtom(kTargets)) {
        const ::Atom* atom_array =
            reinterpret_cast<const ::Atom*>(data->front());
        for (size_t i = 0; i < out_data_items; ++i)
          out.push_back(atom_array[i]);
      }
    } else {
      // There was no target list. Most Java apps doesn't offer a TARGETS list,
      // even though they AWT to. They will offer individual text types if you
      // ask. If this is the case we attempt to make sense of the contents as
      // text. This is pretty unfortunate since it means we have to actually
      // copy the data to see if it is available, but at least this path
      // shouldn't be hit for conforming programs.
      std::vector<::Atom> types = GetTextAtoms();
      for (const auto& text_atom : types) {
        ::Atom type = x11::None;
        if (selection_requestor_.PerformBlockingConvertSelection(
                selection_name, text_atom, nullptr, nullptr, &type) &&
            type == text_atom) {
          out.push_back(text_atom);
        }
      }
    }
  }

  return TargetList(out);
}

std::vector<::Atom> ClipboardX11::X11Details::GetTextAtoms() const {
  return GetTextAtomsFrom();
}

std::vector<::Atom> ClipboardX11::X11Details::GetAtomsForFormat(
    const ClipboardFormatType& format) {
  return {gfx::GetAtom(format.ToString().c_str())};
}

void ClipboardX11::X11Details::Clear(ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste)
    clipboard_owner_.ClearSelectionOwner();
  else
    primary_owner_.ClearSelectionOwner();
}

void ClipboardX11::X11Details::StoreCopyPasteDataAndWait() {
  ::Atom selection = GetCopyPasteSelection();
  if (XGetSelectionOwner(x_display_, selection) != x_window_)
    return;

  ::Atom clipboard_manager_atom = gfx::GetAtom(kClipboardManager);
  if (XGetSelectionOwner(x_display_, clipboard_manager_atom) == x11::None)
    return;

  const SelectionFormatMap& format_map = LookupStorageForAtom(selection);
  if (format_map.size() == 0)
    return;
  std::vector<Atom> targets = format_map.GetTypes();

  base::TimeTicks start = base::TimeTicks::Now();
  selection_requestor_.PerformBlockingConvertSelectionWithParameter(
      gfx::GetAtom(kClipboardManager), gfx::GetAtom(kSaveTargets), targets);
  UMA_HISTOGRAM_TIMES("Clipboard.X11StoreCopyPasteDuration",
                      base::TimeTicks::Now() - start);
}

bool ClipboardX11::X11Details::CanDispatchEvent(const PlatformEvent& event) {
  if (event->xany.window == x_window_)
    return true;

  if (event->type == PropertyNotify) {
    return primary_owner_.CanDispatchPropertyEvent(*event) ||
           clipboard_owner_.CanDispatchPropertyEvent(*event) ||
           selection_requestor_.CanDispatchPropertyEvent(*event);
  }
  return false;
}

uint32_t ClipboardX11::X11Details::DispatchEvent(const PlatformEvent& xev) {
  switch (xev->type) {
    case SelectionRequest: {
      if (xev->xselectionrequest.selection == XA_PRIMARY) {
        primary_owner_.OnSelectionRequest(*xev);
      } else {
        // We should not get requests for the CLIPBOARD_MANAGER selection
        // because we never take ownership of it.
        DCHECK_EQ(GetCopyPasteSelection(), xev->xselectionrequest.selection);
        clipboard_owner_.OnSelectionRequest(*xev);
      }
      break;
    }
    case SelectionNotify: {
      selection_requestor_.OnSelectionNotify(*xev);
      break;
    }
    case SelectionClear: {
      if (xev->xselectionclear.selection == XA_PRIMARY) {
        primary_owner_.OnSelectionClear(*xev);
      } else {
        // We should not get requests for the CLIPBOARD_MANAGER selection
        // because we never take ownership of it.
        DCHECK_EQ(GetCopyPasteSelection(), xev->xselection.selection);
        clipboard_owner_.OnSelectionClear(*xev);
      }
      break;
    }
    case PropertyNotify: {
      if (primary_owner_.CanDispatchPropertyEvent(*xev))
        primary_owner_.OnPropertyEvent(*xev);
      if (clipboard_owner_.CanDispatchPropertyEvent(*xev))
        clipboard_owner_.OnPropertyEvent(*xev);
      if (selection_requestor_.CanDispatchPropertyEvent(*xev))
        selection_requestor_.OnPropertyEvent(*xev);
      break;
    }
    default:
      break;
  }

  return POST_DISPATCH_NONE;
}

///////////////////////////////////////////////////////////////////////////////
// Clipboard factory method.
Clipboard* Clipboard::Create() {
  return new ClipboardX11;
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

uint64_t ClipboardX11::GetSequenceNumber(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  if (buffer == ClipboardBuffer::kCopyPaste)
    return SelectionChangeObserver::GetInstance()->clipboard_sequence_number();
  else
    return SelectionChangeObserver::GetInstance()->primary_sequence_number();
}

bool ClipboardX11::IsFormatAvailable(const ClipboardFormatType& format,
                                     ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  TargetList target_list = x11_details_->WaitAndGetTargetsList(buffer);
  if (format.Equals(ClipboardFormatType::GetPlainTextType()) ||
      format.Equals(ClipboardFormatType::GetUrlType())) {
    return target_list.ContainsText();
  }
  return target_list.ContainsFormat(format);
}

void ClipboardX11::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));
  x11_details_->Clear(buffer);
}

void ClipboardX11::ReadAvailableTypes(ClipboardBuffer buffer,
                                      std::vector<base::string16>* types,
                                      bool* contains_filenames) const {
  DCHECK(CalledOnValidThread());
  if (!types || !contains_filenames) {
    NOTREACHED();
    return;
  }

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
  *contains_filenames = false;

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer, x11_details_->GetAtomsForFormat(
                  ClipboardFormatType::GetWebCustomDataType())));
  if (data.IsValid())
    ReadCustomDataTypes(data.GetData(), data.GetSize(), types);
}

void ClipboardX11::ReadText(ClipboardBuffer buffer,
                            base::string16* result) const {
  DCHECK(CalledOnValidThread());

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer, x11_details_->GetTextAtoms()));
  if (data.IsValid()) {
    std::string text = data.GetText();
    *result = base::UTF8ToUTF16(text);
  }
}

void ClipboardX11::ReadAsciiText(ClipboardBuffer buffer,
                                 std::string* result) const {
  DCHECK(CalledOnValidThread());

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer, x11_details_->GetTextAtoms()));
  if (data.IsValid())
    result->assign(data.GetText());
}

// TODO(estade): handle different charsets.
// TODO(port): set *src_url.
void ClipboardX11::ReadHTML(ClipboardBuffer buffer,
                            base::string16* markup,
                            std::string* src_url,
                            uint32_t* fragment_start,
                            uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
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
    DCHECK(markup->length() <= std::numeric_limits<uint32_t>::max());
    *fragment_end = static_cast<uint32_t>(markup->length());
  }
}

void ClipboardX11::ReadRTF(ClipboardBuffer buffer, std::string* result) const {
  DCHECK(CalledOnValidThread());

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer,
      x11_details_->GetAtomsForFormat(ClipboardFormatType::GetRtfType())));
  if (data.IsValid())
    data.AssignTo(result);
}

SkBitmap ClipboardX11::ReadImage(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());

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

void ClipboardX11::ReadCustomData(ClipboardBuffer buffer,
                                  const base::string16& type,
                                  base::string16* result) const {
  DCHECK(CalledOnValidThread());

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      buffer, x11_details_->GetAtomsForFormat(
                  ClipboardFormatType::GetWebCustomDataType())));
  if (data.IsValid())
    ReadCustomDataForType(data.GetData(), data.GetSize(), type, result);
}

void ClipboardX11::ReadBookmark(base::string16* title, std::string* url) const {
  DCHECK(CalledOnValidThread());
  // TODO(erg): This was left NOTIMPLEMENTED() in the gtk port too.
  NOTIMPLEMENTED();
}

void ClipboardX11::ReadData(const ClipboardFormatType& format,
                            std::string* result) const {
  DCHECK(CalledOnValidThread());

  SelectionData data(x11_details_->RequestAndWaitForTypes(
      ClipboardBuffer::kCopyPaste, x11_details_->GetAtomsForFormat(format)));
  if (data.IsValid())
    data.AssignTo(result);
}

void ClipboardX11::WritePortableRepresentations(ClipboardBuffer buffer,
                                                const ObjectMap& objects) {
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
}

void ClipboardX11::WritePlatformRepresentations(
    ClipboardBuffer buffer,
    std::vector<Clipboard::PlatformRepresentation> platform_representations) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  x11_details_->CreateNewClipboardData();
  DispatchPlatformRepresentations(std::move(platform_representations));
  x11_details_->TakeOwnershipOfSelection(buffer);
}

void ClipboardX11::WriteText(const char* text_data, size_t text_len) {
  std::string str(text_data, text_len);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&str));

  x11_details_->InsertMapping(kMimeTypeText, mem);
  x11_details_->InsertMapping(kText, mem);
  x11_details_->InsertMapping(kString, mem);
  x11_details_->InsertMapping(kUtf8String, mem);
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

void ClipboardX11::WriteRTF(const char* rtf_data, size_t data_len) {
  WriteData(ClipboardFormatType::GetRtfType(), rtf_data, data_len);
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
  ui::AddString16ToVector(url, &data);
  ui::AddString16ToVector(title, &data);
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
  x11_details_->InsertMapping(format.ToString(), mem);
}

}  // namespace ui
