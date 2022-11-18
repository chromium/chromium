// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_cursor_loader.h"

#include <dlfcn.h>

#include <limits>
#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_util.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#endif

extern "C" {
const char* XcursorLibraryPath(void);
}

namespace ui {

namespace {

// These cursor names are indexed by their ID in a cursor font.
constexpr const char* cursor_names[] = {
    "X_cursor",
    "arrow",
    "based_arrow_down",
    "based_arrow_up",
    "boat",
    "bogosity",
    "bottom_left_corner",
    "bottom_right_corner",
    "bottom_side",
    "bottom_tee",
    "box_spiral",
    "center_ptr",
    "circle",
    "clock",
    "coffee_mug",
    "cross",
    "cross_reverse",
    "crosshair",
    "diamond_cross",
    "dot",
    "dotbox",
    "double_arrow",
    "draft_large",
    "draft_small",
    "draped_box",
    "exchange",
    "fleur",
    "gobbler",
    "gumby",
    "hand1",
    "hand2",
    "heart",
    "icon",
    "iron_cross",
    "left_ptr",
    "left_side",
    "left_tee",
    "leftbutton",
    "ll_angle",
    "lr_angle",
    "man",
    "middlebutton",
    "mouse",
    "pencil",
    "pirate",
    "plus",
    "question_arrow",
    "right_ptr",
    "right_side",
    "right_tee",
    "rightbutton",
    "rtl_logo",
    "sailboat",
    "sb_down_arrow",
    "sb_h_double_arrow",
    "sb_left_arrow",
    "sb_right_arrow",
    "sb_up_arrow",
    "sb_v_double_arrow",
    "shuttle",
    "sizing",
    "spider",
    "spraycan",
    "star",
    "target",
    "tcross",
    "top_left_arrow",
    "top_left_corner",
    "top_right_corner",
    "top_side",
    "top_tee",
    "trek",
    "ul_angle",
    "umbrella",
    "ur_angle",
    "watch",
    "xterm",
};

std::string GetEnv(const std::string& var) {
  auto env = base::Environment::Create();
  std::string value;
  env->GetVar(var, &value);
  return value;
}

NO_SANITIZE("cfi-icall")
std::string CursorPathFromLibXcursor() {
  struct DlCloser {
    void operator()(void* ptr) const { dlclose(ptr); }
  };

  std::unique_ptr<void, DlCloser> lib(dlopen("libXcursor.so.1", RTLD_LAZY));
  if (!lib)
    return "";

  if (auto* sym = reinterpret_cast<decltype(&XcursorLibraryPath)>(
          dlsym(lib.get(), "XcursorLibraryPath"))) {
    if (const char* path = sym())
      return path;
  }
  return "";
}

std::string CursorPathImpl() {
  constexpr const char kDefaultPath[] =
      "~/.local/share/icons:~/.icons:/usr/share/icons:/usr/share/pixmaps:"
      "/usr/X11R6/lib/X11/icons";

  auto libxcursor_path = CursorPathFromLibXcursor();
  if (!libxcursor_path.empty())
    return libxcursor_path;

  std::string path = GetEnv("XCURSOR_PATH");
  return path.empty() ? kDefaultPath : path;
}

const std::string& CursorPath() {
  static base::NoDestructor<std::string> path(CursorPathImpl());
  return *path;
}

x11::Render::PictFormat GetRenderARGBFormat(
    const x11::Render::QueryPictFormatsReply& formats) {
  for (const auto& format : formats.formats) {
    if (format.type == x11::Render::PictType::Direct && format.depth == 32 &&
        format.direct.alpha_shift == 24 && format.direct.alpha_mask == 0xff &&
        format.direct.red_shift == 16 && format.direct.red_mask == 0xff &&
        format.direct.green_shift == 8 && format.direct.green_mask == 0xff &&
        format.direct.blue_shift == 0 && format.direct.blue_mask == 0xff) {
      return format.id;
    }
  }
  return {};
}

std::vector<std::string> GetBaseThemes(const base::FilePath& abspath) {
  DCHECK(abspath.IsAbsolute());
  constexpr const char kKeyInherits[] = "Inherits";
  std::string contents;
  base::ReadFileToString(abspath, &contents);
  base::StringPairs pairs;
  base::SplitStringIntoKeyValuePairs(contents, '=', '\n', &pairs);
  for (const auto& pair : pairs) {
    if (base::TrimWhitespaceASCII(pair.first, base::TRIM_ALL) == kKeyInherits) {
      return base::SplitString(pair.second, ",;", base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    }
  }
  return {};
}

base::FilePath CanonicalizePath(base::FilePath path) {
  std::vector<std::string> components = path.GetComponents();
  if (components[0] == "~") {
    path = base::GetHomeDir();
    for (size_t i = 1; i < components.size(); i++)
      path = path.Append(components[i]);
  } else {
    path = base::MakeAbsoluteFilePath(path);
  }
  return path;
}

// Reads the cursor called |name| for the theme named |theme|. Searches  all
// paths in the XCursor path and parent themes.
scoped_refptr<base::RefCountedMemory> ReadCursorFromTheme(
    const std::string& theme,
    const std::string& name) {
  constexpr const char kCursorDir[] = "cursors";
  constexpr const char kThemeInfo[] = "index.theme";
  std::vector<std::string> base_themes;

  auto paths = base::SplitString(CursorPath(), ":", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
  for (const auto& path : paths) {
    auto dir = CanonicalizePath(base::FilePath(path));
    if (dir.empty())
      continue;
    base::FilePath theme_dir = dir.Append(theme);
    base::FilePath cursor_dir = theme_dir.Append(kCursorDir);

    std::string contents;
    if (base::ReadFileToString(cursor_dir.Append(name), &contents))
      return base::MakeRefCounted<base::RefCountedString>(std::move(contents));

    if (base_themes.empty())
      base_themes = GetBaseThemes(theme_dir.Append(kThemeInfo));
  }

  for (const auto& path : base_themes) {
    if (auto contents = ReadCursorFromTheme(path, name))
      return contents;
  }

  return nullptr;
}

scoped_refptr<base::RefCountedMemory> ReadCursorFile(
    const std::string& name,
    const std::string& rm_xcursor_theme) {
  constexpr const char kDefaultTheme[] = "default";
  std::string themes[] = {
#if BUILDFLAG(IS_LINUX)
    // The toolkit theme has the highest priority.
    LinuxUi::instance() ? LinuxUi::instance()->GetCursorThemeName()
                        : std::string(),
#endif

    // Next try Xcursor.theme.
    rm_xcursor_theme,

    // As a last resort, use the default theme.
    kDefaultTheme,
  };

  for (const std::string& theme : themes) {
    if (theme.empty())
      continue;
    if (auto file = ReadCursorFromTheme(theme, name))
      return file;
  }
  return nullptr;
}

std::vector<XCursorLoader::Image> ReadCursorImages(
    const std::vector<std::string>& names,
    const std::string& rm_xcursor_theme,
    uint32_t preferred_size) {
  // Fallback on a left pointer if possible.
  auto names_copy = names;
  names_copy.push_back("left_ptr");
  for (const auto& name : names_copy) {
    if (auto contents = ReadCursorFile(name, rm_xcursor_theme)) {
      auto images = ParseCursorFile(contents, preferred_size);
      if (!images.empty())
        return images;
    }
  }
  return {};
}

}  // namespace

XCursorLoader::XCursorLoader(x11::Connection* connection)
    : connection_(connection) {
  auto ver_cookie = connection_->render().QueryVersion(
      {x11::Render::major_version, x11::Render::minor_version});
  auto pf_cookie = connection_->render().QueryPictFormats();
  cursor_font_ = connection_->GenerateId<x11::Font>();
  connection_->OpenFont({cursor_font_, "cursor"});

  std::vector<char> resource_manager;
  if (GetArrayProperty(connection_->default_root(), x11::Atom::RESOURCE_MANAGER,
                       &resource_manager)) {
    ParseXResources(
        base::StringPiece(resource_manager.data(), resource_manager.size()));
  }

  if (auto reply = ver_cookie.Sync()) {
    render_version_ =
        base::Version({reply->major_version, reply->minor_version});
  }

  if (auto pf_reply = pf_cookie.Sync())
    pict_format_ = GetRenderARGBFormat(*pf_reply.reply);

  for (uint16_t i = 0; i < std::size(cursor_names); i++)
    cursor_name_to_char_[cursor_names[i]] = i;
}

XCursorLoader::~XCursorLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<X11Cursor> XCursorLoader::LoadCursor(
    const std::vector<std::string>& names) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto cursor = base::MakeRefCounted<X11Cursor>();
  if (SupportsCreateCursor()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(ReadCursorImages, names, rm_xcursor_theme_,
                       GetPreferredCursorSize()),
        base::BindOnce(&XCursorLoader::LoadCursorImpl,
                       weak_factory_.GetWeakPtr(), cursor, names));
  } else {
    LoadCursorImpl(cursor, names, {});
  }
  return cursor;
}

scoped_refptr<X11Cursor> XCursorLoader::CreateCursor(
    const std::vector<Image>& images) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<scoped_refptr<X11Cursor>> cursors;
  std::vector<x11::Render::AnimationCursorElement> elements;
  cursors.reserve(images.size());
  elements.reserve(images.size());

