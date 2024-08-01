// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#define INITGUID  // required for GUID_PROP_INPUTSCOPE
#include "ui/base/ime/win/tsf_text_store.h"

#include <InputScope.h>
#include <OleCtl.h>
#include <tsattrs.h>
#include <wrl/client.h>

#include <algorithm>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_variant.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/win/tsf_input_scope.h"
#include "ui/display/win/screen_win.h"
#include "ui/events/event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
namespace {

// We support only one view.
const TsViewCookie kViewCookie = 1;

// Fetches the client rectangle, top left and bottom right points using the
// window handle in screen coordinates.
bool GetWindowClientRect(HWND window_handle,
                         POINT* left_top,
                         POINT* right_bottom) {
  RECT client_rect = {};
  if (!IsWindow(window_handle))
    return false;
  if (!GetClientRect(window_handle, &client_rect))
    return false;
  *left_top = {client_rect.left, client_rect.top};
  *right_bottom = {client_rect.right, client_rect.bottom};
  if (!ClientToScreen(window_handle, left_top))
    return false;
  if (!ClientToScreen(window_handle, right_bottom))
    return false;
  return true;
}

}  // namespace

TSFTextStore::TSFTextStore() {
  TRACE_EVENT0("ime", "TSFTextStore::TSFTextStore");
}

TSFTextStore::~TSFTextStore() {}

HRESULT TSFTextStore::Initialize() {
  HRESULT hr = ::CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&category_manager_));
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to initialize CategoryMgr.";
    return hr;
  }

  hr = ::CoCreateInstance(CLSID_TF_DisplayAttributeMgr, nullptr, CLSCTX_ALL,
                          IID_PPV_ARGS(&display_attribute_manager_));
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to initialize DisplayAttributeMgr.";
    return hr;
  }

  hr = ::CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                          CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&input_processor_profile_mgr_));
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to initialize InputProcessorProfileMgr.";
    return hr;
  }

  return S_OK;
}

ULONG STDMETHODCALLTYPE TSFTextStore::AddRef() {
  return InterlockedIncrement(&ref_count_);
}

ULONG STDMETHODCALLTYPE TSFTextStore::Release() {
  const LONG count = InterlockedDecrement(&ref_count_);
  if (!count) {
    delete this;
    return 0;
  }
  return static_cast<ULONG>(count);
}

