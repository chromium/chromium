// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/ime/win/tsf_bridge.h"

#include <msctf.h>

#include <map>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_variant.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/mock_tsf_bridge.h"
#include "ui/base/ime/win/tsf_text_store.h"
#include "ui/base/ui_base_features.h"

namespace ui {

namespace {

// TSFBridgeImpl -----------------------------------------------------------

// A TLS implementation of TSFBridge.
class TSFBridgeImpl : public TSFBridge {
 public:
  TSFBridgeImpl();

  TSFBridgeImpl(const TSFBridgeImpl&) = delete;
  TSFBridgeImpl& operator=(const TSFBridgeImpl&) = delete;

  ~TSFBridgeImpl() override;

  HRESULT Initialize();

  // TsfBridge:
  void OnTextInputTypeChanged(const TextInputClient* client) override;
  void OnTextLayoutChanged() override;
  bool CancelComposition() override;
  bool ConfirmComposition() override;
  void SetFocusedClient(HWND focused_window, TextInputClient* client) override;
  void RemoveFocusedClient(TextInputClient* client) override;
  void SetImeKeyEventDispatcher(
      ImeKeyEventDispatcher* ime_key_event_dispatcher) override;
  void RemoveImeKeyEventDispatcher(
      ImeKeyEventDispatcher* ime_key_event_dispatcher) override;
  bool IsInputLanguageCJK() override;
  Microsoft::WRL::ComPtr<ITfThreadMgr> GetThreadManager() override;
  TextInputClient* GetFocusedTextInputClient() const override;
  void OnUrlChanged() override;

 private:
  // Returns S_OK if |tsf_document_map_| is successfully initialized. This
  // method should be called from and only from Initialize().
  HRESULT InitializeDocumentMapInternal();

  // Returns S_OK if |context| is successfully updated to be a disabled
  // context, where an IME should be deactivated. This is suitable for some
  // special input context such as password fields.
  HRESULT InitializeDisabledContext(ITfContext* context);

  // Returns S_OK if a TSF document manager and a TSF context is successfully
  // created with associating with given |text_store|. The returned
  // |source_cookie| indicates the binding between |text_store| and |context|.
  // You can pass nullptr to |text_store| and |source_cookie| when text store is
  // not necessary.
  HRESULT CreateDocumentManager(TSFTextStore* text_store,
                                ITfDocumentMgr** document_manager,
                                ITfContext** context,
                                DWORD* source_cookie,
                                DWORD* key_trace_sink_cookie,
                                DWORD* language_profile_cookie);

  // Returns true if |document_manager| is the focused document manager.
  bool IsFocused(ITfDocumentMgr* document_manager);

  // Returns true if already initialized.
  bool IsInitialized();

  // Updates or clears the association maintained in the TSF runtime between
  // |attached_window_handle_| and the current document manager. Keeping this
  // association updated solves some tricky event ordering issues between
  // logical text input focus managed by Chrome and native text input focus
  // managed by the OS.
  // Background:
  //   TSF runtime monitors some Win32 messages such as WM_ACTIVATE to
  //   change the focused document manager. This is problematic when
  //   TSFBridge::SetFocusedClient is called first then the target window
  //   receives WM_ACTIVATE. This actually occurs in Aura environment where
  //   WM_NCACTIVATE is used as a trigger to restore text input focus.
  // Caveats:
  //   TSF runtime does not increment the reference count of the attached
  //   document manager. See the comment inside the method body for
  //   details.
  void UpdateAssociateFocus();
  void ClearAssociateFocus();

