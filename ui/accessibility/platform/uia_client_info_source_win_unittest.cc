// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/uia_client_info_source_win.h"

#include <stdint.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <UIAutomationCore.h>

namespace ui {
namespace {

class FakeUIAutomationClientInfo final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUIAutomationClientInfo> {
 public:
  explicit FakeUIAutomationClientInfo(std::wstring process_name)
      : process_name_(std::move(process_name)) {}

  IFACEMETHODIMP get_ProcessId(DWORD* process_id) override {
    if (!process_id) {
      return E_POINTER;
    }
    *process_id = 1234;
    return S_OK;
  }

  IFACEMETHODIMP get_ProcessName(BSTR* process_name) override {
    if (!process_name) {
      return E_POINTER;
    }
    *process_name = ::SysAllocString(process_name_.c_str());
    return *process_name ? S_OK : E_OUTOFMEMORY;
  }

 private:
  std::wstring process_name_;
};

class FakeUIAutomationClientInfoSource final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUIAutomationClientInfoSource> {
 public:
  FakeUIAutomationClientInfoSource(
      std::vector<Microsoft::WRL::ComPtr<IUIAutomationClientInfo>> clients,
      uint64_t callback_cookie)
      : clients_(std::move(clients)), callback_cookie_(callback_cookie) {}

  IFACEMETHODIMP RegisterClientConnectionCallback(
      IUIAutomationClientConnectionCallback* callback,
      uint64_t* cookie) override {
    if (!callback || !cookie) {
      return E_POINTER;
    }

    registered_callback_ = callback;
    *cookie = callback_cookie_;
    ++register_count_;
    return S_OK;
  }

  IFACEMETHODIMP UnregisterClientConnectionCallback(uint64_t cookie) override {
    if (!registered_callback_ || cookie != callback_cookie_) {
      return E_INVALIDARG;
    }

    registered_callback_.Reset();
    unregistered_cookie_ = cookie;
    ++unregister_count_;
    return S_OK;
  }

  IFACEMETHODIMP GetConnectedClients(SAFEARRAY** clients) override {
    if (!clients) {
      return E_POINTER;
    }

    *clients = nullptr;
    SAFEARRAY* client_array = ::SafeArrayCreateVector(
        VT_UNKNOWN, 0, static_cast<ULONG>(clients_.size()));
    if (!client_array) {
      return E_OUTOFMEMORY;
    }

    for (LONG i = 0; i < static_cast<LONG>(clients_.size()); ++i) {
      IUnknown* client = clients_[i].Get();
      HRESULT hr = ::SafeArrayPutElement(client_array, &i, client);
      if (FAILED(hr)) {
        ::SafeArrayDestroy(client_array);
        return hr;
      }
    }

    *clients = client_array;
    return S_OK;
  }

  void NotifyConnected(IUIAutomationClientInfo* client_info) {
    ASSERT_TRUE(registered_callback_);
    EXPECT_EQ(S_OK, registered_callback_->OnConnected(client_info));
  }

  void NotifyDisconnected(IUIAutomationClientInfo* client_info) {
    ASSERT_TRUE(registered_callback_);
    EXPECT_EQ(S_OK, registered_callback_->OnDisconnected(client_info));
  }

  int register_count() const { return register_count_; }
  int unregister_count() const { return unregister_count_; }
  std::optional<uint64_t> unregistered_cookie() const {
    return unregistered_cookie_;
  }
  bool has_registered_callback() const { return registered_callback_; }

