// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_win.h"

#include <Windows.Media.ClosedCaptioning.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/implements.h>

#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace ui {
namespace {

// Fake implementation of IClosedCaptionPropertiesStatics and
// IClosedCaptionPropertiesStatics2 that stores the event handler passed to
// add_PropertiesChanged so tests can invoke it.
class FakeClosedCaptionPropertiesStatics
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Media::ClosedCaptioning::
              IClosedCaptionPropertiesStatics,
          ABI::Windows::Media::ClosedCaptioning::
              IClosedCaptionPropertiesStatics2> {
 public:
  FakeClosedCaptionPropertiesStatics() = default;

  // IClosedCaptionPropertiesStatics — all getters return E_NOTIMPL since
  // they are not called during listener registration.
  IFACEMETHODIMP get_FontColor(
      ABI::Windows::Media::ClosedCaptioning::ClosedCaptionColor*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_ComputedFontColor(ABI::Windows::UI::Color*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_FontOpacity(
      ABI::Windows::Media::ClosedCaptioning::ClosedCaptionOpacity*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_FontSize(
      ABI::Windows::Media::ClosedCaptioning::ClosedCaptionSize*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_FontStyle(
      ABI::Windows::Media::ClosedCaptioning::ClosedCaptionStyle*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_FontEffect(
      ABI::Windows::Media::ClosedCaptioning::ClosedCaptionEdgeEffect*)
      override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_BackgroundColor(
      ABI::Windows::Media::ClosedCaptioning::ClosedCaptionColor*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_ComputedBackgroundColor(
      ABI::Windows::UI::Color*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_BackgroundOpacity(
      ABI::Windows::Media::ClosedCaptioning::ClosedCaptionOpacity*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_RegionColor(
      ABI::Windows::Media::ClosedCaptioning::ClosedCaptionColor*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_ComputedRegionColor(ABI::Windows::UI::Color*) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_RegionOpacity(
      ABI::Windows::Media::ClosedCaptioning::ClosedCaptionOpacity*) override {
    return E_NOTIMPL;
  }

  // IClosedCaptionPropertiesStatics2:
  IFACEMETHODIMP add_PropertiesChanged(
      ABI::Windows::Foundation::IEventHandler<IInspectable*>* handler,
      EventRegistrationToken* token) override {
    handler_ = handler;
    *token = EventRegistrationToken{1};
    return S_OK;
  }

  IFACEMETHODIMP remove_PropertiesChanged(
      EventRegistrationToken token) override {
    handler_.Reset();
    return S_OK;
  }

  // Fire the stored event handler to simulate a caption style change.
  HRESULT SimulatePropertiesChanged() {
    if (!handler_) {
      return E_FAIL;
    }
    return handler_->Invoke(nullptr, nullptr);
  }

  bool has_handler() const { return handler_.Get() != nullptr; }

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IEventHandler<IInspectable*>>
      handler_;
};

// Subclass to access the protected NativeThemeWin constructor.
class TestNativeThemeWin : public NativeThemeWin {
 public:
  TestNativeThemeWin() = default;
  ~TestNativeThemeWin() override = default;
};

class MockCaptionObserver : public NativeThemeObserver {
 public:
  MOCK_METHOD(void, OnCaptionStyleUpdated, (), (override));
};

class NativeThemeWinCaptionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fake_statics_ = Microsoft::WRL::Make<FakeClosedCaptionPropertiesStatics>();

    // Inject the fake statics so the next NativeThemeWin constructor will
    // register with our fake.
    NativeThemeWin::SetClosedCaptionPropertiesStaticsForTesting(fake_statics_);
  }

  void TearDown() override {
    // Clear the test override.
    NativeThemeWin::SetClosedCaptionPropertiesStaticsForTesting(nullptr);
  }

  FakeClosedCaptionPropertiesStatics& fake_statics() {
    return *fake_statics_.Get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::win::ScopedCOMInitializer com_initializer_;
  Microsoft::WRL::ComPtr<FakeClosedCaptionPropertiesStatics> fake_statics_;
};

// Verifies that constructing a NativeThemeWin registers an event handler
// via the IClosedCaptionPropertiesStatics2 interface, and destroying it
// unregisters the handler.
TEST_F(NativeThemeWinCaptionTest, ListenerLifecycle) {
  EXPECT_FALSE(fake_statics().has_handler());
  {
    TestNativeThemeWin theme;
    ASSERT_TRUE(fake_statics().has_handler());
  }
  EXPECT_FALSE(fake_statics().has_handler());
}

// Verifies that when the WinRT PropertiesChanged event fires,
// OnCaptionStyleUpdated is called on observers of the web NativeTheme.
TEST_F(NativeThemeWinCaptionTest, EventFiresObserverCallback) {
  TestNativeThemeWin theme;
  ASSERT_TRUE(fake_statics().has_handler());

  MockCaptionObserver observer;
  base::ScopedObservation<NativeTheme, NativeThemeObserver> observation(
      &observer);
  observation.Observe(NativeTheme::GetInstanceForWeb());

  EXPECT_CALL(observer, OnCaptionStyleUpdated()).Times(1);

  // Simulate a caption style change from the OS.
  EXPECT_HRESULT_SUCCEEDED(fake_statics().SimulatePropertiesChanged());
}

// Verifies that multiple PropertiesChanged events each trigger
// OnCaptionStyleUpdated.
TEST_F(NativeThemeWinCaptionTest, MultipleEventsFireMultipleCallbacks) {
  TestNativeThemeWin theme;
  ASSERT_TRUE(fake_statics().has_handler());

  MockCaptionObserver observer;
  base::ScopedObservation<NativeTheme, NativeThemeObserver> observation(
      &observer);
  observation.Observe(NativeTheme::GetInstanceForWeb());

  EXPECT_CALL(observer, OnCaptionStyleUpdated()).Times(3);

  EXPECT_HRESULT_SUCCEEDED(fake_statics().SimulatePropertiesChanged());
  EXPECT_HRESULT_SUCCEEDED(fake_statics().SimulatePropertiesChanged());
  EXPECT_HRESULT_SUCCEEDED(fake_statics().SimulatePropertiesChanged());
}

}  // namespace
}  // namespace ui