  // A triple of document manager, text store and binding cookie between
  // a context owned by the document manager and the text store. This is a
  // minimum working set of an editable document in TSF.
  struct TSFDocument {
   public:
    TSFDocument()
        : source_cookie(TF_INVALID_COOKIE),
          key_trace_sink_cookie(TF_INVALID_COOKIE),
          language_profile_cookie(TF_INVALID_COOKIE) {}
    TSFDocument(const TSFDocument& src)
        : document_manager(src.document_manager),
          source_cookie(src.source_cookie),
          key_trace_sink_cookie(src.key_trace_sink_cookie),
          language_profile_cookie(src.language_profile_cookie) {}
    Microsoft::WRL::ComPtr<ITfDocumentMgr> document_manager;
    scoped_refptr<TSFTextStore> text_store;
    DWORD source_cookie;
    DWORD key_trace_sink_cookie;
    DWORD language_profile_cookie;
  };

  // Returns a pointer to TSFDocument that is associated with the current
  // TextInputType of |client_|.
  TSFDocument* GetAssociatedDocument();

  // An ITfThreadMgr object to be used in focus and document management.
  Microsoft::WRL::ComPtr<ITfThreadMgr> thread_manager_;

  // An ITfInputProcessorProfiles object to be used to get current language
  // locale profile.
  Microsoft::WRL::ComPtr<ITfInputProcessorProfiles> input_processor_profiles_;

  // A map from TextInputType to an editable document for TSF. We use multiple
  // TSF documents that have different InputScopes and TSF attributes based on
  // the TextInputType associated with the target document. For a TextInputType
  // that is not coverted by this map, a default document, e.g. the document
  // for TEXT_INPUT_TYPE_TEXT, should be used.
  // Note that some IMEs don't change their state unless the document focus is
  // changed. This is why we use multiple documents instead of changing TSF
  // metadata of a single document on the fly.
  typedef std::map<TextInputType, TSFDocument> TSFDocumentMap;
  TSFDocumentMap tsf_document_map_;

  // An identifier of TSF client.
  TfClientId client_id_ = TF_CLIENTID_NULL;

  // Current focused text input client. Do not free |client_|.
  raw_ptr<TextInputClient> client_ = nullptr;

  // Input Type of current focused text input client.
  TextInputType input_type_ = TEXT_INPUT_TYPE_NONE;

  // Represents the window that is currently owns text input focus.
  HWND attached_window_handle_ = nullptr;

  // Tracks Windows OS support for empty TSF text stores, available on win11+.
  bool empty_tsf_support_ = false;
};

TSFBridgeImpl::TSFBridgeImpl() = default;

TSFBridgeImpl::~TSFBridgeImpl() {
  DCHECK(base::CurrentUIThread::IsSet());
  if (!IsInitialized())
    return;

  for (auto& pair : tsf_document_map_) {
    TSFDocument& document = pair.second;
    Microsoft::WRL::ComPtr<ITfContext> context;
    Microsoft::WRL::ComPtr<ITfSource> source;
    if (document.source_cookie != TF_INVALID_COOKIE &&
        SUCCEEDED(document.document_manager->GetBase(&context)) &&
        SUCCEEDED(context.As(&source))) {
      source->UnadviseSink(document.source_cookie);
    }
    if (thread_manager_ != nullptr) {
      Microsoft::WRL::ComPtr<ITfSource> key_trace_sink_source;
      if (document.key_trace_sink_cookie != TF_INVALID_COOKIE &&
          SUCCEEDED(thread_manager_.As(&key_trace_sink_source))) {
        key_trace_sink_source->UnadviseSink(document.key_trace_sink_cookie);
      }
      Microsoft::WRL::ComPtr<ITfSource> language_profile_source;
      if (document.language_profile_cookie != TF_INVALID_COOKIE &&
          SUCCEEDED(input_processor_profiles_.As(&language_profile_source))) {
        language_profile_source->UnadviseSink(document.language_profile_cookie);
      }
    }
  }
  tsf_document_map_.clear();

  client_id_ = TF_CLIENTID_NULL;
}

HRESULT TSFBridgeImpl::Initialize() {
  DCHECK(base::CurrentUIThread::IsSet());
  if (client_id_ != TF_CLIENTID_NULL) {
    DVLOG(1) << "Already initialized.";
    return S_OK;  // shouldn't return error code in this case.
  }

  HRESULT hr =
      ::CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_ALL,
                         IID_PPV_ARGS(&input_processor_profiles_));
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to create InputProcessorProfiles instance.";
    return hr;
  }

