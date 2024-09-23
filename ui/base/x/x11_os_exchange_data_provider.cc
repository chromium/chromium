// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/x/x11_os_exchange_data_provider.h"

#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_drag_drop_client.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"

// Note: the GetBlah() methods are used immediately by the
// web_contents_view_aura.cc:PrepareDropData(), while the omnibox is a
// little more discriminating and calls HasBlah() before trying to get the
// information.

namespace ui {

namespace {

const char kDndSelection[] = "XdndSelection";
const char kRendererTaint[] = "chromium/x-renderer-taint";
const char kFromPrivileged[] = "chromium/from-privileged";

const char kNetscapeURL[] = "_NETSCAPE_URL";

}  // namespace

XOSExchangeDataProvider::XOSExchangeDataProvider(
    x11::Window x_window,
    x11::Window source_window,
    const SelectionFormatMap& selection)
    : connection_(*x11::Connection::Get()),
      x_root_window_(ui::GetX11RootWindow()),
      own_window_(false),
      x_window_(x_window),
      source_window_(source_window),
      format_map_(selection),
      selection_owner_(connection_.get(),
                       x_window_,
                       x11::GetAtom(kDndSelection)) {}

XOSExchangeDataProvider::XOSExchangeDataProvider()
    : connection_(*x11::Connection::Get()),
      x_root_window_(ui::GetX11RootWindow()),
      own_window_(true),
      x_window_(connection_->CreateDummyWindow("Chromium Drag & Drop Window")),
      source_window_(x_window_),
      selection_owner_(connection_.get(),
                       x_window_,
                       x11::GetAtom(kDndSelection)) {}

XOSExchangeDataProvider::~XOSExchangeDataProvider() {
  if (own_window_) {
    connection_->DestroyWindow({x_window_});
  }
}

void XOSExchangeDataProvider::TakeOwnershipOfSelection() const {
  selection_owner_.TakeOwnershipOfSelection(format_map_);
}

void XOSExchangeDataProvider::RetrieveTargets(
    std::vector<x11::Atom>* targets) const {
  selection_owner_.RetrieveTargets(targets);
}

SelectionFormatMap XOSExchangeDataProvider::GetFormatMap() const {
  // We return the |selection_owner_|'s format map instead of our own in case
  // ours has been modified since TakeOwnershipOfSelection() was called.
  return selection_owner_.selection_format_map();
}

std::unique_ptr<OSExchangeDataProvider> XOSExchangeDataProvider::Clone() const {
  std::unique_ptr<XOSExchangeDataProvider> ret(new XOSExchangeDataProvider());
  ret->set_format_map(format_map());
  return std::move(ret);
}

void XOSExchangeDataProvider::MarkRendererTaintedFromOrigin(
    const url::Origin& origin) {
  format_map_.Insert(x11::GetAtom(kRendererTaint),
                     base::MakeRefCounted<base::RefCountedString>(
                         origin.opaque() ? std::string() : origin.Serialize()));
}

bool XOSExchangeDataProvider::IsRendererTainted() const {
  return format_map_.find(x11::GetAtom(kRendererTaint)) != format_map_.end();
}

std::optional<url::Origin> XOSExchangeDataProvider::GetRendererTaintedOrigin()
    const {
  ui::SelectionData data = format_map_.Get(x11::GetAtom(kRendererTaint));
  if (!data.IsValid()) {
    return std::nullopt;
  }

  std::string data_as_string;
  data.AssignTo(&data_as_string);
  if (data_as_string.empty()) {
    return url::Origin();
  }

  return url::Origin::Create(GURL(data_as_string));
}

void XOSExchangeDataProvider::MarkAsFromPrivileged() {
  format_map_.Insert(
      x11::GetAtom(kFromPrivileged),
      scoped_refptr<base::RefCountedMemory>(
          base::MakeRefCounted<base::RefCountedString>(std::string())));
}

bool XOSExchangeDataProvider::IsFromPrivileged() const {
  return format_map_.find(x11::GetAtom(kFromPrivileged)) != format_map_.end();
}

void XOSExchangeDataProvider::SetString(const std::u16string& text_data) {
  if (HasString()) {
    return;
  }

  scoped_refptr<base::RefCountedMemory> mem(
      base::MakeRefCounted<base::RefCountedString>(
          base::UTF16ToUTF8(text_data)));

  format_map_.Insert(x11::GetAtom(kMimeTypeText), mem);
  format_map_.Insert(x11::GetAtom(kMimeTypeLinuxText), mem);
  format_map_.Insert(x11::GetAtom(kMimeTypeLinuxString), mem);
  format_map_.Insert(x11::GetAtom(kMimeTypeLinuxUtf8String), mem);
}

void XOSExchangeDataProvider::SetURL(const GURL& url,
                                     const std::u16string& title) {
  // TODO(dcheng): The original GTK code tries very hard to avoid writing out an
  // empty title. Is this necessary?
  if (url.is_valid()) {
    // Mozilla's URL format: (UTF16: URL, newline, title)
    std::u16string spec = base::UTF8ToUTF16(url.spec());

    std::vector<unsigned char> data;
    ui::AddString16ToVector(spec, &data);
    ui::AddString16ToVector(u"\n", &data);
    ui::AddString16ToVector(title, &data);
    scoped_refptr<base::RefCountedMemory> mem(
        base::RefCountedBytes::TakeVector(&data));

    format_map_.Insert(x11::GetAtom(kMimeTypeMozillaURL), mem);

    // Set a string fallback as well.
    SetString(spec);

    // Return early if this drag already contains file contents (this implies
    // that file contents must be populated before URLs). Nautilus (and possibly
    // other file managers) prefer _NETSCAPE_URL over the X Direct Save
    // protocol, but we want to prioritize XDS in this case.
    if (!file_contents_name_.empty()) {
      return;
    }

    // Set _NETSCAPE_URL for file managers like Nautilus that use it as a hint
    // to create a link to the URL. Setting text/uri-list doesn't work because
    // Nautilus will fetch and copy the contents of the URL to the drop target
    // instead of linking...
    // Format is UTF8: URL + "\n" + title.
    std::string netscape_url = url.spec();
    netscape_url += "\n";
    netscape_url += base::UTF16ToUTF8(title);
    format_map_.Insert(x11::GetAtom(kNetscapeURL),
                       scoped_refptr<base::RefCountedMemory>(
                           base::MakeRefCounted<base::RefCountedString>(
                               std::move(netscape_url))));
  }
}

void XOSExchangeDataProvider::SetFilename(const base::FilePath& path) {
  std::vector<FileInfo> data;
  data.emplace_back(path, base::FilePath());
  SetFilenames(data);
}

void XOSExchangeDataProvider::SetFilenames(
    const std::vector<FileInfo>& filenames) {
  std::vector<std::string> paths;
  for (const auto& filename : filenames) {
    std::string url_spec = net::FilePathToFileURL(filename.path).spec();
    if (!url_spec.empty()) {
      paths.push_back(url_spec);
    }
  }

  scoped_refptr<base::RefCountedMemory> mem(
      base::MakeRefCounted<base::RefCountedString>(
          base::JoinString(paths, "\n")));
  format_map_.Insert(x11::GetAtom(kMimeTypeURIList), mem);
}

void XOSExchangeDataProvider::SetPickledData(const ClipboardFormatType& format,
                                             const base::Pickle& pickle) {
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(pickle.data());

  std::vector<unsigned char> bytes;
  bytes.insert(bytes.end(), data, data + pickle.size());
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedBytes::TakeVector(&bytes));

