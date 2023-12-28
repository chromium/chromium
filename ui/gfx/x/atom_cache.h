// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_ATOM_CACHE_H_
#define UI_GFX_X_ATOM_CACHE_H_

#include <cstring>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

class Connection;

// Gets the X atom for default display corresponding to atom_name.
// TODO(thomasanderson): This should be removed and usages replaced with
// Connection::GetAtom().
COMPONENT_EXPORT(X11) Atom GetAtom(const char* atom_name);

// Pre-caches all Atoms on first use to minimize round trips to the X11
// server. GetAtom() will CHECK() that atoms accessed
// through GetAtom() are not predefined.
class AtomCache {
 public:
  explicit AtomCache(Connection* connection);

  AtomCache(const AtomCache&&) = delete;
  AtomCache& operator=(const AtomCache&&) = delete;

  ~AtomCache();

  // Returns the pre-interned Atom without having to go to the x server.
  // On failure, None is returned.
  Atom GetAtom(const char* name);

 private:
  struct Compare {
    bool operator()(const char* lhs, const char* rhs) const {
      return strcmp(lhs, rhs) < 0;
    }
  };

  raw_ptr<Connection> connection_;

  // Using base::flat_map, since accesses are common, but inserts are very rare.
  base::flat_map<const char*, Atom, Compare> cached_atoms_;

  // Most strings in `cached_atoms_` have static lifetime.  This vector owns
  // the ones that aren't.  It should be empty in most cases.
  std::vector<std::unique_ptr<std::string>> owned_strings_;
};

}  // namespace x11

#endif  // UI_GFX_X_ATOM_CACHE_H_