  hr = ::CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_ALL,
                          IID_PPV_ARGS(&thread_manager_));
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to create ThreadManager instance.";
    return hr;
  }

  hr = thread_manager_->Activate(&client_id_);
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to activate Thread Manager.";
    return hr;
  }

  hr = InitializeDocumentMapInternal();
  if (FAILED(hr))
    return hr;

  // Japanese IME expects the default value of this compartment is
  // TF_SENTENCEMODE_PHRASEPREDICT like IMM32 implementation. This value is
  // managed per thread, so that it is enough to set this value at once. This
  // value does not affect other language's IME behaviors.
  Microsoft::WRL::ComPtr<ITfCompartmentMgr> thread_compartment_manager;
  hr = thread_manager_.As(&thread_compartment_manager);
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to get ITfCompartmentMgr.";
    return hr;
  }

  Microsoft::WRL::ComPtr<ITfCompartment> sentence_compartment;
  hr = thread_compartment_manager->GetCompartment(
      GUID_COMPARTMENT_KEYBOARD_INPUTMODE_SENTENCE, &sentence_compartment);
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to get sentence compartment.";
    return hr;
  }

  base::win::ScopedVariant sentence_variant;
  sentence_variant.Set(TF_SENTENCEMODE_PHRASEPREDICT);
  hr = sentence_compartment->SetValue(client_id_, sentence_variant.ptr());
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to change the sentence mode.";
    return hr;
  }

  return S_OK;
}

void TSFBridgeImpl::OnTextInputTypeChanged(const TextInputClient* client) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(IsInitialized());

  if (client != client_) {
    // Called from not focusing client. Do nothing.
    return;
  }

  // Since we reuse TSF document for same text input type, there is a case where
  // focus is switched between two text fields with same input type. We should
  // prepare the TSF document for reuse by clearing focus first.
  if (input_type_ != TEXT_INPUT_TYPE_NONE &&
      input_type_ == client_->GetTextInputType()) {
    if (empty_tsf_support_) {
      // Switch focus to empty doc. This optimizes the reuse, since here TSF
      // changes the text store's edit context state. So switching
      // the focus back to the edit context helps to reuse the TSF document.
      // It also reduces focus noise to TSF's default document which is only
      // there for app compat with embedded controls.
      TSFDocumentMap::iterator it =
          tsf_document_map_.find(TEXT_INPUT_TYPE_NONE);
      if (it != tsf_document_map_.end()) {
        thread_manager_->SetFocus(it->second.document_manager.Get());
      } else {
        // If we don't have an empty document, set focus to TSF default
        // document.
        thread_manager_->SetFocus(nullptr);
      }
    } else {
      thread_manager_->SetFocus(nullptr);
    }
  }
  input_type_ = client_->GetTextInputType();
  TSFDocument* document = GetAssociatedDocument();
  if (!document)
    return;
  // We call AssociateFocus for text input type none that also
  // triggers SetFocus internally. We don't want to send multiple
  // focus notifications for the same text input type so we don't
  // call AssociateFocus and SetFocus together. Just calling SetFocus
  // should be sufficient for setting focus on a textstore.
  if (input_type_ != TEXT_INPUT_TYPE_NONE)
    thread_manager_->SetFocus(document->document_manager.Get());
  else
    UpdateAssociateFocus();
  OnTextLayoutChanged();
}

void TSFBridgeImpl::OnTextLayoutChanged() {
  TSFDocument* document = GetAssociatedDocument();
  if (!document)
    return;
  if (!document->text_store)
    return;
  document->text_store->SendOnLayoutChange();
}

bool TSFBridgeImpl::CancelComposition() {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(IsInitialized());

  TSFDocument* document = GetAssociatedDocument();
  if (!document)
    return false;
  if (!document->text_store)
    return false;

  return document->text_store->CancelComposition();
}

