// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/ime/win/tsf_text_store.h"

#include <initguid.h>  // for GUID_NULL and GUID_PROP_INPUTSCOPE

#include <InputScope.h>
#include <OleCtl.h>
#include <tsattrs.h>
#include <wrl/client.h>

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_variant.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace ui {
namespace {

class MockTextInputClient : public TextInputClient {
 public:
  ~MockTextInputClient() override {}
  base::WeakPtr<TextInputClient> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  MOCK_METHOD1(SetCompositionText, void(const ui::CompositionText&));
  MOCK_METHOD1(ConfirmCompositionText, size_t(bool));
  MOCK_METHOD0(ClearCompositionText, void());
  MOCK_METHOD2(
      InsertText,
      void(const std::u16string&,
           ui::TextInputClient::InsertTextCursorBehavior cursor_behavior));
  MOCK_METHOD1(InsertChar, void(const ui::KeyEvent&));
  MOCK_CONST_METHOD0(GetTextInputType, ui::TextInputType());
  MOCK_CONST_METHOD0(GetTextInputMode, ui::TextInputMode());
  MOCK_CONST_METHOD0(GetTextDirection, base::i18n::TextDirection());
  MOCK_CONST_METHOD0(GetTextInputFlags, int());
  MOCK_CONST_METHOD0(CanComposeInline, bool());
  MOCK_CONST_METHOD0(GetCaretBounds, gfx::Rect());
  MOCK_CONST_METHOD0(GetSelectionBoundingBox, gfx::Rect());
  MOCK_CONST_METHOD2(GetCompositionCharacterBounds, bool(size_t, gfx::Rect*));
  MOCK_CONST_METHOD0(HasCompositionText, bool());
  MOCK_CONST_METHOD0(GetFocusReason, ui::TextInputClient::FocusReason());
  MOCK_METHOD0(ShouldDoLearning, bool());
  MOCK_CONST_METHOD1(GetTextRange, bool(gfx::Range*));
  MOCK_CONST_METHOD1(GetCompositionTextRange, bool(gfx::Range*));
  MOCK_CONST_METHOD1(GetEditableSelectionRange, bool(gfx::Range*));
  MOCK_METHOD1(SetEditableSelectionRange, bool(const gfx::Range&));
  MOCK_METHOD1(DeleteRange, bool(const gfx::Range&));
  MOCK_CONST_METHOD2(GetTextFromRange,
                     bool(const gfx::Range&, std::u16string*));
  MOCK_METHOD0(OnInputMethodChanged, void());
  MOCK_METHOD1(ChangeTextDirectionAndLayoutAlignment,
               bool(base::i18n::TextDirection));
  MOCK_METHOD2(ExtendSelectionAndDelete, void(size_t, size_t));
  MOCK_METHOD1(EnsureCaretNotInRect, void(const gfx::Rect&));
  MOCK_CONST_METHOD1(IsTextEditCommandEnabled, bool(TextEditCommand));
  MOCK_METHOD1(SetTextEditCommandForNextKeyEvent, void(TextEditCommand));
  MOCK_CONST_METHOD0(GetClientSourceForMetrics, ukm::SourceId());
  MOCK_METHOD2(SetCompositionFromExistingText,
               bool(const gfx::Range&, const std::vector<ui::ImeTextSpan>&));
  MOCK_METHOD3(SetActiveCompositionForAccessibility,
               void(const gfx::Range&, const std::u16string&, bool));
  MOCK_METHOD2(GetActiveTextInputControlLayoutBounds,
               void(std::optional<gfx::Rect>* control_bounds,
                    std::optional<gfx::Rect>* selection_bounds));
  MOCK_METHOD0(GetTextEditingContext, ui::TextInputClient::EditingContext());

 private:
  base::WeakPtrFactory<MockTextInputClient> weak_ptr_factory_{this};
};

class MockImeKeyEventDispatcher : public ImeKeyEventDispatcher {
 public:
  ~MockImeKeyEventDispatcher() override {}
  MOCK_METHOD1(DispatchKeyEventPostIME, EventDispatchDetails(KeyEvent*));
};

class MockStoreACPSink : public ITextStoreACPSink {
 public:
  MockStoreACPSink() : ref_count_(0) {}

  // IUnknown
  ULONG STDMETHODCALLTYPE AddRef() override {
    return InterlockedIncrement(&ref_count_);
  }
  ULONG STDMETHODCALLTYPE Release() override {
    const LONG count = InterlockedDecrement(&ref_count_);
    if (!count) {
      delete this;
      return 0;
    }
    return static_cast<ULONG>(count);
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** report) override {
    if (iid == IID_IUnknown || iid == IID_ITextStoreACPSink) {
      *report = static_cast<ITextStoreACPSink*>(this);
    } else {
      *report = nullptr;
      return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
  }

  // ITextStoreACPSink
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             OnTextChange,
                             HRESULT(DWORD, const TS_TEXTCHANGE*));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE, OnSelectionChange, HRESULT());
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             OnLayoutChange,
                             HRESULT(TsLayoutCode, TsViewCookie));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, OnStatusChange, HRESULT(DWORD));
  MOCK_METHOD4_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             OnAttrsChange,
                             HRESULT(LONG, LONG, ULONG, const TS_ATTRID*));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, OnLockGranted, HRESULT(DWORD));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             OnStartEditTransaction,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             OnEndEditTransaction,
                             HRESULT());

 private:
  virtual ~MockStoreACPSink() {}

  volatile LONG ref_count_;
};

const HWND kWindowHandle = reinterpret_cast<HWND>(1);

}  // namespace

class TSFTextStoreTest : public testing::Test {
 protected:
  void SetUp() override {
    text_store_ = new TSFTextStore();
    EXPECT_EQ(S_OK, text_store_->Initialize());
    sink_ = new MockStoreACPSink();
    EXPECT_EQ(S_OK, text_store_->AdviseSink(IID_ITextStoreACPSink, sink_.get(),
                                            TS_AS_ALL_SINKS));
    text_store_->SetFocusedTextInputClient(kWindowHandle, &text_input_client_);
    text_store_->SetImeKeyEventDispatcher(&ime_key_event_dispatcher_);
  }

  void TearDown() override {
    EXPECT_EQ(S_OK, text_store_->UnadviseSink(sink_.get()));
    sink_ = nullptr;
    text_store_ = nullptr;
  }

  // Accessors to the internal state of TSFTextStore.
  std::u16string* string_buffer() {
    return &text_store_->string_buffer_document_;
  }
  size_t* composition_start() { return &text_store_->composition_start_; }

  base::win::ScopedCOMInitializer com_initializer_;
  MockTextInputClient text_input_client_;
  MockImeKeyEventDispatcher ime_key_event_dispatcher_;
  scoped_refptr<TSFTextStore> text_store_;
  scoped_refptr<MockStoreACPSink> sink_;
};

class TSFTextStoreTestCallback {
 public:
  explicit TSFTextStoreTestCallback(TSFTextStore* text_store)
      : text_store_(text_store) {
    CHECK(text_store_);
  }

  TSFTextStoreTestCallback(const TSFTextStoreTestCallback&) = delete;
  TSFTextStoreTestCallback& operator=(const TSFTextStoreTestCallback&) = delete;

  virtual ~TSFTextStoreTestCallback() {}

  bool HasCompositionText() { return has_composition_text_; }
  bool GetTextRange(gfx::Range* range) {
    range->set_start(text_range_.start());
    range->set_end(text_range_.end());
    return true;
  }
  bool GetTextFromRange(const gfx::Range& range, std::u16string* text) {
    *text = text_buffer_.substr(range.GetMin(), range.length());
    return true;
  }
  bool GetEditableSelectionRange(gfx::Range* range) {
    range->set_start(selection_range_.start());
    range->set_end(selection_range_.end());
    return true;
  }

  bool GetCompositionTextRange(gfx::Range* range) {
    range->set_start(composition_range_.start());
    range->set_end(composition_range_.end());
    return true;
  }

 protected:
  // Accessors to the internal state of TSFTextStore.
  bool* edit_flag() { return &text_store_->edit_flag_; }
  bool* new_text_inserted() { return &text_store_->new_text_inserted_; }
  std::u16string* string_buffer() {
    return &text_store_->string_buffer_document_;
  }
  std::u16string* string_pending_insertion() {
    return &text_store_->string_pending_insertion_;
  }
  size_t* composition_start() { return &text_store_->composition_start_; }
  gfx::Range* selection() { return &text_store_->selection_; }
  ImeTextSpans* text_spans() { return &text_store_->text_spans_; }
  gfx::Range* composition_range() { return &text_store_->composition_range_; }
  bool* has_composition_range() { return &text_store_->has_composition_range_; }

  void SetInternalState(const std::u16string& new_string_buffer,
                        LONG new_composition_start,
                        LONG new_selection_start,
                        LONG new_selection_end) {
    ASSERT_LE(0, new_composition_start);
    ASSERT_LE(new_composition_start, new_selection_start);
    ASSERT_LE(new_selection_start, new_selection_end);
    ASSERT_LE(new_selection_end, static_cast<LONG>(new_string_buffer.size()));
    *string_buffer() = new_string_buffer;
    *string_pending_insertion() = new_string_buffer;
    *composition_start() = new_composition_start;
    selection()->set_start(new_selection_start);
    selection()->set_end(new_selection_end);
  }

  bool HasReadLock() const { return text_store_->HasReadLock(); }
  bool HasReadWriteLock() const { return text_store_->HasReadWriteLock(); }

  void GetSelectionTest(LONG expected_acp_start, LONG expected_acp_end) {
    TS_SELECTION_ACP selection = {};
    ULONG fetched = 0;
    EXPECT_EQ(S_OK, text_store_->GetSelection(0, 1, &selection, &fetched));
    EXPECT_EQ(1u, fetched);
    EXPECT_EQ(expected_acp_start, selection.acpStart);
    EXPECT_EQ(expected_acp_end, selection.acpEnd);
  }

  void SetSelectionTest(LONG acp_start, LONG acp_end, HRESULT expected_result) {
    TS_SELECTION_ACP selection = {};
    selection.acpStart = acp_start;
    selection.acpEnd = acp_end;
    selection.style.ase = TS_AE_NONE;
    selection.style.fInterimChar = 0;
    EXPECT_EQ(expected_result, text_store_->SetSelection(1, &selection));
    if (expected_result == S_OK) {
      GetSelectionTest(acp_start, acp_end);
    }
  }

  void SetTextTest(LONG acp_start,
                   LONG acp_end,
                   const std::wstring& text,
                   HRESULT error_code) {
    TS_TEXTCHANGE change = {};
    ASSERT_EQ(error_code,
              text_store_->SetText(0, acp_start, acp_end, text.c_str(),
                                   text.size(), &change));
    if (error_code == S_OK) {
      EXPECT_EQ(acp_start, change.acpStart);
      EXPECT_EQ(acp_end, change.acpOldEnd);
      EXPECT_EQ(acp_start + text.size(), (size_t)change.acpNewEnd);
    }
  }

  void GetTextTest(LONG acp_start,
                   LONG acp_end,
                   const std::wstring& expected_string,
                   LONG expected_next_acp) {
    wchar_t buffer[1024] = {};
    ULONG text_buffer_copied = 0;
    TS_RUNINFO run_info = {};
    ULONG run_info_buffer_copied = 0;
    LONG next_acp = 0;
    ASSERT_EQ(S_OK, text_store_->GetText(acp_start, acp_end, buffer, 1024,
                                         &text_buffer_copied, &run_info, 1,
                                         &run_info_buffer_copied, &next_acp));
    ASSERT_EQ(expected_string.size(), text_buffer_copied);
    EXPECT_EQ(expected_string,
              std::wstring(buffer, buffer + text_buffer_copied));
    if (text_buffer_copied > 0) {
      EXPECT_EQ(1u, run_info_buffer_copied);
      EXPECT_EQ(expected_string.size(), run_info.uCount);
      EXPECT_EQ(TS_RT_PLAIN, run_info.type);
      EXPECT_EQ(expected_next_acp, next_acp);
    } else {
      EXPECT_EQ(0u, run_info_buffer_copied);
    }
  }

  void GetTextErrorTest(LONG acp_start, LONG acp_end, HRESULT error_code) {
    wchar_t buffer[1024] = {};
    ULONG text_buffer_copied = 0;
    TS_RUNINFO run_info = {};
    ULONG run_info_buffer_copied = 0;
    LONG next_acp = 0;
    EXPECT_EQ(error_code,
              text_store_->GetText(acp_start, acp_end, buffer, 1024,
                                   &text_buffer_copied, &run_info, 1,
                                   &run_info_buffer_copied, &next_acp));
  }

  void InsertTextAtSelectionTest(const wchar_t* buffer,
                                 ULONG buffer_size,
                                 LONG expected_start,
                                 LONG expected_end,
                                 LONG expected_change_start,
                                 LONG expected_change_old_end,
                                 LONG expected_change_new_end) {
    LONG start = 0;
    LONG end = 0;
    TS_TEXTCHANGE change = {};
    EXPECT_EQ(S_OK, text_store_->InsertTextAtSelection(0, buffer, buffer_size,
                                                       &start, &end, &change));
    EXPECT_EQ(expected_start, start);
    EXPECT_EQ(expected_end, end);
    EXPECT_EQ(expected_change_start, change.acpStart);
    EXPECT_EQ(expected_change_old_end, change.acpOldEnd);
    EXPECT_EQ(expected_change_new_end, change.acpNewEnd);
  }

  void InsertTextAtSelectionQueryOnlyTest(const wchar_t* buffer,
                                          ULONG buffer_size,
                                          LONG expected_start,
                                          LONG expected_end) {
    LONG start = 0;
    LONG end = 0;
    EXPECT_EQ(S_OK, text_store_->InsertTextAtSelection(TS_IAS_QUERYONLY, buffer,
                                                       buffer_size, &start,
                                                       &end, nullptr));
    EXPECT_EQ(expected_start, start);
    EXPECT_EQ(expected_end, end);
  }

  void GetTextExtTest(TsViewCookie view_cookie,
                      LONG acp_start,
                      LONG acp_end,
                      LONG expected_left,
                      LONG expected_top,
                      LONG expected_right,
                      LONG expected_bottom) {
    RECT rect = {};
    BOOL clipped = FALSE;
    EXPECT_EQ(S_OK, text_store_->GetTextExt(view_cookie, acp_start, acp_end,
                                            &rect, &clipped));
    EXPECT_EQ(expected_left, rect.left);
    EXPECT_EQ(expected_top, rect.top);
    EXPECT_EQ(expected_right, rect.right);
    EXPECT_EQ(expected_bottom, rect.bottom);
    EXPECT_EQ(FALSE, clipped);
  }

  void GetTextExtNoLayoutTest(TsViewCookie view_cookie,
                              LONG acp_start,
                              LONG acp_end) {
    RECT rect = {};
    BOOL clipped = FALSE;
    EXPECT_EQ(TS_E_NOLAYOUT, text_store_->GetTextExt(view_cookie, acp_start,
                                                     acp_end, &rect, &clipped));
  }

  void ResetCompositionStateTest() {
    EXPECT_TRUE(text_store_->previous_composition_string_.empty());
    EXPECT_EQ(0u, text_store_->previous_composition_start_);
    EXPECT_EQ(gfx::Range::InvalidRange(),
              text_store_->previous_composition_selection_range_);
    EXPECT_TRUE(text_store_->previous_text_spans_.empty());

    EXPECT_TRUE(text_store_->string_pending_insertion_.empty());
    EXPECT_TRUE(text_store_->composition_range_.is_empty());
    EXPECT_EQ(text_store_->composition_from_client_.end(),
              text_store_->selection_.start());
    EXPECT_EQ(text_store_->composition_from_client_.end(),
              text_store_->selection_.end());
    EXPECT_EQ(text_store_->selection_.end(), text_store_->composition_start_);
  }

  void SetHasCompositionText(bool compText) {
    has_composition_text_ = compText;
  }

  void SetTextRange(uint32_t start, uint32_t end) {
    text_range_.set_start(start);
    text_range_.set_end(end);
  }

