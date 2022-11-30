// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/ipc/url_param_traits.h"

#include <string>

#include "base/pickle.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace IPC {

void ParamTraits<GURL>::Write(base::Pickle* m, const GURL& p) {
  if (p.possibly_invalid_spec().length() > url::kMaxURLChars) {
    m->WriteString(std::string());
    return;
  }

  // Beware of print-parse inconsistency which would change an invalid
  // URL into a valid one. Ideally, the message would contain this flag
  // so that the read side could make the check, but performing it here
  // avoids changing the on-the-wire representation of such a fundamental
  // type as GURL. See https://crbug.com/166486 for additional work in
  // this area.
  if (!p.is_valid()) {
    m->WriteString(std::string());
    return;
  }

  m->WriteString(p.possibly_invalid_spec());
  // TODO(brettw) bug 684583: Add encoding for query params.
}

bool ParamTraits<GURL>::Read(const base::Pickle* m,
                             base::PickleIterator* iter,
                             GURL* p) {
  std::string s;
  if (!iter->ReadString(&s) || s.length() > url::kMaxURLChars) {
    *p = GURL();
    return false;
  }
  *p = GURL(s);
  if (!s.empty() && !p->is_valid()) {
    *p = GURL();
    return false;
  }
  return true;
}

void ParamTraits<GURL>::Log(const GURL& p, std::string* l) {
  l->append(p.spec());
}

}  // namespace IPC