bool TSFBridgeImpl::ConfirmComposition() {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(IsInitialized());

  TSFDocument* document = GetAssociatedDocument();
  if (!document)
    return false;
  if (!document->text_store)
    return false;

  return document->text_store->ConfirmComposition();
}

void TSFBridgeImpl::SetFocusedClient(HWND focused_window,
                                     TextInputClient* client) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(client);
  DCHECK(IsInitialized());
  if (attached_window_handle_ != focused_window)
    ClearAssociateFocus();
  client_ = client;
  attached_window_handle_ = focused_window;

  for (TSFDocumentMap::iterator it = tsf_document_map_.begin();
       it != tsf_document_map_.end(); ++it) {
    if (it->second.text_store.get() == nullptr)
      continue;
    it->second.text_store->SetFocusedTextInputClient(focused_window, client);
  }

  // Synchronize text input type state.
  OnTextInputTypeChanged(client);
}

void TSFBridgeImpl::RemoveFocusedClient(TextInputClient* client) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(IsInitialized());
  if (client_ != client)
    return;
  ClearAssociateFocus();
  client_ = nullptr;
  attached_window_handle_ = nullptr;
  for (TSFDocumentMap::iterator it = tsf_document_map_.begin();
       it != tsf_document_map_.end(); ++it) {
    if (it->second.text_store.get() == nullptr)
      continue;
    it->second.text_store->SetFocusedTextInputClient(nullptr, nullptr);
  }
}

void TSFBridgeImpl::SetImeKeyEventDispatcher(
    ImeKeyEventDispatcher* ime_key_event_dispatcher) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(ime_key_event_dispatcher);
  DCHECK(IsInitialized());

  for (TSFDocumentMap::iterator it = tsf_document_map_.begin();
       it != tsf_document_map_.end(); ++it) {
    if (it->second.text_store.get() == nullptr)
      continue;
    it->second.text_store->SetImeKeyEventDispatcher(ime_key_event_dispatcher);
  }
}

void TSFBridgeImpl::RemoveImeKeyEventDispatcher(
    ImeKeyEventDispatcher* ime_key_event_dispatcher) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(IsInitialized());

  for (TSFDocumentMap::iterator it = tsf_document_map_.begin();
       it != tsf_document_map_.end(); ++it) {
    if (it->second.text_store.get() == nullptr)
      continue;
    it->second.text_store->RemoveImeKeyEventDispatcher(
        ime_key_event_dispatcher);
  }
}

bool TSFBridgeImpl::IsInputLanguageCJK() {
  // See the following article about how LANGID in HKL is determined.
  // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getkeyboardlayout
  LANGID lang_locale =
      PRIMARYLANGID(LOWORD(HandleToLong(GetKeyboardLayout(0))));
  return lang_locale == LANG_CHINESE || lang_locale == LANG_JAPANESE ||
         lang_locale == LANG_KOREAN;
}

TextInputClient* TSFBridgeImpl::GetFocusedTextInputClient() const {
  return client_;
}

void TSFBridgeImpl::OnUrlChanged() {
  TSFDocument* document = GetAssociatedDocument();
  if (!document || !document->text_store) {
    return;
  }
  document->text_store->MaybeSendOnUrlChanged();
}

Microsoft::WRL::ComPtr<ITfThreadMgr> TSFBridgeImpl::GetThreadManager() {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(IsInitialized());
  return thread_manager_;
}

