// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include <iterator>
#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

base::LazyInstance<Clipboard::AllowedThreadsVector>::DestructorAtExit
    Clipboard::allowed_threads_ = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<Clipboard::ClipboardMap>::DestructorAtExit
    Clipboard::clipboard_map_ = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::Lock>::Leaky Clipboard::clipboard_map_lock_ =
    LAZY_INSTANCE_INITIALIZER;

// static
void Clipboard::SetAllowedThreads(
    const std::vector<base::PlatformThreadId>& allowed_threads) {
  base::AutoLock lock(clipboard_map_lock_.Get());

  allowed_threads_.Get().clear();
  std::copy(allowed_threads.begin(), allowed_threads.end(),
            std::back_inserter(allowed_threads_.Get()));
}

// static
void Clipboard::SetClipboardForCurrentThread(
    std::unique_ptr<Clipboard> platform_clipboard) {
  base::AutoLock lock(clipboard_map_lock_.Get());
  base::PlatformThreadId id = Clipboard::GetAndValidateThreadID();

  ClipboardMap* clipboard_map = clipboard_map_.Pointer();
  ClipboardMap::const_iterator it = clipboard_map->find(id);
  if (it != clipboard_map->end()) {
    // This shouldn't happen. The clipboard should not already exist.
    NOTREACHED();
  }
  clipboard_map->insert(std::make_pair(id, std::move(platform_clipboard)));
}

// static
Clipboard* Clipboard::GetForCurrentThread() {
  base::AutoLock lock(clipboard_map_lock_.Get());
  base::PlatformThreadId id = GetAndValidateThreadID();

  ClipboardMap* clipboard_map = clipboard_map_.Pointer();
  ClipboardMap::const_iterator it = clipboard_map->find(id);
  if (it != clipboard_map->end())
    return it->second.get();

  Clipboard* clipboard = Clipboard::Create();
  clipboard_map->insert(std::make_pair(id, base::WrapUnique(clipboard)));
  return clipboard;
}

// static
void Clipboard::OnPreShutdownForCurrentThread() {
  base::AutoLock lock(clipboard_map_lock_.Get());
  base::PlatformThreadId id = GetAndValidateThreadID();

  ClipboardMap* clipboard_map = clipboard_map_.Pointer();
  ClipboardMap::const_iterator it = clipboard_map->find(id);
  if (it != clipboard_map->end())
    it->second->OnPreShutdown();
}

// static
void Clipboard::DestroyClipboardForCurrentThread() {
  base::AutoLock lock(clipboard_map_lock_.Get());

  ClipboardMap* clipboard_map = clipboard_map_.Pointer();
  base::PlatformThreadId id = base::PlatformThread::CurrentId();
  auto it = clipboard_map->find(id);
  if (it != clipboard_map->end())
    clipboard_map->erase(it);
}

base::Time Clipboard::GetLastModifiedTime() const {
  return base::Time();
}

void Clipboard::ClearLastModifiedTime() {}

void Clipboard::DispatchObject(ObjectType type, const ObjectMapParams& params) {
  // Ignore writes with empty parameters.
  for (const auto& param : params) {
    if (param.empty())
      return;
  }

  switch (type) {
    case CBF_TEXT:
      WriteText(&(params[0].front()), params[0].size());
      break;

    case CBF_HTML:
      if (params.size() == 2) {
        if (params[1].empty())
          return;
        WriteHTML(&(params[0].front()), params[0].size(),
                  &(params[1].front()), params[1].size());
      } else if (params.size() == 1) {
        WriteHTML(&(params[0].front()), params[0].size(), NULL, 0);
      }
      break;

    case CBF_RTF:
      WriteRTF(&(params[0].front()), params[0].size());
      break;

    case CBF_BOOKMARK:
      WriteBookmark(&(params[0].front()), params[0].size(),
                    &(params[1].front()), params[1].size());
      break;

    case CBF_WEBKIT:
      WriteWebSmartPaste();
      break;

    case CBF_SMBITMAP: {
      // Usually, the params are just UTF-8 strings. However, for images,
      // ScopedClipboardWriter actually sizes the buffer to sizeof(SkBitmap*),
      // aliases the contents of the vector to a SkBitmap**, and writes the
      // pointer to the actual SkBitmap in the clipboard object param.
      const char* packed_pointer_buffer = &params[0].front();
      WriteBitmap(**reinterpret_cast<SkBitmap* const*>(packed_pointer_buffer));
      break;
    }

    case CBF_DATA:
      WriteData(
          FormatType::Deserialize(
              std::string(&(params[0].front()), params[0].size())),
          &(params[1].front()),
          params[1].size());
      break;

    default:
      NOTREACHED();
  }
}

base::PlatformThreadId Clipboard::GetAndValidateThreadID() {
  clipboard_map_lock_.Get().AssertAcquired();

  const base::PlatformThreadId id = base::PlatformThread::CurrentId();

  // A Clipboard instance must be allocated for every thread that uses the
  // clipboard. To prevented unbounded memory use, CHECK that the current thread
  // was whitelisted to use the clipboard. This is a CHECK rather than a DCHECK
  // to catch incorrect usage in production (e.g. https://crbug.com/872737).
  AllowedThreadsVector* allowed_threads = allowed_threads_.Pointer();
  CHECK(allowed_threads->empty() || base::ContainsValue(*allowed_threads, id));

  return id;
}

}  // namespace ui