  for (const Image& image : images) {
    auto cursor = CreateCursor(image.bitmap, image.hotspot);
    cursors.push_back(cursor);
    elements.push_back(x11::Render::AnimationCursorElement{
        cursor->xcursor_,
        static_cast<uint32_t>(image.frame_delay.InMilliseconds())});
  }

  if (elements.empty())
    return nullptr;
  if (elements.size() == 1 || !SupportsCreateAnimCursor())
    return cursors[0];

  auto cursor = connection_->GenerateId<x11::Cursor>();
  connection_->render().CreateAnimCursor({cursor, elements});
  return base::MakeRefCounted<X11Cursor>(cursor);
}

scoped_refptr<X11Cursor> XCursorLoader::CreateCursor(
    const SkBitmap& bitmap,
    const gfx::Point& hotspot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto pixmap = connection_->GenerateId<x11::Pixmap>();
  auto gc = connection_->GenerateId<x11::GraphicsContext>();
  uint16_t width = bitmap.width();
  uint16_t height = bitmap.height();
  connection_->CreatePixmap(
      {32, pixmap, connection_->default_root(), width, height});
  connection_->CreateGC({gc, pixmap});

  size_t size = bitmap.computeByteSize();
  std::vector<uint8_t> vec(size);
  memcpy(vec.data(), bitmap.getPixels(), size);
  auto* connection = x11::Connection::Get();
  x11::PutImageRequest put_image_request{
      .format = x11::ImageFormat::ZPixmap,
      .drawable = pixmap,
      .gc = gc,
      .width = width,
      .height = height,
      .depth = 32,
      .data = base::RefCountedBytes::TakeVector(&vec),
  };
  connection->PutImage(put_image_request);

  x11::Render::Picture pic = connection_->GenerateId<x11::Render::Picture>();
  connection_->render().CreatePicture({pic, pixmap, pict_format_});

  auto cursor = connection_->GenerateId<x11::Cursor>();
  connection_->render().CreateCursor({cursor, pic,
                                      static_cast<uint16_t>(hotspot.x()),
                                      static_cast<uint16_t>(hotspot.y())});

  connection_->render().FreePicture({pic});
  connection_->FreePixmap({pixmap});
  connection_->FreeGC({gc});

  return base::MakeRefCounted<X11Cursor>(cursor);
}

