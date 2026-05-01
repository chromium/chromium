// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_UIA_CLIENT_INFO_SOURCE_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_UIA_CLIENT_INFO_SOURCE_WIN_H_

#include <stdint.h>
#include <wrl/client.h>

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/win/windows_types.h"

struct IUIAutomationClientInfoSource;

namespace ui {

// Monitors the Windows UIA client-info source API. The API is available on
// Windows build 26100 (24H2) and later; Create() returns nullopt on older
// versions.
class COMPONENT_EXPORT(AX_PLATFORM) UiaClientInfoSource final {
 public:
  enum class ConnectionState {
    kConnected,
    kDisconnected,
  };

  // Called from an arbitrary UIA thread with the UIA client's normalized
  // lowercase process basename and connection state.
  using ConnectionCallback =
      base::RepeatingCallback<void(const std::string&, ConnectionState)>;

  // Requires COM to be initialized on the calling sequence. If `callback` is
  // non-null, UIA client connection notifications are registered immediately.
  static std::optional<UiaClientInfoSource> Create(
      ConnectionCallback callback = ConnectionCallback());
  static std::optional<UiaClientInfoSource> CreateForTesting(
      Microsoft::WRL::ComPtr<IUIAutomationClientInfoSource> client_info_source,
      ConnectionCallback callback = ConnectionCallback());

  ~UiaClientInfoSource();

  UiaClientInfoSource(const UiaClientInfoSource&) = delete;
  UiaClientInfoSource& operator=(const UiaClientInfoSource&) = delete;
  UiaClientInfoSource(UiaClientInfoSource&& other) noexcept;
  UiaClientInfoSource& operator=(UiaClientInfoSource&& other);

  // Synchronously queries the UIA system for currently connected clients.
  // Clients without a usable process name are ignored.
  std::vector<std::string> GetConnectedClientProcessNames();

 private:
  UiaClientInfoSource(
      Microsoft::WRL::ComPtr<IUIAutomationClientInfoSource> client_info_source,
      std::optional<uint64_t> callback_cookie);

  static std::optional<UiaClientInfoSource> CreateFromClientInfoSource(
      Microsoft::WRL::ComPtr<IUIAutomationClientInfoSource> client_info_source,
      ConnectionCallback callback);

  HRESULT UnregisterConnectionCallback()
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  Microsoft::WRL::ComPtr<IUIAutomationClientInfoSource> client_info_source_;
  std::optional<uint64_t> callback_cookie_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_UIA_CLIENT_INFO_SOURCE_WIN_H_
