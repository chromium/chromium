// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_CANON_STDSTRING_H_
#define URL_URL_CANON_STDSTRING_H_

// This header file defines a canonicalizer output method class for STL
// strings. Because the canonicalizer tries not to be dependent on the STL,
// we have segregated it here.

#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "url/url_canon.h"

namespace url {

// Write into a std::string given in the constructor. This object does not own
// the string itself, and the user must ensure that the string stays alive
// throughout the lifetime of this object.
//
// The given string will be appended to; any existing data in the string will
// be preserved.
//
// Note that when canonicalization is complete, the string will likely have
// unused space at the end because we make the string very big to start out
// with (by |initial_size|). This ends up being important because resize
// operations are slow, and because the base class needs to write directly
// into the buffer.
//
// Therefore, the user should call Complete() before using the string that
// this class wrote into.
class COMPONENT_EXPORT(URL) StdStringCanonOutput : public CanonOutput {
 public:
  StdStringCanonOutput(std::string* str);
  ~StdStringCanonOutput() override;

  // Must be called after writing has completed but before the string is used.
  void Complete();

  void Resize(int sz) override;

 protected:
  std::string* str_;
  DISALLOW_COPY_AND_ASSIGN(StdStringCanonOutput);
};

// An extension of the Replacements class that allows the setters to use
// StringPieces (implicitly allowing strings or char*s).
//
// The contents of the StringPieces are not copied and must remain valid until
// the StringPieceReplacements object goes out of scope.
//
// In order to make it harder to misuse the API the setters do not accept rvalue
// references to std::strings.
// Note: Extra const char* overloads are necessary to break ambiguities that
// would otherwise exist for char literals.
template <typename STR>
class StringPieceReplacements : public Replacements<typename STR::value_type> {
 private:
  using CharT = typename STR::value_type;
  using StringPieceT = base::BasicStringPiece<STR>;
  using ParentT = Replacements<CharT>;
  using SetterFun = void (ParentT::*)(const CharT*, const Component&);

  void SetImpl(SetterFun fun, StringPieceT str) {
    (this->*fun)(str.data(), Component(0, static_cast<int>(str.size())));
  }

 public:
  void SetSchemeStr(const CharT* str) { SetImpl(&ParentT::SetScheme, str); }
  void SetSchemeStr(StringPieceT str) { SetImpl(&ParentT::SetScheme, str); }
  void SetSchemeStr(const STR&&) = delete;

  void SetUsernameStr(const CharT* str) { SetImpl(&ParentT::SetUsername, str); }
  void SetUsernameStr(StringPieceT str) { SetImpl(&ParentT::SetUsername, str); }
  void SetUsernameStr(const STR&&) = delete;

  void SetPasswordStr(const CharT* str) { SetImpl(&ParentT::SetPassword, str); }
  void SetPasswordStr(StringPieceT str) { SetImpl(&ParentT::SetPassword, str); }
  void SetPasswordStr(const STR&&) = delete;

  void SetHostStr(const CharT* str) { SetImpl(&ParentT::SetHost, str); }
  void SetHostStr(StringPieceT str) { SetImpl(&ParentT::SetHost, str); }
  void SetHostStr(const STR&&) = delete;

  void SetPortStr(const CharT* str) { SetImpl(&ParentT::SetPort, str); }
  void SetPortStr(StringPieceT str) { SetImpl(&ParentT::SetPort, str); }
  void SetPortStr(const STR&&) = delete;

  void SetPathStr(const CharT* str) { SetImpl(&ParentT::SetPath, str); }
  void SetPathStr(StringPieceT str) { SetImpl(&ParentT::SetPath, str); }
  void SetPathStr(const STR&&) = delete;

  void SetQueryStr(const CharT* str) { SetImpl(&ParentT::SetQuery, str); }
  void SetQueryStr(StringPieceT str) { SetImpl(&ParentT::SetQuery, str); }
  void SetQueryStr(const STR&&) = delete;

  void SetRefStr(const CharT* str) { SetImpl(&ParentT::SetRef, str); }
  void SetRefStr(StringPieceT str) { SetImpl(&ParentT::SetRef, str); }
  void SetRefStr(const STR&&) = delete;
};

}  // namespace url

#endif  // URL_URL_CANON_STDSTRING_H_