void XCursorLoader::LoadCursorImpl(
    scoped_refptr<X11Cursor> cursor,
    const std::vector<std::string>& names,
    const std::vector<XCursorLoader::Image>& images) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto xcursor = connection_->GenerateId<x11::Cursor>();
  if (!images.empty()) {
    xcursor = CreateCursor(images)->ReleaseCursor();
  } else {
    // Fallback to using a font cursor.
    auto core_char = CursorNamesToChar(names);
    constexpr uint16_t kFontCursorFgColor = 0;
    constexpr uint16_t kFontCursorBgColor = 65535;
    connection_->CreateGlyphCursor({xcursor, cursor_font_, cursor_font_,
                                    static_cast<uint16_t>(2 * core_char),
                                    static_cast<uint16_t>(2 * core_char + 1),
                                    kFontCursorFgColor, kFontCursorFgColor,
                                    kFontCursorFgColor, kFontCursorBgColor,
                                    kFontCursorBgColor, kFontCursorBgColor});
  }
  cursor->SetCursor(xcursor);
}

uint32_t XCursorLoader::GetPreferredCursorSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  constexpr const char kXcursorSizeEnv[] = "XCURSOR_SIZE";
  constexpr unsigned int kCursorSizeInchNum = 16;
  constexpr unsigned int kCursorSizeInchDen = 72;
  constexpr unsigned int kScreenCursorRatio = 48;

  // Allow the XCURSOR_SIZE environment variable to override GTK settings.
  int size;
  if (base::StringToInt(GetEnv(kXcursorSizeEnv), &size) && size > 0)
    return size;

#if BUILDFLAG(IS_LINUX)
  // Let the toolkit have the next say.
  auto* linux_ui = LinuxUi::instance();
  size = linux_ui ? linux_ui->GetCursorThemeSize() : 0;
  if (size > 0)
    return size;
#endif

  // Use Xcursor.size from RESOURCE_MANAGER if available.
  if (rm_xcursor_size_)
    return rm_xcursor_size_;

  // Guess the cursor size based on the DPI.
  if (rm_xft_dpi_)
    return rm_xft_dpi_ * kCursorSizeInchNum / kCursorSizeInchDen;

  // As a last resort, guess the cursor size based on the screen size.
  const auto& screen = connection_->default_screen();
  return std::min(screen.width_in_pixels, screen.height_in_pixels) /
         kScreenCursorRatio;
}