  void SetSelectionRange(uint32_t start, uint32_t end) {
    selection_range_.set_start(start);
    selection_range_.set_end(end);
  }

  void SetCompositionTextRange(uint32_t start, uint32_t end) {
    composition_range_.set_start(start);
    composition_range_.set_end(end);
  }

  void SetTextBuffer(const char16_t* buffer) {
    text_buffer_.clear();
    text_buffer_.assign(buffer);
  }

  bool has_composition_text_ = false;
  gfx::Range text_range_;
  gfx::Range selection_range_;
  gfx::Range composition_range_;
  std::u16string text_buffer_;
  scoped_refptr<TSFTextStore> text_store_;
};

namespace {

const HRESULT kInvalidResult = 0x12345678;

TEST_F(TSFTextStoreTest, GetStatusTest) {
  TS_STATUS status = {};
  EXPECT_EQ(S_OK, text_store_->GetStatus(&status));
  EXPECT_EQ((ULONG)TS_SD_INPUTPANEMANUALDISPLAYENABLE, status.dwDynamicFlags);
  EXPECT_EQ((ULONG)(TS_SS_TRANSITORY | TS_SS_NOHIDDENTEXT),
            status.dwStaticFlags);

  text_store_->UseEmptyTextStore(true);
  status = {};
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_NONE));
  EXPECT_EQ(S_OK, text_store_->GetStatus(&status));
  EXPECT_EQ((ULONG)TS_SD_READONLY, status.dwDynamicFlags & TS_SD_READONLY);

  status = {};
  text_store_->UseEmptyTextStore(false);
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  EXPECT_EQ(S_OK, text_store_->GetStatus(&status));
  EXPECT_EQ((ULONG)0, status.dwDynamicFlags & TS_SD_READONLY);
}

TEST_F(TSFTextStoreTest, DummyLockTest) {
  HRESULT result = kInvalidResult;
  text_store_->UseEmptyTextStore(false);
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  EXPECT_EQ(S_OK,
            text_store_->RequestLock(TS_LF_READWRITE | TS_LF_SYNC, &result));

  text_store_->UseEmptyTextStore(true);
  EXPECT_EQ(E_FAIL,
            text_store_->RequestLock(TS_LF_READWRITE | TS_LF_SYNC, &result));
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_NONE));
  EXPECT_EQ(E_FAIL,
            text_store_->RequestLock(TS_LF_READWRITE | TS_LF_SYNC, &result));
}

TEST_F(TSFTextStoreTest, QueryInsertTest) {
  LONG result_start = 0;
  LONG result_end = 0;
  *string_buffer() = std::u16string();
  *composition_start() = 0;
  EXPECT_EQ(E_INVALIDARG,
            text_store_->QueryInsert(0, 0, 0, nullptr, &result_end));
  EXPECT_EQ(E_INVALIDARG,
            text_store_->QueryInsert(0, 0, 0, &result_start, nullptr));
  EXPECT_EQ(S_OK,
            text_store_->QueryInsert(0, 0, 0, &result_start, &result_end));
  EXPECT_EQ(0, result_start);
  EXPECT_EQ(0, result_end);
  *string_buffer() = u"1234";
  *composition_start() = 1;
  EXPECT_EQ(S_OK,
            text_store_->QueryInsert(0, 1, 0, &result_start, &result_end));
  EXPECT_EQ(1, result_start);
  EXPECT_EQ(1, result_end);
  EXPECT_EQ(E_INVALIDARG,
            text_store_->QueryInsert(1, 0, 0, &result_start, &result_end));
  EXPECT_EQ(S_OK,
            text_store_->QueryInsert(2, 2, 0, &result_start, &result_end));
  EXPECT_EQ(2, result_start);
  EXPECT_EQ(2, result_end);
  EXPECT_EQ(S_OK,
            text_store_->QueryInsert(2, 3, 0, &result_start, &result_end));
  EXPECT_EQ(2, result_start);
  EXPECT_EQ(3, result_end);
  EXPECT_EQ(E_INVALIDARG,
            text_store_->QueryInsert(3, 2, 0, &result_start, &result_end));
  EXPECT_EQ(S_OK,
            text_store_->QueryInsert(3, 4, 0, &result_start, &result_end));
  EXPECT_EQ(3, result_start);
  EXPECT_EQ(4, result_end);
  EXPECT_EQ(S_OK,
            text_store_->QueryInsert(3, 5, 0, &result_start, &result_end));
  EXPECT_EQ(3, result_start);
  EXPECT_EQ(4, result_end);

  *string_buffer() = u"";
  *composition_start() = 2;
  EXPECT_EQ(S_OK,
            text_store_->QueryInsert(0, 2, 5, &result_start, &result_end));
  EXPECT_EQ(0, result_start);
  EXPECT_EQ(5, result_end);
}

class SyncRequestLockTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit SyncRequestLockTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  SyncRequestLockTestCallback(const SyncRequestLockTestCallback&) = delete;
  SyncRequestLockTestCallback& operator=(const SyncRequestLockTestCallback&) =
      delete;

  HRESULT LockGranted1(DWORD flags) {
    EXPECT_TRUE(HasReadLock());
    EXPECT_FALSE(HasReadWriteLock());
    return S_OK;
  }

  HRESULT LockGranted2(DWORD flags) {
    EXPECT_TRUE(HasReadLock());
    EXPECT_TRUE(HasReadWriteLock());
    return S_OK;
  }

  HRESULT LockGranted3(DWORD flags) {
    EXPECT_TRUE(HasReadLock());
    EXPECT_FALSE(HasReadWriteLock());
    HRESULT result = kInvalidResult;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READ | TS_LF_SYNC, &result));
    EXPECT_EQ(TS_E_SYNCHRONOUS, result);
    return S_OK;
  }

  HRESULT LockGranted4(DWORD flags) {
    EXPECT_TRUE(HasReadLock());
    EXPECT_FALSE(HasReadWriteLock());
    HRESULT result = kInvalidResult;
    EXPECT_EQ(S_OK,
              text_store_->RequestLock(TS_LF_READWRITE | TS_LF_SYNC, &result));
    EXPECT_EQ(TS_E_SYNCHRONOUS, result);
    return S_OK;
  }

  HRESULT LockGranted5(DWORD flags) {
    EXPECT_TRUE(HasReadLock());
    EXPECT_TRUE(HasReadWriteLock());
    HRESULT result = kInvalidResult;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READ | TS_LF_SYNC, &result));
    EXPECT_EQ(TS_E_SYNCHRONOUS, result);
    return S_OK;
  }

  HRESULT LockGranted6(DWORD flags) {
    EXPECT_TRUE(HasReadLock());
    EXPECT_TRUE(HasReadWriteLock());
    HRESULT result = kInvalidResult;
    EXPECT_EQ(S_OK,
              text_store_->RequestLock(TS_LF_READWRITE | TS_LF_SYNC, &result));
    EXPECT_EQ(TS_E_SYNCHRONOUS, result);
    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, SynchronousRequestLockTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  SyncRequestLockTestCallback callback(text_store_.get());
  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &SyncRequestLockTestCallback::LockGranted1))
      .WillOnce(Invoke(&callback, &SyncRequestLockTestCallback::LockGranted2))
      .WillOnce(Invoke(&callback, &SyncRequestLockTestCallback::LockGranted3))
      .WillOnce(Invoke(&callback, &SyncRequestLockTestCallback::LockGranted4))
      .WillOnce(Invoke(&callback, &SyncRequestLockTestCallback::LockGranted5))
      .WillOnce(Invoke(&callback, &SyncRequestLockTestCallback::LockGranted6));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READ | TS_LF_SYNC, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK,
            text_store_->RequestLock(TS_LF_READWRITE | TS_LF_SYNC, &result));
  EXPECT_EQ(S_OK, result);

  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READ | TS_LF_SYNC, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READ | TS_LF_SYNC, &result));
  EXPECT_EQ(S_OK, result);

  result = kInvalidResult;
  EXPECT_EQ(S_OK,
            text_store_->RequestLock(TS_LF_READWRITE | TS_LF_SYNC, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK,
            text_store_->RequestLock(TS_LF_READWRITE | TS_LF_SYNC, &result));
  EXPECT_EQ(S_OK, result);
}

class AsyncRequestLockTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit AsyncRequestLockTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store), state_(0) {}

  AsyncRequestLockTestCallback(const AsyncRequestLockTestCallback&) = delete;
  AsyncRequestLockTestCallback& operator=(const AsyncRequestLockTestCallback&) =
      delete;

  HRESULT LockGranted1(DWORD flags) {
    EXPECT_EQ(0, state_);
    state_ = 1;
    EXPECT_TRUE(HasReadLock());
    EXPECT_FALSE(HasReadWriteLock());
    HRESULT result = kInvalidResult;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READ, &result));
    EXPECT_EQ(TS_S_ASYNC, result);
    EXPECT_EQ(1, state_);
    state_ = 2;
    return S_OK;
  }

  HRESULT LockGranted2(DWORD flags) {
    EXPECT_EQ(2, state_);
    EXPECT_TRUE(HasReadLock());
    EXPECT_FALSE(HasReadWriteLock());
    HRESULT result = kInvalidResult;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(TS_S_ASYNC, result);
    EXPECT_EQ(2, state_);
    state_ = 3;
    return S_OK;
  }

  HRESULT LockGranted3(DWORD flags) {
    EXPECT_EQ(3, state_);
    EXPECT_TRUE(HasReadLock());
    EXPECT_TRUE(HasReadWriteLock());
    HRESULT result = kInvalidResult;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(TS_S_ASYNC, result);
    EXPECT_EQ(3, state_);
    state_ = 4;
    return S_OK;
  }

  HRESULT LockGranted4(DWORD flags) {
    EXPECT_EQ(4, state_);
    EXPECT_TRUE(HasReadLock());
    EXPECT_TRUE(HasReadWriteLock());
    HRESULT result = kInvalidResult;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READ, &result));
    EXPECT_EQ(TS_S_ASYNC, result);
    EXPECT_EQ(4, state_);
    state_ = 5;
    return S_OK;
  }

  HRESULT LockGranted5(DWORD flags) {
    EXPECT_EQ(5, state_);
    EXPECT_TRUE(HasReadLock());
    EXPECT_FALSE(HasReadWriteLock());
    state_ = 6;
    return S_OK;
  }

 private:
  int state_;
};

TEST_F(TSFTextStoreTest, AsynchronousRequestLockTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  AsyncRequestLockTestCallback callback(text_store_.get());
  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &AsyncRequestLockTestCallback::LockGranted1))
      .WillOnce(Invoke(&callback, &AsyncRequestLockTestCallback::LockGranted2))
      .WillOnce(Invoke(&callback, &AsyncRequestLockTestCallback::LockGranted3))
      .WillOnce(Invoke(&callback, &AsyncRequestLockTestCallback::LockGranted4))
      .WillOnce(Invoke(&callback, &AsyncRequestLockTestCallback::LockGranted5));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READ, &result));
  EXPECT_EQ(S_OK, result);
}

class RequestLockTextChangeTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit RequestLockTextChangeTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store), state_(0) {}

  RequestLockTextChangeTestCallback(const RequestLockTextChangeTestCallback&) =
      delete;
  RequestLockTextChangeTestCallback& operator=(
      const RequestLockTextChangeTestCallback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    EXPECT_EQ(0, state_);
    state_ = 1;
    EXPECT_TRUE(HasReadLock());
    EXPECT_TRUE(HasReadWriteLock());

    *edit_flag() = true;
    SetInternalState(u"012345", 6, 6, 6);
    text_spans()->clear();

    state_ = 2;
    return S_OK;
  }

  void InsertText(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(2, state_);
    EXPECT_EQ(u"012345", text);
    state_ = 3;
  }

  bool GetTextRange(gfx::Range* range) const {
    range->set_start(0);
    range->set_end(6);
    return true;
  }

  bool GetTextFromRange(const gfx::Range& range, std::u16string* text) const {
    std::u16string string_buffer = u"012345";
    *text = string_buffer.substr(range.GetMin(), range.length());
    return true;
  }

  bool GetEditableSelectionRange(gfx::Range* range) const {
    range->set_start(0);
    range->set_end(0);
    return true;
  }

  HRESULT OnSelectionChange() {
    EXPECT_EQ(3, state_);
    HRESULT result = kInvalidResult;
    state_ = 4;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(5, state_);
    state_ = 6;
    return S_OK;
  }

  HRESULT LockGranted2(DWORD flags) {
    EXPECT_EQ(4, state_);
    EXPECT_TRUE(HasReadLock());
    EXPECT_TRUE(HasReadWriteLock());
    state_ = 5;
    return S_OK;
  }

 private:
  int state_;
};

TEST_F(TSFTextStoreTest, RequestLockOnTextChangeTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RequestLockTextChangeTestCallback callback(text_store_.get());
  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(
          Invoke(&callback, &RequestLockTextChangeTestCallback::LockGranted1))
      .WillOnce(
          Invoke(&callback, &RequestLockTextChangeTestCallback::LockGranted2));

  EXPECT_CALL(*sink_, OnSelectionChange())
      .WillOnce(Invoke(&callback,
                       &RequestLockTextChangeTestCallback::OnSelectionChange));
  EXPECT_CALL(text_input_client_, InsertText(_, _))
      .WillOnce(
          Invoke(&callback, &RequestLockTextChangeTestCallback::InsertText));
  EXPECT_CALL(text_input_client_, GetEditableSelectionRange(_))
      .WillOnce(Invoke(
          &callback,
          &RequestLockTextChangeTestCallback::GetEditableSelectionRange));
  EXPECT_CALL(text_input_client_, GetTextFromRange(_, _))
      .WillOnce(Invoke(&callback,
                       &RequestLockTextChangeTestCallback::GetTextFromRange));
  EXPECT_CALL(text_input_client_, GetTextRange(_))
      .WillOnce(
          Invoke(&callback, &RequestLockTextChangeTestCallback::GetTextRange));

  ON_CALL(text_input_client_, GetCompositionTextRange(_))
      .WillByDefault(Invoke(
          &callback, &TSFTextStoreTestCallback::GetCompositionTextRange));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

class SelectionTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit SelectionTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  HRESULT ReadLockGranted(DWORD flags) {
    SetInternalState(std::u16string(), 0, 0, 0);

    GetSelectionTest(0, 0);
    SetSelectionTest(0, 0, TF_E_NOLOCK);

    SetInternalState(u"012345", 0, 0, 3);

    GetSelectionTest(0, 3);
    SetSelectionTest(0, 0, TF_E_NOLOCK);

    return S_OK;
  }

  HRESULT ReadWriteLockGranted(DWORD flags) {
    SetInternalState(std::u16string(), 0, 0, 0);

    SetSelectionTest(0, 0, S_OK);
    GetSelectionTest(0, 0);
    SetSelectionTest(0, 1, TF_E_INVALIDPOS);
    SetSelectionTest(1, 0, TF_E_INVALIDPOS);
    SetSelectionTest(1, 1, TF_E_INVALIDPOS);

    SetInternalState(u"0123456", 3, 3, 3);

    SetSelectionTest(0, 0, S_OK);
    SetSelectionTest(0, 1, S_OK);
    SetSelectionTest(0, 3, S_OK);
    SetSelectionTest(0, 6, S_OK);
    SetSelectionTest(0, 7, S_OK);
    SetSelectionTest(0, 8, TF_E_INVALIDPOS);

    SetSelectionTest(1, 0, TF_E_INVALIDPOS);
    SetSelectionTest(1, 1, S_OK);
    SetSelectionTest(1, 3, S_OK);
    SetSelectionTest(1, 6, S_OK);
    SetSelectionTest(1, 7, S_OK);
    SetSelectionTest(1, 8, TF_E_INVALIDPOS);

    SetSelectionTest(3, 0, TF_E_INVALIDPOS);
    SetSelectionTest(3, 1, TF_E_INVALIDPOS);
    SetSelectionTest(3, 3, S_OK);
    SetSelectionTest(3, 6, S_OK);
    SetSelectionTest(3, 7, S_OK);
    SetSelectionTest(3, 8, TF_E_INVALIDPOS);

    SetSelectionTest(6, 0, TF_E_INVALIDPOS);
    SetSelectionTest(6, 1, TF_E_INVALIDPOS);
    SetSelectionTest(6, 3, TF_E_INVALIDPOS);
    SetSelectionTest(6, 6, S_OK);
    SetSelectionTest(6, 7, S_OK);
    SetSelectionTest(6, 8, TF_E_INVALIDPOS);

    SetSelectionTest(7, 0, TF_E_INVALIDPOS);
    SetSelectionTest(7, 1, TF_E_INVALIDPOS);
    SetSelectionTest(7, 3, TF_E_INVALIDPOS);
    SetSelectionTest(7, 6, TF_E_INVALIDPOS);
    SetSelectionTest(7, 7, S_OK);
    SetSelectionTest(7, 8, TF_E_INVALIDPOS);

    SetSelectionTest(8, 0, TF_E_INVALIDPOS);
    SetSelectionTest(8, 1, TF_E_INVALIDPOS);
    SetSelectionTest(8, 3, TF_E_INVALIDPOS);
    SetSelectionTest(8, 6, TF_E_INVALIDPOS);
    SetSelectionTest(8, 7, TF_E_INVALIDPOS);
    SetSelectionTest(8, 8, TF_E_INVALIDPOS);

    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, SetGetSelectionTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  SelectionTestCallback callback(text_store_.get());
  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &SelectionTestCallback::ReadLockGranted))
      .WillOnce(
          Invoke(&callback, &SelectionTestCallback::ReadWriteLockGranted));

  TS_SELECTION_ACP selection_buffer = {};
  ULONG fetched_count = 0;
  EXPECT_EQ(TS_E_NOLOCK,
            text_store_->GetSelection(0, 1, &selection_buffer, &fetched_count));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READ, &result));
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
}

class SetGetTextTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit SetGetTextTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  SetGetTextTestCallback(const SetGetTextTestCallback&) = delete;
  SetGetTextTestCallback& operator=(const SetGetTextTestCallback&) = delete;

  HRESULT ReadLockGranted(DWORD flags) {
    SetTextTest(0, 0, L"", TF_E_NOLOCK);

    GetTextTest(0, -1, L"", 0);
    GetTextTest(0, 0, L"", 0);
    GetTextErrorTest(0, 1, TF_E_INVALIDPOS);

    SetInternalState(u"0123456", 3, 3, 3);

    GetTextErrorTest(-1, -1, TF_E_INVALIDPOS);
    GetTextErrorTest(-1, 0, TF_E_INVALIDPOS);
    GetTextErrorTest(-1, 1, TF_E_INVALIDPOS);
    GetTextErrorTest(-1, 3, TF_E_INVALIDPOS);
    GetTextErrorTest(-1, 6, TF_E_INVALIDPOS);
    GetTextErrorTest(-1, 7, TF_E_INVALIDPOS);
    GetTextErrorTest(-1, 8, TF_E_INVALIDPOS);

    GetTextTest(0, -1, L"0123456", 7);
    GetTextTest(0, 0, L"", 0);
    GetTextTest(0, 1, L"0", 1);
    GetTextTest(0, 3, L"012", 3);
    GetTextTest(0, 6, L"012345", 6);
    GetTextTest(0, 7, L"0123456", 7);
    GetTextErrorTest(0, 8, TF_E_INVALIDPOS);

    GetTextTest(1, -1, L"123456", 7);
    GetTextErrorTest(1, 0, TF_E_INVALIDPOS);
    GetTextTest(1, 1, L"", 1);
    GetTextTest(1, 3, L"12", 3);
    GetTextTest(1, 6, L"12345", 6);
    GetTextTest(1, 7, L"123456", 7);
    GetTextErrorTest(1, 8, TF_E_INVALIDPOS);

    GetTextTest(3, -1, L"3456", 7);
    GetTextErrorTest(3, 0, TF_E_INVALIDPOS);
    GetTextErrorTest(3, 1, TF_E_INVALIDPOS);
    GetTextTest(3, 3, L"", 3);
    GetTextTest(3, 6, L"345", 6);
    GetTextTest(3, 7, L"3456", 7);
    GetTextErrorTest(3, 8, TF_E_INVALIDPOS);

    GetTextTest(6, -1, L"6", 7);
    GetTextErrorTest(6, 0, TF_E_INVALIDPOS);
    GetTextErrorTest(6, 1, TF_E_INVALIDPOS);
    GetTextErrorTest(6, 3, TF_E_INVALIDPOS);
    GetTextTest(6, 6, L"", 6);
    GetTextTest(6, 7, L"6", 7);
    GetTextErrorTest(6, 8, TF_E_INVALIDPOS);

    GetTextTest(7, -1, L"", 7);
    GetTextErrorTest(7, 0, TF_E_INVALIDPOS);
    GetTextErrorTest(7, 1, TF_E_INVALIDPOS);
    GetTextErrorTest(7, 3, TF_E_INVALIDPOS);
    GetTextErrorTest(7, 6, TF_E_INVALIDPOS);
    GetTextTest(7, 7, L"", 7);
    GetTextErrorTest(7, 8, TF_E_INVALIDPOS);

    GetTextErrorTest(8, -1, TF_E_INVALIDPOS);
    GetTextErrorTest(8, 0, TF_E_INVALIDPOS);
    GetTextErrorTest(8, 1, TF_E_INVALIDPOS);
    GetTextErrorTest(8, 3, TF_E_INVALIDPOS);
    GetTextErrorTest(8, 6, TF_E_INVALIDPOS);
    GetTextErrorTest(8, 7, TF_E_INVALIDPOS);
    GetTextErrorTest(8, 8, TF_E_INVALIDPOS);

    return S_OK;
  }

  HRESULT ReadWriteLockGranted(DWORD flags) {
    SetInternalState(std::u16string(), 0, 0, 0);
    SetTextTest(0, 0, L"", S_OK);

    SetInternalState(std::u16string(), 0, 0, 0);
    SetTextTest(0, 1, L"", TS_E_INVALIDPOS);

    SetInternalState(u"0123456", 3, 3, 3);

    SetTextTest(0, 0, L"", S_OK);
    SetTextTest(0, 1, L"", S_OK);
    SetTextTest(0, 3, L"", S_OK);
    SetTextTest(0, 6, L"", TS_E_INVALIDPOS);
    SetTextTest(0, 7, L"", TS_E_INVALIDPOS);
    SetTextTest(0, 8, L"", TS_E_INVALIDPOS);

    SetTextTest(1, 0, L"", TS_E_INVALIDPOS);
    SetTextTest(1, 1, L"", S_OK);
    SetTextTest(1, 3, L"", S_OK);
    SetTextTest(1, 6, L"", TS_E_INVALIDPOS);
    SetTextTest(1, 7, L"", TS_E_INVALIDPOS);
    SetTextTest(1, 8, L"", TS_E_INVALIDPOS);

    SetTextTest(3, 0, L"", TS_E_INVALIDPOS);
    SetTextTest(3, 1, L"", TS_E_INVALIDPOS);

    SetTextTest(3, 3, L"", TS_E_INVALIDPOS);
    GetTextTest(0, -1, L"4", 1);
    GetSelectionTest(1, 1);
    SetInternalState(u"0123456", 3, 3, 3);

    SetTextTest(3, 6, L"", S_OK);
    GetTextTest(0, -1, L"0126", 4);
    GetSelectionTest(3, 3);
    SetInternalState(u"0123456", 3, 3, 3);

    SetTextTest(3, 7, L"", S_OK);
    GetTextTest(0, -1, L"012", 3);
    GetSelectionTest(3, 3);
    SetInternalState(u"0123456", 3, 3, 3);

    SetTextTest(3, 8, L"", TS_E_INVALIDPOS);

    SetTextTest(6, 0, L"", TS_E_INVALIDPOS);
    SetTextTest(6, 1, L"", TS_E_INVALIDPOS);
    SetTextTest(6, 3, L"", TS_E_INVALIDPOS);

    SetTextTest(6, 6, L"", S_OK);
    GetTextTest(0, -1, L"0123456", 7);
    GetSelectionTest(6, 6);
    SetInternalState(u"0123456", 3, 3, 3);

    SetTextTest(6, 7, L"", S_OK);
    GetTextTest(0, -1, L"012345", 6);
    GetSelectionTest(6, 6);
    SetInternalState(u"0123456", 3, 3, 3);

    SetTextTest(6, 8, L"", TS_E_INVALIDPOS);

    SetTextTest(7, 0, L"", TS_E_INVALIDPOS);
    SetTextTest(7, 1, L"", TS_E_INVALIDPOS);
    SetTextTest(7, 3, L"", TS_E_INVALIDPOS);
    SetTextTest(7, 6, L"", TS_E_INVALIDPOS);

    SetTextTest(7, 7, L"", S_OK);
    GetTextTest(0, -1, L"0123456", 7);
    GetSelectionTest(7, 7);
    SetInternalState(u"0123456", 3, 3, 3);

    SetTextTest(7, 8, L"", TS_E_INVALIDPOS);

    SetInternalState(u"0123456", 3, 3, 3);
    SetTextTest(3, 3, L"abc", S_OK);
    GetTextTest(0, -1, L"012abc3456", 10);
    GetSelectionTest(3, 6);

    SetInternalState(u"0123456", 3, 3, 3);
    SetTextTest(3, 6, L"abc", S_OK);
    GetTextTest(0, -1, L"012abc6", 7);
    GetSelectionTest(3, 6);

    SetInternalState(u"0123456", 3, 3, 3);
    SetTextTest(3, 7, L"abc", S_OK);
    GetTextTest(0, -1, L"012abc", 6);
    GetSelectionTest(3, 6);

    SetInternalState(u"0123456", 3, 3, 3);
    SetTextTest(6, 6, L"abc", S_OK);
    GetTextTest(0, -1, L"012345abc6", 10);
    GetSelectionTest(6, 9);

    SetInternalState(u"0123456", 3, 3, 3);
    SetTextTest(6, 7, L"abc", S_OK);
    GetTextTest(0, -1, L"012345abc", 9);
    GetSelectionTest(6, 9);

    SetInternalState(u"0123456", 3, 3, 3);
    SetTextTest(7, 7, L"abc", S_OK);
    GetTextTest(0, -1, L"0123456abc", 10);
    GetSelectionTest(7, 10);

    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, SetGetTextTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  SetGetTextTestCallback callback(text_store_.get());
  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &SetGetTextTestCallback::ReadLockGranted))
      .WillOnce(
          Invoke(&callback, &SetGetTextTestCallback::ReadWriteLockGranted));

  wchar_t buffer[1024] = {};
  ULONG text_buffer_copied = 0;
  TS_RUNINFO run_info = {};
  ULONG run_info_buffer_copied = 0;
  LONG next_acp = 0;
  EXPECT_EQ(TF_E_NOLOCK, text_store_->GetText(
                             0, -1, buffer, 1024, &text_buffer_copied,
                             &run_info, 1, &run_info_buffer_copied, &next_acp));
  TS_TEXTCHANGE change = {};
  EXPECT_EQ(TF_E_NOLOCK, text_store_->SetText(0, 0, 0, L"abc", 3, &change));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READ, &result));
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
}

class InsertTextAtSelectionTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit InsertTextAtSelectionTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  InsertTextAtSelectionTestCallback(const InsertTextAtSelectionTestCallback&) =
      delete;
  InsertTextAtSelectionTestCallback& operator=(
      const InsertTextAtSelectionTestCallback&) = delete;

  HRESULT ReadLockGranted(DWORD flags) {
    const wchar_t kBuffer[] = L"0123456789";

    SetInternalState(u"abcedfg", 0, 0, 0);
    InsertTextAtSelectionQueryOnlyTest(kBuffer, 10, 0, 0);
    GetSelectionTest(0, 0);
    InsertTextAtSelectionQueryOnlyTest(kBuffer, 0, 0, 0);

    SetInternalState(u"abcedfg", 0, 2, 5);
    InsertTextAtSelectionQueryOnlyTest(kBuffer, 10, 2, 5);
    GetSelectionTest(2, 5);
    InsertTextAtSelectionQueryOnlyTest(kBuffer, 0, 2, 5);

    LONG start = 0;
    LONG end = 0;
    TS_TEXTCHANGE change = {};
    EXPECT_EQ(TS_E_NOLOCK, text_store_->InsertTextAtSelection(
                               0, kBuffer, 10, &start, &end, &change));
    return S_OK;
  }

  HRESULT ReadWriteLockGranted(DWORD flags) {
    SetInternalState(u"abcedfg", 0, 0, 0);

    const wchar_t kBuffer[] = L"0123456789";
    InsertTextAtSelectionQueryOnlyTest(kBuffer, 10, 0, 0);
    GetSelectionTest(0, 0);
    InsertTextAtSelectionQueryOnlyTest(kBuffer, 0, 0, 0);

    SetInternalState(std::u16string(), 0, 0, 0);
    InsertTextAtSelectionTest(kBuffer, 10, 0, 10, 0, 0, 10);
    GetSelectionTest(0, 10);
    GetTextTest(0, -1, L"0123456789", 10);

    SetInternalState(u"abcedfg", 0, 0, 0);
    InsertTextAtSelectionTest(kBuffer, 10, 0, 10, 0, 0, 10);
    GetSelectionTest(0, 10);
    GetTextTest(0, -1, L"0123456789abcedfg", 17);

    SetInternalState(u"abcedfg", 0, 0, 3);
    InsertTextAtSelectionTest(kBuffer, 0, 0, 0, 0, 3, 0);
    GetSelectionTest(0, 0);
    GetTextTest(0, -1, L"edfg", 4);

    SetInternalState(u"abcedfg", 0, 3, 7);
    InsertTextAtSelectionTest(kBuffer, 10, 3, 13, 3, 7, 13);
    GetSelectionTest(3, 13);
    GetTextTest(0, -1, L"abc0123456789", 13);

    SetInternalState(u"abcedfg", 0, 7, 7);
    InsertTextAtSelectionTest(kBuffer, 10, 7, 17, 7, 7, 17);
    GetSelectionTest(7, 17);
    GetTextTest(0, -1, L"abcedfg0123456789", 17);

    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, InsertTextAtSelectionTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  InsertTextAtSelectionTestCallback callback(text_store_.get());
  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback,
                       &InsertTextAtSelectionTestCallback::ReadLockGranted))
      .WillOnce(Invoke(
          &callback, &InsertTextAtSelectionTestCallback::ReadWriteLockGranted));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READ, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

class ScenarioTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit ScenarioTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  ScenarioTestCallback(const ScenarioTestCallback&) = delete;
  ScenarioTestCallback& operator=(const ScenarioTestCallback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"abc", S_OK);
    SetTextTest(1, 2, L"xyz", S_OK);

    GetTextTest(0, -1, L"axyzc", 5);

    SetSelectionTest(0, 5, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 5;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(5);

    // Need to set |has_composition_range_| to indicate composition scenario.
    *has_composition_range() = true;
    text_store_->OnKeyTraceDown(66u, 3145729u);
    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u"axyzc", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(5u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(SK_ColorBLACK, composition.ime_text_spans[0].underline_color);
    EXPECT_EQ(SK_ColorTRANSPARENT,
              composition.ime_text_spans[0].background_color);
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(5u, composition.ime_text_spans[0].end_offset);
    EXPECT_EQ(ImeTextSpan::Thickness::kThin,
              composition.ime_text_spans[0].thickness);
    has_composition_text_ = true;
  }

  HRESULT LockGranted2(DWORD flags) {
    SetTextTest(0, 5, L"axyZCPc", S_OK);
    GetTextTest(0, -1, L"axyZCPc", 7);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 5;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThick;
    text_spans()->push_back(text_span);

    *edit_flag() = true;
    *composition_start() = 3;
    composition_range()->set_start(3);
    composition_range()->set_end(7);

    return S_OK;
  }

  void InsertText2(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"axy", text);
  }

  void SetCompositionText2(const ui::CompositionText& composition) {
    EXPECT_EQ(u"ZCPc", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(4u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    // There is no styling applied from TSF in English typing
  }

  HRESULT LockGranted3(DWORD flags) {
    GetTextTest(0, -1, L"axyZCPc", 7);
    SetSelectionTest(7, 7, S_OK);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 7;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    return S_OK;
  }

  void InsertText3(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"ZCPc", text);
    has_composition_text_ = false;
  }

  HRESULT LockGranted4(DWORD flags) {
    GetTextTest(0, -1, L"axyZCPc", 7);
    SetTextTest(7, 7, L"EFGH", S_OK);
    GetTextTest(0, -1, L"axyZCPcEFGH", 11);
    SetSelectionTest(11, 11, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 4;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThick;
    text_spans()->push_back(text_span);

    *edit_flag() = true;
    *composition_start() = 7;
    composition_range()->set_start(7);
    composition_range()->set_end(11);

    *has_composition_range() = true;
    return S_OK;
  }

  void SetCompositionText4(const ui::CompositionText& composition) {
    EXPECT_EQ(u"EFGH", composition.text);
    EXPECT_EQ(4u, composition.selection.start());
    EXPECT_EQ(4u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    *has_composition_range() = true;
    has_composition_text_ = true;
  }

  HRESULT LockGranted5(DWORD flags) {
    GetTextTest(0, -1, L"axyZCPcEFGH", 11);
    SetSelectionTest(9, 9, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 4;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThick;
    text_spans()->push_back(text_span);

    *edit_flag() = true;
    *composition_start() = 7;
    composition_range()->set_start(7);
    composition_range()->set_end(11);

    return S_OK;
  }

  // still need to call into TextInputClient to set composition text
  // to update selection range even though composition text is unchanged.
  void SetCompositionText5(const ui::CompositionText& composition) {
    EXPECT_EQ(u"EFGH", composition.text);
    EXPECT_EQ(2u, composition.selection.start());
    EXPECT_EQ(2u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
  }

  bool ClientHasCompositionText() { return has_composition_text_; }

 private:
  bool has_composition_text_;
};

TEST_F(TSFTextStoreTest, ScenarioTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  ScenarioTestCallback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(Invoke(&callback, &ScenarioTestCallback::SetCompositionText1))
      .WillOnce(Invoke(&callback, &ScenarioTestCallback::SetCompositionText2))
      .WillOnce(Invoke(&callback, &ScenarioTestCallback::SetCompositionText4))
      .WillOnce(Invoke(&callback, &ScenarioTestCallback::SetCompositionText5));

  EXPECT_CALL(text_input_client_, InsertText(_, _))
      .WillOnce(Invoke(&callback, &ScenarioTestCallback::InsertText2))
      .WillOnce(Invoke(&callback, &ScenarioTestCallback::InsertText3));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &ScenarioTestCallback::LockGranted1))
      .WillOnce(Invoke(&callback, &ScenarioTestCallback::LockGranted2))
      .WillOnce(Invoke(&callback, &ScenarioTestCallback::LockGranted3))
      .WillOnce(Invoke(&callback, &ScenarioTestCallback::LockGranted4))
      .WillOnce(Invoke(&callback, &ScenarioTestCallback::LockGranted5));

  EXPECT_CALL(text_input_client_, HasCompositionText())
      .WillRepeatedly(
          Invoke(&callback, &ScenarioTestCallback::ClientHasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

class GetTextExtTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit GetTextExtTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store),
        layout_prepared_character_num_(0) {}

  GetTextExtTestCallback(const GetTextExtTestCallback&) = delete;
  GetTextExtTestCallback& operator=(const GetTextExtTestCallback&) = delete;

  HRESULT LockGranted(DWORD flags) {
    SetInternalState(u"0123456789012", 0, 0, 0);
    layout_prepared_character_num_ = 13;
    has_composition_text_ = true;

    TsViewCookie view_cookie = 0;
    EXPECT_EQ(S_OK, text_store_->GetActiveView(&view_cookie));
    GetTextExtTest(view_cookie, 0, 0, 11, 12, 11, 20);
    GetTextExtTest(view_cookie, 0, 1, 11, 12, 20, 20);
    GetTextExtTest(view_cookie, 0, 2, 11, 12, 30, 20);
    GetTextExtTest(view_cookie, 9, 9, 100, 12, 100, 20);
    GetTextExtTest(view_cookie, 9, 10, 101, 12, 110, 20);
    GetTextExtTest(view_cookie, 10, 10, 110, 12, 110, 20);
    GetTextExtTest(view_cookie, 11, 11, 20, 112, 20, 120);
    GetTextExtTest(view_cookie, 11, 12, 21, 112, 30, 120);
    GetTextExtTest(view_cookie, 9, 12, 101, 12, 101, 120);
    GetTextExtTest(view_cookie, 9, 13, 101, 12, 101, 120);
    GetTextExtTest(view_cookie, 0, 13, 11, 12, 40, 120);
    GetTextExtTest(view_cookie, 13, 13, 40, 112, 40, 120);

    layout_prepared_character_num_ = 12;
    GetTextExtNoLayoutTest(view_cookie, 13, 13);

    layout_prepared_character_num_ = 0;
    has_composition_text_ = false;
    GetTextExtTest(view_cookie, 0, 0, 1, 2, 4, 6);

    SetInternalState(std::u16string(), 0, 0, 0);
    GetTextExtTest(view_cookie, 0, 0, 1, 2, 4, 6);

    // Last character is not available due to timing issue of async API.
    // In this case, we will get first character bounds instead of whole text
    // bounds.
    SetInternalState(u"abc", 0, 0, 3);
    layout_prepared_character_num_ = 2;
    has_composition_text_ = true;
    GetTextExtTest(view_cookie, 0, 0, 11, 12, 11, 20);

    return S_OK;
  }

  bool GetCompositionCharacterBounds(uint32_t index, gfx::Rect* rect) {
    if (index >= layout_prepared_character_num_)
      return false;
    rect->set_x((index % 10) * 10 + 11);
    rect->set_y((index / 10) * 100 + 12);
    rect->set_width(9);
    rect->set_height(8);
    return true;
  }

  gfx::Rect GetCaretBounds() { return gfx::Rect(1, 2, 3, 4); }

  bool ClientHasCompositionText() { return has_composition_text_; }

 private:
  uint32_t layout_prepared_character_num_;
  bool has_composition_text_;
};

TEST_F(TSFTextStoreTest, GetTextExtTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  GetTextExtTestCallback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, GetCaretBounds())
      .WillRepeatedly(
          Invoke(&callback, &GetTextExtTestCallback::GetCaretBounds));

  EXPECT_CALL(text_input_client_, GetCompositionCharacterBounds(_, _))
      .WillRepeatedly(Invoke(
          &callback, &GetTextExtTestCallback::GetCompositionCharacterBounds));

  EXPECT_CALL(text_input_client_, HasCompositionText())
      .WillRepeatedly(
          Invoke(&callback, &GetTextExtTestCallback::ClientHasCompositionText));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &GetTextExtTestCallback::LockGranted));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READ, &result));
  EXPECT_EQ(S_OK, result);
}

TEST_F(TSFTextStoreTest, RequestSupportedAttrs) {
  ui::TextInputClient::EditingContext expected_editing_context;
  expected_editing_context.page_url = GURL("http://example.com");
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  EXPECT_CALL(text_input_client_, GetTextInputMode())
      .WillRepeatedly(Return(TEXT_INPUT_MODE_DEFAULT));
  EXPECT_CALL(text_input_client_, GetTextEditingContext())
      .WillOnce(Return(ui::TextInputClient::EditingContext()))
      .WillOnce(Return(expected_editing_context));

  EXPECT_HRESULT_FAILED(text_store_->RequestSupportedAttrs(0, 1, nullptr));

  const TS_ATTRID kUnknownAttributes[] = {GUID_NULL};
  EXPECT_HRESULT_SUCCEEDED(text_store_->RequestSupportedAttrs(
      0, std::size(kUnknownAttributes), kUnknownAttributes))
      << "Mustn't fail for unknown attributes";

  const TS_ATTRID kAttributes[] = {GUID_NULL, GUID_PROP_INPUTSCOPE, GUID_NULL};
  EXPECT_EQ(S_OK, text_store_->RequestSupportedAttrs(0, std::size(kAttributes),
                                                     kAttributes))
      << "InputScope must be supported";
  const TS_ATTRID urlAttributes[] = {GUID_PROP_URL};
  ui::TextInputClient::EditingContext actual_editing_context =
      text_input_client_.GetTextEditingContext();
  EXPECT_TRUE(actual_editing_context.page_url.is_empty());
  EXPECT_EQ(S_OK, text_store_->RequestSupportedAttrs(
                      0, std::size(urlAttributes), urlAttributes))
      << "Should return S_OK even if URL not supported";

  actual_editing_context = text_input_client_.GetTextEditingContext();
  EXPECT_TRUE(!actual_editing_context.page_url.is_empty());
  EXPECT_TRUE(actual_editing_context.page_url.spec().compare(
                  expected_editing_context.page_url.spec()) == 0);
  EXPECT_EQ(S_OK, text_store_->RequestSupportedAttrs(
                      0, std::size(urlAttributes), urlAttributes))
      << "Expect URL to be supported";

  {
    SCOPED_TRACE("Check if RequestSupportedAttrs fails while focus is lost");
    // Emulate focus lost
    text_store_->SetFocusedTextInputClient(nullptr, nullptr);
    EXPECT_HRESULT_FAILED(text_store_->RequestSupportedAttrs(0, 0, nullptr));
    EXPECT_HRESULT_FAILED(text_store_->RequestSupportedAttrs(
        0, std::size(kAttributes), kAttributes));
  }
}

TEST_F(TSFTextStoreTest, RetrieveRequestedAttrs) {
  ui::TextInputClient::EditingContext expected_editing_context;
  expected_editing_context.page_url = GURL("http://example.com");
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  EXPECT_CALL(text_input_client_, GetTextInputMode())
      .WillRepeatedly(Return(TEXT_INPUT_MODE_DEFAULT));
  EXPECT_CALL(text_input_client_, GetTextEditingContext())
      .WillRepeatedly(Return(expected_editing_context));

  ULONG num_copied = 0xfffffff;
  EXPECT_HRESULT_FAILED(
      text_store_->RetrieveRequestedAttrs(1, nullptr, &num_copied));

  {
    SCOPED_TRACE("Make sure that InputScope is supported");
    TS_ATTRVAL buffer[2] = {};
    num_copied = 0xfffffff;
    const TS_ATTRID kAttributes[] = {GUID_PROP_INPUTSCOPE};
    ASSERT_EQ(S_OK, text_store_->RequestSupportedAttrs(
                        0, std::size(kAttributes), kAttributes));
    ASSERT_EQ(S_OK, text_store_->RetrieveRequestedAttrs(std::size(buffer),
                                                        buffer, &num_copied));
    bool input_scope_found = false;
    for (size_t i = 0; i < num_copied; ++i) {
      base::win::ScopedVariant variant;
      // Move ownership from |buffer[i].varValue| to |variant|.
      std::swap(*variant.Receive(), buffer[i].varValue);
      if (IsEqualGUID(buffer[i].idAttr, GUID_PROP_INPUTSCOPE)) {
        EXPECT_EQ(VT_UNKNOWN, variant.type());
        Microsoft::WRL::ComPtr<ITfInputScope> input_scope;
        EXPECT_HRESULT_SUCCEEDED(variant.AsInput()->punkVal->QueryInterface(
            IID_PPV_ARGS(&input_scope)));
        input_scope_found = true;
        // we do not break here to clean up all the retrieved VARIANTs.
      }
    }
    EXPECT_TRUE(input_scope_found);
  }
  {
    SCOPED_TRACE("Verify URL support");
    TS_ATTRVAL buffer[2] = {};
    num_copied = 0xfffffff;
    base::win::ScopedVariant variant;
    const TS_ATTRID urlAttributes[] = {GUID_PROP_URL};

    // This call should have a valid URL so the URL property should be returned
    // and is expected to match the test_url value set above.
    ASSERT_EQ(S_OK, text_store_->RequestSupportedAttrs(
                        0, std::size(urlAttributes), urlAttributes));
    ASSERT_EQ(S_OK, text_store_->RetrieveRequestedAttrs(std::size(buffer),
                                                        buffer, &num_copied));
    EXPECT_EQ(num_copied, 1U) << "Expect only URL property to be supported";
    EXPECT_TRUE(IsEqualGUID(buffer[0].idAttr, GUID_PROP_URL));
    std::swap(*variant.Receive(), buffer[0].varValue);
    EXPECT_EQ(VT_BSTR, variant.type());
    std::string url_string = base::WideToUTF8(std::wstring(
        variant.ptr()->bstrVal, SysStringLen(variant.ptr()->bstrVal)));
    EXPECT_EQ(expected_editing_context.page_url.spec(), url_string)
        << "Expected url strings to match";
  }
  {
    SCOPED_TRACE("Verify URL and InputScope support");
    TS_ATTRVAL buffer[2] = {};
    num_copied = 0xfffffff;
    const TS_ATTRID inputScopeAndUrlAttributes[] = {GUID_PROP_INPUTSCOPE,
                                                    GUID_PROP_URL};

    // This call should have a valid URL so the URL property should be returned
    // and is expected to match the test_url value set above.
    ASSERT_EQ(S_OK, text_store_->RequestSupportedAttrs(
                        0, std::size(inputScopeAndUrlAttributes),
                        inputScopeAndUrlAttributes));
    ASSERT_EQ(S_OK, text_store_->RetrieveRequestedAttrs(std::size(buffer),
                                                        buffer, &num_copied));
    EXPECT_EQ(num_copied, 2U)
        << "Expect both URL & InputScope properties to be supported";
    for (size_t i = 0; i < num_copied; ++i) {
      base::win::ScopedVariant variant;
      // Move ownership from |buffer[i].varValue| to |variant|.
      std::swap(*variant.Receive(), buffer[i].varValue);
      if (IsEqualGUID(buffer[i].idAttr, GUID_PROP_INPUTSCOPE)) {
        EXPECT_EQ(VT_UNKNOWN, variant.type());
        Microsoft::WRL::ComPtr<ITfInputScope> input_scope;
        EXPECT_HRESULT_SUCCEEDED(variant.AsInput()->punkVal->QueryInterface(
            IID_PPV_ARGS(&input_scope)));
      }
      if (IsEqualGUID(buffer[i].idAttr, GUID_PROP_URL)) {
        EXPECT_EQ(VT_BSTR, variant.type());
        std::string url_string = base::WideToUTF8(std::wstring(
            variant.ptr()->bstrVal, SysStringLen(variant.ptr()->bstrVal)));
        EXPECT_EQ(expected_editing_context.page_url.spec(), url_string)
            << "Expected url strings to match";
      }
      // we do not break here to clean up all the retrieved VARIANTs.
    }
  }

  {
    SCOPED_TRACE("Verify TSATTRID_Text_VerticalWriting support");
    TS_ATTRVAL buffer[2] = {};
    num_copied = 0xfffffff;
    const TS_ATTRID attributes[] = {TSATTRID_Text_VerticalWriting};

    ASSERT_EQ(S_OK, text_store_->RequestSupportedAttrs(0, std::size(attributes),
                                                       attributes));

    EXPECT_CALL(text_input_client_, GetTextInputFlags()).WillOnce(Return(0));
    ASSERT_EQ(S_OK, text_store_->RetrieveRequestedAttrs(std::size(buffer),
                                                        buffer, &num_copied));
    EXPECT_EQ(num_copied, 1U);
    EXPECT_TRUE(IsEqualGUID(buffer[0].idAttr, TSATTRID_Text_VerticalWriting));
    EXPECT_EQ(VT_BOOL, buffer[0].varValue.vt);
    EXPECT_FALSE(buffer[0].varValue.boolVal);

    EXPECT_CALL(text_input_client_, GetTextInputFlags())
        .WillOnce(Return(ui::TEXT_INPUT_FLAG_VERTICAL));
    ASSERT_EQ(S_OK, text_store_->RetrieveRequestedAttrs(std::size(buffer),
                                                        buffer, &num_copied));
    EXPECT_EQ(num_copied, 1U);
    EXPECT_TRUE(IsEqualGUID(buffer[0].idAttr, TSATTRID_Text_VerticalWriting));
    EXPECT_EQ(VT_BOOL, buffer[0].varValue.vt);
    EXPECT_TRUE(buffer[0].varValue.boolVal);
  }

  {
    SCOPED_TRACE("Check if RetrieveRequestedAttrs fails while focus is lost");
    // Emulate focus lost
    text_store_->SetFocusedTextInputClient(nullptr, nullptr);
    num_copied = 0xfffffff;
    TS_ATTRVAL buffer[2] = {};
    EXPECT_HRESULT_FAILED(text_store_->RetrieveRequestedAttrs(
        std::size(buffer), buffer, &num_copied));
  }
}

