// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/ime_compat_check.h"

#include <dlfcn.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "ui/gfx/x/connection.h"
#include "ui/linux/linux_ui_delegate.h"

// The functions in this file are run before GTK is loaded, so it must not
// depend on any GTK functions or types.

namespace gtk {

namespace {

struct InputMethod {
  std::string_view path;
  std::string_view id;
  std::string_view domain;
  std::vector<std::string_view> locales;
};

std::vector<base::FilePath> GetLibrarySearchPaths() {
  std::vector<base::FilePath> search_path;
  void* handle = dlopen("libc.so.6", RTLD_GLOBAL | RTLD_LAZY | RTLD_NOLOAD);
  if (!handle) {
    return search_path;
  }

  Dl_serinfo serinfo;
  if (dlinfo(handle, RTLD_DI_SERINFOSIZE, &serinfo) == -1) {
    return search_path;
  }

  std::unique_ptr<Dl_serinfo, base::FreeDeleter> sip(
      static_cast<Dl_serinfo*>(malloc(serinfo.dls_size)));

  if (dlinfo(handle, RTLD_DI_SERINFOSIZE, sip.get()) == -1) {
    return search_path;
  }

  if (dlinfo(handle, RTLD_DI_SERINFO, sip.get()) == -1) {
    return search_path;
  }

  for (size_t j = 0; j < serinfo.dls_cnt; j++) {
    // SAFETY: The range is bound by `serinfo.dls_cnt`.
    search_path.emplace_back(UNSAFE_BUFFERS(sip->dls_serpath[j].dls_name));
  }

  return search_path;
}

base::FilePath GetGtk3ImModulesCacheFile() {
  auto env = base::Environment::Create();
  auto module_file_var = env->GetVar("GTK_IM_MODULE_FILE");
  base::FilePath immodules_cache;
  if (module_file_var) {
    immodules_cache = base::FilePath(*module_file_var);
  } else {
    auto gtk_exe_prefix = env->GetVar("GTK_EXE_PREFIX");
    if (gtk_exe_prefix) {
      immodules_cache = base::FilePath(*gtk_exe_prefix)
                            .Append("lib/gtk-3.0/3.0.0/immodules.cache");
    } else {
      for (const auto& libdir : GetLibrarySearchPaths()) {
        base::FilePath path = libdir.Append("gtk-3.0/3.0.0/immodules.cache");
        if (base::PathExists(path)) {
          immodules_cache = path;
          break;
        }
      }
    }
  }
  return immodules_cache;
}

std::vector<std::string_view> ParseImModulesCacheLine(std::string_view line) {
  std::vector<std::string_view> result;
  size_t pos = 0;

  while (true) {
    pos = line.find('"', pos);
    if (pos == std::string_view::npos) {
      break;
    }

    size_t start = pos + 1;
    size_t quote = start;

    // Find the matching closing quote.
    while (true) {
      quote = line.find('"', quote);
      if (quote == std::string_view::npos) {
        // Unmatched quote
        return result;
      }

      if (quote > start && line.substr(quote - 1, 1) == "\\") {
        // If there's a backslash immediately before it, it's escaped.
        ++quote;
      } else {
        // Otherwise, this is the real closing quote.
        result.push_back(line.substr(start, quote - start));
        pos = quote + 1;
        break;
      }
    }
  }

  return result;
}

void ParseImModulesCacheFile(std::string_view contents,
                             std::vector<InputMethod>& ims,
                             std::map<std::string_view, size_t>& im_map) {
  std::string_view current_path;
  for (const auto& line : base::SplitStringPiece(
           contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (line.starts_with("#")) {
      continue;
    }
    auto parts = ParseImModulesCacheLine(line);
    if (parts.size() == 1) {
      current_path = parts[0];
    } else if (parts.size() == 5) {
      ims.emplace_back(
          current_path, parts[0], parts[2],
          base::SplitStringPiece(parts[4], ":", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY));
      im_map[parts[0]] = ims.size() - 1;
    } else {
      LOG(ERROR) << "Invalid immodules.cache line: " << line;
    }
  }
}

std::vector<std::string> GetForcedIms() {
  auto env = base::Environment::Create();
  std::string forced_ims = env->GetVar("GTK_IM_MODULE").value_or(std::string());
  if (auto* connection = x11::Connection::Get()) {
    const auto& resources = connection->GetXResources();
    if (auto it = resources.find("gtk-im-module"); it != resources.end()) {
      forced_ims += ':' + it->second;
    }
  }
  return base::SplitString(forced_ims, ":", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

std::string GetLocale() {
  const char* lc_ctype = setlocale(LC_CTYPE, nullptr);
  std::string locale = lc_ctype ? lc_ctype : "";
  // Remove everything after the first "." or "@".
  size_t pos = locale.find_first_of(".@");
  if (pos != std::string::npos) {
    locale = locale.substr(0, pos);
  }
  return locale.empty() ? "C" : locale;
}

const InputMethod* GetGtk3Im(const std::vector<InputMethod>& ims,
                             const std::map<std::string_view, size_t>& im_map) {
  const InputMethod* gtk3_im = nullptr;
  for (const std::string& im : GetForcedIms()) {
    if (im == "gtk-im-context-simple" || im == "gtk-im-context-none") {
      // GTK4 has these available.
      return nullptr;
    }
    auto it = im_map.find(im);
    if (it != im_map.end()) {
      gtk3_im = &ims[it->second];
      break;
    }
  }

  const std::string locale = GetLocale();
  if (!gtk3_im) {
    int best_score = 0;
    for (const auto& entry : ims) {
      if (entry.id == "wayland" || entry.id == "waylandgtk" ||
          entry.id == "broadway") {
        continue;
      }
      for (std::string_view lc : entry.locales) {
        // This is the scoring that GTK3 IM module loading uses.
        int score = 0;
        if (lc == "*") {
          score = 1;
        } else if (locale == lc) {
          score = 4;
        } else if (locale.substr(0, 2) == lc.substr(0, 2)) {
          score = lc.size() == 2 ? 3 : 2;
        }
        if (score > best_score) {
          best_score = score;
          gtk3_im = &entry;
        }
      }
    }
  }
  return gtk3_im;
}

std::vector<base::FilePath> GetGtk4ImModulePaths() {
  auto env = base::Environment::Create();

  base::FilePath default_dir;
  auto exe_prefix = env->GetVar("GTK_EXE_PREFIX");
  if (exe_prefix) {
    default_dir = base::FilePath(*exe_prefix).Append("lib/gtk-4.0");
  } else {
    for (const auto& libdir : GetLibrarySearchPaths()) {
      base::FilePath path = libdir.Append("gtk-4.0");
      if (base::PathExists(path)) {
        default_dir = path;
        break;
      }
    }
  }

  std::vector<base::FilePath> result;
  auto add_path = [&](const base::FilePath& path) {
    result.emplace_back(path.Append("4.0.0/linux/immodules"));
    result.emplace_back(path.Append("4.0.0/immodules"));
    result.emplace_back(path.Append("linux/immodules"));
    result.emplace_back(path.Append("immodules"));
  };

  if (auto module_path_env = env->GetVar("GTK_PATH")) {
    for (const auto& path :
         base::SplitStringPiece(*module_path_env, "", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY)) {
      add_path(base::FilePath(path));
    }
  }
  if (!default_dir.empty()) {
    add_path(default_dir);
  }
  return result;
}

}  // namespace

bool CheckGtk4X11ImeCompatibility() {
  auto* delegate = ui::LinuxUiDelegate::GetInstance();
  CHECK(delegate);
  if (delegate->GetBackend() != ui::LinuxUiBackend::kX11) {
    // This function is only relevant for X11.
    return true;
  }

  const base::FilePath immodules_cache = GetGtk3ImModulesCacheFile();
  if (!base::PathExists(immodules_cache)) {
    // GTK3 not installed or no immodules.cache file found.
    return true;
  }

  std::string contents;
  base::ReadFileToString(base::FilePath(immodules_cache), &contents);
  std::vector<InputMethod> ims;
  std::map<std::string_view, size_t> im_map;
  ParseImModulesCacheFile(contents, ims, im_map);

  const auto* gtk3_im = GetGtk3Im(ims, im_map);
  if (!gtk3_im) {
    // Using a supported built-in input method, or GTK3 is not installed, or no
    // input method is available. Allow GTK4 to use it's default input method.
    return true;
  }

  const std::string locale = GetLocale();
  if (locale.substr(0, 2) == "ko" && gtk3_im->id == "ibus") {
    // Older versions of IBus are buggy with Korean locales.
    return false;
  }

  if (gtk3_im->domain == "gtk30") {
    // Builtin modules have been removed in GTK4.
    return false;
  }

  auto base_name = base::FilePath(gtk3_im->path).BaseName().value();
  for (const auto& path : GetGtk4ImModulePaths()) {
    base::FilePath gtk4_im = path.Append("lib" + base_name);
    if (base::PathExists(gtk4_im)) {
      // GTK4 has a compatible input method.
      return true;
    }
  }
  return false;
}

}  // namespace gtk