HRESULT TSFBridgeImpl::CreateDocumentManager(TSFTextStore* text_store,
                                             ITfDocumentMgr** document_manager,
                                             ITfContext** context,
                                             DWORD* source_cookie,
                                             DWORD* key_trace_sink_cookie,
                                             DWORD* language_profile_cookie) {
  HRESULT hr = thread_manager_->CreateDocumentMgr(document_manager);
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to create Document Manager.";
    return hr;
  }

  if (!text_store || !source_cookie || !key_trace_sink_cookie ||
      !language_profile_cookie) {
    return S_OK;
  }

  DWORD edit_cookie = TF_INVALID_EDIT_COOKIE;
  hr = (*document_manager)
           ->CreateContext(client_id_, 0,
                           static_cast<ITextStoreACP*>(text_store), context,
                           &edit_cookie);
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to create Context.";
    return hr;
  }

  hr = (*document_manager)->Push(*context);
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to push context.";
    return hr;
  }

  Microsoft::WRL::ComPtr<ITfSource> source;
  hr = (*context)->QueryInterface(IID_PPV_ARGS(&source));
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to get source.";
    return hr;
  }

  hr = source->AdviseSink(IID_ITfTextEditSink,
                          static_cast<ITfTextEditSink*>(text_store),
                          source_cookie);
  if (FAILED(hr)) {
    DVLOG(1) << "AdviseSink failed.";
    return hr;
  }

  Microsoft::WRL::ComPtr<ITfSource> source_ITfThreadMgr;
  hr = thread_manager_->QueryInterface(IID_PPV_ARGS(&source_ITfThreadMgr));
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to get source_ITfThreadMgr.";
    return hr;
  }

  hr = source_ITfThreadMgr->AdviseSink(
      IID_ITfKeyTraceEventSink, static_cast<ITfKeyTraceEventSink*>(text_store),
      key_trace_sink_cookie);
  if (FAILED(hr)) {
    DVLOG(1) << "AdviseSink for ITfKeyTraceEventSink failed.";
    return hr;
  }

  Microsoft::WRL::ComPtr<ITfSource> language_source;
  hr =
      input_processor_profiles_->QueryInterface(IID_PPV_ARGS(&language_source));
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to get source_ITfInputProcessorProfiles.";
    return hr;
  }

  hr = language_source->AdviseSink(IID_ITfLanguageProfileNotifySink,
                                   static_cast<ITfTextEditSink*>(text_store),
                                   language_profile_cookie);
  if (FAILED(hr)) {
    DVLOG(1) << "AdviseSink for language profile notify sink failed.";
    return hr;
  }

  if (*source_cookie == TF_INVALID_COOKIE) {
    DVLOG(1) << "The result of cookie is invalid.";
    return E_FAIL;
  }
  return S_OK;
}