TEST_F(TSFTextStoreTest, SendOnUrlChanged) {
  text_store_->UseEmptyTextStore(true);
  EXPECT_TRUE(text_store_->MaybeSendOnUrlChanged());

  text_store_->UseEmptyTextStore(false);
  EXPECT_FALSE(text_store_->MaybeSendOnUrlChanged());
}

class KeyEventTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit KeyEventTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  KeyEventTestCallback(const KeyEventTestCallback&) = delete;
  KeyEventTestCallback& operator=(const KeyEventTestCallback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"a", S_OK);

    GetTextTest(0, -1, L"a", 1);

    SetSelectionTest(0, 1, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 1;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(1);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u"a", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(1u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(SK_ColorBLACK, composition.ime_text_spans[0].underline_color);
    EXPECT_EQ(SK_ColorTRANSPARENT,
              composition.ime_text_spans[0].background_color);
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(1u, composition.ime_text_spans[0].end_offset);
    EXPECT_EQ(ImeTextSpan::Thickness::kThin,
              composition.ime_text_spans[0].thickness);
    SetHasCompositionText(true);
  }

  ui::EventDispatchDetails DispatchKeyEventPostIME1(KeyEvent* key) {
    EXPECT_EQ(ui::EventType::kKeyPressed, key->type());
    EXPECT_EQ(VKEY_PROCESSKEY, key->key_code());
    return ui::EventDispatchDetails();
  }

  HRESULT LockGranted2(DWORD flags) {
    SetSelectionTest(1, 1, S_OK);
    InsertTextAtSelectionTest(L"B", 1, 1, 2, 1, 1, 2);
    GetTextTest(0, -1, L"aB", 2);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 1;
    text_span.end_offset = 2;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThick;
    text_spans()->push_back(text_span);

    *edit_flag() = true;
    *composition_start() = 1;
    composition_range()->set_start(1);
    composition_range()->set_end(2);

    text_store_->OnKeyTraceUp(65u, 1966081u);
    text_store_->OnKeyTraceDown(66u, 3145729u);
    return S_OK;
  }

  void InsertText2(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"a", text);
    SetHasCompositionText(false);
  }

  void SetCompositionText2(const ui::CompositionText& composition) {
    EXPECT_EQ(u"B", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(1u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(SK_ColorBLACK, composition.ime_text_spans[0].underline_color);
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(1u, composition.ime_text_spans[0].end_offset);
    EXPECT_EQ(ImeTextSpan::Thickness::kThick,
              composition.ime_text_spans[0].thickness);
    SetHasCompositionText(true);
  }

  ui::EventDispatchDetails DispatchKeyEventPostIME2(KeyEvent* key) {
    EXPECT_EQ(ui::EventType::kKeyReleased, key->type());
    EXPECT_EQ(VKEY_PROCESSKEY, key->key_code());
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails DispatchKeyEventPostIME3a(KeyEvent* key) {
    EXPECT_EQ(ui::EventType::kKeyPressed, key->type());
    EXPECT_EQ(VKEY_PROCESSKEY, key->key_code());
    return ui::EventDispatchDetails();
  }

  HRESULT LockGranted3(DWORD flags) {
    GetTextTest(0, -1, L"aB", 2);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 2;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    return S_OK;
  }

  void InsertText3(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"B", text);
    SetHasCompositionText(false);
  }

  ui::EventDispatchDetails DispatchKeyEventPostIME3b(KeyEvent* key) {
    EXPECT_EQ(ui::EventType::kKeyReleased, key->type());
    EXPECT_EQ(VKEY_PROCESSKEY, key->key_code());
    return ui::EventDispatchDetails();
  }

  HRESULT LockGranted4(DWORD flags) {
    text_store_->OnKeyTraceUp(66u, 3145729u);
    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, KeyEventTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  KeyEventTestCallback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(Invoke(&callback, &KeyEventTestCallback::SetCompositionText1))
      .WillOnce(Invoke(&callback, &KeyEventTestCallback::SetCompositionText2));

  EXPECT_CALL(text_input_client_, InsertText(_, _))
      .WillOnce(Invoke(&callback, &KeyEventTestCallback::InsertText2))
      .WillOnce(Invoke(&callback, &KeyEventTestCallback::InsertText3));

  EXPECT_CALL(ime_key_event_dispatcher_, DispatchKeyEventPostIME(_))
      .WillOnce(
          Invoke(&callback, &KeyEventTestCallback::DispatchKeyEventPostIME1))
      .WillOnce(
          Invoke(&callback, &KeyEventTestCallback::DispatchKeyEventPostIME2))
      .WillOnce(
          Invoke(&callback, &KeyEventTestCallback::DispatchKeyEventPostIME3a))
      .WillOnce(
          Invoke(&callback, &KeyEventTestCallback::DispatchKeyEventPostIME3b));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &KeyEventTestCallback::LockGranted1))
      .WillOnce(Invoke(&callback, &KeyEventTestCallback::LockGranted2))
      .WillOnce(Invoke(&callback, &KeyEventTestCallback::LockGranted3))
      .WillOnce(Invoke(&callback, &KeyEventTestCallback::LockGranted4));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// Below test covers the notification sent to accessibility about the
// composition
class AccessibilityEventTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit AccessibilityEventTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  AccessibilityEventTestCallback(const AccessibilityEventTestCallback&) =
      delete;
  AccessibilityEventTestCallback& operator=(
      const AccessibilityEventTestCallback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"a", S_OK);

    GetTextTest(0, -1, L"a", 1);

    SetSelectionTest(0, 1, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 1;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(1);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u"a", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(1u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(SK_ColorBLACK, composition.ime_text_spans[0].underline_color);
    EXPECT_EQ(SK_ColorTRANSPARENT,
              composition.ime_text_spans[0].background_color);
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(1u, composition.ime_text_spans[0].end_offset);
    EXPECT_EQ(ImeTextSpan::Thickness::kThin,
              composition.ime_text_spans[0].thickness);
    SetHasCompositionText(true);
  }

  void SetActiveCompositionForAccessibility1(
      const gfx::Range& range,
      const std::u16string& active_composition_text,
      bool committed_composition) {
    EXPECT_EQ(u"a", active_composition_text);
    EXPECT_EQ(0u, range.start());
    EXPECT_EQ(1u, range.end());
  }
};

TEST_F(TSFTextStoreTest, AccessibilityEventTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  AccessibilityEventTestCallback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(Invoke(&callback,
                       &AccessibilityEventTestCallback::SetCompositionText1));

  EXPECT_CALL(text_input_client_, SetActiveCompositionForAccessibility(_, _, _))
      .WillOnce(Invoke(&callback, &AccessibilityEventTestCallback::
                                      SetActiveCompositionForAccessibility1));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(
          Invoke(&callback, &AccessibilityEventTestCallback::LockGranted1));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// Summary of test scenarios:
// 1.  renderer proc changes buffer from "" to "a".
// 2.  input service changes buffer from "a" to "abcde".
// 3.  renderer proc changes buffer from "abcde" to "about".
// 4.  renderer proc changes buffer from "about" to "abFGt".
// 5.  renderer proc changes buffer from "abFGt" to "aHIGt".
// 6.  renderer proc changes buffer from "aHIGt" to "JKLMN".
// 7.  renderer proc changes buffer from "JKLMN" to "".
// 8.  renderer proc changes buffer from "" to "OPQ".
// 9.  renderer proc changes buffer from "OPQ" to "OPR".
// 10. renderer proc changes buffer from "OPR" to "SPR".
// 11. renderer proc changes buffer from "SPR" to "STPR".
// 12. renderer proc changes buffer from "STPR" to "PR".
// 13. renderer proc changes buffer from "PR" to "UPR".
// 14. renderer proc changes buffer from "UPR" to "UPVWR".
class DiffingAlgorithmTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit DiffingAlgorithmTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  DiffingAlgorithmTestCallback(const DiffingAlgorithmTestCallback&) = delete;
  DiffingAlgorithmTestCallback& operator=(const DiffingAlgorithmTestCallback&) =
      delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"", S_OK);
    GetTextTest(0, -1, L"", 0);

    SetTextRange(0, 1);
    SetTextBuffer(u"a");
    SetSelectionRange(1, 1);
    *composition_start() = 1;
    return S_OK;
  }

  HRESULT OnTextChange1(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(0, pChange->acpStart);
    EXPECT_EQ(0, pChange->acpOldEnd);
    EXPECT_EQ(1, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted1a(DWORD flags) {
    GetTextTest(0, -1, L"a", 1);

    return S_OK;
  }

  HRESULT OnSelectionChange1() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted1b(DWORD flags) {
    GetSelectionTest(1, 1);
    return S_OK;
  }

  HRESULT LockGranted2(DWORD flags) {
    SetTextTest(1, 1, L"bcde", S_OK);
    GetTextTest(0, -1, L"abcde", 5);
    SetSelectionTest(5, 5, S_OK);

    *edit_flag() = true;
    *composition_start() = 5;
    return S_OK;
  }

  void InsertText2(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"bcde", text);
    SetTextRange(0, 5);
    SetSelectionRange(5, 5);
    SetTextBuffer(u"abcde");
  }

  HRESULT LockGranted3(DWORD flags) {
    SetTextRange(0, 5);
    SetTextBuffer(u"about");
    SetSelectionRange(0, 5);
    return S_OK;
  }

  HRESULT OnTextChange3(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(2, pChange->acpStart);
    EXPECT_EQ(5, pChange->acpOldEnd);
    EXPECT_EQ(5, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted3a(DWORD flags) {
    GetTextTest(1, 5, L"bout", 5);

    return S_OK;
  }

  HRESULT OnSelectionChange3() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted3b(DWORD flags) {
    GetSelectionTest(0, 5);
    return S_OK;
  }

  HRESULT LockGranted4(DWORD flags) {
    SetTextRange(0, 5);
    SetTextBuffer(u"abFGt");
    SetSelectionRange(3, 4);
    return S_OK;
  }

  HRESULT OnTextChange4(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(2, pChange->acpStart);
    EXPECT_EQ(4, pChange->acpOldEnd);
    EXPECT_EQ(4, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted4a(DWORD flags) {
    GetTextTest(2, 4, L"FG", 4);

    return S_OK;
  }

  HRESULT OnSelectionChange4() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted4b(DWORD flags) {
    GetSelectionTest(3, 4);
    return S_OK;
  }

  HRESULT LockGranted5(DWORD flags) {
    SetTextRange(0, 3);
    SetTextBuffer(u"aHI");
    SetSelectionRange(3, 3);
    return S_OK;
  }

  HRESULT OnTextChange5(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(1, pChange->acpStart);
    EXPECT_EQ(5, pChange->acpOldEnd);
    EXPECT_EQ(3, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted5a(DWORD flags) {
    GetTextTest(1, 3, L"HI", 3);

    return S_OK;
  }

  HRESULT OnSelectionChange5() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted5b(DWORD flags) {
    GetSelectionTest(3, 3);
    return S_OK;
  }

  HRESULT LockGranted6(DWORD flags) {
    SetTextRange(0, 5);
    SetTextBuffer(u"JKLMN");
    SetSelectionRange(2, 5);
    return S_OK;
  }

  HRESULT OnTextChange6(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(0, pChange->acpStart);
    EXPECT_EQ(3, pChange->acpOldEnd);
    EXPECT_EQ(5, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted6a(DWORD flags) {
    GetTextTest(3, 5, L"MN", 5);

    return S_OK;
  }

  HRESULT OnSelectionChange6() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted6b(DWORD flags) {
    GetSelectionTest(2, 5);
    return S_OK;
  }

  HRESULT LockGranted7(DWORD flags) {
    SetTextRange(0, 0);
    SetTextBuffer(u"");
    SetSelectionRange(0, 0);
    return S_OK;
  }

  HRESULT OnTextChange7(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(0, pChange->acpStart);
    EXPECT_EQ(5, pChange->acpOldEnd);
    EXPECT_EQ(0, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted7a(DWORD flags) {
    GetTextTest(0, -1, L"", 0);

    return S_OK;
  }

  HRESULT OnSelectionChange7() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted7b(DWORD flags) {
    GetSelectionTest(0, 0);
    return S_OK;
  }

  HRESULT LockGranted8(DWORD flags) {
    SetTextRange(0, 3);
    SetTextBuffer(u"OPQ");
    SetSelectionRange(0, 2);
    return S_OK;
  }

  HRESULT OnTextChange8(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(0, pChange->acpStart);
    EXPECT_EQ(0, pChange->acpOldEnd);
    EXPECT_EQ(3, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted8a(DWORD flags) {
    GetTextTest(0, -1, L"OPQ", 3);

    return S_OK;
  }

  HRESULT OnSelectionChange8() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted8b(DWORD flags) {
    GetSelectionTest(0, 2);
    return S_OK;
  }

  HRESULT LockGranted9(DWORD flags) {
    SetTextRange(0, 3);
    SetTextBuffer(u"OPR");
    SetSelectionRange(2, 3);
    return S_OK;
  }

  HRESULT OnTextChange9(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(2, pChange->acpStart);
    EXPECT_EQ(3, pChange->acpOldEnd);
    EXPECT_EQ(3, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted9a(DWORD flags) {
    GetTextTest(2, 3, L"R", 3);

    return S_OK;
  }

  HRESULT OnSelectionChange9() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted9b(DWORD flags) {
    GetSelectionTest(2, 3);
    return S_OK;
  }

  HRESULT LockGranted10(DWORD flags) {
    SetTextRange(0, 3);
    SetTextBuffer(u"SPR");
    SetSelectionRange(0, 1);
    return S_OK;
  }

  HRESULT OnTextChange10(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(0, pChange->acpStart);
    EXPECT_EQ(1, pChange->acpOldEnd);
    EXPECT_EQ(1, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted10a(DWORD flags) {
    GetTextTest(0, 1, L"S", 1);

    return S_OK;
  }

  HRESULT OnSelectionChange10() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted10b(DWORD flags) {
    GetSelectionTest(0, 1);
    return S_OK;
  }
  // 11. renderer proc changes buffer from "SPR" to "STPR".
  HRESULT LockGranted11(DWORD flags) {
    SetTextRange(0, 4);
    SetTextBuffer(u"STPR");
    SetSelectionRange(2, 2);
    return S_OK;
  }

  HRESULT OnTextChange11(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(1, pChange->acpStart);
    EXPECT_EQ(1, pChange->acpOldEnd);
    EXPECT_EQ(2, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted11a(DWORD flags) {
    GetTextTest(1, 2, L"T", 2);

    return S_OK;
  }

  HRESULT OnSelectionChange11() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted11b(DWORD flags) {
    GetSelectionTest(2, 2);
    return S_OK;
  }

  // 12. renderer proc changes buffer from "STPR" to "PR".
  HRESULT LockGranted12(DWORD flags) {
    SetTextRange(0, 2);
    SetTextBuffer(u"PR");
    SetSelectionRange(0, 0);
    return S_OK;
  }

  HRESULT OnTextChange12(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(0, pChange->acpStart);
    EXPECT_EQ(2, pChange->acpOldEnd);
    EXPECT_EQ(0, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted12a(DWORD flags) {
    GetTextTest(0, 2, L"PR", 2);

    return S_OK;
  }

  HRESULT OnSelectionChange12() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted12b(DWORD flags) {
    GetSelectionTest(0, 0);
    return S_OK;
  }

  // 13. renderer proc changes buffer from "PR" to "UPR".
  HRESULT LockGranted13(DWORD flags) {
    SetTextRange(0, 3);
    SetTextBuffer(u"UPR");
    SetSelectionRange(1, 1);
    return S_OK;
  }

  HRESULT OnTextChange13(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(0, pChange->acpStart);
    EXPECT_EQ(0, pChange->acpOldEnd);
    EXPECT_EQ(1, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted13a(DWORD flags) {
    GetTextTest(0, 1, L"U", 1);

    return S_OK;
  }

  HRESULT OnSelectionChange13() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted13b(DWORD flags) {
    GetSelectionTest(1, 1);
    return S_OK;
  }

  // 14. renderer proc changes buffer from "UPR" to "UPVWR".
  HRESULT LockGranted14(DWORD flags) {
    SetTextRange(0, 5);
    SetTextBuffer(u"UPVWR");
    SetSelectionRange(4, 4);
    return S_OK;
  }

  HRESULT OnTextChange14(DWORD flag, const TS_TEXTCHANGE* pChange) {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(2, pChange->acpStart);
    EXPECT_EQ(2, pChange->acpOldEnd);
    EXPECT_EQ(4, pChange->acpNewEnd);

    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted14a(DWORD flags) {
    GetTextTest(2, 4, L"VW", 4);

    return S_OK;
  }

  HRESULT OnSelectionChange14() {
    HRESULT result = S_OK;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
    return S_OK;
  }

  HRESULT LockGranted14b(DWORD flags) {
    GetSelectionTest(4, 4);
    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, DiffingAlgorithmTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  DiffingAlgorithmTestCallback callback(text_store_.get());

  EXPECT_CALL(*sink_, OnTextChange(_, _))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange1))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange3))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange4))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange5))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange6))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange7))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange8))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange9))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange10))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange11))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange12))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange13))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnTextChange14));

  EXPECT_CALL(*sink_, OnSelectionChange())
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnSelectionChange1))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnSelectionChange3))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnSelectionChange4))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnSelectionChange5))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnSelectionChange6))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnSelectionChange7))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnSelectionChange8))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnSelectionChange9))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnSelectionChange10))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnSelectionChange11))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnSelectionChange12))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::OnSelectionChange13))
      .WillOnce(Invoke(&callback,
                       &DiffingAlgorithmTestCallback::OnSelectionChange14));

  EXPECT_CALL(text_input_client_, InsertText(_, _))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::InsertText2));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted1))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted1a))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted1b))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted2))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted3))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted3a))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted3b))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted4))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted4a))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted4b))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted5))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted5a))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted5b))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted6))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted6a))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted6b))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted7))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted7a))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted7b))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted8))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted8a))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted8b))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted9))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted9a))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted9b))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted10))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted10a))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted10b))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted11))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted11a))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted11b))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted12))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted12a))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted12b))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted13))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted13a))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted13b))
      .WillOnce(Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted14))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted14a))
      .WillOnce(
          Invoke(&callback, &DiffingAlgorithmTestCallback::LockGranted14b));

  ON_CALL(text_input_client_, GetCompositionTextRange(_))
      .WillByDefault(Invoke(
          &callback, &TSFTextStoreTestCallback::GetCompositionTextRange));

  ON_CALL(text_input_client_, GetTextRange(_))
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::GetTextRange));

  ON_CALL(text_input_client_, GetTextFromRange(_, _))
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::GetTextFromRange));

  ON_CALL(text_input_client_, GetEditableSelectionRange(_))
      .WillByDefault(Invoke(
          &callback, &TSFTextStoreTestCallback::GetEditableSelectionRange));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// regression tests for crbug.com/944452.