void XCursorLoader::ParseXResources(base::StringPiece resources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::StringPairs pairs;
  base::SplitStringIntoKeyValuePairs(resources, ':', '\n', &pairs);
  for (const auto& pair : pairs) {
    auto key = base::TrimWhitespaceASCII(pair.first, base::TRIM_ALL);
    auto value = base::TrimWhitespaceASCII(pair.second, base::TRIM_ALL);

    if (key == "Xcursor.theme")
      rm_xcursor_theme_ = std::string(value);
    else if (key == "Xcursor.size")
      base::StringToUint(value, &rm_xcursor_size_);
    else if (key == "Xft.dpi")
      base::StringToUint(value, &rm_xft_dpi_);
  }
}

uint16_t XCursorLoader::CursorNamesToChar(
    const std::vector<std::string>& names) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& name : names) {
    auto it = cursor_name_to_char_.find(name);
    if (it != cursor_name_to_char_.end())
      return it->second;
  }
  // Use a left pointer as a fallback.
  return 0;
}

bool XCursorLoader::SupportsCreateCursor() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return render_version_.IsValid() && render_version_ >= base::Version("0.5");
}

bool XCursorLoader::SupportsCreateAnimCursor() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return render_version_.IsValid() && render_version_ >= base::Version("0.8");
}

// This is ported from libxcb-cursor's parse_cursor_file.c:
// https://gitlab.freedesktop.org/xorg/lib/libxcb-cursor/-/blob/master/cursor/parse_cursor_file.c
std::vector<XCursorLoader::Image> ParseCursorFile(
    scoped_refptr<base::RefCountedMemory> file,
    uint32_t preferred_size) {
  constexpr uint32_t kMagic = 0x72756358;
  constexpr uint32_t kImageType = 0xfffd0002;

  const uint8_t* mem = file->data();
  size_t offset = 0;

  auto ReadU32s = [&](void* dest, size_t len) {
    DCHECK_EQ(len % 4, 0u);
    if (offset >= file->size() || offset + len > file->size())
      return false;
    const auto* src32 = reinterpret_cast<const uint32_t*>(mem + offset);
    auto* dest32 = reinterpret_cast<uint32_t*>(dest);
    for (size_t i = 0; i < len / 4; i++)
      dest32[i] = base::ByteSwapToLE32(src32[i]);
    offset += len;
    return true;
  };

  struct FileHeader {
    uint32_t magic;
    uint32_t header;
    uint32_t version;
    uint32_t ntoc;
  } header;
  if (!ReadU32s(&header, sizeof(FileHeader)) || header.magic != kMagic)
    return {};

  struct TableOfContentsEntry {
    uint32_t type;
    uint32_t subtype;
    uint32_t position;
  };
  std::vector<TableOfContentsEntry> toc;
  for (uint32_t i = 0; i < header.ntoc; i++) {
    TableOfContentsEntry entry;
    if (!ReadU32s(&entry, sizeof(TableOfContentsEntry)))
      return {};
    toc.push_back(entry);
  }

  uint32_t best_size = std::numeric_limits<uint32_t>::max();
  for (const auto& entry : toc) {
    auto delta = [](uint32_t x, uint32_t y) {
      return std::max(x, y) - std::min(x, y);
    };
    if (entry.type != kImageType)
      continue;
    if (delta(entry.subtype, preferred_size) < delta(best_size, preferred_size))
      best_size = entry.subtype;
  }

  std::vector<XCursorLoader::Image> images;
  for (const auto& entry : toc) {
    if (entry.type != kImageType || entry.subtype != best_size)
      continue;
    offset = entry.position;
    struct ChunkHeader {
      uint32_t header;
      uint32_t type;
      uint32_t subtype;
      uint32_t version;
    } chunk_header;
    if (!ReadU32s(&chunk_header, sizeof(ChunkHeader)) ||
        chunk_header.type != entry.type ||
        chunk_header.subtype != entry.subtype) {
      continue;
    }

    struct ImageHeader {
      uint32_t width;
      uint32_t height;
      uint32_t xhot;
      uint32_t yhot;
      uint32_t delay;
    } image;
    if (!ReadU32s(&image, sizeof(ImageHeader)))
      continue;
    SkBitmap bitmap;
    bitmap.allocN32Pixels(image.width, image.height);
    if (!ReadU32s(bitmap.getPixels(), bitmap.computeByteSize()))
      continue;
    images.push_back(XCursorLoader::Image{bitmap,
                                          gfx::Point(image.xhot, image.yhot),
                                          base::Milliseconds(image.delay)});
  }
  return images;
}

}  // namespace ui
