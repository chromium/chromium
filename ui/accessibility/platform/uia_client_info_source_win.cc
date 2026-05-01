// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/uia_client_info_source_win.h"

#include <wrl/implements.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"

#include <UIAutomationCore.h>

namespace ui {
namespace {

std::optional<std::string> GetProcessName(
    IUIAutomationClientInfo* client_info) {
  if (!client_info) {
    return std::nullopt;
  }

  base::win::ScopedBstr process_name_bstr;
  HRESULT hr = client_info->get_ProcessName(process_name_bstr.Receive());
  if (FAILED(hr) || !process_name_bstr.Get()) {
    return std::nullopt;
  }

  std::wstring_view wide_name(process_name_bstr.Get(),
                              process_name_bstr.Length());
  const size_t last_slash = wide_name.find_last_of(L"\\/");
  if (last_slash != std::wstring_view::npos) {
    wide_name = wide_name.substr(last_slash + 1);
  }

  if (wide_name.empty()) {
    return std::nullopt;
  }

  return base::ToLowerASCII(base::WideToUTF8(wide_name));
}

std::vector<std::string> GetProcessNamesFromClientInfoSource(
    IUIAutomationClientInfoSource* client_info_source) {
  std::vector<std::string> process_names;
  if (!client_info_source) {
    return process_names;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::win::ScopedSafearray scoped_client_array;
  HRESULT hr =
      client_info_source->GetConnectedClients(scoped_client_array.Receive());
  if (FAILED(hr)) {
    return process_names;
  }

  auto locked_clients = scoped_client_array.CreateLockScope<VT_UNKNOWN>();
  if (!locked_clients) {
    return process_names;
  }

  for (IUnknown* element : *locked_clients) {
    if (!element) {
      continue;
    }

    Microsoft::WRL::ComPtr<IUIAutomationClientInfo> client_info;
    hr = element->QueryInterface(IID_PPV_ARGS(&client_info));
    if (FAILED(hr) || !client_info) {
      continue;
    }

    std::optional<std::string> process_name = GetProcessName(client_info.Get());
    if (process_name) {
      process_names.push_back(*std::move(process_name));
    }
  }

  return process_names;
}

class ClientConnectionCallback final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUIAutomationClientConnectionCallback> {
 public:
  explicit ClientConnectionCallback(
      UiaClientInfoSource::ConnectionCallback callback)
      : callback_(std::move(callback)) {}

  IFACEMETHODIMP OnConnected(IUIAutomationClientInfo* client_info) override {
    Notify(client_info, UiaClientInfoSource::ConnectionState::kConnected);
    return S_OK;
  }

  IFACEMETHODIMP OnDisconnected(IUIAutomationClientInfo* client_info) override {
    Notify(client_info, UiaClientInfoSource::ConnectionState::kDisconnected);
    return S_OK;
  }

 private:
  void Notify(IUIAutomationClientInfo* client_info,
              UiaClientInfoSource::ConnectionState state) {
    std::optional<std::string> process_name = GetProcessName(client_info);
    if (process_name) {
      callback_.Run(*std::move(process_name), state);
    }
  }

  UiaClientInfoSource::ConnectionCallback callback_;
};

}  // namespace

UiaClientInfoSource::~UiaClientInfoSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(SUCCEEDED(UnregisterConnectionCallback()));
}

// static
std::optional<UiaClientInfoSource> UiaClientInfoSource::Create(
    ConnectionCallback callback) {
  Microsoft::WRL::ComPtr<IUIAutomationClientInfoSource> client_info_source;
  HRESULT hr;
  {
    // CoCreateInstance may acquire the loader lock to load UIAutomationCore.
    // Doing it at background priority can cause a priority inversion with the
    // main thread, perceived as a hang by the user.
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    hr = ::CoCreateInstance(CLSID_CUIAutomationClientInfoSource, nullptr,
                            CLSCTX_INPROC_SERVER,
                            IID_PPV_ARGS(&client_info_source));
  }
  if (FAILED(hr) || !client_info_source) {
    return std::nullopt;
  }

  return CreateFromClientInfoSource(std::move(client_info_source),
                                    std::move(callback));
}

UiaClientInfoSource::UiaClientInfoSource(UiaClientInfoSource&& other) noexcept
    : client_info_source_(std::move(other.client_info_source_)),
      callback_cookie_(std::exchange(other.callback_cookie_, std::nullopt)) {}

UiaClientInfoSource& UiaClientInfoSource::operator=(
    UiaClientInfoSource&& other) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (this == &other) {
    return *this;
  }

  CHECK(SUCCEEDED(UnregisterConnectionCallback()));
  client_info_source_ = std::move(other.client_info_source_);
  callback_cookie_ = std::exchange(other.callback_cookie_, std::nullopt);
  return *this;
}

std::vector<std::string> UiaClientInfoSource::GetConnectedClientProcessNames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetProcessNamesFromClientInfoSource(client_info_source_.Get());
}

UiaClientInfoSource::UiaClientInfoSource(
    Microsoft::WRL::ComPtr<IUIAutomationClientInfoSource> client_info_source,
    std::optional<uint64_t> callback_cookie)
    : client_info_source_(std::move(client_info_source)),
      callback_cookie_(callback_cookie) {
  CHECK(client_info_source_);
}

// static
std::optional<UiaClientInfoSource>
UiaClientInfoSource::CreateForTesting(  // IN-TEST
    Microsoft::WRL::ComPtr<IUIAutomationClientInfoSource> client_info_source,
    ConnectionCallback callback) {
  return CreateFromClientInfoSource(std::move(client_info_source),
                                    std::move(callback));
}

// static
std::optional<UiaClientInfoSource>
UiaClientInfoSource::CreateFromClientInfoSource(
    Microsoft::WRL::ComPtr<IUIAutomationClientInfoSource> client_info_source,
    ConnectionCallback callback) {
  Microsoft::WRL::ComPtr<IUIAutomationClientConnectionCallback>
      connection_callback;
  std::optional<uint64_t> callback_cookie;

  if (callback) {
    connection_callback =
        Microsoft::WRL::Make<ClientConnectionCallback>(std::move(callback));
    if (!connection_callback) {
      return std::nullopt;
    }
    HRESULT hr = client_info_source->RegisterClientConnectionCallback(
        connection_callback.Get(), &callback_cookie.emplace());
    if (FAILED(hr)) {
      return std::nullopt;
    }
  }

  return UiaClientInfoSource(std::move(client_info_source), callback_cookie);
}

HRESULT UiaClientInfoSource::UnregisterConnectionCallback() {
  if (!callback_cookie_) {
    return S_OK;
  }
  CHECK(client_info_source_);
  HRESULT hr = client_info_source_->UnregisterClientConnectionCallback(
      *callback_cookie_);
  if (FAILED(hr)) {
    return hr;
  }

  callback_cookie_.reset();
  return S_OK;
}

}  // namespace ui