  format_map_.Insert(x11::GetAtom(format.GetName().c_str()), mem);
}

std::optional<std::u16string> XOSExchangeDataProvider::GetString() const {
  if (HasFile()) {
    // Various Linux file managers both pass a list of file:// URIs and set the
    // string representation to the URI. We explicitly don't want to return use
    // this representation.
    return std::nullopt;
  }

  std::vector<x11::Atom> text_atoms = ui::GetTextAtomsFrom();
  std::vector<x11::Atom> requested_types;
  GetAtomIntersection(text_atoms, GetTargets(), &requested_types);

  ui::SelectionData data = format_map_.GetFirstOf(requested_types);
  if (data.IsValid()) {
    return base::UTF8ToUTF16(data.GetText());
  }

  return std::nullopt;
}

std::optional<OSExchangeDataProvider::UrlInfo>
XOSExchangeDataProvider::GetURLAndTitle(FilenameToURLPolicy policy) const {
  std::vector<x11::Atom> url_atoms = ui::GetURLAtomsFrom();
  std::vector<x11::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  ui::SelectionData data = format_map_.GetFirstOf(requested_types);
  if (data.IsValid()) {
    // TODO(erg): Technically, both of these forms can accept multiple URLs,
    // but that doesn't match the assumptions of the rest of the system which
    // expect single types.

    if (data.GetType() == x11::GetAtom(kMimeTypeMozillaURL)) {
      // Mozilla URLs are (UTF16: URL, newline, title).
      std::u16string unparsed;
      data.AssignTo(&unparsed);

      std::vector<std::u16string> tokens = base::SplitString(
          unparsed, u"\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (tokens.size() > 0) {
        GURL url = GURL(tokens[0]);
        if (!url.is_valid()) {
          return std::nullopt;
        }
        return UrlInfo{std::move(url), tokens.size() > 1 ? std::move(tokens[1])
                                                         : std::u16string()};
      }
    } else if (data.GetType() == x11::GetAtom(kMimeTypeURIList)) {
      std::vector<std::string> tokens = ui::ParseURIList(data);
      for (const std::string& token : tokens) {
        GURL test_url(token);
        if (!test_url.is_valid()) {
          continue;
        }
        if (!test_url.SchemeIsFile() ||
            policy == FilenameToURLPolicy::CONVERT_FILENAMES) {
          return UrlInfo{std::move(test_url), std::u16string()};
        }
      }
    }
  }

  return std::nullopt;
}