 private:
  std::vector<Microsoft::WRL::ComPtr<IUIAutomationClientInfo>> clients_;
  const uint64_t callback_cookie_;
  Microsoft::WRL::ComPtr<IUIAutomationClientConnectionCallback>
      registered_callback_;
  int register_count_ = 0;
  int unregister_count_ = 0;
  std::optional<uint64_t> unregistered_cookie_;
};

std::vector<Microsoft::WRL::ComPtr<IUIAutomationClientInfo>> CreateClientInfos(
    const std::vector<std::wstring>& process_names) {
  std::vector<Microsoft::WRL::ComPtr<IUIAutomationClientInfo>> clients;
  for (const std::wstring& process_name : process_names) {
    clients.push_back(
        Microsoft::WRL::Make<FakeUIAutomationClientInfo>(process_name));
  }
  return clients;
}

Microsoft::WRL::ComPtr<FakeUIAutomationClientInfoSource> CreateClientInfoSource(
    const std::vector<std::wstring>& process_names,
    uint64_t callback_cookie = 1) {
  return Microsoft::WRL::Make<FakeUIAutomationClientInfoSource>(
      CreateClientInfos(process_names), callback_cookie);
}

}  // namespace

TEST(UiaClientInfoSourceTest, NotifiesObserverWithProcessBasename) {
  Microsoft::WRL::ComPtr<FakeUIAutomationClientInfoSource> client_info_source =
      CreateClientInfoSource({}, /*callback_cookie=*/1);
  std::vector<std::pair<std::string, UiaClientInfoSource::ConnectionState>>
      notifications;
  std::optional<UiaClientInfoSource> source =
      UiaClientInfoSource::CreateForTesting(
          client_info_source,
          base::BindLambdaForTesting(
              [&](const std::string& process_name,
                  UiaClientInfoSource::ConnectionState state) {
                notifications.emplace_back(process_name, state);
              }));
  ASSERT_TRUE(source);

  Microsoft::WRL::ComPtr<IUIAutomationClientInfo> client_info =
      Microsoft::WRL::Make<FakeUIAutomationClientInfo>(
          L"C:\\Windows\\System32\\Narrator.EXE");

  client_info_source->NotifyConnected(client_info.Get());
  client_info_source->NotifyDisconnected(client_info.Get());

  ASSERT_EQ(2u, notifications.size());
  EXPECT_EQ(std::make_pair(std::string("narrator.exe"),
                           UiaClientInfoSource::ConnectionState::kConnected),
            notifications[0]);
  EXPECT_EQ(std::make_pair(std::string("narrator.exe"),
                           UiaClientInfoSource::ConnectionState::kDisconnected),
            notifications[1]);
}

TEST(UiaClientInfoSourceTest,
     RegistersAndUnregistersClientConnectionCallbackWithZeroCookie) {
  constexpr uint64_t kCallbackCookie = 0;
  Microsoft::WRL::ComPtr<FakeUIAutomationClientInfoSource> client_info_source =
      CreateClientInfoSource({}, kCallbackCookie);

  {
    std::optional<UiaClientInfoSource> source =
        UiaClientInfoSource::CreateForTesting(
            client_info_source,
            base::BindRepeating([](const std::string&,
                                   UiaClientInfoSource::ConnectionState) {}));
    ASSERT_TRUE(source);
    EXPECT_EQ(1, client_info_source->register_count());
    EXPECT_TRUE(client_info_source->has_registered_callback());
  }

  EXPECT_EQ(1, client_info_source->unregister_count());
  ASSERT_TRUE(client_info_source->unregistered_cookie().has_value());
  EXPECT_EQ(kCallbackCookie, *client_info_source->unregistered_cookie());
  EXPECT_FALSE(client_info_source->has_registered_callback());
}

TEST(UiaClientInfoSourceTest, DoesNotRegisterCallbackWithoutObserver) {
  Microsoft::WRL::ComPtr<FakeUIAutomationClientInfoSource> client_info_source =
      CreateClientInfoSource({}, /*callback_cookie=*/1);

  {
    std::optional<UiaClientInfoSource> source =
        UiaClientInfoSource::CreateForTesting(client_info_source);
    ASSERT_TRUE(source);
    EXPECT_EQ(0, client_info_source->register_count());
    EXPECT_FALSE(client_info_source->has_registered_callback());
  }

  EXPECT_EQ(0, client_info_source->unregister_count());
}

TEST(UiaClientInfoSourceTest, DestroyingMovedFromInstanceDoesNotCrash) {
  Microsoft::WRL::ComPtr<FakeUIAutomationClientInfoSource> client_info_source =
      CreateClientInfoSource({}, /*callback_cookie=*/1);
  std::optional<UiaClientInfoSource> source =
      UiaClientInfoSource::CreateForTesting(
          client_info_source,
          base::BindRepeating(
              [](const std::string&, UiaClientInfoSource::ConnectionState) {}));
  ASSERT_TRUE(source);

  {
    UiaClientInfoSource moved_source(std::move(*source));
    source.reset();

    EXPECT_EQ(0, client_info_source->unregister_count());
  }

  EXPECT_EQ(1, client_info_source->unregister_count());
}

TEST(UiaClientInfoSourceTest, MoveConstructionTransfersCallbackRegistration) {
  Microsoft::WRL::ComPtr<FakeUIAutomationClientInfoSource> client_info_source =
      CreateClientInfoSource({}, /*callback_cookie=*/1);

  {
    std::optional<UiaClientInfoSource> source =
        UiaClientInfoSource::CreateForTesting(
            client_info_source,
            base::BindRepeating([](const std::string&,
                                   UiaClientInfoSource::ConnectionState) {}));
    ASSERT_TRUE(source);
    EXPECT_EQ(1, client_info_source->register_count());

    {
      UiaClientInfoSource moved_source(std::move(*source));
      source.reset();
      EXPECT_EQ(0, client_info_source->unregister_count());
    }
  }

  EXPECT_EQ(1, client_info_source->unregister_count());
}

TEST(UiaClientInfoSourceTest, MoveAssignmentTransfersCallbackRegistration) {
  Microsoft::WRL::ComPtr<FakeUIAutomationClientInfoSource>
      first_client_info_source =
          CreateClientInfoSource({}, /*callback_cookie=*/1);
  Microsoft::WRL::ComPtr<FakeUIAutomationClientInfoSource>
      second_client_info_source =
          CreateClientInfoSource({}, /*callback_cookie=*/2);
  std::optional<UiaClientInfoSource> first_source =
      UiaClientInfoSource::CreateForTesting(
          first_client_info_source,
          base::BindRepeating(
              [](const std::string&, UiaClientInfoSource::ConnectionState) {}));
  std::optional<UiaClientInfoSource> second_source =
      UiaClientInfoSource::CreateForTesting(
          second_client_info_source,
          base::BindRepeating(
              [](const std::string&, UiaClientInfoSource::ConnectionState) {}));
  ASSERT_TRUE(first_source);
  ASSERT_TRUE(second_source);

  *first_source = std::move(*second_source);

  EXPECT_EQ(1, first_client_info_source->unregister_count());
  EXPECT_EQ(0, second_client_info_source->unregister_count());

  second_source.reset();
  EXPECT_EQ(0, second_client_info_source->unregister_count());

  first_source.reset();
  EXPECT_EQ(1, second_client_info_source->unregister_count());
  ASSERT_TRUE(second_client_info_source->unregistered_cookie().has_value());
  EXPECT_EQ(2u, *second_client_info_source->unregistered_cookie());
}

TEST(UiaClientInfoSourceTest, ReturnsConnectedClientProcessNames) {
  Microsoft::WRL::ComPtr<FakeUIAutomationClientInfoSource> client_info_source =
      CreateClientInfoSource({L"C:\\Windows\\System32\\Narrator.EXE",
                              L"C:\\Windows\\System32\\Inspect.exe"});
  std::optional<UiaClientInfoSource> source =
      UiaClientInfoSource::CreateForTesting(client_info_source);
  ASSERT_TRUE(source);

  std::vector<std::string> process_names =
      source->GetConnectedClientProcessNames();

  ASSERT_EQ(2u, process_names.size());
  EXPECT_EQ("narrator.exe", process_names[0]);
  EXPECT_EQ("inspect.exe", process_names[1]);
}

}  // namespace ui
