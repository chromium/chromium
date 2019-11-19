// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/tsf_input_scope.h"

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/stl_util.h"
#include "base/win/windows_version.h"

namespace ui {
namespace tsf_inputscope {
namespace {

void AppendNonTrivialInputScope(std::vector<InputScope>* input_scopes,
                                InputScope input_scope) {
  DCHECK(input_scopes);

  if (input_scope == IS_DEFAULT)
    return;

  if (base::Contains(*input_scopes, input_scope))
    return;

  input_scopes->push_back(input_scope);
}

class TSFInputScope final : public ITfInputScope {
 public:
  explicit TSFInputScope(const std::vector<InputScope>& input_scopes)
      : input_scopes_(input_scopes),
        ref_count_(0) {}

  // ITfInputScope:
  STDMETHOD_(ULONG, AddRef)() override {
    return InterlockedIncrement(&ref_count_);
  }

  STDMETHOD_(ULONG, Release)() override {
    const LONG count = InterlockedDecrement(&ref_count_);
    if (!count) {
      delete this;
      return 0;
    }
    return static_cast<ULONG>(count);
  }

  STDMETHOD(QueryInterface)(REFIID iid, void** result) override {
    if (!result)
      return E_INVALIDARG;
    if (iid == IID_IUnknown || iid == IID_ITfInputScope) {
      *result = static_cast<ITfInputScope*>(this);
    } else {
      *result = NULL;
      return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
  }

  STDMETHOD(GetInputScopes)(InputScope** input_scopes, UINT* count) override {
    if (!count || !input_scopes)
      return E_INVALIDARG;
    *input_scopes = static_cast<InputScope*>(CoTaskMemAlloc(
        sizeof(InputScope) * input_scopes_.size()));
    if (!input_scopes) {
      *count = 0;
      return E_OUTOFMEMORY;
    }

    for (size_t i = 0; i < input_scopes_.size(); ++i)
      (*input_scopes)[i] = input_scopes_[i];
    *count = input_scopes_.size();
    return S_OK;
  }

  STDMETHOD(GetPhrase)(BSTR** phrases, UINT* count) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetRegularExpression)(BSTR* regexp) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetSRGS)(BSTR* srgs) override {
    return E_NOTIMPL;
  }

  STDMETHOD(GetXML)(BSTR* xml) override {
    return E_NOTIMPL;
  }

 private:
  // The corresponding text input types.
  std::vector<InputScope> input_scopes_;

  // The refrence count of this instance.
  volatile LONG ref_count_;

  DISALLOW_COPY_AND_ASSIGN(TSFInputScope);
};

typedef HRESULT (WINAPI *SetInputScopesFunc)(HWND window_handle,
                                             const InputScope* input_scope_list,
                                             UINT num_input_scopes,
                                             WCHAR**, /* unused */
                                             UINT, /* unused */
                                             WCHAR*, /* unused */
                                             WCHAR* /* unused */);

SetInputScopesFunc g_set_input_scopes = NULL;
bool g_get_proc_done = false;

InputScope ConvertTextInputTypeToInputScope(TextInputType text_input_type) {
  // Following mapping is based in IE10 on Windows 8.
  switch (text_input_type) {
    case TEXT_INPUT_TYPE_PASSWORD:
      return IS_PASSWORD;
    case TEXT_INPUT_TYPE_SEARCH:
      return IS_SEARCH;
    case TEXT_INPUT_TYPE_EMAIL:
      return IS_EMAIL_SMTPEMAILADDRESS;
    case TEXT_INPUT_TYPE_NUMBER:
      return IS_NUMBER;
    case TEXT_INPUT_TYPE_TELEPHONE:
      return IS_TELEPHONE_FULLTELEPHONENUMBER;
    case TEXT_INPUT_TYPE_URL:
      return IS_URL;
    default:
      return IS_DEFAULT;
  }
}

InputScope ConvertTextInputModeToInputScope(TextInputMode text_input_mode) {
  switch (text_input_mode) {
    case TEXT_INPUT_MODE_NUMERIC:
      return IS_DIGITS;
    case TEXT_INPUT_MODE_DECIMAL:
      return IS_NUMBER;
    case TEXT_INPUT_MODE_TEL:
      return IS_TELEPHONE_FULLTELEPHONENUMBER;
    case TEXT_INPUT_MODE_EMAIL:
      return IS_EMAIL_SMTPEMAILADDRESS;
    case TEXT_INPUT_MODE_URL:
      return IS_URL;
    case TEXT_INPUT_MODE_SEARCH:
      return IS_SEARCH;
    default:
      return IS_DEFAULT;
  }
}

}  // namespace

void InitializeTsfForInputScopes() {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  // Thread safety is not required because this function is under UI thread.
  if (!g_get_proc_done) {
    g_get_proc_done = true;

    HMODULE module = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"msctf.dll",
        &module)) {
      g_set_input_scopes = reinterpret_cast<SetInputScopesFunc>(
          GetProcAddress(module, "SetInputScopes"));
    }
  }
}

std::vector<InputScope> GetInputScopes(TextInputType text_input_type,
                                       TextInputMode text_input_mode) {
  std::vector<InputScope> input_scopes;

  AppendNonTrivialInputScope(&input_scopes,
                             ConvertTextInputTypeToInputScope(text_input_type));
  AppendNonTrivialInputScope(&input_scopes,
                             ConvertTextInputModeToInputScope(text_input_mode));

  if (input_scopes.empty())
    input_scopes.push_back(IS_DEFAULT);

  return input_scopes;
}

ITfInputScope* CreateInputScope(TextInputType text_input_type,
                                TextInputMode text_input_mode,
                                bool should_do_learning) {
  std::vector<InputScope> input_scopes;
  // Should set input scope to IS_PRIVATE if we are in "incognito" or "guest"
  // mode. Note that the IS_PRIVATE input scope is only support from WIN10.
  if (!should_do_learning &&
      (base::win::GetVersion() >= base::win::Version::WIN10)) {
    input_scopes.push_back(IS_PRIVATE);
  } else {
    input_scopes = GetInputScopes(text_input_type, text_input_mode);
  }
  return new TSFInputScope(input_scopes);
}

void SetInputScopeForTsfUnawareWindow(HWND window_handle,
                                      TextInputType text_input_type,
                                      TextInputMode text_input_mode) {
  if (!g_set_input_scopes)
    return;

  std::vector<InputScope> input_scopes = GetInputScopes(text_input_type,
                                                        text_input_mode);
  g_set_input_scopes(window_handle, &input_scopes[0], input_scopes.size(),
                     NULL, 0, NULL, NULL);
}

}  // namespace tsf_inputscope
}  // namespace ui