std::optional<std::vector<GURL>> XOSExchangeDataProvider::GetURLs(
    FilenameToURLPolicy policy) const {
  std::vector<GURL> local_urls;

  ui::SelectionData data = format_map_.Get(x11::GetAtom(kMimeTypeURIList));
  if (data.IsValid()) {
    std::vector<std::string> tokens = ui::ParseURIList(data);
    for (const std::string& token : tokens) {
      GURL test_url(token);
      if (!test_url.SchemeIsFile() ||
          policy == FilenameToURLPolicy::CONVERT_FILENAMES) {
        local_urls.push_back(test_url);
      }
    }
  }

  data = format_map_.Get(x11::GetAtom(kMimeTypeMozillaURL));
  if (data.IsValid()) {
    std::u16string unparsed;
    data.AssignTo(&unparsed);

    // Mozilla URLs are (UTF16: URL, newline, title).
    std::vector<std::u16string> tokens = base::SplitString(
        unparsed, u"\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (tokens.size() > 0) {
      GURL url(tokens[0]);
      if (!base::Contains(local_urls, url)) {
        local_urls.push_back(url);
      }
    }
  }

  if (local_urls.size()) {
    return local_urls;
  }
  return std::nullopt;
}

std::optional<std::vector<FileInfo>> XOSExchangeDataProvider::GetFilenames()
    const {
  std::vector<x11::Atom> url_atoms = ui::GetURIListAtomsFrom();
  std::vector<x11::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  ui::SelectionData data = format_map_.GetFirstOf(requested_types);
  if (!data.IsValid()) {
    return std::nullopt;
  }

  std::vector<FileInfo> filenames;
  std::vector<std::string> tokens = ui::ParseURIList(data);
  for (const std::string& token : tokens) {
    GURL url(token);
    base::FilePath file_path;
    if (url.SchemeIsFile() && net::FileURLToFilePath(url, &file_path)) {
      filenames.emplace_back(file_path, base::FilePath());
    }
  }

  return filenames;
}