HRESULT TSFBridgeImpl::InitializeDocumentMapInternal() {
  const TextInputType kTextInputTypes[] = {
      TEXT_INPUT_TYPE_NONE,      TEXT_INPUT_TYPE_TEXT,
      TEXT_INPUT_TYPE_PASSWORD,  TEXT_INPUT_TYPE_SEARCH,
      TEXT_INPUT_TYPE_EMAIL,     TEXT_INPUT_TYPE_NUMBER,
      TEXT_INPUT_TYPE_TELEPHONE, TEXT_INPUT_TYPE_URL,
      TEXT_INPUT_TYPE_TEXT_AREA,
  };
  // Query TSF for empty TSF text store support, introduced with Windows 11.
  // If support is present, as indicated by successful return of an interface
  // for the IID value GUID_COMPARTMENT_EMPTYCONTEXT, we use a dummy/empty Text
  // store when there is no text.
  Microsoft::WRL::ComPtr<IUnknown> flag_empty_context;
  HRESULT res = thread_manager_->QueryInterface(GUID_COMPARTMENT_EMPTYCONTEXT,
                                                &flag_empty_context);
  if (SUCCEEDED(res)) {
    empty_tsf_support_ = true;
  }

  for (size_t i = 0; i < std::size(kTextInputTypes); ++i) {
    const TextInputType input_type = kTextInputTypes[i];
    Microsoft::WRL::ComPtr<ITfContext> context;
    Microsoft::WRL::ComPtr<ITfDocumentMgr> document_manager;
    DWORD source_cookie = TF_INVALID_COOKIE;
    DWORD key_trace_sink_cookie = TF_INVALID_COOKIE;
    DWORD language_profile_cookie = TF_INVALID_COOKIE;
    // Use a null text store if empty tsf text store is not supported.
    const bool use_null_text_store =
        (input_type == TEXT_INPUT_TYPE_NONE && !empty_tsf_support_);
    DWORD* source_cookie_ptr = use_null_text_store ? nullptr : &source_cookie;
    DWORD* key_trace_sink_cookie_ptr =
        use_null_text_store ? nullptr : &key_trace_sink_cookie;
    DWORD* language_profile_cookie_ptr =
        use_null_text_store ? nullptr : &language_profile_cookie;
    scoped_refptr<TSFTextStore> text_store =
        use_null_text_store ? nullptr : new TSFTextStore();
    if (text_store && input_type != TEXT_INPUT_TYPE_NONE) {
      // No need to initialize for TEXT_INPUT_TYPE_NONE.
      HRESULT hr = text_store->Initialize();
      if (FAILED(hr))
        return hr;
    }
    HRESULT hr = CreateDocumentManager(
        text_store.get(), &document_manager, &context, source_cookie_ptr,
        key_trace_sink_cookie_ptr, language_profile_cookie_ptr);
    if (FAILED(hr))
      return hr;
    if (input_type == TEXT_INPUT_TYPE_PASSWORD ||
        (empty_tsf_support_ && input_type == TEXT_INPUT_TYPE_NONE)) {
      // Disable context for TEXT_INPUT_TYPE_NONE, if empty text store is
      // supported.
      hr = InitializeDisabledContext(context.Get());
      if (FAILED(hr))
        return hr;
    }
    tsf_document_map_[input_type].text_store = text_store;
    tsf_document_map_[input_type].document_manager = document_manager;
    tsf_document_map_[input_type].source_cookie = source_cookie;
    tsf_document_map_[input_type].key_trace_sink_cookie = key_trace_sink_cookie;
    tsf_document_map_[input_type].language_profile_cookie =
        language_profile_cookie;
    if (text_store)
      text_store->OnContextInitialized(context.Get());

    // Set the flag for empty text store.
    if (text_store && input_type == TEXT_INPUT_TYPE_NONE) {
      text_store->UseEmptyTextStore(empty_tsf_support_);
    }
  }
  return S_OK;
}

HRESULT TSFBridgeImpl::InitializeDisabledContext(ITfContext* context) {
  Microsoft::WRL::ComPtr<ITfCompartmentMgr> compartment_mgr;
  HRESULT hr = context->QueryInterface(IID_PPV_ARGS(&compartment_mgr));
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to get CompartmentMgr.";
    return hr;
  }

  Microsoft::WRL::ComPtr<ITfCompartment> disabled_compartment;
  hr = compartment_mgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED,
                                       &disabled_compartment);
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to get keyboard disabled compartment.";
    return hr;
  }

  base::win::ScopedVariant variant;
  variant.Set(1);
  hr = disabled_compartment->SetValue(client_id_, variant.ptr());
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to disable the DocumentMgr.";
    return hr;
  }

  Microsoft::WRL::ComPtr<ITfCompartment> empty_context;
  hr = compartment_mgr->GetCompartment(GUID_COMPARTMENT_EMPTYCONTEXT,
                                       &empty_context);
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to get empty context compartment.";
    return hr;
  }
  base::win::ScopedVariant empty_context_variant;
  empty_context_variant.Set(static_cast<int32_t>(1));
  hr = empty_context->SetValue(client_id_, empty_context_variant.ptr());
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to set empty context.";
    return hr;
  }

  return S_OK;
}

bool TSFBridgeImpl::IsFocused(ITfDocumentMgr* document_manager) {
  if (!IsInitialized()) {
    // Hasn't been initialized yet. Return false.
    return false;
  }
  Microsoft::WRL::ComPtr<ITfDocumentMgr> focused_document_manager;
  if (FAILED(thread_manager_->GetFocus(&focused_document_manager)))
    return false;
  return focused_document_manager.Get() == document_manager;
}