// This test covers several corner cases:
// 1. User may commit existing composition and start new composition in the same
//    edit session with same composition text.
// 2. some third-party IMEs use SetText() API instead of InsertTextAtSelection()
//    API to insert new composition text. We should allow IMEs to use both
//    APIs to insert new text.
// 3. Some Japanese IMEs such as CorvusSKK can start and end composition with
//    single key stroke. We should still fire keydown/keyup event for such case.
class RegressionTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTestCallback(const RegressionTestCallback&) = delete;
  RegressionTestCallback& operator=(const RegressionTestCallback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"a", S_OK);
    GetTextTest(0, -1, L"a", 1);
    SetSelectionTest(0, 1, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 1;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(1);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    *has_composition_range() = true;

    return S_OK;
  }

  ui::EventDispatchDetails DispatchKeyEventPostIME1(KeyEvent* key) {
    EXPECT_EQ(ui::EventType::kKeyPressed, key->type());
    EXPECT_EQ(VKEY_PROCESSKEY, key->key_code());
    return ui::EventDispatchDetails();
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u"a", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(1u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(SK_ColorBLACK, composition.ime_text_spans[0].underline_color);
    EXPECT_EQ(SK_ColorTRANSPARENT,
              composition.ime_text_spans[0].background_color);
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(1u, composition.ime_text_spans[0].end_offset);
    EXPECT_EQ(ImeTextSpan::Thickness::kThin,
              composition.ime_text_spans[0].thickness);
    SetHasCompositionText(true);
  }

  // commit existing composition from [0,1] and start new composition at [1,2]
  HRESULT LockGranted2(DWORD flags) {
    SetSelectionTest(1, 1, S_OK);
    InsertTextAtSelectionTest(L"a", 1, 1, 2, 1, 1, 2);
    GetTextTest(0, -1, L"aa", 2);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 1;
    text_span.end_offset = 2;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThick;
    text_spans()->push_back(text_span);

    *edit_flag() = true;
    *composition_start() = 1;
    composition_range()->set_start(1);
    composition_range()->set_end(2);

    text_store_->OnKeyTraceUp(65u, 1966081u);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    return S_OK;
  }

  ui::EventDispatchDetails DispatchKeyEventPostIME2a(KeyEvent* key) {
    EXPECT_EQ(ui::EventType::kKeyReleased, key->type());
    EXPECT_EQ(VKEY_PROCESSKEY, key->key_code());
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails DispatchKeyEventPostIME2b(KeyEvent* key) {
    EXPECT_EQ(ui::EventType::kKeyPressed, key->type());
    EXPECT_EQ(VKEY_PROCESSKEY, key->key_code());
    return ui::EventDispatchDetails();
  }

  void InsertText2(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"a", text);
    SetHasCompositionText(false);
  }

  void SetCompositionText2(const ui::CompositionText& composition) {
    EXPECT_EQ(u"a", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(1u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(SK_ColorBLACK, composition.ime_text_spans[0].underline_color);
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(1u, composition.ime_text_spans[0].end_offset);
    EXPECT_EQ(ImeTextSpan::Thickness::kThick,
              composition.ime_text_spans[0].thickness);
    SetHasCompositionText(true);
  }

  HRESULT LockGranted3(DWORD flags) {
    GetTextTest(0, -1, L"aa", 2);
    SetSelectionTest(2, 2, S_OK);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 2;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    return S_OK;
  }

  void InsertText3(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"a", text);
    SetHasCompositionText(false);
  }

  // Insert new composition text using SetText().
  // We also fire key events here.
  HRESULT LockGranted4(DWORD flags) {
    SetTextTest(2, 2, L"b", S_OK);
    GetTextTest(0, -1, L"aab", 3);
    SetTextTest(2, 3, L"c", S_OK);
    SetSelectionTest(3, 3, S_OK);

    *edit_flag() = true;
    *composition_start() = 3;

    text_store_->OnKeyTraceUp(65u, 1966081u);
    text_store_->OnStartComposition(nullptr, nullptr);
    text_store_->OnEndComposition(nullptr);
    return S_OK;
  }

  ui::EventDispatchDetails DispatchKeyEventPostIME4(KeyEvent* key) {
    EXPECT_EQ(ui::EventType::kKeyReleased, key->type());
    EXPECT_EQ(VKEY_PROCESSKEY, key->key_code());
    return ui::EventDispatchDetails();
  }

  // We expect this call since the composition was started and committed during
  // same edit session.
  void SetCompositionText4(const ui::CompositionText& composition) {
    EXPECT_EQ(u"c", composition.text);
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    ASSERT_EQ(gfx::Range(1, 1), composition.selection);
  }

  void InsertText4(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"c", text);
  }

  HRESULT LockGranted5(DWORD flags) {
    GetTextTest(0, -1, L"aac", 3);
    SetTextTest(3, 3, L"d", S_OK);
    GetTextTest(0, -1, L"aacd", 4);
    SetSelectionTest(3, 4, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 3;
    text_span.end_offset = 4;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 3;
    composition_range()->set_start(3);
    composition_range()->set_end(4);

    text_store_->OnKeyTraceDown(65u, 1966081u);

    *has_composition_range() = true;

    return S_OK;
  }

  ui::EventDispatchDetails DispatchKeyEventPostIME5(KeyEvent* key) {
    EXPECT_EQ(ui::EventType::kKeyPressed, key->type());
    EXPECT_EQ(VKEY_PROCESSKEY, key->key_code());
    return ui::EventDispatchDetails();
  }

  void SetCompositionText5(const ui::CompositionText& composition) {
    EXPECT_EQ(u"d", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(1u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(SK_ColorBLACK, composition.ime_text_spans[0].underline_color);
    EXPECT_EQ(SK_ColorTRANSPARENT,
              composition.ime_text_spans[0].background_color);
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(1u, composition.ime_text_spans[0].end_offset);
    EXPECT_EQ(ImeTextSpan::Thickness::kThin,
              composition.ime_text_spans[0].thickness);
    SetHasCompositionText(true);
  }

  // change existing composition text using SetText().
  HRESULT LockGranted6(DWORD flags) {
    SetTextTest(3, 4, L"e", S_OK);
    SetSelectionTest(4, 4, S_OK);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 4;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    return S_OK;
  }

  void InsertText6(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"e", text);
    SetHasCompositionText(false);
  }
};

TEST_F(TSFTextStoreTest, RegressionTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTestCallback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::SetCompositionText1))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::SetCompositionText2))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::SetCompositionText4))
      .WillOnce(
          Invoke(&callback, &RegressionTestCallback::SetCompositionText5));

  EXPECT_CALL(ime_key_event_dispatcher_, DispatchKeyEventPostIME(_))
      .WillOnce(
          Invoke(&callback, &RegressionTestCallback::DispatchKeyEventPostIME1))
      .WillOnce(
          Invoke(&callback, &RegressionTestCallback::DispatchKeyEventPostIME2a))
      .WillOnce(
          Invoke(&callback, &RegressionTestCallback::DispatchKeyEventPostIME2b))
      .WillOnce(
          Invoke(&callback, &RegressionTestCallback::DispatchKeyEventPostIME4))
      .WillOnce(
          Invoke(&callback, &RegressionTestCallback::DispatchKeyEventPostIME5));

  EXPECT_CALL(text_input_client_, InsertText(_, _))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::InsertText2))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::InsertText3))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::InsertText4))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::InsertText6));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::LockGranted2))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::LockGranted3))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::LockGranted4))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::LockGranted5))
      .WillOnce(Invoke(&callback, &RegressionTestCallback::LockGranted6));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// Some IMEs may replace existing text with new text and start new composition
// on the new text. We should replace old text with new text and start new
// composition. This test covers the above scenario.
class RegressionTest2Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest2Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest2Callback(const RegressionTest2Callback&) = delete;
  RegressionTest2Callback& operator=(const RegressionTest2Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"abc", S_OK);
    SetSelectionTest(3, 3, S_OK);

    *composition_start() = 3;
    return S_OK;
  }

  HRESULT LockGranted2(DWORD flags) {
    GetTextTest(0, -1, L"abc", 3);
    SetTextTest(1, 3, L"DE", S_OK);
    GetTextTest(0, -1, L"aDE", 3);
    SetSelectionTest(3, 3, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 1;
    text_span.end_offset = 3;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 1;
    composition_range()->set_start(1);
    composition_range()->set_end(3);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText2(const ui::CompositionText& composition) {
    EXPECT_EQ(u"DE", composition.text);
    EXPECT_EQ(2u, composition.selection.start());
    EXPECT_EQ(2u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(SK_ColorBLACK, composition.ime_text_spans[0].underline_color);
    EXPECT_EQ(SK_ColorTRANSPARENT,
              composition.ime_text_spans[0].background_color);
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(2u, composition.ime_text_spans[0].end_offset);
    EXPECT_EQ(ImeTextSpan::Thickness::kThin,
              composition.ime_text_spans[0].thickness);
    SetHasCompositionText(true);
  }

  HRESULT LockGranted3(DWORD flags) {
    GetTextTest(0, -1, L"aDE", 3);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 3;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    return S_OK;
  }

  void InsertText3(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"DE", text);
    SetHasCompositionText(false);
  }
};

TEST_F(TSFTextStoreTest, RegressionTest2) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest2Callback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback, &RegressionTest2Callback::SetCompositionText2));

  EXPECT_CALL(text_input_client_, InsertText(_, _))
      .WillOnce(Invoke(&callback, &RegressionTest2Callback::InsertText3));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest2Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest2Callback::LockGranted2))
      .WillOnce(Invoke(&callback, &RegressionTest2Callback::LockGranted3));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// Due to crbug.com/978678, we should not call TextInputClient::InsertText if
// provided text is empty.
class RegressionTest3Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest3Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest3Callback(const RegressionTest3Callback&) = delete;
  RegressionTest3Callback& operator=(const RegressionTest3Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    GetTextTest(0, -1, L"", 0);
    SetTextTest(0, 0, L"a", S_OK);
    GetTextTest(0, -1, L"a", 1);
    SetSelectionTest(0, 1, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 1;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(1);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u"a", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(1u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(SK_ColorBLACK, composition.ime_text_spans[0].underline_color);
    EXPECT_EQ(SK_ColorTRANSPARENT,
              composition.ime_text_spans[0].background_color);
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(1u, composition.ime_text_spans[0].end_offset);
    EXPECT_EQ(ImeTextSpan::Thickness::kThin,
              composition.ime_text_spans[0].thickness);
    SetHasCompositionText(true);
  }

  HRESULT LockGranted2(DWORD flags) {
    GetTextTest(0, -1, L"a", 1);
    SetTextTest(0, 1, L"", S_OK);
    GetTextTest(0, -1, L"", 0);
    SetSelectionTest(0, 0, S_OK);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    return S_OK;
  }

  void SetCompositionText2(const ui::CompositionText& composition) {
    EXPECT_EQ(std::u16string(), composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(0u, composition.selection.end());
    ASSERT_EQ(0u, composition.ime_text_spans.size());
  }
};

TEST_F(TSFTextStoreTest, RegressionTest3) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest3Callback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback, &RegressionTest3Callback::SetCompositionText1))
      .WillOnce(
          Invoke(&callback, &RegressionTest3Callback::SetCompositionText2));

  EXPECT_CALL(text_input_client_, InsertText(_, _)).Times(0);

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest3Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest3Callback::LockGranted2));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// Due to crbug.com/978678, we should not call TextInputClient::InsertText if
// provided text is empty. In fact, we should call TextInputClient::InsertText
// with current composition text to commit composition without losing text.
class RegressionTest4Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest4Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest4Callback(const RegressionTest4Callback&) = delete;
  RegressionTest4Callback& operator=(const RegressionTest4Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    GetTextTest(0, -1, L"", 0);
    SetTextTest(0, 0, L"a", S_OK);
    GetTextTest(0, -1, L"a", 1);
    SetSelectionTest(0, 1, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 1;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(1);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u"a", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(1u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(SK_ColorBLACK, composition.ime_text_spans[0].underline_color);
    EXPECT_EQ(SK_ColorTRANSPARENT,
              composition.ime_text_spans[0].background_color);
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(1u, composition.ime_text_spans[0].end_offset);
    EXPECT_EQ(ImeTextSpan::Thickness::kThin,
              composition.ime_text_spans[0].thickness);
    SetHasCompositionText(true);
  }

  HRESULT LockGranted2(DWORD flags) {
    GetTextTest(0, -1, L"a", 1);
    SetSelectionTest(1, 1, S_OK);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    return S_OK;
  }

  void InsertText2(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"a", text);
    SetHasCompositionText(false);
  }
};

