// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_CANON_STDSTRING_H_
#define URL_URL_CANON_STDSTRING_H_

// This header file defines a canonicalizer output method class for STL
// strings. Because the canonicalizer tries not to be dependent on the STL,
// we have segregated it here.

#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"
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

  StdStringCanonOutput(const StdStringCanonOutput&) = delete;
  StdStringCanonOutput& operator=(const StdStringCanonOutput&) = delete;

  ~StdStringCanonOutput() override;

  // Must be called after writing has completed but before the string is used.
  void Complete();

  void Resize(size_t sz) override;

 protected:
  // `str_` is not a raw_ptr<...> for performance reasons (based on analysis of
  // sampling profiler data and tab_search:top100:2020).
  RAW_PTR_EXCLUSION std::string* str_;
};

// An extension of the Replacements class that allows the setters to use
// string_views (implicitly allowing strings or char*s).
//
// The contents of the string_views are not copied and must remain valid until
// the StringViewReplacements object goes out of scope.
//
// In order to make it harder to misuse the API the setters do not accept rvalue
// references to std::strings.
// Note: Extra const char* overloads are necessary to break ambiguities that
// would otherwise exist for char literals.
template <typename CharT>
class StringViewReplacements : public Replacements<CharT> {
 private:
  using StringT = std::basic_string<CharT>;
  using StringViewT = std::basic_string_view<CharT>;
  using ParentT = Replacements<CharT>;
  using SetterFun = void (ParentT::*)(const CharT*, const Component&);

  void SetImpl(SetterFun fun, StringViewT str) {
    (this->*fun)(str.data(), Component(0, static_cast<int>(str.size())));
  }

 public:
  void SetSchemeStr(const CharT* str) { SetImpl(&ParentT::SetScheme, str); }
  void SetSchemeStr(StringViewT str) { SetImpl(&ParentT::SetScheme, str); }
  void SetSchemeStr(const StringT&&) = delete;

  void SetUsernameStr(const CharT* str) { SetImpl(&ParentT::SetUsername, str); }
  void SetUsernameStr(StringViewT str) { SetImpl(&ParentT::SetUsername, str); }
  void SetUsernameStr(const StringT&&) = delete;
  using ParentT::ClearUsername;

  void SetPasswordStr(const CharT* str) { SetImpl(&ParentT::SetPassword, str); }
  void SetPasswordStr(StringViewT str) { SetImpl(&ParentT::SetPassword, str); }
  void SetPasswordStr(const StringT&&) = delete;
  using ParentT::ClearPassword;

  void SetHostStr(const CharT* str) { SetImpl(&ParentT::SetHost, str); }
  void SetHostStr(StringViewT str) { SetImpl(&ParentT::SetHost, str); }
  void SetHostStr(const StringT&&) = delete;
  using ParentT::ClearHost;

  void SetPortStr(const CharT* str) { SetImpl(&ParentT::SetPort, str); }
  void SetPortStr(StringViewT str) { SetImpl(&ParentT::SetPort, str); }
  void SetPortStr(const StringT&&) = delete;
  using ParentT::ClearPort;

  void SetPathStr(const CharT* str) { SetImpl(&ParentT::SetPath, str); }
  void SetPathStr(StringViewT str) { SetImpl(&ParentT::SetPath, str); }
  void SetPathStr(const StringT&&) = delete;
  using ParentT::ClearPath;

  void SetQueryStr(const CharT* str) { SetImpl(&ParentT::SetQuery, str); }
  void SetQueryStr(StringViewT str) { SetImpl(&ParentT::SetQuery, str); }
  void SetQueryStr(const StringT&&) = delete;
  using ParentT::ClearQuery;

  void SetRefStr(const CharT* str) { SetImpl(&ParentT::SetRef, str); }
  void SetRefStr(StringViewT str) { SetImpl(&ParentT::SetRef, str); }
  void SetRefStr(const StringT&&) = delete;
  using ParentT::ClearRef;

 private:
  using ParentT::SetHost;
  using ParentT::SetPassword;
  using ParentT::SetPath;
  using ParentT::SetPort;
  using ParentT::SetQuery;
  using ParentT::SetRef;
  using ParentT::SetScheme;
  using ParentT::SetUsername;
};

}  // namespace url

#endif  // URL_URL_CANON_STDSTRING_H_