bool TSFBridgeImpl::IsInitialized() {
  return client_id_ != TF_CLIENTID_NULL;
}

void TSFBridgeImpl::UpdateAssociateFocus() {
  if (!IsInitialized()) {
    // Hasn't been initialized yet. Do nothing.
    return;
  }
  if (attached_window_handle_ == nullptr)
    return;
  TSFDocument* document = GetAssociatedDocument();
  if (document == nullptr) {
    ClearAssociateFocus();
    return;
  }
  // NOTE: ITfThreadMgr::AssociateFocus does not increment the ref count of
  // the document manager to be attached. It is our responsibility to make sure
  // the attached document manager will not be destroyed while it is attached.
  // This should be true as long as TSFBridge::Shutdown() is called late phase
  // of UI thread shutdown.
  // AssociateFocus calls SetFocus on the document manager internally
  Microsoft::WRL::ComPtr<ITfDocumentMgr> previous_focus;
  thread_manager_->AssociateFocus(attached_window_handle_,
                                  document->document_manager.Get(),
                                  &previous_focus);
}

void TSFBridgeImpl::ClearAssociateFocus() {
  if (!IsInitialized()) {
    // Hasn't been initialized yet. Do nothing.
    return;
  }
  if (attached_window_handle_ == nullptr)
    return;
  Microsoft::WRL::ComPtr<ITfDocumentMgr> previous_focus;
  thread_manager_->AssociateFocus(attached_window_handle_, nullptr,
                                  &previous_focus);
}

TSFBridgeImpl::TSFDocument* TSFBridgeImpl::GetAssociatedDocument() {
  if (!client_)
    return nullptr;
  TSFDocumentMap::iterator it = tsf_document_map_.find(input_type_);
  if (it == tsf_document_map_.end()) {
    it = tsf_document_map_.find(TEXT_INPUT_TYPE_TEXT);
    // This check is necessary because it's possible that we failed to
    // initialize |tsf_document_map_| and it has no TEXT_INPUT_TYPE_TEXT.
    if (it == tsf_document_map_.end())
      return nullptr;
  }
  return &it->second;
}

base::ThreadLocalOwnedPointer<TSFBridge>& GetThreadLocalTSFBridge() {
  static base::NoDestructor<base::ThreadLocalOwnedPointer<TSFBridge>>
      tsf_bridge;
  return *tsf_bridge;
}

}  // namespace

// TsfBridge  -----------------------------------------------------------------

// static
HRESULT TSFBridge::Initialize() {
  TRACE_EVENT0("ime", "TSFBridge::Initialize");
  if (!base::CurrentUIThread::IsSet()) {
    return E_FAIL;
  }

  if (GetThreadLocalTSFBridge().Get()) {
    return S_OK;
  }

  auto delegate = std::make_unique<TSFBridgeImpl>();
  HRESULT hr = delegate->Initialize();
  if (SUCCEEDED(hr)) {
    ReplaceThreadLocalTSFBridge(std::move(delegate));
  }
  return hr;
}

// static
void TSFBridge::InitializeForTesting() {
  if (!base::CurrentUIThread::IsSet()) {
    return;
  }
  ReplaceThreadLocalTSFBridge(std::make_unique<MockTSFBridge>());
}

// static
void TSFBridge::ReplaceThreadLocalTSFBridge(
    std::unique_ptr<TSFBridge> new_instance) {
  if (!base::CurrentUIThread::IsSet()) {
    return;
  }
  GetThreadLocalTSFBridge().Set(std::move(new_instance));
}

// static
void TSFBridge::Shutdown() {
  TRACE_EVENT0("ime", "TSFBridge::Shutdown");
  ReplaceThreadLocalTSFBridge(nullptr);
}

// static
TSFBridge* TSFBridge::GetInstance() {
  return base::CurrentUIThread::IsSet() ? GetThreadLocalTSFBridge().Get()
                                        : nullptr;
}

}  // namespace ui