HRESULT TSFTextStore::QueryInterface(REFIID iid, void** result) {
  if (iid == IID_IUnknown || iid == IID_ITextStoreACP) {
    *result = static_cast<ITextStoreACP*>(this);
  } else if (iid == IID_ITfContextOwnerCompositionSink) {
    *result = static_cast<ITfContextOwnerCompositionSink*>(this);
  } else if (iid == IID_ITfLanguageProfileNotifySink) {
    *result = static_cast<ITfLanguageProfileNotifySink*>(this);
  } else if (iid == IID_ITfTextEditSink) {
    *result = static_cast<ITfTextEditSink*>(this);
  } else if (iid == IID_ITfKeyTraceEventSink) {
    *result = static_cast<ITfKeyTraceEventSink*>(this);
  } else {
    *result = nullptr;
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

HRESULT TSFTextStore::AdviseSink(REFIID iid, IUnknown* unknown, DWORD mask) {
  if (!IsEqualGUID(iid, IID_ITextStoreACPSink))
    return E_INVALIDARG;
  if (text_store_acp_sink_) {
    if (text_store_acp_sink_.Get() == unknown) {
      text_store_acp_sink_mask_ = mask;
      return S_OK;
    } else {
      return CONNECT_E_ADVISELIMIT;
    }
  }
  if (FAILED(unknown->QueryInterface(IID_PPV_ARGS(&text_store_acp_sink_))))
    return E_UNEXPECTED;
  text_store_acp_sink_mask_ = mask;

  return S_OK;
}

HRESULT TSFTextStore::FindNextAttrTransition(LONG acp_start,
                                             LONG acp_halt,
                                             ULONG num_filter_attributes,
                                             const TS_ATTRID* filter_attributes,
                                             DWORD flags,
                                             LONG* acp_next,
                                             BOOL* found,
                                             LONG* found_offset) {
  if (!acp_next || !found || !found_offset)
    return E_INVALIDARG;
  // We don't support any attributes.
  // So we always return "not found".
  *acp_next = 0;
  *found = FALSE;
  *found_offset = 0;
  return S_OK;
}

HRESULT TSFTextStore::GetACPFromPoint(TsViewCookie view_cookie,
                                      const POINT* point,
                                      DWORD flags,
                                      LONG* acp) {
  NOTIMPLEMENTED();
  if (view_cookie != kViewCookie)
    return E_INVALIDARG;
  return E_NOTIMPL;
}

HRESULT TSFTextStore::GetActiveView(TsViewCookie* view_cookie) {
  if (!view_cookie)
    return E_INVALIDARG;
  // We support only one view.
  *view_cookie = kViewCookie;
  return S_OK;
}

HRESULT TSFTextStore::GetEmbedded(LONG acp_pos,
                                  REFGUID service,
                                  REFIID iid,
                                  IUnknown** unknown) {
  // We don't support any embedded objects.
  NOTIMPLEMENTED();
  if (!unknown)
    return E_INVALIDARG;
  *unknown = nullptr;
  return E_NOTIMPL;
}

HRESULT TSFTextStore::GetEndACP(LONG* acp) {
  if (!acp)
    return E_INVALIDARG;
  if (!HasReadLock())
    return TS_E_NOLOCK;
  *acp = string_buffer_document_.size();
  return S_OK;
}

HRESULT TSFTextStore::GetFormattedText(LONG acp_start,
                                       LONG acp_end,
                                       IDataObject** data_object) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT TSFTextStore::GetScreenExt(TsViewCookie view_cookie, RECT* rect) {
  if (view_cookie != kViewCookie)
    return E_INVALIDARG;
  if (!rect)
    return E_INVALIDARG;
  if (!text_input_client_)
    return E_UNEXPECTED;

  // {0, 0, 0, 0} means that the document rect is not currently displayed.
  SetRect(rect, 0, 0, 0, 0);
  std::optional<gfx::Rect> result_rect;
  std::optional<gfx::Rect> tmp_rect;
  // If the EditContext is active, then fetch the layout bounds from
  // the active EditContext, else get it from the focused element's
  // bounding client rect.
  text_input_client_->GetActiveTextInputControlLayoutBounds(&result_rect,
                                                            &tmp_rect);
  if (result_rect) {
    // This conversion is required for high dpi monitors.
    *rect = display::win::ScreenWin::DIPToScreenRect(window_handle_,
                                                     result_rect.value())
                .ToRECT();
  } else {
    // Default if the layout bounds are not present in text input client.
    POINT left_top;
    POINT right_bottom;
    if (!GetWindowClientRect(window_handle_, &left_top, &right_bottom))
      return E_FAIL;
    rect->left = left_top.x;
    rect->top = left_top.y;
    rect->right = right_bottom.x;
    rect->bottom = right_bottom.y;
  }

  TRACE_EVENT1("ime", "TSFTextStore::GetScreenExt", "control_bounding_rect",
               gfx::Rect(*rect).ToString());
  return S_OK;
}

HRESULT TSFTextStore::GetSelection(ULONG selection_index,
                                   ULONG selection_buffer_size,
                                   TS_SELECTION_ACP* selection_buffer,
                                   ULONG* fetched_count) {
  if (!selection_buffer)
    return E_INVALIDARG;
  if (!fetched_count)
    return E_INVALIDARG;
  if (!HasReadLock())
    return TS_E_NOLOCK;
  *fetched_count = 0;
  if ((selection_buffer_size > 0) &&
      ((selection_index == 0) || (selection_index == TS_DEFAULT_SELECTION))) {
    selection_buffer[0].acpStart = selection_.start();
    selection_buffer[0].acpEnd = selection_.end();
    selection_buffer[0].style.ase = TS_AE_END;
    selection_buffer[0].style.fInterimChar = FALSE;
    *fetched_count = 1;
  }
  return S_OK;
}

HRESULT TSFTextStore::GetStatus(TS_STATUS* status) {
  if (!status)
    return E_INVALIDARG;

  status->dwDynamicFlags |= TS_SD_INPUTPANEMANUALDISPLAYENABLE;
  // We don't support hidden text.
  // TODO(IME): Remove TS_SS_TRANSITORY to support Korean reconversion
  status->dwStaticFlags = TS_SS_TRANSITORY | TS_SS_NOHIDDENTEXT;

  // No text support is needed for empty text store.
  if (is_empty_text_store_) {
    status->dwDynamicFlags |= TS_SD_READONLY;
  }
  return S_OK;
}

HRESULT TSFTextStore::GetText(LONG acp_start,
                              LONG acp_end,
                              wchar_t* text_buffer,
                              ULONG text_buffer_size,
                              ULONG* text_buffer_copied,
                              TS_RUNINFO* run_info_buffer,
                              ULONG run_info_buffer_size,
                              ULONG* run_info_buffer_copied,
                              LONG* next_acp) {
  if (!text_buffer_copied || !run_info_buffer_copied)
    return E_INVALIDARG;
  if (!text_buffer && text_buffer_size != 0)
    return E_INVALIDARG;
  if (!run_info_buffer && run_info_buffer_size != 0)
    return E_INVALIDARG;
  if (!next_acp)
    return E_INVALIDARG;
  if (!HasReadLock())
    return TF_E_NOLOCK;
  const LONG string_buffer_size = string_buffer_document_.size();
  if (acp_end == -1)
    acp_end = string_buffer_size;
  if (!((0 <= acp_start) && (acp_start <= acp_end) &&
        (acp_end <= string_buffer_size))) {
    return TF_E_INVALIDPOS;
  }
  acp_end = std::min(acp_end, acp_start + static_cast<LONG>(text_buffer_size));
  *text_buffer_copied = acp_end - acp_start;

  const std::u16string& result =
      string_buffer_document_.substr(acp_start, *text_buffer_copied);
  for (size_t i = 0; i < result.size(); ++i) {
    text_buffer[i] = result[i];
  }

  if (*text_buffer_copied > 0 && run_info_buffer_size) {
    run_info_buffer[0].uCount = *text_buffer_copied;
    run_info_buffer[0].type = TS_RT_PLAIN;
    *run_info_buffer_copied = 1;
  } else {
    *run_info_buffer_copied = 0;
  }

  *next_acp = acp_end;
  return S_OK;
}

HRESULT TSFTextStore::GetTextExt(TsViewCookie view_cookie,
                                 LONG acp_start,
                                 LONG acp_end,
                                 RECT* rect,
                                 BOOL* clipped) {
  if (!rect || !clipped)
    return E_INVALIDARG;
  if (!text_input_client_)
    return E_UNEXPECTED;
  if (view_cookie != kViewCookie)
    return E_INVALIDARG;
  if (!HasReadLock())
    return TS_E_NOLOCK;
  if (!((static_cast<LONG>(composition_start_) <= acp_start) &&
        (acp_start <= acp_end) &&
        (acp_end <= static_cast<LONG>(string_buffer_document_.size())))) {
    return TS_E_INVALIDPOS;
  }

  TRACE_EVENT1("ime", "TSFTextStore::GetTextExt", "start, end",
               std::to_string(acp_start) + ", " + std::to_string(acp_end));

  // According to a behavior of notepad.exe and wordpad.exe, top left corner of
  // rect indicates a first character's one, and bottom right corner of rect
  // indicates a last character's one.
  // TODO(IME): add tests for scenario that left position is bigger than right
  // position.
  std::optional<gfx::Rect> result_rect;
  const uint32_t start_pos = acp_start - composition_start_;
  const uint32_t end_pos = acp_end - composition_start_;

  gfx::Rect tmp_rect;
  if (start_pos == end_pos) {
    if (text_input_client_->HasCompositionText()) {
      // According to MSDN document, if |acp_start| and |acp_end| are equal it
      // is OK to just return E_INVALIDARG.
      // http://msdn.microsoft.com/en-us/library/ms538435
      // But when using Pinin IME of Windows 8, this method is called with the
      // equal values of |acp_start| and |acp_end|. So we handle this condition.
      if (start_pos == 0) {
        if (text_input_client_->GetCompositionCharacterBounds(0, &tmp_rect)) {
          tmp_rect.set_width(0);
          result_rect = gfx::Rect(tmp_rect);
        } else {
          return TS_E_NOLAYOUT;
        }
      } else if (text_input_client_->GetCompositionCharacterBounds(
                     start_pos - 1, &tmp_rect)) {
        tmp_rect.set_x(tmp_rect.right());
        tmp_rect.set_width(0);
        result_rect = gfx::Rect(tmp_rect);

      } else {
        return TS_E_NOLAYOUT;
      }
    } else {
      result_rect = gfx::Rect(text_input_client_->GetCaretBounds());
    }
  } else {
    if (text_input_client_->HasCompositionText()) {
      if (text_input_client_->GetCompositionCharacterBounds(start_pos,
                                                            &tmp_rect)) {
        result_rect = gfx::Rect(tmp_rect);
        if (text_input_client_->GetCompositionCharacterBounds(end_pos - 1,
                                                              &tmp_rect)) {
          result_rect->set_width(tmp_rect.x() - result_rect->x() +
                                 tmp_rect.width());
          result_rect->set_height(tmp_rect.y() - result_rect->y() +
                                  tmp_rect.height());
        } else {
          // We may not be able to get the last character bounds, so we use the
          // first character bounds instead of returning TS_E_NOLAYOUT.
        }
      } else {
        return TS_E_NOLAYOUT;
      }
    } else {
      result_rect = gfx::Rect(text_input_client_->GetCaretBounds());
    }
  }
  TRACE_EVENT1("ime", "TSFTextStore::GetTextExt", "DIP rect",
               result_rect->ToString());

  *rect = display::win::ScreenWin::DIPToScreenRect(window_handle_,
                                                   result_rect.value())
              .ToRECT();
  *clipped = FALSE;
  TRACE_EVENT1("ime", "TSFTextStore::GetTextExt", "screen rect",
               gfx::Rect(*rect).ToString());
  return S_OK;
}

HRESULT TSFTextStore::GetWnd(TsViewCookie view_cookie, HWND* window_handle) {
  if (!window_handle)
    return E_INVALIDARG;
  if (view_cookie != kViewCookie)
    return E_INVALIDARG;
  *window_handle = window_handle_;
  return S_OK;
}

HRESULT TSFTextStore::InsertEmbedded(DWORD flags,
                                     LONG acp_start,
                                     LONG acp_end,
                                     IDataObject* data_object,
                                     TS_TEXTCHANGE* change) {
  // We don't support any embedded objects.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT TSFTextStore::InsertEmbeddedAtSelection(DWORD flags,
                                                IDataObject* data_object,
                                                LONG* acp_start,
                                                LONG* acp_end,
                                                TS_TEXTCHANGE* change) {
  // We don't support any embedded objects.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT TSFTextStore::InsertTextAtSelection(DWORD flags,
                                            const wchar_t* text_buffer,
                                            ULONG text_buffer_size,
                                            LONG* acp_start,
                                            LONG* acp_end,
                                            TS_TEXTCHANGE* text_change) {
  const LONG start_pos = selection_.start();
  const LONG end_pos = selection_.end();
  const LONG new_end_pos = start_pos + text_buffer_size;

  if (flags & TS_IAS_QUERYONLY) {
    if (!HasReadLock())
      return TS_E_NOLOCK;
    if (acp_start)
      *acp_start = start_pos;
    if (acp_end) {
      *acp_end = end_pos;
    }
    return S_OK;
  }

  if (!HasReadWriteLock())
    return TS_E_NOLOCK;
  if (!text_buffer)
    return E_INVALIDARG;

  if (text_buffer_size >= 0) {
    if (!new_text_inserted_) {
      new_text_inserted_ = true;
      replace_text_range_.set_start(start_pos);
      replace_text_range_.set_end(end_pos);
      replace_text_size_ = text_buffer_size;
    } else {
      // aggregate new replace text with previous replace text into one range.
      LONG old_delta = (LONG)replace_text_range_.start() -
                       (LONG)replace_text_range_.end() + replace_text_size_;
      LONG new_delta = start_pos - end_pos + text_buffer_size;
      replace_text_range_.set_start(std::min(static_cast<size_t>(start_pos),
                                             replace_text_range_.start()));
      // New replacement text ends after previous replacement text. We need to
      // use the new end after adjusting with previous delta.
      if ((uint32_t)end_pos >=
          replace_text_range_.start() + replace_text_size_) {
        replace_text_range_.set_end(end_pos - old_delta);
      }
      replace_text_size_ = replace_text_range_.length() + old_delta + new_delta;
    }
  }

  DCHECK_LE(start_pos, end_pos);
  string_buffer_document_ =
      string_buffer_document_.substr(0, start_pos) +
      std::u16string(text_buffer, text_buffer + text_buffer_size) +
      string_buffer_document_.substr(end_pos);

  // reconstruct string that needs to be inserted.
  string_pending_insertion_ =
      string_buffer_document_.substr(start_pos, text_buffer_size);

  if (acp_start)
    *acp_start = start_pos;
  if (acp_end)
    *acp_end = new_end_pos;
  if (text_change) {
    text_change->acpStart = start_pos;
    text_change->acpOldEnd = end_pos;
    text_change->acpNewEnd = new_end_pos;
  }
  selection_.set_start(start_pos);
  selection_.set_end(new_end_pos);
  return S_OK;
}

HRESULT TSFTextStore::QueryInsert(LONG acp_test_start,
                                  LONG acp_test_end,
                                  ULONG text_size,
                                  LONG* acp_result_start,
                                  LONG* acp_result_end) {
  if (!acp_result_start || !acp_result_end || acp_test_start > acp_test_end)
    return E_INVALIDARG;
  const LONG composition_start = static_cast<LONG>(composition_start_);
  const LONG buffer_size = static_cast<LONG>(string_buffer_document_.size());
  *acp_result_start =
      std::min(std::max(acp_test_start, composition_start), buffer_size);
  *acp_result_end =
      std::min(std::max(acp_test_end, composition_start), buffer_size) +
      text_size;
  return S_OK;
}

HRESULT TSFTextStore::QueryInsertEmbedded(const GUID* service,
                                          const FORMATETC* format,
                                          BOOL* insertable) {
  if (!format)
    return E_INVALIDARG;
  // We don't support any embedded objects.
  if (insertable)
    *insertable = FALSE;
  return S_OK;
}

HRESULT TSFTextStore::RequestAttrsAtPosition(LONG acp_pos,
                                             ULONG attribute_buffer_size,
                                             const TS_ATTRID* attribute_buffer,
                                             DWORD flags) {
  // We don't support any document attributes.
  // This method just returns S_OK, and the subsequently called
  // RetrieveRequestedAttrs() returns 0 as the number of supported attributes.
  return S_OK;
}

HRESULT TSFTextStore::RequestAttrsTransitioningAtPosition(
    LONG acp_pos,
    ULONG attribute_buffer_size,
    const TS_ATTRID* attribute_buffer,
    DWORD flags) {
  // We don't support any document attributes.
  // This method just returns S_OK, and the subsequently called
  // RetrieveRequestedAttrs() returns 0 as the number of supported attributes.
  return S_OK;
}

HRESULT TSFTextStore::RequestLock(DWORD lock_flags, HRESULT* result) {
  if (!text_input_client_)
    return E_UNEXPECTED;
  // No lock is necessary for an empty text store. This is to deny lock to an
  // unsuspecting TSF in the wild that always assumes a text update with a
  // store.
  if (is_empty_text_store_) {
    return E_FAIL;
  }
  // If the text input type has already switched to NONE in the text input
  // client, then do nothing. See crbug.com/1483978.
  if (text_input_client_->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE) {
    return E_FAIL;
  }

  if (!text_store_acp_sink_.Get())
    return E_FAIL;
  if (!result)
    return E_INVALIDARG;

  if (current_lock_type_ != 0) {
    if (lock_flags & TS_LF_SYNC) {
      // Can't lock synchronously.
      *result = TS_E_SYNCHRONOUS;
      return S_OK;
    }
    // Queue the lock request.
    lock_queue_.push_back(lock_flags & TS_LF_READWRITE);
    *result = TS_S_ASYNC;
    return S_OK;
  }

  // Lock
  current_lock_type_ = (lock_flags & TS_LF_READWRITE);

  edit_flag_ = false;
  // if there is not already some composition text, they we are about to start
  // composition. we need to set last_composition_start to the selection start.
  // Otherwise we are updating an existing composition, we should use the cached
  // composition_start_ for reference.
  const size_t last_composition_start = text_input_client_->HasCompositionText()
                                            ? composition_start_
                                            : selection_.start();

  // Grant the lock.
  *result = text_store_acp_sink_->OnLockGranted(current_lock_type_);

  // Unlock
  current_lock_type_ = 0;

  // Handles the pending lock requests.
  while (!lock_queue_.empty()) {
    current_lock_type_ = lock_queue_.front();
    lock_queue_.pop_front();
    text_store_acp_sink_->OnLockGranted(current_lock_type_);
    current_lock_type_ = 0;
  }

  // if nothing has changed from input service, then only need to
  // compare our cache with latest textinputstate.
  if (!edit_flag_) {
    ResetCacheAfterEditSession();
    CalculateTextandSelectionDiffAndNotifyIfNeeded();
    return S_OK;
  }

  if (!text_input_client_)
    return E_UNEXPECTED;

  // If string_pending_insertion_ is empty, then there are four cases:
  // 1. there is no composition We only need to do comparison between our
  //    cache and latest textinputstate and send notifications accordingly.
  //    There might be selection change from input service without staring new
  //    composition. We should update tic selection.
  // 2. A new composition is about to start on existing text. We need to start
  //    composition on range from composition_range_.
  // 3. There is composition. User cancels the composition by deleting all of
  //    the composing text, we need to reset the composition_start_ and call
  //    into blink to complete the existing composition(later in this method).
  // 4. There is no composition. IME removes previous inserted text. We need to
  //    ask tic to delete the text range.
  if (string_pending_insertion_.empty()) {
    if (!text_input_client_->HasCompositionText()) {
      // Remove replacing text.
      if (new_text_inserted_ && !replace_text_range_.is_empty() &&
          !replace_text_size_) {
        is_tic_write_in_progress_ = true;
        text_input_client_->SetEditableSelectionRange(replace_text_range_);
        text_input_client_->ExtendSelectionAndDelete(0, 0);
        is_tic_write_in_progress_ = false;
      }
      if (has_composition_range_ && on_start_composition_called_) {
        is_tic_write_in_progress_ = true;
        string_pending_insertion_ = string_buffer_document_.substr(
            composition_range_.GetMin(), composition_range_.length());
        StartCompositionOnExistingText();
        is_tic_write_in_progress_ = false;
      } else {
        composition_start_ = selection_.start();
        if (!selection_.EqualsIgnoringDirection(selection_from_client_) &&
            !IsInputIME()) {
          text_input_client_->SetEditableSelectionRange(selection_);
        }
        CalculateTextandSelectionDiffAndNotifyIfNeeded();
      }
      ResetCacheAfterEditSession();
      return S_OK;
    } else {
      composition_start_ = last_composition_start;
    }
  }

  // If we saved a keydown event before this, now is the right time to fire it
  // We should only fire JS key event during composition or OnStartComposition()
  // is called during current edit session.
  if ((has_composition_range_ || on_start_composition_called_) &&
      wparam_keydown_cached_ != 0 && lparam_keydown_cached_ != 0) {
    DispatchKeyEvent(ui::EventType::kKeyPressed, wparam_keydown_cached_,
                     lparam_keydown_cached_);
  }

  // If the text store is edited in OnLockGranted(), we may need to call
  // TextInputClient::InsertText() or TextInputClient::SetCompositionText().
  // Calculate the end location. we use the replace text end pos if there is no
  // more active composition.
  size_t new_composition_start =
      !has_composition_range_ && new_text_inserted_
          ? replace_text_range_.start() + replace_text_size_
          : composition_start_;

  // There are several scenarios that we want to commit composition text. For
  // those scenarios, we need to call TextInputClient::InsertText to complete
  // the current composition. When there are some committed text.
  // 1. If new_composition_start is greater than last_composition_start and
  // there is active composition, then we know that there are some committed
  // text. It is not necessarily true that composition_string is empty. We need
  // to complete current composition with committed text and start new
  // composition with composition_string.
  // 2. If the replacement text is coming from on-screen keyboard, we should
  // replace current selection with new text.
  // 3. User commits current composition text.
  if (((new_composition_start > last_composition_start &&
        text_input_client_->HasCompositionText()) ||
       !has_composition_range_) &&
      text_input_client_) {
    is_tic_write_in_progress_ = true;
    CommitTextAndEndCompositionIfAny(last_composition_start,
                                     new_composition_start);
    is_tic_write_in_progress_ = false;
  }

  const std::u16string& composition_string = string_buffer_document_.substr(
      composition_range_.GetMin(), composition_range_.length());

  // Only need to set composition if the current composition string
  // (composition_string) is not the same as previous composition string
  // (prev_composition_string_) during same composition or the composition
  // string is the same for different composition or selection is changed during
  // composition or IME spans are changed during same composition. If
  // composition_string is empty and there is an existing composition going on,
  // we still need to call into blink to complete the composition started by
  // TSF.
  if ((has_composition_range_ &&
       (previous_composition_start_ != composition_range_.start() ||
        previous_composition_string_ != composition_string ||
        !previous_composition_selection_range_.EqualsIgnoringDirection(
            selection_) ||
        previous_text_spans_ != text_spans_)) ||
      ((wparam_keydown_fired_ != 0) &&
       text_input_client_->HasCompositionText() &&
       composition_string.empty())) {
    previous_composition_string_ = composition_string;
    previous_composition_start_ = composition_range_.start();
    previous_composition_selection_range_ = selection_;
    previous_text_spans_ = text_spans_;

    // We need to remove replacing text first before starting new composition if
    // there are any.
    is_tic_write_in_progress_ = true;
    if (new_text_inserted_ && !replace_text_range_.is_empty() &&
        !text_input_client_->HasCompositionText() &&
        last_composition_start > replace_text_range_.start()) {
      text_input_client_->ExtendSelectionAndDelete(
          last_composition_start - replace_text_range_.start(), 0);
    }

    StartCompositionOnNewText(new_composition_start, composition_string);
    is_tic_write_in_progress_ = false;
  }

  ResetCacheAfterEditSession();
  CalculateTextandSelectionDiffAndNotifyIfNeeded();

  return S_OK;
}

HRESULT TSFTextStore::RequestSupportedAttrs(
    DWORD /* flags */,  // Seems that we should ignore this.
    ULONG attribute_buffer_size,
    const TS_ATTRID* attribute_buffer) {
  if (!attribute_buffer)
    return E_INVALIDARG;
  if (!text_input_client_)
    return E_FAIL;

  supported_attrs_.clear();
  for (size_t i = 0; i < attribute_buffer_size; ++i) {
    const auto& attribute = attribute_buffer[i];
    if (IsEqualGUID(GUID_PROP_INPUTSCOPE, attribute) ||
        IsEqualGUID(GUID_PROP_URL, attribute) ||
        IsEqualGUID(TSATTRID_Text_VerticalWriting, attribute)) {
      supported_attrs_.push_back(attribute);
    }
  }
  return S_OK;
}

HRESULT TSFTextStore::RetrieveRequestedAttrs(ULONG attribute_buffer_size,
                                             TS_ATTRVAL* attribute_buffer,
                                             ULONG* attribute_buffer_copied) {
  if (!attribute_buffer_copied)
    return E_INVALIDARG;
  if (!attribute_buffer)
    return E_INVALIDARG;
  if (!text_input_client_)
    return E_UNEXPECTED;

  *attribute_buffer_copied = 0;
  if (attribute_buffer_size == 0)
    return S_OK;

  *attribute_buffer_copied = std::min(
      attribute_buffer_size, static_cast<ULONG>(supported_attrs_.size()));

  for (size_t i = 0; i < *attribute_buffer_copied; ++i) {
    attribute_buffer[i].idAttr = supported_attrs_[i];
    // In TSF, this parameter value is zero.
    // https://docs.microsoft.com/en-us/windows/win32/api/textstor/ns-textstor-ts_attrval
    attribute_buffer[i].dwOverlapId = 0;
    // If the caller is asking for the input scope, then create one based on
    // the input client and return the COM object for it.
    if (IsEqualGUID(GUID_PROP_INPUTSCOPE, supported_attrs_[i])) {
      attribute_buffer[i].varValue.vt = VT_UNKNOWN;
      attribute_buffer[i].varValue.punkVal = tsf_inputscope::CreateInputScope(
          text_input_client_->GetTextInputType(),
          text_input_client_->GetTextInputMode(),
          text_input_client_->ShouldDoLearning());
      attribute_buffer[i].varValue.punkVal->AddRef();
    } else if (IsEqualGUID(GUID_PROP_URL, supported_attrs_[i])) {
      const ui::TextInputClient::EditingContext editing_context =
          text_input_client_->GetTextEditingContext();
      attribute_buffer[i].varValue.vt = VT_BSTR;
      std::wstring wide_url;
      // If the caller is asking for the URL, get the URL from the
      // the text input client (if there is one).
      if (!editing_context.page_url.is_empty()) {
        const std::string& url_string = editing_context.page_url.spec();
        wide_url = base::UTF8ToWide(url_string);
      }
      attribute_buffer[i].varValue.bstrVal =
          SysAllocStringLen(wide_url.c_str(), wide_url.length());
    } else if (IsEqualGUID(TSATTRID_Text_VerticalWriting,
                           supported_attrs_[i])) {
      attribute_buffer[i].varValue.vt = VT_BOOL;
      attribute_buffer[i].varValue.boolVal =
          !!(text_input_client_->GetTextInputFlags() &
             ui::TEXT_INPUT_FLAG_VERTICAL);
    }
  }
  return S_OK;
}

HRESULT TSFTextStore::SetSelection(ULONG selection_buffer_size,
                                   const TS_SELECTION_ACP* selection_buffer) {
  if (!HasReadWriteLock())
    return TF_E_NOLOCK;
  if (selection_buffer_size > 0) {
    const LONG start_pos = selection_buffer[0].acpStart;
    const LONG end_pos = selection_buffer[0].acpEnd;
    if (!((start_pos <= end_pos) &&
          (end_pos <= static_cast<LONG>(string_buffer_document_.size())))) {
      return TF_E_INVALIDPOS;
    }
    selection_.set_start(start_pos);
    selection_.set_end(end_pos);
    is_selection_interim_char_ = selection_buffer[0].style.fInterimChar;
  }
  return S_OK;
}

HRESULT TSFTextStore::SetText(DWORD flags,
                              LONG acp_start,
                              LONG acp_end,
                              const wchar_t* text_buffer,
                              ULONG text_buffer_size,
                              TS_TEXTCHANGE* text_change) {
  TRACE_EVENT0("ime", "TSFTextStore::SetText");
  if (!HasReadWriteLock())
    return TS_E_NOLOCK;

  TS_SELECTION_ACP selection;
  selection.acpStart = acp_start;
  selection.acpEnd = acp_end;
  selection.style.ase = TS_AE_NONE;
  selection.style.fInterimChar = 0;

  HRESULT ret;
  ret = SetSelection(1, &selection);
  if (ret != S_OK)
    return ret;

  TS_TEXTCHANGE change;
  ret = InsertTextAtSelection(0, text_buffer, text_buffer_size, &acp_start,
                              &acp_end, &change);
  if (ret != S_OK)
    return ret;

  if (text_change)
    *text_change = change;

  return S_OK;
}

HRESULT TSFTextStore::UnadviseSink(IUnknown* unknown) {
  if (text_store_acp_sink_.Get() != unknown)
    return CONNECT_E_NOCONNECTION;
  text_store_acp_sink_.Reset();
  text_store_acp_sink_mask_ = 0;
  return S_OK;
}

HRESULT TSFTextStore::OnStartComposition(ITfCompositionView* composition_view,
                                         BOOL* ok) {
  TRACE_EVENT0("ime", "TSFTextStore::OnStartComposition");
  if (ok)
    *ok = TRUE;

  on_start_composition_called_ = true;
  return S_OK;
}

HRESULT TSFTextStore::OnUpdateComposition(ITfCompositionView* composition_view,
                                          ITfRange* range) {
  return S_OK;
}

HRESULT TSFTextStore::OnEndComposition(ITfCompositionView* composition_view) {
  TRACE_EVENT0("ime", "TSFTextStore::OnEndComposition");
  return S_OK;
}

HRESULT TSFTextStore::OnLanguageChange(LANGID langid, BOOL* pfAccept) {
  *pfAccept = TRUE;
  return S_OK;
}

HRESULT TSFTextStore::OnLanguageChanged() {
  if (text_input_client_)
    text_input_client_->OnInputMethodChanged();
  return S_OK;
}

HRESULT TSFTextStore::OnKeyTraceDown(WPARAM wParam, LPARAM lParam) {
  // fire the event right away if we're in composition
  if (has_composition_range_) {
    DispatchKeyEvent(ui::EventType::kKeyPressed, wParam, lParam);
  } else {
    // we're not in composition but we might be starting it - remember these key
    // events to fire when composition starts
    wparam_keydown_cached_ = wParam;
    lparam_keydown_cached_ = lParam;
  }
  return S_OK;
}

HRESULT TSFTextStore::OnKeyTraceUp(WPARAM wParam, LPARAM lParam) {
  if (has_composition_range_ || wparam_keydown_fired_ == wParam) {
    DispatchKeyEvent(ui::EventType::kKeyReleased, wParam, lParam);
  } else if (wparam_keydown_cached_ == wParam) {
    // If we didn't fire corresponding keydown event, then we need to clear the
    // cached keydown wParam and lParam.
    wparam_keydown_cached_ = 0;
    lparam_keydown_cached_ = 0;
  }
  return S_OK;
}

void TSFTextStore::DispatchKeyEvent(ui::EventType type,
                                    WPARAM wparam,
                                    LPARAM lparam) {
  if (!text_input_client_)
    return;

  if (type == ui::EventType::kKeyPressed) {
    // clear the saved values since we just fired a keydown
    wparam_keydown_cached_ = 0;
    lparam_keydown_cached_ = 0;
    wparam_keydown_fired_ = wparam;
  } else if (type == ui::EventType::kKeyReleased) {
    // clear the saved values since we just fired a keyup
    wparam_keydown_fired_ = 0;
  } else {
    // shouldn't expect event other than et_key_pressed and et_key_released;
    return;
  }

  // prepare ui::KeyEvent.
  UINT message = type == ui::EventType::kKeyPressed ? WM_KEYDOWN : WM_KEYUP;
  const CHROME_MSG key_event_MSG = {window_handle_, message, VK_PROCESSKEY,
                                    lparam};
  ui::KeyEvent key_event = KeyEventFromMSG(key_event_MSG);

  if (ime_key_event_dispatcher_) {
    ime_key_event_dispatcher_->DispatchKeyEventPostIME(&key_event);
  }
}

HRESULT TSFTextStore::OnEndEdit(ITfContext* context,
                                TfEditCookie read_only_edit_cookie,
                                ITfEditRecord* edit_record) {
  TRACE_EVENT0("ime", "TSFTextStore::OnEndEdit");
  if (!context || !edit_record)
    return E_INVALIDARG;

  size_t committed_size;
  ImeTextSpans spans;
  if (!GetCompositionStatus(context, read_only_edit_cookie, &committed_size,
                            &spans)) {
    return S_OK;
  }
  text_spans_ = spans;
  edit_flag_ = true;

  // This function is guaranteed to be called after each keystroke during
  // composition Therefore we can use this function to update composition status
  // after each keystroke. If there is existing composition range, we can cache
  // the composition range and set composition start position as the start of
  // composition range. If there is no existing composition range, then we know
  // that there is no active composition, we then need to reset the cached
  // composition range and set the new composition start as the current
  // selection start.
  DCHECK(context);
  HRESULT hr = S_OK;
  Microsoft::WRL::ComPtr<ITfContextComposition> context_composition;
  hr = context->QueryInterface(IID_PPV_ARGS(&context_composition));
  if (FAILED(hr)) {
    return hr;
  }

  Microsoft::WRL::ComPtr<IEnumITfCompositionView> enum_composition_view;
  hr = context_composition->EnumCompositions(&enum_composition_view);
  if (FAILED(hr)) {
    return hr;
  }

  Microsoft::WRL::ComPtr<ITfCompositionView> composition_view;
  Microsoft::WRL::ComPtr<ITfRange> range;
  Microsoft::WRL::ComPtr<ITfRangeACP> range_acp;
  if (enum_composition_view->Next(1, &composition_view, nullptr) == S_OK
      && SUCCEEDED(composition_view->GetRange(&range))
      && SUCCEEDED(range->QueryInterface(IID_PPV_ARGS(&range_acp)))) {
    LONG start = 0;
    LONG length = 0;
    // We should only consider it as a valid composition if the
    // composition range is not collapsed (|length| > 0).
    if (SUCCEEDED(range_acp->GetExtent(&start, &length)) && length > 0) {
      composition_start_ = start;
      has_composition_range_ = true;
      composition_range_.set_start(start);
      composition_range_.set_end(start + length);
      return S_OK;
    }
  }

  composition_start_ = selection_.start();
  if (has_composition_range_) {
    has_composition_range_ = false;
    composition_range_.set_start(0);
    composition_range_.set_end(0);
    previous_composition_string_.clear();
    previous_composition_start_ = 0;
    previous_composition_selection_range_ = gfx::Range::InvalidRange();
    previous_text_spans_.clear();
  }

  return S_OK;
}

bool TSFTextStore::GetDisplayAttribute(TfGuidAtom guid_atom,
                                       TF_DISPLAYATTRIBUTE* attribute) {
  TRACE_EVENT0("ime", "TSFTextStore::GetDisplayAttribute");
  GUID guid;
  if (FAILED(category_manager_->GetGUID(guid_atom, &guid)))
    return false;

  Microsoft::WRL::ComPtr<ITfDisplayAttributeInfo> display_attribute_info;
  if (FAILED(display_attribute_manager_->GetDisplayAttributeInfo(
          guid, &display_attribute_info, nullptr))) {
    return false;
  }
  // Display Attribute can be null so query for attributes only when its
  // available
  if (display_attribute_info)
    return SUCCEEDED(display_attribute_info->GetAttributeInfo(attribute));
  return false;
}

bool TSFTextStore::GetCompositionStatus(
    ITfContext* context,
    const TfEditCookie read_only_edit_cookie,
    size_t* committed_size,
    ImeTextSpans* spans) {
  DCHECK(context);
  DCHECK(committed_size);
  DCHECK(spans);
  const GUID* rgGuids[2] = {&GUID_PROP_COMPOSING, &GUID_PROP_ATTRIBUTE};
  Microsoft::WRL::ComPtr<ITfReadOnlyProperty> track_property;
  if (FAILED(
          context->TrackProperties(rgGuids, 2, nullptr, 0, &track_property))) {
    return false;
  }

  *committed_size = 0;
  spans->clear();
  Microsoft::WRL::ComPtr<ITfRange> start_to_end_range;
  Microsoft::WRL::ComPtr<ITfRange> end_range;
  if (FAILED(context->GetStart(read_only_edit_cookie, &start_to_end_range))) {
    return false;
  }
  if (FAILED(context->GetEnd(read_only_edit_cookie, &end_range)))
    return false;
  if (FAILED(start_to_end_range->ShiftEndToRange(
          read_only_edit_cookie, end_range.Get(), TF_ANCHOR_END))) {
    return false;
  }

  Microsoft::WRL::ComPtr<IEnumTfRanges> ranges;
  if (FAILED(track_property->EnumRanges(read_only_edit_cookie, &ranges,
                                        start_to_end_range.Get()))) {
    return false;
  }

  while (true) {
    Microsoft::WRL::ComPtr<ITfRange> range;
    if (ranges->Next(1, &range, nullptr) != S_OK)
      break;
    base::win::ScopedVariant value;
    Microsoft::WRL::ComPtr<IEnumTfPropertyValue> enum_prop_value;
    if (FAILED(track_property->GetValue(read_only_edit_cookie, range.Get(),
                                        value.Receive()))) {
      return false;
    }
    if (FAILED(value.AsInput()->punkVal->QueryInterface(
            IID_PPV_ARGS(&enum_prop_value))))
      return false;

    TF_PROPERTYVAL property_value;
    bool is_composition = false;
    bool has_display_attribute = false;
    TF_DISPLAYATTRIBUTE display_attribute = {};
    while (enum_prop_value->Next(1, &property_value, nullptr) == S_OK) {
      if (IsEqualGUID(property_value.guidId, GUID_PROP_COMPOSING)) {
        is_composition = (property_value.varValue.lVal == TRUE);
      } else if (IsEqualGUID(property_value.guidId, GUID_PROP_ATTRIBUTE)) {
        TfGuidAtom guid_atom =
            static_cast<TfGuidAtom>(property_value.varValue.lVal);
        if (GetDisplayAttribute(guid_atom, &display_attribute))
          has_display_attribute = true;
      }
      VariantClear(&property_value.varValue);
    }

    Microsoft::WRL::ComPtr<ITfRangeACP> range_acp;
    range.As(&range_acp);
    LONG start_pos, length;
    range_acp->GetExtent(&start_pos, &length);
    if (!is_composition) {
      if (*committed_size < static_cast<size_t>(start_pos + length))
        *committed_size = start_pos + length;
    } else {
      // Check for the formats of the actively composed text.
      ImeTextSpan span;
      span.start_offset = start_pos;
      span.end_offset = start_pos + length;
      span.background_color = SK_ColorTRANSPARENT;
      if (selection_.EqualsIgnoringDirection(
              gfx::Range(span.start_offset, span.end_offset))) {
        span.interim_char_selection = is_selection_interim_char_;
      }
      if (has_display_attribute)
        GetStyle(display_attribute, &span);
      spans->push_back(span);
    }
  }
  return true;
}

void TSFTextStore::ResetCompositionState() {
  previous_composition_string_.clear();
  previous_composition_start_ = 0;
  previous_composition_selection_range_ = gfx::Range::InvalidRange();
  previous_text_spans_.clear();

  string_pending_insertion_.clear();
  composition_range_.set_start(0);
  composition_range_.set_end(0);

  selection_ = gfx::Range(composition_from_client_.end(),
                          composition_from_client_.end());
  composition_start_ = selection_.end();
}

bool TSFTextStore::TerminateComposition() {
  TRACE_EVENT0("ime", "TSFTextStore::TerminateComposition");
  if (context_ && has_composition_range_) {
    Microsoft::WRL::ComPtr<ITfContextOwnerCompositionServices> service;

    if (SUCCEEDED(context_->QueryInterface(IID_PPV_ARGS(&service)))) {
      service->TerminateComposition(nullptr);
      has_composition_range_ = false;
      return true;
    }
  }

  return false;
}

void TSFTextStore::CalculateTextandSelectionDiffAndNotifyIfNeeded() {
  // If this is a re-entrant call, then bail out early so we don't end up
  // in an infinite loop of sending notifications as TSF calls back into us
  // when we send a text/selection change notification.
  if (!text_input_client_ || is_notification_in_progress_ ||
      is_tic_write_in_progress_)
    return;

  // TODO(snianu) Perhaps we can do the diff at the TextInputManager layer and
  // only report the diffs?
  TRACE_EVENT0("ime",
               "TSFTextStore::CalculateTextandSelectionDiffAndNotifyIfNeeded");
  gfx::Range latest_buffer_range_from_client;
  std::u16string latest_buffer_from_client;
  gfx::Range latest_selection_from_client;

  if (text_input_client_->GetTextRange(&latest_buffer_range_from_client) &&
      text_input_client_->GetTextFromRange(latest_buffer_range_from_client,
                                           &latest_buffer_from_client) &&
      text_input_client_->GetEditableSelectionRange(
          &latest_selection_from_client) &&
      latest_selection_from_client.IsBoundedBy(
          latest_buffer_range_from_client)) {
    gfx::Range latest_composition_from_client;
    if (text_input_client_->HasCompositionText() &&
        text_input_client_->GetCompositionTextRange(
            &latest_composition_from_client))
      composition_from_client_ = latest_composition_from_client;
    else
      composition_from_client_ = latest_selection_from_client;
    // if the text and selection from text input client is the same as the text
    // and buffer we got last time, either the state hasn't changed since last
    // time we synced or the change hasn't completed yet. Either case we don't
    // want to update our buffer and selection cache. We also don't notify
    // input service about the change.
    if (!buffer_from_client_.compare(latest_buffer_from_client) &&
        selection_from_client_.EqualsIgnoringDirection(
            latest_selection_from_client)) {
      return;
    }

    // update cache value for next comparison.
    buffer_from_client_ = latest_buffer_from_client;
    selection_from_client_.set_start(latest_selection_from_client.start());
    selection_from_client_.set_end(latest_selection_from_client.end());

    if (has_composition_range_) {
      return;
    }

    bool notify_text_change =
        (text_store_acp_sink_mask_ & TS_AS_TEXT_CHANGE) != 0;
    bool notify_selection_change =
        (text_store_acp_sink_mask_ & TS_AS_SEL_CHANGE) != 0;

    bool text_changed = false;
    bool selection_changed = false;
    TS_TEXTCHANGE text_change = {};

    if (latest_buffer_from_client.compare(string_buffer_document_)) {

      // Execute diffing algorithm only if we need to send notification.
      if (notify_text_change) {
        size_t acp_start = 0;
        size_t acp_old_end = string_buffer_document_.size();
        size_t acp_new_end = latest_buffer_from_client.size();

        // Compare two strings to find first difference.
        for (; acp_start < std::min(latest_buffer_from_client.size(),
                                    string_buffer_document_.size());
             acp_start++) {
          if (latest_buffer_from_client.at(acp_start) !=
              string_buffer_document_.at(acp_start)) {
            break;
          }
        }

        // Compare two strings to find last difference.
        while (acp_old_end > 0 && acp_new_end > 0) {
          acp_old_end--;
          acp_new_end--;
          if (acp_old_end >= acp_start && acp_new_end >= acp_start) {
            if (latest_buffer_from_client.at(acp_new_end) !=
                string_buffer_document_.at(acp_old_end)) {
              acp_old_end++;
              acp_new_end++;
              break;
            }
          } else {
            acp_old_end++;
            acp_new_end++;
            break;
          }
        }

        text_change.acpStart = acp_start;
        text_change.acpOldEnd = acp_old_end;
        text_change.acpNewEnd = acp_new_end;
      }

      string_buffer_document_ = latest_buffer_from_client;
      text_changed = true;
    }

    if (!selection_.EqualsIgnoringDirection(latest_selection_from_client)) {
      selection_.set_start(latest_selection_from_client.GetMin());
      selection_.set_end(latest_selection_from_client.GetMax());

      selection_changed = true;
    }

    // We should notify input service about text/selection change only after
    // the cache has already been updated because input service may call back
    // into us during notification.
    is_notification_in_progress_ = true;
    if (notify_text_change && text_changed) {
      TRACE_EVENT2(
          "ime", "TSFTextStore::CalculateTextandSelectionDiffAndNotifyIfNeeded",
          "text_change_start", std::to_string(text_change.acpStart),
          "text_change_end", std::to_string(text_change.acpNewEnd));
      text_store_acp_sink_->OnTextChange(0, &text_change);
    }

    if (notify_selection_change && selection_changed) {
      TRACE_EVENT1(
          "ime", "TSFTextStore::CalculateTextandSelectionDiffAndNotifyIfNeeded",
          "new_selection", selection_.ToString());
      text_store_acp_sink_->OnSelectionChange();
    }
    is_notification_in_progress_ = false;
  }
}

void TSFTextStore::OnContextInitialized(ITfContext* context) {
  context_ = context;
}

void TSFTextStore::SetFocusedTextInputClient(
    HWND focused_window,
    TextInputClient* text_input_client) {
  window_handle_ = focused_window;
  text_input_client_ = text_input_client;
}

void TSFTextStore::RemoveFocusedTextInputClient(
    TextInputClient* text_input_client) {
  if (text_input_client_ == text_input_client) {
    window_handle_ = nullptr;
    text_input_client_ = nullptr;
  }
}

void TSFTextStore::SetImeKeyEventDispatcher(
    ImeKeyEventDispatcher* ime_key_event_dispatcher) {
  ime_key_event_dispatcher_ = ime_key_event_dispatcher;
}

void TSFTextStore::RemoveImeKeyEventDispatcher(
    ImeKeyEventDispatcher* ime_key_event_dispatcher) {
  if (ime_key_event_dispatcher == ime_key_event_dispatcher_) {
    ime_key_event_dispatcher_ = nullptr;
  }
}

bool TSFTextStore::CancelComposition() {
  // This method should correspond to
  //   ImmNotifyIME(NI_COMPOSITIONSTR, CPS_CANCEL, 0)
  // in IMM32 hence calling falling back to |ConfirmComposition()| is not
  // technically correct, because |ConfirmComposition()| corresponds to
  // |CPS_COMPLETE| rather than |CPS_CANCEL|.
  // However in Chromium it seems that |InputMethod::CancelComposition()|
  // might have already committed composing text despite its name.
  // TODO(IME): Check other platforms to see if |CancelComposition()| is
  //            actually working or not.

  if (edit_flag_ || !text_input_client_)
    return false;

  TRACE_EVENT0("ime", "TSFTextStore::CancelComposition");

  ResetCompositionState();

  return TerminateComposition();
}

bool TSFTextStore::ConfirmComposition() {
  // If there is an on-going document lock, we must not edit the text.
  if (edit_flag_)
    return false;

  if (string_pending_insertion_.empty())
    return true;

  if (!text_input_client_)
    return false;

  ResetCompositionState();

  return TerminateComposition();
}

void TSFTextStore::SendOnLayoutChange() {
  // A re-entrant call leads to infinite loop in TSF.
  // We bail out if are in the process of notifying TSF about changes.
  if (is_notification_in_progress_ || is_empty_text_store_) {
    return;
  }
  CalculateTextandSelectionDiffAndNotifyIfNeeded();
  if (text_store_acp_sink_ &&
      (text_store_acp_sink_mask_ & TS_AS_LAYOUT_CHANGE)) {
    TRACE_EVENT0("ime", "TSFTextStore::SendOnLayoutChange");
    text_store_acp_sink_->OnLayoutChange(TS_LC_CHANGE, 0);
  }
}

bool TSFTextStore::HasReadLock() const {
  return (current_lock_type_ & TS_LF_READ) == TS_LF_READ;
}

bool TSFTextStore::HasReadWriteLock() const {
  return (current_lock_type_ & TS_LF_READWRITE) == TS_LF_READWRITE;
}

void TSFTextStore::StartCompositionOnExistingText() const {
  ui::ImeTextSpans text_spans = text_spans_;
  // Adjusts the offset.
  for (size_t i = 0; i < text_spans.size(); ++i) {
    text_spans[i].start_offset -= composition_start_;
    text_spans[i].end_offset -= composition_start_;
  }

  text_input_client_->SetCompositionFromExistingText(composition_range_,
                                                     text_spans);
}

void TSFTextStore::CommitTextAndEndCompositionIfAny(size_t old_size,
                                                    size_t new_size) const {
  size_t new_committed_string_offset;
  size_t new_committed_string_size;
  if (new_text_inserted_ && !text_input_client_->HasCompositionText()) {
    // This is a special case to handle text replacement scenarios during
    // English typing when we are trying to replace an existing text with some
    // new text. Some third-party IMEs also use SetText() API instead of
    // InsertTextAtSelection() API to insert new text.
    size_t new_text_size;
    if (new_size == replace_text_range_.start()) {
      // This usually happens when TSF is trying to replace a part of a string
      // from the selection end
      new_text_size = new_size;
    } else {
      new_text_size = new_size - replace_text_range_.start();
    }
    // If |new_text_size| is 0, then we want to commit composition with current
    // composition text if there is any. Construct |new_committed_string| to be
    // current composition text so that |TextInputClient::InsertText| will
    // commit current composition text.
    // Also clamp the offsets if they are out of bounds of the buffer
    new_committed_string_offset =
        std::min(static_cast<ULONG>(replace_text_range_.start()),
                 static_cast<ULONG>(string_buffer_document_.size()));
    new_committed_string_size =
        (new_text_size == 0 && selection_.end() > new_committed_string_offset)
            ? selection_.end() - new_committed_string_offset
            : new_text_size;
    // if the |replace_text_range_| start is greater than |old_size|, then we
    // don't need to delete anything because the replacement text hasn't been
    // inserted into blink yet.
    if (old_size > replace_text_range_.start()) {
      text_input_client_->ExtendSelectionAndDelete(
          old_size - replace_text_range_.start(), 0);
    }
  } else {
    new_committed_string_offset = old_size;
    new_committed_string_size = new_size - old_size;
    // This is a special case. We should only replace existing text and commit
    // the new text if replacement text has already been inserted into Blink.
    if (new_text_inserted_ && (old_size > replace_text_range_.start()) &&
        !replace_text_range_.is_empty()) {
      // Delete text that has already been inserted into blink.
      text_input_client_->ExtendSelectionAndDelete(
          replace_text_range_.end() - replace_text_range_.start(), 0);

      new_committed_string_offset = replace_text_range_.start();
      new_committed_string_size = replace_text_size_;
    }
    // If |new_committed_string_size| is 0, then we want to commit composition
    // with current composition text if there is any. Construct
    // |new_committed_string| to be current composition text so that
    // |TextInputClient::InsertText| will commit current composition text.
    // Also clamp the offsets if they are out of bounds of the buffer
    new_committed_string_offset =
        std::min(static_cast<ULONG>(new_committed_string_offset),
                 static_cast<ULONG>(string_buffer_document_.size()));
    new_committed_string_size =
        (new_committed_string_size == 0 &&
         selection_.end() > new_committed_string_offset)
            ? selection_.end() - new_committed_string_offset
            : new_committed_string_size;
  }

  // Construct string to be committed.
  const std::u16string& new_committed_string = string_buffer_document_.substr(
      new_committed_string_offset, new_committed_string_size);
  // TODO(crbug.com/41467857): Unify the behavior of
  //     |TextInputClient::InsertText(text)| for the empty text.
  if (!new_committed_string.empty()) {
    // If composition was started and committed in one edit session, we still
    // need to start the composition first and then commit it.
    if (!text_input_client_->HasCompositionText() &&
        on_start_composition_called_) {
      ImeTextSpans spans;
      ImeTextSpan span;
      span.start_offset = 0;
      span.end_offset = new_committed_string.size();
      spans.push_back(span);
      CompositionText composition_text;
      composition_text.text = new_committed_string;
      composition_text.ime_text_spans = spans;
      composition_text.selection.set_start(new_committed_string.size());
      composition_text.selection.set_end(new_committed_string.size());
      text_input_client_->SetCompositionText(composition_text);
    }
    text_input_client_->InsertText(
        new_committed_string,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  } else {
    text_input_client_->ClearCompositionText();
  }

  if (!selection_.is_empty() && !IsInputIME() &&
      selection_.GetMax() <= string_buffer_document_.size()) {
    text_input_client_->SetEditableSelectionRange(selection_);
  }

  // Notify accessibility about this committed composition
  text_input_client_->SetActiveCompositionForAccessibility(
      replace_text_range_, new_committed_string,
      /*is_composition_committed*/ true);
}

void TSFTextStore::StartCompositionOnNewText(
    size_t start_offset,
    const std::u16string& composition_string) {
  CompositionText composition_text;
  composition_text.text = composition_string;
  composition_text.ime_text_spans = text_spans_;

  for (size_t i = 0; i < composition_text.ime_text_spans.size(); ++i) {
    composition_text.ime_text_spans[i].start_offset -= start_offset;
    composition_text.ime_text_spans[i].end_offset -= start_offset;
  }

  if (selection_.start() < start_offset) {
    composition_text.selection.set_start(0);
  } else {
    composition_text.selection.set_start(selection_.start() - start_offset);
  }

  if (selection_.end() < start_offset) {
    composition_text.selection.set_end(0);
  } else {
    composition_text.selection.set_end(selection_.end() - start_offset);
  }

  if (text_input_client_) {
    text_input_client_->SetCompositionText(composition_text);
    // Notify accessibility about this ongoing composition if the string is not
    // empty
    if (!composition_string.empty()) {
      text_input_client_->SetActiveCompositionForAccessibility(
          composition_range_, composition_string,
          /*is_composition_committed*/ false);
    } else {
      // User wants to commit the current composition
      const std::u16string& committed_string = string_buffer_document_.substr(
          composition_range_.GetMin(), composition_range_.length());
      text_input_client_->SetActiveCompositionForAccessibility(
          composition_range_, committed_string,
          /*is_composition_committed*/ true);
    }
  }
}

void TSFTextStore::GetStyle(const TF_DISPLAYATTRIBUTE& attribute,
                            ImeTextSpan* span) {
  // Use the display attribute to pick the right formats for the underline and
  // text.
  // Set the default values first and then check if display attribute has
  // any style or not.
  span->thickness = attribute.fBoldLine ? ImeTextSpan::Thickness::kThick
                                        : ImeTextSpan::Thickness::kThin;
  switch (attribute.lsStyle) {
    case TF_LS_SOLID: {
      span->underline_style = ImeTextSpan::UnderlineStyle::kSolid;
      break;
    }
    case TF_LS_DOT: {
      span->underline_style = ImeTextSpan::UnderlineStyle::kDot;
      break;
    }
    case TF_LS_DASH: {
      span->underline_style = ImeTextSpan::UnderlineStyle::kDash;
      break;
    }
    case TF_LS_SQUIGGLE: {
      span->underline_style = ImeTextSpan::UnderlineStyle::kSquiggle;
      break;
    }
    case TF_LS_NONE: {
      span->underline_style = ImeTextSpan::UnderlineStyle::kNone;
      break;
    }
    default: {
      span->underline_style = ImeTextSpan::UnderlineStyle::kSolid;
    }
  }
  if (attribute.crText.type != TF_CT_NONE) {
    span->text_color = SkColorSetRGB(GetRValue(attribute.crText.cr),
                                     GetGValue(attribute.crText.cr),
                                     GetBValue(attribute.crText.cr));
  }
  if (attribute.crLine.type != TF_CT_NONE) {
    span->underline_color = SkColorSetRGB(GetRValue(attribute.crLine.cr),
                                          GetGValue(attribute.crLine.cr),
                                          GetBValue(attribute.crLine.cr));
  }
}

void TSFTextStore::ResetCacheAfterEditSession() {
  // reset the flag since we've already inserted/replaced the text.
  new_text_inserted_ = false;
  is_selection_interim_char_ = false;
  // reset |on_start_composition_called_| for next edit session.
  on_start_composition_called_ = false;

  // reset string_buffer_ if composition is no longer active.
  if (text_input_client_ && !text_input_client_->HasCompositionText())
    string_pending_insertion_.clear();
}

bool TSFTextStore::IsInputIME() const {
  TF_INPUTPROCESSORPROFILE profile;
  if (SUCCEEDED(input_processor_profile_mgr_->GetActiveProfile(
          GUID_TFCAT_TIP_KEYBOARD, &profile))) {
    return profile.hkl == NULL &&
           profile.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR;
  }
  return false;
}

void TSFTextStore::UseEmptyTextStore(bool is_enabled) {
  is_empty_text_store_ = is_enabled;
}

bool TSFTextStore::MaybeSendOnUrlChanged() {
  // When the user interacts with a traditional editing control, TSF will query
  // for the current Url as needed. However, when TSF supports empty stores, we
  // will also notify the OS when a frame with a committed Url is focused, to
  // enable scenarios where, for example, a page implements its own controls in
  // JavaScript (crbug.com/1447061).
  if (!is_empty_text_store_ || (text_store_acp_sink_ == nullptr)) {
    return false;
  }
  TS_ATTRID attrs[1];
  attrs[0] = GUID_PROP_URL;
  text_store_acp_sink_->OnAttrsChange(NULL, NULL, 1, attrs);
  return true;
}

}  // namespace ui
