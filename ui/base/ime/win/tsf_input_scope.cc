// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/ime/win/tsf_input_scope.h"

#include <windows.h>

#include <stddef.h>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/task/current_thread.h"
#include "base/trace_event/trace_event.h"

namespace ui::tsf_inputscope {
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

  TSFInputScope(const TSFInputScope&) = delete;
  TSFInputScope& operator=(const TSFInputScope&) = delete;

  // ITfInputScope:
  IFACEMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&ref_count_);
  }

  IFACEMETHODIMP_(ULONG) Release() override {
    const LONG count = InterlockedDecrement(&ref_count_);
    if (!count) {
      delete this;
      return 0;
    }
    return static_cast<ULONG>(count);
  }

  IFACEMETHODIMP QueryInterface(REFIID iid, void** result) override {
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

  IFACEMETHODIMP GetInputScopes(InputScope** input_scopes,
                                UINT* count) override {
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

  IFACEMETHODIMP GetPhrase(BSTR** phrases, UINT* count) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP GetRegularExpression(BSTR* regexp) override {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP GetSRGS(BSTR* srgs) override { return E_NOTIMPL; }

  IFACEMETHODIMP GetXML(BSTR* xml) override { return E_NOTIMPL; }

 private:
  // The corresponding text input types.
  std::vector<InputScope> input_scopes_;

  // The reference count of this instance.
  volatile ULONG ref_count_;
};

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
  // mode.
  if (!should_do_learning) {
    input_scopes.push_back(IS_PRIVATE);
  } else {
    input_scopes = GetInputScopes(text_input_type, text_input_mode);
  }
  return new TSFInputScope(input_scopes);
}

typedef HRESULT(WINAPI* SetInputScopeFunc)(HWND window_handle,
                                           InputScope input_scope);

SetInputScopeFunc g_set_input_scope = NULL;
bool g_get_set_input_scope_done = false;

void SetInputScope(HWND window_handle, InputScope input_scope) {
  CHECK(base::CurrentUIThread::IsSet());
  // Thread safety is not required because this function is under UI thread.
  if (!g_get_set_input_scope_done) {
    g_get_set_input_scope_done = true;

    HMODULE module = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"msctf.dll",
                           &module)) {
      g_set_input_scope = reinterpret_cast<SetInputScopeFunc>(
          GetProcAddress(module, "SetInputScope"));
    }
  }

  if (g_set_input_scope) {
    HRESULT hr = g_set_input_scope(window_handle, input_scope);
    if (hr != S_OK) {
      TRACE_EVENT2("ime", "SetInputScope", "input_scope", input_scope, "hr",
                   hr);
    }
  }
}

}  // namespace ui::tsf_inputscope