std::optional<base::Pickle> XOSExchangeDataProvider::GetPickledData(
    const ClipboardFormatType& format) const {
  ui::SelectionData data =
      format_map_.Get(x11::GetAtom(format.GetName().c_str()));
  if (!data.IsValid()) {
    return std::nullopt;
  }

  return base::Pickle::WithData(base::span(data.GetData(), data.GetSize()));
}

bool XOSExchangeDataProvider::HasString() const {
  std::vector<x11::Atom> text_atoms = ui::GetTextAtomsFrom();
  std::vector<x11::Atom> requested_types;
  GetAtomIntersection(text_atoms, GetTargets(), &requested_types);
  return !requested_types.empty() && !HasFile();
}

bool XOSExchangeDataProvider::HasURL(FilenameToURLPolicy policy) const {
  std::vector<x11::Atom> url_atoms = ui::GetURLAtomsFrom();
  std::vector<x11::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  if (requested_types.empty()) {
    return false;
  }

  // The Linux desktop doesn't differentiate between files and URLs like
  // Windows does and stuffs all the data into one mime type.
  ui::SelectionData data = format_map_.GetFirstOf(requested_types);
  if (data.IsValid()) {
    if (data.GetType() == x11::GetAtom(kMimeTypeMozillaURL)) {
      // File managers shouldn't be using this type, so this is a URL.
      return true;
    } else if (data.GetType() == x11::GetAtom(ui::kMimeTypeURIList)) {
      std::vector<std::string> tokens = ui::ParseURIList(data);
      for (const std::string& token : tokens) {
        if (!GURL(token).SchemeIsFile() ||
            policy == FilenameToURLPolicy::CONVERT_FILENAMES) {
          return true;
        }
      }

      return false;
    }
  }

  return false;
}

bool XOSExchangeDataProvider::HasFile() const {
  std::vector<x11::Atom> url_atoms = ui::GetURIListAtomsFrom();
  std::vector<x11::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  if (requested_types.empty()) {
    return false;
  }

  // To actually answer whether we have a file, we need to look through the
  // contents of the kMimeTypeURIList type, and see if any of them are file://
  // URIs.
  ui::SelectionData data = format_map_.GetFirstOf(requested_types);
  if (data.IsValid()) {
    std::vector<std::string> tokens = ui::ParseURIList(data);
    for (const std::string& token : tokens) {
      GURL url(token);
      base::FilePath file_path;
      if (url.SchemeIsFile() && net::FileURLToFilePath(url, &file_path)) {
        return true;
      }
    }
  }

  return false;
}

bool XOSExchangeDataProvider::HasCustomFormat(
    const ClipboardFormatType& format) const {
  std::vector<x11::Atom> url_atoms;
  url_atoms.push_back(x11::GetAtom(format.GetName().c_str()));
  std::vector<x11::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  return !requested_types.empty();
}

void XOSExchangeDataProvider::SetFileContents(
    const base::FilePath& filename,
    const std::string& file_contents) {
  DCHECK(!filename.empty());
  DCHECK(!base::Contains(format_map(), x11::GetAtom(kMimeTypeMozillaURL)));
  set_file_contents_name(filename);
  // Direct save handling is a complicated juggling affair between this class,
  // SelectionFormat, and XDragDropClient. The general idea behind
  // the protocol is this:
  // - The source window sets its XdndDirectSave0 window property to the
  //   proposed filename.
  // - When a target window receives the drop, it updates the XdndDirectSave0
  //   property on the source window to the filename it would like the contents
  //   to be saved to and then requests the XdndDirectSave0 type from the
  //   source.
  // - The source is supposed to copy the file here and return success (S),
  //   failure (F), or error (E).
  // - In this case, failure means the destination should try to populate the
  //   file itself by copying the data from application/octet-stream. To make
  //   things simpler for Chrome, we always 'fail' and let the destination do
  //   the work.
  InsertData(
      x11::GetAtom(kXdndDirectSave0),
      scoped_refptr<base::RefCountedMemory>(
          base::MakeRefCounted<base::RefCountedString>(std::string("F"))));
  InsertData(x11::GetAtom(kMimeTypeOctetStream),
             scoped_refptr<base::RefCountedMemory>(
                 base::MakeRefCounted<base::RefCountedString>(file_contents)));
}