TEST_F(TSFTextStoreTest, RegressionTest4) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest4Callback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback, &RegressionTest4Callback::SetCompositionText1));

  EXPECT_CALL(text_input_client_, InsertText(_, _))
      .WillOnce(Invoke(&callback, &RegressionTest4Callback::InsertText2));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest4Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest4Callback::LockGranted2));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// regression tests for crbug.com/1006067.
// We should call |TextInputClient::SetCompositionText()| if ImeTextSpans are
// changed from previous edit session during same composition even though
// composition string and composition selection remain unchanged.
class RegressionTest5Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest5Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest5Callback(const RegressionTest5Callback&) = delete;
  RegressionTest5Callback& operator=(const RegressionTest5Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"aa", S_OK);
    GetTextTest(0, -1, L"aa", 2);
    SetSelectionTest(2, 2, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span_1;
    text_span_1.start_offset = 0;
    text_span_1.end_offset = 1;
    text_span_1.underline_color = SK_ColorBLACK;
    text_span_1.thickness = ImeTextSpan::Thickness::kThick;
    text_span_1.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span_1);
    ImeTextSpan text_span_2;
    text_span_2.start_offset = 1;
    text_span_2.end_offset = 2;
    text_span_2.underline_color = SK_ColorBLACK;
    text_span_2.thickness = ImeTextSpan::Thickness::kThin;
    text_span_2.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span_2);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(2);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u"aa", composition.text);
    EXPECT_EQ(2u, composition.selection.start());
    EXPECT_EQ(2u, composition.selection.end());
    ASSERT_EQ(2u, composition.ime_text_spans.size());
    EXPECT_EQ(ImeTextSpan::Thickness::kThick,
              composition.ime_text_spans[0].thickness);
    EXPECT_EQ(ImeTextSpan::Thickness::kThin,
              composition.ime_text_spans[1].thickness);
    SetHasCompositionText(true);
  }

  // Only change underline thickness in IME spans. Other states (composition
  // string, selection) remain unchanged.
  HRESULT LockGranted2(DWORD flags) {
    GetTextTest(0, -1, L"aa", 2);

    text_spans()->clear();
    ImeTextSpan text_span_1;
    text_span_1.start_offset = 0;
    text_span_1.end_offset = 1;
    text_span_1.underline_color = SK_ColorBLACK;
    text_span_1.thickness = ImeTextSpan::Thickness::kThin;
    text_span_1.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span_1);
    ImeTextSpan text_span_2;
    text_span_2.start_offset = 1;
    text_span_2.end_offset = 2;
    text_span_2.underline_color = SK_ColorBLACK;
    text_span_2.thickness = ImeTextSpan::Thickness::kThick;
    text_span_2.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span_2);

    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(2);

    text_store_->OnKeyTraceUp(65u, 1966081u);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    return S_OK;
  }

  void SetCompositionText2(const ui::CompositionText& composition) {
    EXPECT_EQ(u"aa", composition.text);
    EXPECT_EQ(2u, composition.selection.start());
    EXPECT_EQ(2u, composition.selection.end());
    ASSERT_EQ(2u, composition.ime_text_spans.size());
    EXPECT_EQ(ImeTextSpan::Thickness::kThin,
              composition.ime_text_spans[0].thickness);
    EXPECT_EQ(ImeTextSpan::Thickness::kThick,
              composition.ime_text_spans[1].thickness);
    SetHasCompositionText(true);
  }

  HRESULT LockGranted3(DWORD flags) {
    GetTextTest(0, -1, L"aa", 2);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 2;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    text_store_->OnKeyTraceUp(65u, 1966081u);
    return S_OK;
  }

  void InsertText3(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"aa", text);
    SetHasCompositionText(false);
  }
};

TEST_F(TSFTextStoreTest, RegressionTest5) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest5Callback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback, &RegressionTest5Callback::SetCompositionText1))
      .WillOnce(
          Invoke(&callback, &RegressionTest5Callback::SetCompositionText2));

  EXPECT_CALL(text_input_client_, InsertText(_, _))
      .WillOnce(Invoke(&callback, &RegressionTest5Callback::InsertText3));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest5Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest5Callback::LockGranted2))
      .WillOnce(Invoke(&callback, &RegressionTest5Callback::LockGranted3));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// regression tests for crbug.com/1013472.
// We should reset |new_text_inserted_| at the end of
// |TSFTextStore::RequestLock| since the text should have been already
// inserted/replaced.
class RegressionTest6Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest6Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest6Callback(const RegressionTest6Callback&) = delete;
  RegressionTest6Callback& operator=(const RegressionTest6Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    EXPECT_EQ(false, *new_text_inserted());
    SetTextTest(0, 0, L"a", S_OK);
    GetTextTest(0, -1, L"a", 1);
    SetSelectionTest(1, 1, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span_1;
    text_span_1.start_offset = 0;
    text_span_1.end_offset = 2;
    text_span_1.underline_color = SK_ColorBLACK;
    text_span_1.thickness = ImeTextSpan::Thickness::kThick;
    text_span_1.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span_1);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(2);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u"a", composition.text);
    EXPECT_EQ(1u, composition.selection.start());
    EXPECT_EQ(1u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(true, *new_text_inserted());
    SetHasCompositionText(true);
  }

  HRESULT LockGranted2(DWORD flags) {
    EXPECT_EQ(false, *new_text_inserted());
    GetTextTest(0, -1, L"a", 1);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 1;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    text_store_->OnKeyTraceUp(65u, 1966081u);
    return S_OK;
  }

  void InsertText2(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"a", text);
    SetHasCompositionText(false);
  }
};

TEST_F(TSFTextStoreTest, RegressionTest6) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest6Callback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback, &RegressionTest6Callback::SetCompositionText1));

  EXPECT_CALL(text_input_client_, InsertText(_, _))
      .WillOnce(Invoke(&callback, &RegressionTest6Callback::InsertText2));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest6Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest6Callback::LockGranted2));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

class UnderlineStyleTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit UnderlineStyleTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  UnderlineStyleTestCallback(const UnderlineStyleTestCallback&) = delete;
  UnderlineStyleTestCallback& operator=(const UnderlineStyleTestCallback&) =
      delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"a", S_OK);

    GetTextTest(0, -1, L"a", 1);

    SetSelectionTest(0, 1, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 1;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.underline_style = ImeTextSpan::UnderlineStyle::kSolid;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(1);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u"a", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(1u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(SK_ColorBLACK, composition.ime_text_spans[0].underline_color);
    EXPECT_EQ(SK_ColorTRANSPARENT,
              composition.ime_text_spans[0].background_color);
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(1u, composition.ime_text_spans[0].end_offset);
    EXPECT_EQ(ImeTextSpan::Thickness::kThin,
              composition.ime_text_spans[0].thickness);
    EXPECT_EQ(ImeTextSpan::UnderlineStyle::kSolid,
              composition.ime_text_spans[0].underline_style);
    SetHasCompositionText(true);
  }
};

TEST_F(TSFTextStoreTest, UnderlineStyleTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  UnderlineStyleTestCallback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback, &UnderlineStyleTestCallback::SetCompositionText1));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &UnderlineStyleTestCallback::LockGranted1));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// regression tests for crbug.com/1013154.
// We should remove selected text before start composition on existing text.
class RegressionTest7Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest7Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest7Callback(const RegressionTest7Callback&) = delete;
  RegressionTest7Callback& operator=(const RegressionTest7Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"aaaa", S_OK);
    SetSelectionTest(0, 4, S_OK);
    return S_OK;
  }

  HRESULT LockGranted2(DWORD flags) {
    GetTextTest(0, -1, L"aaaa", 4);
    SetTextTest(1, 4, L"", S_OK);
    SetSelectionTest(1, 1, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span_1;
    text_span_1.start_offset = 0;
    text_span_1.end_offset = 1;
    text_span_1.underline_color = SK_ColorBLACK;
    text_span_1.thickness = ImeTextSpan::Thickness::kThin;
    text_span_1.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span_1);

    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(1);
    *has_composition_range() = true;

    text_store_->OnKeyTraceDown(65u, 1966081u);
    text_store_->OnStartComposition(nullptr, nullptr);
    return S_OK;
  }

  bool SetCompositionFromExistingText2(
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& text_spans) {
    EXPECT_EQ(0u, range.start());
    EXPECT_EQ(1u, range.end());
    EXPECT_EQ(1u, text_spans.size());
    SetHasCompositionText(true);
    return true;
  }

  HRESULT LockGranted3(DWORD flags) {
    GetTextTest(0, -1, L"a", 1);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 1;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    text_store_->OnKeyTraceUp(65u, 1966081u);
    return S_OK;
  }

  void InsertText3(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"a", text);
    SetHasCompositionText(false);
  }
};

TEST_F(TSFTextStoreTest, RegressionTest7) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest7Callback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionFromExistingText(_, _))
      .WillOnce(
          Invoke(&callback,
                 &RegressionTest7Callback::SetCompositionFromExistingText2));

  EXPECT_CALL(text_input_client_, InsertText(_, _))
      .WillOnce(Invoke(&callback, &RegressionTest7Callback::InsertText3));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest7Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest7Callback::LockGranted2))
      .WillOnce(Invoke(&callback, &RegressionTest7Callback::LockGranted3));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// regression tests for crbug.com/1091069.
// We should allow inserting empty compositon string to cancel composition.
class RegressionTest8Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest8Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest8Callback(const RegressionTest8Callback&) = delete;
  RegressionTest8Callback& operator=(const RegressionTest8Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"bbbb", S_OK);
    SetSelectionTest(0, 4, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span_1;
    text_span_1.start_offset = 0;
    text_span_1.end_offset = 4;
    text_span_1.underline_color = SK_ColorBLACK;
    text_span_1.thickness = ImeTextSpan::Thickness::kThin;
    text_span_1.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span_1);

    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(4);
    *has_composition_range() = true;

    text_store_->OnKeyTraceDown(65u, 1966081u);
    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u"bbbb", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(4u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(4u, composition.ime_text_spans[0].end_offset);
    SetHasCompositionText(true);
  }

  HRESULT LockGranted2(DWORD flags) {
    GetTextTest(0, -1, L"bbbb", 4);
    SetTextTest(0, 4, L"", S_OK);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    text_store_->OnKeyTraceUp(65u, 1966081u);
    return S_OK;
  }

  void ClearCompositionText2() { EXPECT_EQ(false, *has_composition_range()); }
};

TEST_F(TSFTextStoreTest, RegressionTest8) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest8Callback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback, &RegressionTest8Callback::SetCompositionText1));

  EXPECT_CALL(text_input_client_, ClearCompositionText())
      .WillOnce(
          Invoke(&callback, &RegressionTest8Callback::ClearCompositionText2));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest8Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest8Callback::LockGranted2));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// We should use the last composition end pos instead of the cached
// |coposition_start_| to calculate last composition.
class RegressionTest9Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest9Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest9Callback(const RegressionTest9Callback&) = delete;
  RegressionTest9Callback& operator=(const RegressionTest9Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"a", S_OK);
    SetSelectionTest(1, 1, S_OK);
    *composition_start() = 1;
    return S_OK;
  }

  HRESULT LockGranted2(DWORD flags) {
    GetTextTest(0, -1, L"a", 1);
    SetTextTest(1, 1, L"bbbb", S_OK);
    SetSelectionTest(1, 5, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span_1;
    text_span_1.start_offset = 0;
    text_span_1.end_offset = 4;
    text_span_1.underline_color = SK_ColorBLACK;
    text_span_1.thickness = ImeTextSpan::Thickness::kThin;
    text_span_1.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span_1);

    *edit_flag() = true;
    composition_range()->set_start(1);
    composition_range()->set_end(5);
    *has_composition_range() = true;

    text_store_->OnKeyTraceDown(65u, 1966081u);
    return S_OK;
  }

  void SetCompositionText2(const ui::CompositionText& composition) {
    EXPECT_EQ(u"bbbb", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(4u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    *has_composition_range() = true;
    has_composition_text_ = true;
  }

  HRESULT LockGranted3(DWORD flags) {
    GetTextTest(0, -1, L"abbbb", 5);
    SetTextTest(1, 3, L"bb", S_OK);
    SetTextTest(3, 5, L"cc", S_OK);

    text_spans()->clear();
    ImeTextSpan text_span_1;
    text_span_1.start_offset = 0;
    text_span_1.end_offset = 4;
    text_span_1.underline_color = SK_ColorBLACK;
    text_span_1.thickness = ImeTextSpan::Thickness::kThin;
    text_span_1.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span_1);

    *edit_flag() = true;
    composition_range()->set_start(1);
    composition_range()->set_end(5);
    *has_composition_range() = true;

    text_store_->OnKeyTraceDown(65u, 1966081u);
    return S_OK;
  }

  void SetCompositionText3(const ui::CompositionText& composition) {
    EXPECT_EQ(u"bbcc", composition.text);
    EXPECT_EQ(2u, composition.selection.start());
    EXPECT_EQ(4u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    *has_composition_range() = true;
    has_composition_text_ = true;
  }

  HRESULT LockGranted4(DWORD flags) {
    GetTextTest(0, -1, L"abbcc", 5);
    SetTextTest(3, 5, L"cc", S_OK);
    SetSelectionTest(3, 5, S_OK);

    text_spans()->clear();
    *edit_flag() = true;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    *composition_start() = 3;
    text_store_->OnKeyTraceUp(65u, 1966081u);
    return S_OK;
  }

  void InsertText4(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"bbcc", text);
    SetHasCompositionText(false);
  }

  HRESULT LockGranted5(DWORD flags) {
    GetTextTest(0, -1, L"abbcc", 5);
    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, RegressionTest9) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest9Callback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback, &RegressionTest9Callback::SetCompositionText2))
      .WillOnce(
          Invoke(&callback, &RegressionTest9Callback::SetCompositionText3));

  EXPECT_CALL(text_input_client_, InsertText(_, _))
      .WillOnce(Invoke(&callback, &RegressionTest9Callback::InsertText4));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest9Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest9Callback::LockGranted2))
      .WillOnce(Invoke(&callback, &RegressionTest9Callback::LockGranted3))
      .WillOnce(Invoke(&callback, &RegressionTest9Callback::LockGranted4))
      .WillOnce(Invoke(&callback, &RegressionTest9Callback::LockGranted5));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// |ConfirmComposition| should set selection to the end of compositiont text.
