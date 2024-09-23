// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_TSF_TEXT_STORE_H_
#define UI_BASE_IME_WIN_TSF_TEXT_STORE_H_

#include <msctf.h>
#include <wrl/client.h>
#include <deque>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/range/range.h"

namespace ui {
class TextInputClient;

// d5138268-a1bf-4308-bcbf-2e739398e234
// Bootstrap the definition of the GUID for the URL property which
// will be defined in the windows SDK.
// https://docs.microsoft.com/en-us/windows/win32/tsf/predefined-properties
const GUID GUID_PROP_URL = {0xd5138268,
                            0xa1bf,
                            0x4308,
                            {0xbc, 0xbf, 0x2e, 0x73, 0x93, 0x98, 0xe2, 0x34}};

// TSFTextStore is used to interact with the input method via TSF manager.
// TSFTextStore have a string buffer which is manipulated by TSF manager through
// ITextStoreACP interface methods such as SetText().
// When the input method updates the composition, TSFTextStore calls
// TextInputClient::SetCompositionText(). And when the input method finishes the
// composition, TSFTextStore calls TextInputClient::InsertText().
//
// How TSFTextStore works:
//  - Assume the document is empty and in focus.
//  - The user enters "a".
//    - The input method set composition as "a".
//    - TSF manager calls TSFTextStore::RequestLock().
//    - TSFTextStore callbacks ITextStoreACPSink::OnLockGranted().
//    - In OnLockGranted(), TSF manager calls
//      - TSFTextStore::OnStartComposition()
//      - TSFTextStore::SetText()
//        The pending string buffer is set as "a".
//        The document whole buffer is set as "a".
//      - TSFTextStore::OnUpdateComposition()
//      - TSFTextStore::OnEndEdit()
//        TSFTextStore can get the composition information such as underlines.
//   - TSFTextStore calls TextInputClient::SetCompositionText().
//     "a" is shown with an underline as composition string.
// - The user enters 'b'.
//    - The input method set composition as "ab".
//    - TSF manager calls TSFTextStore::RequestLock().
//    - TSFTextStore callbacks ITextStoreACPSink::OnLockGranted().
//    - In OnLockGranted(), TSF manager calls
//      - TSFTextStore::SetText()
//        The pending string buffer is set as "b".
//        The document whole buffer is changed to "ab".
//      - TSFTextStore::OnUpdateComposition()
//      - TSFTextStore::OnEndEdit()
//   - TSFTextStore calls TextInputClient::SetCompositionText().
//     "ab" is shown with an underline as composition string.
// - The user enters <space>.
//    - The input method set composition as "aB".
//    - TSF manager calls TSFTextStore::RequestLock().
//    - TSFTextStore callbacks ITextStoreACPSink::OnLockGranted().
//    - In OnLockGranted(), TSF manager calls
//      - TSFTextStore::SetText()
//        The pending string buffer is set as "B".
//        The document whole buffer is changed to "aB".
//      - TSFTextStore::OnUpdateComposition()
//      - TSFTextStore::OnEndEdit()
//   - TSFTextStore calls TextInputClient::SetCompositionText().
//     "aB" is shown with an underline as composition string.
// - The user enters <enter>.
//    - The input method commits "aB".
//    - TSF manager calls TSFTextStore::RequestLock().
//    - TSFTextStore callbacks ITextStoreACPSink::OnLockGranted().
//    - In OnLockGranted(), TSF manager calls
//      - TSFTextStore::OnEndComposition()
//      - TSFTextStore::OnEndEdit()
//        TSFTextStore knows "aB" is committed.
//   - TSFTextStore calls TextInputClient::InsertText().
//     "aB" is shown as committed string.
//   - TSFTextStore clears the pending string buffer.
//   - TSFTextStore verified if the document whole buffer is the same as the
//     buffer returned from TextInputClient. If the buffer is different, then
//     call OnSelectionChange(), OnLayoutChange() and
//     OnTextChange() of ITextStoreACPSink to let TSF manager know that the
//     string buffer has been changed other than IME.
//
// About the locking scheme:
// When TSF manager manipulates the string buffer it calls RequestLock() to get
// the lock of the document. If TSFTextStore can grant the lock request, it
// callbacks ITextStoreACPSink::OnLockGranted().
// RequestLock() is called from only one thread, but called recursively in
// OnLockGranted() or OnSelectionChange() or OnLayoutChange() or OnTextChange().
// If the document is locked and the lock request is asynchronous, TSFTextStore
// queues the request. The queued requests will be handled after the current
// lock is removed.
// More information about document locks can be found here:
//   http://msdn.microsoft.com/en-us/library/ms538064
//
// More information about TSF can be found here:
//   http://msdn.microsoft.com/en-us/library/ms629032
class COMPONENT_EXPORT(UI_BASE_IME_WIN) TSFTextStore
    : public ITextStoreACP,
      public ITfContextOwnerCompositionSink,
      public ITfLanguageProfileNotifySink,
      public ITfKeyTraceEventSink,
      public ITfTextEditSink {
 public:
  TSFTextStore();

  TSFTextStore(const TSFTextStore&) = delete;
  TSFTextStore& operator=(const TSFTextStore&) = delete;

  virtual ~TSFTextStore();
  HRESULT Initialize();

  // ITextStoreACP:
  IFACEMETHODIMP_(ULONG) AddRef() override;
  IFACEMETHODIMP_(ULONG) Release() override;
  IFACEMETHODIMP QueryInterface(REFIID iid, void** ppv) override;
  IFACEMETHODIMP AdviseSink(REFIID iid, IUnknown* unknown, DWORD mask) override;
  IFACEMETHODIMP FindNextAttrTransition(LONG acp_start,
                                        LONG acp_halt,
                                        ULONG num_filter_attributes,
                                        const TS_ATTRID* filter_attributes,
                                        DWORD flags,
                                        LONG* acp_next,
                                        BOOL* found,
                                        LONG* found_offset) override;
  IFACEMETHODIMP GetACPFromPoint(TsViewCookie view_cookie,
                                 const POINT* point,
                                 DWORD flags,
                                 LONG* acp) override;
  IFACEMETHODIMP GetActiveView(TsViewCookie* view_cookie) override;
  IFACEMETHODIMP GetEmbedded(LONG acp_pos,
                             REFGUID service,
                             REFIID iid,
                             IUnknown** unknown) override;
  IFACEMETHODIMP GetEndACP(LONG* acp) override;
  IFACEMETHODIMP GetFormattedText(LONG acp_start,
                                  LONG acp_end,
                                  IDataObject** data_object) override;
  IFACEMETHODIMP GetScreenExt(TsViewCookie view_cookie, RECT* rect) override;
  IFACEMETHODIMP GetSelection(ULONG selection_index,
                              ULONG selection_buffer_size,
                              TS_SELECTION_ACP* selection_buffer,
                              ULONG* fetched_count) override;
  IFACEMETHODIMP GetStatus(TS_STATUS* pdcs) override;
  IFACEMETHODIMP GetText(LONG acp_start,
                         LONG acp_end,
                         wchar_t* text_buffer,
                         ULONG text_buffer_size,
                         ULONG* text_buffer_copied,
                         TS_RUNINFO* run_info_buffer,
                         ULONG run_info_buffer_size,
                         ULONG* run_info_buffer_copied,
                         LONG* next_acp) override;
  IFACEMETHODIMP GetTextExt(TsViewCookie view_cookie,
                            LONG acp_start,
                            LONG acp_end,
                            RECT* rect,
                            BOOL* clipped) override;
  IFACEMETHODIMP GetWnd(TsViewCookie view_cookie, HWND* window_handle) override;
  IFACEMETHODIMP InsertEmbedded(DWORD flags,
                                LONG acp_start,
                                LONG acp_end,
                                IDataObject* data_object,
                                TS_TEXTCHANGE* change) override;
  IFACEMETHODIMP InsertEmbeddedAtSelection(DWORD flags,
                                           IDataObject* data_object,
                                           LONG* acp_start,
                                           LONG* acp_end,
                                           TS_TEXTCHANGE* change) override;
  IFACEMETHODIMP InsertTextAtSelection(DWORD flags,
                                       const wchar_t* text_buffer,
                                       ULONG text_buffer_size,
                                       LONG* acp_start,
                                       LONG* acp_end,
                                       TS_TEXTCHANGE* text_change) override;
  IFACEMETHODIMP QueryInsert(LONG acp_test_start,
                             LONG acp_test_end,
                             ULONG text_size,
                             LONG* acp_result_start,
                             LONG* acp_result_end) override;
  IFACEMETHODIMP QueryInsertEmbedded(const GUID* service,
                                     const FORMATETC* format,
                                     BOOL* insertable) override;
  IFACEMETHODIMP RequestAttrsAtPosition(LONG acp_pos,
                                        ULONG attribute_buffer_size,
                                        const TS_ATTRID* attribute_buffer,
                                        DWORD flags) override;
  IFACEMETHODIMP RequestAttrsTransitioningAtPosition(
      LONG acp_pos,
      ULONG attribute_buffer_size,
      const TS_ATTRID* attribute_buffer,
      DWORD flags) override;
  IFACEMETHODIMP RequestLock(DWORD lock_flags, HRESULT* result) override;
  IFACEMETHODIMP RequestSupportedAttrs(
      DWORD flags,
      ULONG attribute_buffer_size,
      const TS_ATTRID* attribute_buffer) override;
  IFACEMETHODIMP RetrieveRequestedAttrs(
      ULONG attribute_buffer_size,
      TS_ATTRVAL* attribute_buffer,
      ULONG* attribute_buffer_copied) override;
  IFACEMETHODIMP SetSelection(
      ULONG selection_buffer_size,
      const TS_SELECTION_ACP* selection_buffer) override;
  IFACEMETHODIMP SetText(DWORD flags,
                         LONG acp_start,
                         LONG acp_end,
                         const wchar_t* text_buffer,
                         ULONG text_buffer_size,
                         TS_TEXTCHANGE* text_change) override;
  IFACEMETHODIMP UnadviseSink(IUnknown* unknown) override;

  // ITfContextOwnerCompositionSink:
  IFACEMETHODIMP OnStartComposition(ITfCompositionView* composition_view,
                                    BOOL* ok) override;
  IFACEMETHODIMP OnUpdateComposition(ITfCompositionView* composition_view,
                                     ITfRange* range) override;
  IFACEMETHODIMP OnEndComposition(
      ITfCompositionView* composition_view) override;

  // ITfLanguageProfileNotifySink:
  IFACEMETHODIMP OnLanguageChange(LANGID langid, BOOL* pfAccept) override;
  IFACEMETHODIMP OnLanguageChanged() override;

  // ITfTextEditSink:
  IFACEMETHODIMP OnEndEdit(ITfContext* context,
                           TfEditCookie read_only_edit_cookie,
                           ITfEditRecord* edit_record) override;

  // ITfKeyTraceEventSink
  IFACEMETHODIMP OnKeyTraceDown(WPARAM wParam, LPARAM lParam) override;
  IFACEMETHODIMP OnKeyTraceUp(WPARAM wParam, LPARAM lParam) override;

  // Called after |TSFBridgeImpl::CreateDocumentManager| to tell that the
  // text-store is successfully associated with a Context.
  void OnContextInitialized(ITfContext* context);

  // Sets currently focused TextInputClient.
  void SetFocusedTextInputClient(HWND focused_window,
                                 TextInputClient* text_input_client);
  // Removes currently focused TextInputClient.
  void RemoveFocusedTextInputClient(TextInputClient* text_input_client);

  // Sets ImeKeyEventDispatcher pointer.
  void SetImeKeyEventDispatcher(
      ImeKeyEventDispatcher* ime_key_event_dispatcher);

  // Removes ImeKeyEventDispatcher pointer.
  void RemoveImeKeyEventDispatcher(
      ImeKeyEventDispatcher* ime_key_event_dispatcher);

  // Cancels the ongoing composition if exists.
  bool CancelComposition();

  // Confirms the ongoing composition if exists.
  bool ConfirmComposition();

  // Sends OnLayoutChange() via |text_store_acp_sink_|.
  void SendOnLayoutChange();

  // Sends a Url change notification via |text_store_acp_sink_| if the current
  // version of TSF supports empty text stores.
  bool MaybeSendOnUrlChanged();

  // Sets the flag to indicate TSF support for empty text stores.
  void UseEmptyTextStore(bool is_enabled);

 private:
  friend class TSFTextStoreTest;
  friend class TSFTextStoreTestCallback;

  // Reset states tracking the composition in the text store.
  void ResetCompositionState();

  // Terminate an active composition for this text store.
  bool TerminateComposition();

  // Compare our cached text buffer and selection with the up-to-date
  // text buffer and selection from TextInputClient. We also update
  // cached text buffer and selection with the new version. Then notify
  // input service about the change.
  void CalculateTextandSelectionDiffAndNotifyIfNeeded();

  // Synthesize keyevent and send to text input client to fire corresponding
  // javascript keyevent during composition.
  void DispatchKeyEvent(ui::EventType type, WPARAM wparam, LPARAM lparam);

  // Start new composition on existing text.
  void StartCompositionOnExistingText() const;

  // Start new composition with new text.
  void StartCompositionOnNewText(size_t start_offset,
                                 const std::u16string& composition_string);

  // Commit and insert text into TextInputClient. End any ongoing composition.
  void CommitTextAndEndCompositionIfAny(size_t old_size, size_t new_size) const;

  // Checks if the document has a read-only lock.
  bool HasReadLock() const;

  // Checks if the document has a read and write lock.
  bool HasReadWriteLock() const;

  // Gets the display attribute structure.
  bool GetDisplayAttribute(TfGuidAtom guid_atom,
                           TF_DISPLAYATTRIBUTE* attribute);

  // Gets the committed string size and underline information of the context.
  bool GetCompositionStatus(ITfContext* context,
                            const TfEditCookie read_only_edit_cookie,
                            size_t* committed_size,
                            ImeTextSpans* spans);

  // Reset all cached flags when |TSFTextStore::RequestLock| returns.
  void ResetCacheAfterEditSession();

  // Returns if current input method is an IME.
  bool IsInputIME() const;

  // Gets the style information from the display attribute for the actively
  // composed text.
  void GetStyle(const TF_DISPLAYATTRIBUTE& attribute, ImeTextSpan* span);

  // Indicates if it is a dummy/empty text store without any text capability.
  bool is_empty_text_store_ = false;

  // The reference count of this instance.
  volatile LONG ref_count_ = 0;

  // A pointer of ITextStoreACPSink, this instance is given in AdviseSink.
  Microsoft::WRL::ComPtr<ITextStoreACPSink> text_store_acp_sink_;

  // The current mask of |text_store_acp_sink_|.
  DWORD text_store_acp_sink_mask_ = 0;

  // HWND of the current view window which is set in SetFocusedTextInputClient.
  HWND window_handle_ = nullptr;

  // Current TextInputClient which is set in SetFocusedTextInputClient.
  raw_ptr<TextInputClient> text_input_client_ = nullptr;

  // ImeKeyEventDispatcher instance which is used dispatch key events.
  raw_ptr<ImeKeyEventDispatcher, AcrossTasksDanglingUntriaged>
      ime_key_event_dispatcher_ = nullptr;

  //  |string_buffer_document_| contains all string in current active view.
  //  |string_pending_insertion_| contains only string in current edit session.
  //  |composition_start_| indicates the location for a composition to start at.
  //  |has_composition_range_| indicates the state of composition.
  //  |composition_range_| indicates the range of composition if any.
  //  Example: "aoi" is committed, and "umi" is under composition.
  //  In current edit session, user press "i" on keyboard.
  //    |string_buffer_document_|: "aoiumi"
  //    |string_pending_insertion_| : "i"
  //    |composition_start_|: 3
  //    |has_composition_range_| = true;
  //    |composition_range_start_| = 3;
  //    |composition_range_end_| = 6;
  std::u16string string_buffer_document_;
  std::u16string string_pending_insertion_;
  size_t composition_start_ = 0;
  bool has_composition_range_ = false;
  gfx::Range composition_range_;

  // |on_start_composition_called_| indicates that OnStartComposition() is
  // called duriing current edit session.
  bool on_start_composition_called_ = false;

  // |previous_composition_string_| indicicates composition string in last
  // edit session during same composition. |previous_composition_start_|
  // indicates composition start in last session during same composition. If
  // RequestLock() is called during two edit sessions, we don't want to set same
  // composition string twice. |previous_composition_selection_range_| indicates
  // the selection range during composition. We want to send the selection
  // change to blink if IME only change the selection range but not the
  // composition text. |previous_text_spans_| saves the IME spans in previous
  // edit session during same composition.
  std::u16string previous_composition_string_;
  size_t previous_composition_start_ = 0;
  gfx::Range previous_composition_selection_range_ = gfx::Range::InvalidRange();
  ImeTextSpans previous_text_spans_;

  // |new_text_inserted_| indicates there is text to be inserted
  // into blink during ITextStoreACP::SetText().
  // |replace_text_range_| indicates the start and end offsets of the text to be
  // replaced by the new text to be inserted.
  // |replace_text_size_| indicates the size of the text to be inserted.
  // Example: "k" is going to replace "i"
  //   |string_buffer_document_|: "aeiou"
  //   |new_text_inserted_|: true
  //   |replace_text_range_start_|: 2
  //   |replace_text_range_end_|: 3
  //   |replace_text_size_|: 1
  bool new_text_inserted_ = false;
  gfx::Range replace_text_range_;
  size_t replace_text_size_;

  // |buffer_from_client_| contains all string returned from
  // TextInputClient::GetTextFromRange();
  std::u16string buffer_from_client_;

  // |selection_from_client_| indicates the selection range returned from
  // TextInputClient::GetEditableSelectionRange();
  gfx::Range selection_from_client_;

  // |composition_range_from_client_| indicates the composition range returned
  // from TextInputClient::GetCompositionTextRange();
  gfx::Range composition_from_client_;

  // |wparam_keydown_cached_| and |lparam_keydown_cached_| contains key event
  // info that is used to synthesize key event during composition.
  // |wparam_keydown_fired_| indicates if a keydown event has been fired.
  WPARAM wparam_keydown_cached_ = 0;
  LPARAM lparam_keydown_cached_ = 0;
  WPARAM wparam_keydown_fired_ = 0;

  //  |selection_start_| and |selection_end_| indicates the selection range.
  //  Example: "iue" is selected
  //    |string_buffer_document_|: "aiueo"
  //    |selection_.start()|: 1
  //    |selection_.end()|: 4
  gfx::Range selection_;

  // Indicates if the selection is an interim character. Please refer to
  // https://docs.microsoft.com/en-us/windows/win32/api/textstor/ns-textstor-ts_selectionstyle
  bool is_selection_interim_char_ = false;

  //  |start_offset| and |end_offset| of |text_spans_| indicates
  //  the offsets in |string_buffer_document_|.
  //  Example: "aoi" is committed. There are two underlines in "umi" and "no".
  //    |string_buffer_document_|: "aoiumino"
  //    |composition_start_|: 3
  //    text_spans_[0].start_offset: 3
  //    text_spans_[0].end_offset: 6
  //    text_spans_[1].start_offset: 6
  //    text_spans_[1].end_offset: 8
  ImeTextSpans text_spans_;

  // |edit_flag_| indicates that the status is edited during
  // ITextStoreACPSink::OnLockGranted().
  bool edit_flag_ = false;

  // Checks for re-entrancy while notifying changes to TSF.
  bool is_notification_in_progress_ = false;

  // Checks for re-entrancy while writing to text input client.
  bool is_tic_write_in_progress_ = false;

  // The type of current lock.
  //   0: No lock.
  //   TS_LF_READ: read-only lock.
  //   TS_LF_READWRITE: read/write lock.
  DWORD current_lock_type_ = 0;

  // Queue of the lock request used in RequestLock().
  std::deque<DWORD> lock_queue_;

  // Category manager and Display attribute manager are used to obtain the
  // attributes of the composition string.
  Microsoft::WRL::ComPtr<ITfCategoryMgr> category_manager_;
  Microsoft::WRL::ComPtr<ITfDisplayAttributeMgr> display_attribute_manager_;
  Microsoft::WRL::ComPtr<ITfContext> context_;
  Microsoft::WRL::ComPtr<ITfInputProcessorProfileMgr>
      input_processor_profile_mgr_;

  // Current list of requested supported attribute values.
  // Currently the supported attributes are URL and InputScope.
  std::vector<TS_ATTRID> supported_attrs_;
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_TSF_TEXT_STORE_H_