std::optional<OSExchangeDataProvider::FileContentsInfo>
XOSExchangeDataProvider::GetFileContents() const {
  std::vector<char> str;
  if (!connection_->GetArrayProperty(source_window_,
                                     x11::GetAtom(kXdndDirectSave0), &str)) {
    return std::nullopt;
  }

  base::FilePath filename =
      base::FilePath(base::FilePath::StringPieceType(str.data(), str.size()));
  if (filename.empty()) {
    return std::nullopt;
  }

  std::vector<x11::Atom> file_contents_atoms;
  file_contents_atoms.push_back(x11::GetAtom(kMimeTypeOctetStream));
  std::vector<x11::Atom> requested_types;
  GetAtomIntersection(file_contents_atoms, GetTargets(), &requested_types);

  ui::SelectionData data = format_map_.GetFirstOf(requested_types);
  if (data.IsValid()) {
    std::string file_contents;
    data.AssignTo(&file_contents);
    return FileContentsInfo{.filename = filename,
                            .file_contents = std::move(file_contents)};
  }
  return std::nullopt;
}

bool XOSExchangeDataProvider::HasFileContents() const {
  NOTIMPLEMENTED();
  return false;
}

void XOSExchangeDataProvider::SetHtml(const std::u16string& html,
                                      const GURL& base_url) {
  std::vector<unsigned char> bytes;
  // Manually jam a UTF16 BOM into bytes because otherwise, other programs will
  // assume UTF-8.
  bytes.push_back(0xFF);
  bytes.push_back(0xFE);
  ui::AddString16ToVector(html, &bytes);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedBytes::TakeVector(&bytes));

  format_map_.Insert(x11::GetAtom(kMimeTypeHTML), mem);
}

std::optional<OSExchangeDataProvider::HtmlInfo>
XOSExchangeDataProvider::GetHtml() const {
  std::vector<x11::Atom> url_atoms;
  url_atoms.push_back(x11::GetAtom(kMimeTypeHTML));
  std::vector<x11::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  ui::SelectionData data = format_map_.GetFirstOf(requested_types);
  if (!data.IsValid()) {
    return std::nullopt;
  }

  return HtmlInfo{
      .html = data.GetHtml(),
      .base_url = GURL(),
  };
}

bool XOSExchangeDataProvider::HasHtml() const {
  std::vector<x11::Atom> url_atoms;
  url_atoms.push_back(x11::GetAtom(kMimeTypeHTML));
  std::vector<x11::Atom> requested_types;
  GetAtomIntersection(url_atoms, GetTargets(), &requested_types);

  return !requested_types.empty();
}

void XOSExchangeDataProvider::SetDragImage(const gfx::ImageSkia& image,
                                           const gfx::Vector2d& cursor_offset) {
  drag_image_ = image;
  drag_image_offset_ = cursor_offset;
}

gfx::ImageSkia XOSExchangeDataProvider::GetDragImage() const {
  return drag_image_;
}

gfx::Vector2d XOSExchangeDataProvider::GetDragImageOffset() const {
  return drag_image_offset_;
}

std::vector<x11::Atom> XOSExchangeDataProvider::GetTargets() const {
  return format_map_.GetTypes();
}

void XOSExchangeDataProvider::InsertData(
    x11::Atom format,
    const scoped_refptr<base::RefCountedMemory>& data) {
  format_map_.Insert(format, data);
}

void XOSExchangeDataProvider::SetSource(
    std::unique_ptr<DataTransferEndpoint> data_source) {}

DataTransferEndpoint* XOSExchangeDataProvider::GetSource() const {
  return nullptr;
}

}  // namespace ui