class RegressionTest10Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest10Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest10Callback(const RegressionTest10Callback&) = delete;
  RegressionTest10Callback& operator=(const RegressionTest10Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"abcd", S_OK);
    SetSelectionTest(0, 4, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 4;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(4);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u"abcd", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(4u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(4u, composition.ime_text_spans[0].end_offset);
    SetHasCompositionText(true);
    SetCompositionTextRange(0, 4);
  }

  HRESULT LockGranted2(DWORD flags) {
    SetSelectionTest(2, 2, S_OK);
    GetTextTest(0, -1, L"abcd", 4);

    text_spans()->clear();
    ImeTextSpan text_span_1;
    text_span_1.start_offset = 0;
    text_span_1.end_offset = 2;
    text_span_1.underline_color = SK_ColorBLACK;
    text_span_1.thickness = ImeTextSpan::Thickness::kThick;
    text_span_1.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span_1);
    ImeTextSpan text_span_2;
    text_span_2.start_offset = 2;
    text_span_2.end_offset = 4;
    text_span_2.underline_color = SK_ColorBLACK;
    text_span_2.thickness = ImeTextSpan::Thickness::kThin;
    text_span_2.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span_2);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(4);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    *has_composition_range() = true;

    text_store_->OnKeyTraceUp(65u, 1966081u);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    return S_OK;
  }

  void SetCompositionText2(const ui::CompositionText& composition) {
    EXPECT_EQ(u"abcd", composition.text);
    EXPECT_EQ(2u, composition.selection.start());
    EXPECT_EQ(2u, composition.selection.end());
    ASSERT_EQ(2u, composition.ime_text_spans.size());
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(2u, composition.ime_text_spans[0].end_offset);
    EXPECT_EQ(2u, composition.ime_text_spans[1].start_offset);
    EXPECT_EQ(4u, composition.ime_text_spans[1].end_offset);
    SetHasCompositionText(true);
  }

  HRESULT LockGranted3(DWORD flags) {
    GetTextTest(0, -1, L"abcd", 4);
    GetSelectionTest(2, 2);
    *edit_flag() = false;
    return S_OK;
  }

  HRESULT LockGranted4(DWORD flags) {
    GetTextTest(0, -1, L"abcd", 4);
    GetSelectionTest(4, 4);
    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, RegressionTest10) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest10Callback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback, &RegressionTest10Callback::SetCompositionText1))
      .WillOnce(
          Invoke(&callback, &RegressionTest10Callback::SetCompositionText2));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest10Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest10Callback::LockGranted2))
      .WillOnce(Invoke(&callback, &RegressionTest10Callback::LockGranted3))
      .WillOnce(Invoke(&callback, &RegressionTest10Callback::LockGranted4));

  ON_CALL(text_input_client_, GetCompositionTextRange(_))
      .WillByDefault(Invoke(
          &callback, &TSFTextStoreTestCallback::GetCompositionTextRange));

  ON_CALL(text_input_client_, GetTextRange(_))
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::GetTextRange));

  ON_CALL(text_input_client_, GetTextFromRange(_, _))
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::GetTextFromRange));

  ON_CALL(text_input_client_, GetEditableSelectionRange(_))
      .WillByDefault(Invoke(
          &callback, &TSFTextStoreTestCallback::GetEditableSelectionRange));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);

  text_store_->ConfirmComposition();

  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// |CancelComposition| should reset all tracking composition state.
class RegressionTest11Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest11Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest11Callback(const RegressionTest11Callback&) = delete;
  RegressionTest11Callback& operator=(const RegressionTest11Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"abcd", S_OK);
    SetSelectionTest(0, 4, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 4;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(4);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText(const ui::CompositionText& composition) {
    EXPECT_EQ(u"abcd", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(4u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(4u, composition.ime_text_spans[0].end_offset);
    SetHasCompositionText(true);
    SetCompositionTextRange(0, 4);
  }

  HRESULT LockGranted2(DWORD flags) {
    *edit_flag() = false;
    return S_OK;
  }

  HRESULT LockGranted3(DWORD flags) {
    ResetCompositionStateTest();
    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, RegressionTest11) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest11Callback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback, &RegressionTest11Callback::SetCompositionText));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest11Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest11Callback::LockGranted2))
      .WillOnce(Invoke(&callback, &RegressionTest11Callback::LockGranted3));

  ON_CALL(text_input_client_, GetCompositionTextRange(_))
      .WillByDefault(Invoke(
          &callback, &TSFTextStoreTestCallback::GetCompositionTextRange));

  ON_CALL(text_input_client_, GetTextRange(_))
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::GetTextRange));

  ON_CALL(text_input_client_, GetTextFromRange(_, _))
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::GetTextFromRange));

  ON_CALL(text_input_client_, GetEditableSelectionRange(_))
      .WillByDefault(Invoke(
          &callback, &TSFTextStoreTestCallback::GetEditableSelectionRange));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);

  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);

  text_store_->CancelComposition();

  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// regression tests for crbug.com/1156612.
// We should remove selected text even if there is no new composition and IME
// ask us to delete a previously inserted text.
class RegressionTest12Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest12Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest12Callback(const RegressionTest12Callback&) = delete;
  RegressionTest12Callback& operator=(const RegressionTest12Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"a", S_OK);
    SetSelectionTest(1, 1, S_OK);
    return S_OK;
  }

  HRESULT LockGranted2(DWORD flags) {
    GetTextTest(0, -1, L"a", 1);
    SetTextTest(0, 1, L"", S_OK);

    text_spans()->clear();
    *edit_flag() = true;

    return S_OK;
  }

  HRESULT LockGranted3(DWORD flags) {
    GetTextTest(0, -1, L"", 0);

    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, RegressionTest12) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest12Callback callback(text_store_.get());

  EXPECT_CALL(text_input_client_, ExtendSelectionAndDelete(_, _)).Times(1);
  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest12Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest12Callback::LockGranted2))
      .WillOnce(Invoke(&callback, &RegressionTest12Callback::LockGranted3));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// regression tests for crbug.com/1225896.
// Some IMEs (e.g. voice typing panel) may remove text before active
// composition. We should delete text before inserting new text.
class RegressionTest13Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest13Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest13Callback(const RegressionTest13Callback&) = delete;
  RegressionTest13Callback& operator=(const RegressionTest13Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"text ", S_OK);
    SetTextTest(4, 5, L"", S_OK);
    SetTextTest(4, 4, L" delete that ", S_OK);
    SetSelectionTest(17, 17, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 4;
    text_span.end_offset = 17;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 4;
    composition_range()->set_start(4);
    composition_range()->set_end(17);
    // text_store_->OnKeyTraceDown(65u, 1966081u);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u" delete that ", composition.text);
    EXPECT_EQ(13u, composition.selection.start());
    EXPECT_EQ(13u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(13u, composition.ime_text_spans[0].end_offset);
    SetHasCompositionText(true);
    SetTextRange(0, 17);
    SetTextBuffer(u"text delete that ");
    SetSelectionRange(17, 17);
  }

  HRESULT LockGranted2(DWORD flags) {
    GetTextTest(0, -1, L"text delete that ", 17);
    SetTextTest(4, 17, L"", S_OK);
    GetTextTest(0, -1, L"text", 4);
    SetTextTest(0, 4, L"", S_OK);
    GetTextTest(0, -1, L"", 0);
    SetSelectionTest(0, 0, S_OK);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 3;
    composition_range()->set_start(0);
    composition_range()->set_end(0);
    *has_composition_range() = false;

    return S_OK;
  }

  HRESULT LockGranted3(DWORD flags) {
    GetTextTest(0, -1, L"", 0);

    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, RegressionTest13) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest13Callback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, ExtendSelectionAndDelete(_, _)).Times(1);
  EXPECT_CALL(text_input_client_, InsertText(_, _)).Times(0);
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback, &RegressionTest13Callback::SetCompositionText1));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest13Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest13Callback::LockGranted2))
      .WillOnce(Invoke(&callback, &RegressionTest13Callback::LockGranted3));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// regression tests for crbug.com/1295578.
// Some IMEs (e.g. voice typing panel) may select text before active
// composition. We should select text after composition end.
class RegressionTest14Callback : public TSFTextStoreTestCallback {
 public:
  explicit RegressionTest14Callback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  RegressionTest14Callback(const RegressionTest14Callback&) = delete;
  RegressionTest14Callback& operator=(const RegressionTest14Callback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"text ", S_OK);
    SetTextTest(4, 5, L"", S_OK);
    SetTextTest(4, 4, L" select that ", S_OK);
    SetSelectionTest(17, 17, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 4;
    text_span.end_offset = 17;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 4;
    composition_range()->set_start(4);
    composition_range()->set_end(17);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u" select that ", composition.text);
    EXPECT_EQ(13u, composition.selection.start());
    EXPECT_EQ(13u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(13u, composition.ime_text_spans[0].end_offset);
    SetHasCompositionText(true);
    SetTextRange(0, 17);
    SetTextBuffer(u"text select that ");
    SetSelectionRange(17, 17);
  }

  HRESULT LockGranted2(DWORD flags) {
    GetTextTest(0, -1, L"text select that ", 17);
    SetTextTest(4, 17, L"", S_OK);
    GetTextTest(0, -1, L"text", 4);
    SetSelectionTest(0, 4, S_OK);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(0);
    *has_composition_range() = false;

    return S_OK;
  }

  bool SetEditableSelectionRange2(const gfx::Range& range) {
    EXPECT_EQ(range.GetMin(), 0u);
    EXPECT_EQ(range.length(), 4u);
    return true;
  }

  HRESULT LockGranted3(DWORD flags) {
    GetTextTest(0, -1, L"text", 4);
    GetSelectionTest(0, 4);

    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, RegressionTest14) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  RegressionTest14Callback callback(text_store_.get());
  EXPECT_CALL(text_input_client_, InsertText(_, _)).Times(0);
  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback, &RegressionTest14Callback::SetCompositionText1));
  EXPECT_CALL(text_input_client_, SetEditableSelectionRange(_))
      .WillOnce(Invoke(&callback,
                       &RegressionTest14Callback::SetEditableSelectionRange2));

  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &RegressionTest14Callback::LockGranted1))
      .WillOnce(Invoke(&callback, &RegressionTest14Callback::LockGranted2))
      .WillOnce(Invoke(&callback, &RegressionTest14Callback::LockGranted3));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// Test multiple |SetText| call in one edit session.
class MultipleSetTextCallback : public TSFTextStoreTestCallback {
 public:
  explicit MultipleSetTextCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  MultipleSetTextCallback(const MultipleSetTextCallback&) = delete;
  MultipleSetTextCallback& operator=(const MultipleSetTextCallback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextRange(0, 6);
    SetTextBuffer(u"123456");
    SetTextTest(0, 0, L"123456", S_OK);
    SetSelectionRange(6, 6);
    *composition_start() = 1;
    return S_OK;
  }

  HRESULT LockGranted2(DWORD flags) {
    SetTextTest(3, 3, L"a", S_OK);
    SetTextTest(4, 4, L"b", S_OK);
    GetTextTest(0, -1, L"123ab456", 8);
    SetSelectionTest(5, 5, S_OK);

    *edit_flag() = true;
    *composition_start() = 5;
    return S_OK;
  }

  HRESULT LockGranted3(DWORD flags) {
    SetTextTest(2, 6, L"cd", S_OK);
    SetTextTest(5, 6, L"e", S_OK);
    GetTextTest(0, -1, L"12cd5e", 6);
    SetSelectionRange(6, 6);

    *edit_flag() = true;
    *composition_start() = 6;
    return S_OK;
  }

  HRESULT LockGranted4(DWORD flags) {
    SetTextTest(5, 6, L"fg", S_OK);
    GetTextTest(0, -1, L"12cd5fg", 7);
    SetTextTest(1, 3, L"h", S_OK);
    GetTextTest(0, -1, L"1hd5fg", 6);
    SetSelectionRange(6, 6);

    *edit_flag() = true;
    *composition_start() = 5;
    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, MultipleSetText) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  MultipleSetTextCallback callback(text_store_.get());
  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback, &MultipleSetTextCallback::LockGranted1))
      .WillOnce(Invoke(&callback, &MultipleSetTextCallback::LockGranted2))
      .WillOnce(Invoke(&callback, &MultipleSetTextCallback::LockGranted3))
      .WillOnce(Invoke(&callback, &MultipleSetTextCallback::LockGranted4));

  ON_CALL(text_input_client_, GetTextRange(_))
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::GetTextRange));

  ON_CALL(text_input_client_, GetTextFromRange(_, _))
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::GetTextFromRange));

  ON_CALL(text_input_client_, GetEditableSelectionRange(_))
      .WillByDefault(Invoke(
          &callback, &TSFTextStoreTestCallback::GetEditableSelectionRange));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

// Test re-entrancy scanerio while writing to text input client.
class TextInputClientReentrancyTestCallback : public TSFTextStoreTestCallback {
 public:
  explicit TextInputClientReentrancyTestCallback(TSFTextStore* text_store)
      : TSFTextStoreTestCallback(text_store) {}

  TextInputClientReentrancyTestCallback(
      const TextInputClientReentrancyTestCallback&) = delete;
  TextInputClientReentrancyTestCallback& operator=(
      const TextInputClientReentrancyTestCallback&) = delete;

  HRESULT LockGranted1(DWORD flags) {
    SetTextTest(0, 0, L"a", S_OK);
    SetSelectionTest(0, 1, S_OK);

    text_spans()->clear();
    ImeTextSpan text_span;
    text_span.start_offset = 0;
    text_span.end_offset = 1;
    text_span.underline_color = SK_ColorBLACK;
    text_span.thickness = ImeTextSpan::Thickness::kThin;
    text_span.background_color = SK_ColorTRANSPARENT;
    text_spans()->push_back(text_span);
    *edit_flag() = true;
    *composition_start() = 0;
    composition_range()->set_start(0);
    composition_range()->set_end(1);
    text_store_->OnKeyTraceDown(65u, 1966081u);
    *has_composition_range() = true;

    return S_OK;
  }

  void SetCompositionText1(const ui::CompositionText& composition) {
    EXPECT_EQ(u"a", composition.text);
    EXPECT_EQ(0u, composition.selection.start());
    EXPECT_EQ(1u, composition.selection.end());
    ASSERT_EQ(1u, composition.ime_text_spans.size());
    EXPECT_EQ(0u, composition.ime_text_spans[0].start_offset);
    EXPECT_EQ(1u, composition.ime_text_spans[0].end_offset);
    SetHasCompositionText(true);
    SetTextRange(0, 1);
    SetTextBuffer(u"a");
    SetSelectionRange(0, 1);
  }

  HRESULT LockGranted2(DWORD flags) {
    GetTextTest(0, -1, L"a", 1);

    text_spans()->clear();
    *edit_flag() = true;
    *composition_start() = 1;
    composition_range()->set_start(0);
    composition_range()->set_end(0);

    *has_composition_range() = false;
    return S_OK;
  }

  void InsertText2(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) {
    EXPECT_EQ(u"a", text);
    SetHasCompositionText(false);
    SetSelectionRange(1, 1);
    SetTextBuffer(u"b");
    HRESULT result = kInvalidResult;
    EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
    EXPECT_EQ(S_OK, result);
  }

  HRESULT LockGranted3(DWORD flags) {
    GetTextTest(0, -1, L"a", 1);
    *edit_flag() = false;
    return S_OK;
  }

  HRESULT LockGranted4(DWORD flags) {
    GetTextTest(0, -1, L"b", 1);
    *edit_flag() = false;
    return S_OK;
  }
};

TEST_F(TSFTextStoreTest, TextInputClientReentrancTest) {
  EXPECT_CALL(text_input_client_, GetTextInputType())
      .WillRepeatedly(Return(TEXT_INPUT_TYPE_TEXT));
  TextInputClientReentrancyTestCallback callback(text_store_.get());
  EXPECT_CALL(*sink_, OnLockGranted(_))
      .WillOnce(Invoke(&callback,
                       &TextInputClientReentrancyTestCallback::LockGranted1))
      .WillOnce(Invoke(&callback,
                       &TextInputClientReentrancyTestCallback::LockGranted2))
      .WillOnce(Invoke(&callback,
                       &TextInputClientReentrancyTestCallback::LockGranted3))
      .WillOnce(Invoke(&callback,
                       &TextInputClientReentrancyTestCallback::LockGranted4));

  EXPECT_CALL(text_input_client_, InsertText(_, _))
      .WillOnce(Invoke(&callback,
                       &TextInputClientReentrancyTestCallback::InsertText2));

  EXPECT_CALL(text_input_client_, SetCompositionText(_))
      .WillOnce(
          Invoke(&callback,
                 &TextInputClientReentrancyTestCallback::SetCompositionText1));

  ON_CALL(text_input_client_, GetTextRange(_))
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::GetTextRange));

  ON_CALL(text_input_client_, GetTextFromRange(_, _))
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::GetTextFromRange));

  ON_CALL(text_input_client_, GetEditableSelectionRange(_))
      .WillByDefault(Invoke(
          &callback, &TSFTextStoreTestCallback::GetEditableSelectionRange));

  ON_CALL(text_input_client_, HasCompositionText())
      .WillByDefault(
          Invoke(&callback, &TSFTextStoreTestCallback::HasCompositionText));

  ON_CALL(text_input_client_, GetCompositionTextRange(_))
      .WillByDefault(Invoke(
          &callback, &TSFTextStoreTestCallback::GetCompositionTextRange));

  HRESULT result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
  result = kInvalidResult;
  EXPECT_EQ(S_OK, text_store_->RequestLock(TS_LF_READWRITE, &result));
  EXPECT_EQ(S_OK, result);
}

}  // namespace
}  // namespace ui
