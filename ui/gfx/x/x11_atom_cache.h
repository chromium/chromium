// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_X11_ATOM_CACHE_H_
#define UI_GFX_X_X11_ATOM_CACHE_H_

#include <map>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/x/xproto.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace x11 {
class Connection;
}

namespace x11 {

// Gets the X atom for default display corresponding to atom_name.
COMPONENT_EXPORT(X11) Atom GetAtom(const std::string& atom_name);

// Pre-caches all Atoms on first use to minimize roundtrips to the X11
// server. By default, GetAtom() will CHECK() that atoms accessed through
// GetAtom() were passed to the constructor, but this behaviour can be changed
// with allow_uncached_atoms().
class COMPONENT_EXPORT(X11) X11AtomCache {
 public:
  static X11AtomCache* GetInstance();

  X11AtomCache(const X11AtomCache&) = delete;
  X11AtomCache& operator=(const X11AtomCache&) = delete;

 private:
  friend Atom GetAtom(const std::string& atom_name);
  friend struct base::DefaultSingletonTraits<X11AtomCache>;

  X11AtomCache();
  ~X11AtomCache();

  // Returns the pre-interned Atom without having to go to the x server.
  // On failure, None is returned.
  Atom GetAtom(const std::string&) const;

  raw_ptr<Connection> connection_;

  // Using std::map, as it is possible for thousands of atoms to be registered.
  mutable std::map<std::string, Atom> cached_atoms_;
};

}  // namespace x11

#endif  // UI_GFX_X_X11_ATOM_CACHE_H_
