// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <msctf.h>

#include <map>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_current.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/threading/thread_local_storage.h"
#include "base/win/scoped_variant.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/tsf_bridge.h"
#include "ui/base/ime/win/tsf_text_store.h"
#include "ui/base/ui_base_features.h"

namespace ui {

namespace {

// TSFBridgeImpl -----------------------------------------------------------

// A TLS implementation of TSFBridge.
class TSFBridgeImpl : public TSFBridge {
 public:
  TSFBridgeImpl();
  ~TSFBridgeImpl() override;

  bool Initialize();

  // TsfBridge:
  void OnTextInputTypeChanged(const TextInputClient* client) override;
  void OnTextLayoutChanged() override;
  bool CancelComposition() override;
  bool ConfirmComposition() override;
  void SetFocusedClient(HWND focused_window, TextInputClient* client) override;
  void RemoveFocusedClient(TextInputClient* client) override;
  void SetInputMethodDelegate(internal::InputMethodDelegate* delegate) override;
  void RemoveInputMethodDelegate() override;
  bool IsInputLanguageCJK() override;
  Microsoft::WRL::ComPtr<ITfThreadMgr> GetThreadManager() override;
  TextInputClient* GetFocusedTextInputClient() const override;
  void SetInputPanelPolicy(bool input_panel_policy_manual) override;

 private:
  // Returns true if |tsf_document_map_| is successfully initialized. This
  // method should be called from and only from Initialize().
  bool InitializeDocumentMapInternal();

  // Returns true if |context| is successfully updated to be a disabled
  // context, where an IME should be deactivated. This is suitable for some
  // special input context such as password fields.
  bool InitializeDisabledContext(ITfContext* context);

  // Returns true if a TSF document manager and a TSF context is successfully
  // created with associating with given |text_store|. The returned
  // |source_cookie| indicates the binding between |text_store| and |context|.
  // You can pass nullptr to |text_store| and |source_cookie| when text store is
  // not necessary.
  bool CreateDocumentManager(TSFTextStore* text_store,
                             ITfDocumentMgr** document_manager,
                             ITfContext** context,
                             DWORD* source_cookie);

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
    TSFDocument() : cookie(TF_INVALID_COOKIE) {}
    TSFDocument(const TSFDocument& src)
        : document_manager(src.document_manager), cookie(src.cookie) {}
    Microsoft::WRL::ComPtr<ITfDocumentMgr> document_manager;
    scoped_refptr<TSFTextStore> text_store;
    DWORD cookie;
  };

  // Returns a pointer to TSFDocument that is associated with the current
  // TextInputType of |client_|.
  TSFDocument* GetAssociatedDocument();

  // An ITfThreadMgr object to be used in focus and document management.
  Microsoft::WRL::ComPtr<ITfThreadMgr> thread_manager_;

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
  TextInputClient* client_ = nullptr;

  // Represents the window that is currently owns text input focus.
  HWND attached_window_handle_ = nullptr;

  // Handle to ITfKeyTraceEventSink.
  DWORD key_trace_sink_cookie_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TSFBridgeImpl);
};

TSFBridgeImpl::TSFBridgeImpl() = default;

TSFBridgeImpl::~TSFBridgeImpl() {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  if (!IsInitialized())
    return;

  if (thread_manager_ != nullptr) {
    Microsoft::WRL::ComPtr<ITfSource> source;
    if (SUCCEEDED(thread_manager_->QueryInterface(IID_PPV_ARGS(&source)))) {
      source->UnadviseSink(key_trace_sink_cookie_);
    }
  }

  for (TSFDocumentMap::iterator it = tsf_document_map_.begin();
       it != tsf_document_map_.end(); ++it) {
    Microsoft::WRL::ComPtr<ITfContext> context;
    Microsoft::WRL::ComPtr<ITfSource> source;
    if (it->second.cookie != TF_INVALID_COOKIE &&
        SUCCEEDED(
            it->second.document_manager->GetBase(context.GetAddressOf())) &&
        SUCCEEDED(context.CopyTo(source.GetAddressOf()))) {
      source->UnadviseSink(it->second.cookie);
    }
  }
  tsf_document_map_.clear();

  client_id_ = TF_CLIENTID_NULL;
}

bool TSFBridgeImpl::Initialize() {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  if (client_id_ != TF_CLIENTID_NULL) {
    DVLOG(1) << "Already initialized.";
    return false;
  }

  if (FAILED(::CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&thread_manager_)))) {
    DVLOG(1) << "Failed to create ThreadManager instance.";
    return false;
  }

  if (FAILED(thread_manager_->Activate(&client_id_))) {
    DVLOG(1) << "Failed to activate Thread Manager.";
    return false;
  }

  if (!InitializeDocumentMapInternal())
    return false;

  // Japanese IME expects the default value of this compartment is
  // TF_SENTENCEMODE_PHRASEPREDICT like IMM32 implementation. This value is
  // managed per thread, so that it is enough to set this value at once. This
  // value does not affect other language's IME behaviors.
  Microsoft::WRL::ComPtr<ITfCompartmentMgr> thread_compartment_manager;
  if (FAILED(
          thread_manager_.CopyTo(thread_compartment_manager.GetAddressOf()))) {
    DVLOG(1) << "Failed to get ITfCompartmentMgr.";
    return false;
  }

  Microsoft::WRL::ComPtr<ITfCompartment> sentence_compartment;
  if (FAILED(thread_compartment_manager->GetCompartment(
          GUID_COMPARTMENT_KEYBOARD_INPUTMODE_SENTENCE,
          sentence_compartment.GetAddressOf()))) {
    DVLOG(1) << "Failed to get sentence compartment.";
    return false;
  }

  base::win::ScopedVariant sentence_variant;
  sentence_variant.Set(TF_SENTENCEMODE_PHRASEPREDICT);
  if (FAILED(
          sentence_compartment->SetValue(client_id_, sentence_variant.ptr()))) {
    DVLOG(1) << "Failed to change the sentence mode.";
    return false;
  }

  return true;
}

void TSFBridgeImpl::OnTextInputTypeChanged(const TextInputClient* client) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  DCHECK(IsInitialized());

  if (client != client_) {
    // Called from not focusing client. Do nothing.
    return;
  }

  TSFDocument* document = GetAssociatedDocument();
  if (!document)
    return;
  // We call AssociateFocus for text input type none that also
  // triggers SetFocus internally. We don't want to send multiple
  // focus notifications for the same text input type so we don't
  // call AssociateFocus and SetFocus together. Just calling SetFocus
  // should be sufficient for setting focus on a textstore.
  if (client_->GetTextInputType() != TEXT_INPUT_TYPE_NONE)
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

void TSFBridgeImpl::SetInputPanelPolicy(bool input_panel_policy_manual) {
  TSFDocument* document = GetAssociatedDocument();
  if (!document)
    return;
  if (!document->text_store)
    return;
  document->text_store->SetInputPanelPolicy(input_panel_policy_manual);
}

bool TSFBridgeImpl::CancelComposition() {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  DCHECK(IsInitialized());

  TSFDocument* document = GetAssociatedDocument();
  if (!document)
    return false;
  if (!document->text_store)
    return false;

  return document->text_store->CancelComposition();
}

bool TSFBridgeImpl::ConfirmComposition() {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
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
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
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
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
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

void TSFBridgeImpl::SetInputMethodDelegate(
    internal::InputMethodDelegate* delegate) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  DCHECK(delegate);
  DCHECK(IsInitialized());

  for (TSFDocumentMap::iterator it = tsf_document_map_.begin();
       it != tsf_document_map_.end(); ++it) {
    if (it->second.text_store.get() == nullptr)
      continue;
    it->second.text_store->SetInputMethodDelegate(delegate);
  }
}

void TSFBridgeImpl::RemoveInputMethodDelegate() {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  DCHECK(IsInitialized());

  for (TSFDocumentMap::iterator it = tsf_document_map_.begin();
       it != tsf_document_map_.end(); ++it) {
    if (it->second.text_store.get() == nullptr)
      continue;
    it->second.text_store->RemoveInputMethodDelegate();
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

Microsoft::WRL::ComPtr<ITfThreadMgr> TSFBridgeImpl::GetThreadManager() {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  DCHECK(IsInitialized());
  return thread_manager_;
}

bool TSFBridgeImpl::CreateDocumentManager(TSFTextStore* text_store,
                                          ITfDocumentMgr** document_manager,
                                          ITfContext** context,
                                          DWORD* source_cookie) {
  if (FAILED(thread_manager_->CreateDocumentMgr(document_manager))) {
    DVLOG(1) << "Failed to create Document Manager.";
    return false;
  }

  if (!text_store || !source_cookie)
    return true;

  DWORD edit_cookie = TF_INVALID_EDIT_COOKIE;
  if (FAILED((*document_manager)
                 ->CreateContext(client_id_, 0,
                                 static_cast<ITextStoreACP*>(text_store),
                                 context, &edit_cookie))) {
    DVLOG(1) << "Failed to create Context.";
    return false;
  }

  if (FAILED((*document_manager)->Push(*context))) {
    DVLOG(1) << "Failed to push context.";
    return false;
  }

  Microsoft::WRL::ComPtr<ITfSource> source;
  if (FAILED((*context)->QueryInterface(IID_PPV_ARGS(&source)))) {
    DVLOG(1) << "Failed to get source.";
    return false;
  }

  if (FAILED(source->AdviseSink(IID_ITfTextEditSink,
                                static_cast<ITfTextEditSink*>(text_store),
                                source_cookie))) {
    DVLOG(1) << "AdviseSink failed.";
    return false;
  }

  Microsoft::WRL::ComPtr<ITfSource> source_ITfThreadMgr;
  if (FAILED(thread_manager_->QueryInterface(
          IID_PPV_ARGS(&source_ITfThreadMgr)))) {
    DVLOG(1) << "Failed to get source_ITfThreadMgr.";
    return false;
  }

  if (FAILED(source_ITfThreadMgr->AdviseSink(
          IID_ITfKeyTraceEventSink,
          static_cast<ITfKeyTraceEventSink*>(text_store),
          &key_trace_sink_cookie_))) {
    DVLOG(1) << "AdviseSink for ITfKeyTraceEventSink failed.";
    return false;
  }

  if (*source_cookie == TF_INVALID_COOKIE) {
    DVLOG(1) << "The result of cookie is invalid.";
    return false;
  }
  return true;
}

bool TSFBridgeImpl::InitializeDocumentMapInternal() {
  const TextInputType kTextInputTypes[] = {
      TEXT_INPUT_TYPE_NONE,      TEXT_INPUT_TYPE_TEXT,
      TEXT_INPUT_TYPE_PASSWORD,  TEXT_INPUT_TYPE_SEARCH,
      TEXT_INPUT_TYPE_EMAIL,     TEXT_INPUT_TYPE_NUMBER,
      TEXT_INPUT_TYPE_TELEPHONE, TEXT_INPUT_TYPE_URL,
  };
  for (size_t i = 0; i < base::size(kTextInputTypes); ++i) {
    const TextInputType input_type = kTextInputTypes[i];
    Microsoft::WRL::ComPtr<ITfContext> context;
    Microsoft::WRL::ComPtr<ITfDocumentMgr> document_manager;
    DWORD cookie = TF_INVALID_COOKIE;
    const bool use_null_text_store = (input_type == TEXT_INPUT_TYPE_NONE);
    DWORD* cookie_ptr = use_null_text_store ? nullptr : &cookie;
    scoped_refptr<TSFTextStore> text_store =
        use_null_text_store ? nullptr : new TSFTextStore();
    if (!CreateDocumentManager(text_store.get(),
                               document_manager.GetAddressOf(),
                               context.GetAddressOf(), cookie_ptr))
      return false;
    if ((input_type == TEXT_INPUT_TYPE_PASSWORD) &&
        !InitializeDisabledContext(context.Get()))
      return false;
    tsf_document_map_[input_type].text_store = text_store;
    tsf_document_map_[input_type].document_manager = document_manager;
    tsf_document_map_[input_type].cookie = cookie;
    if (text_store)
      text_store->OnContextInitialized(context.Get());
  }
  return true;
}

bool TSFBridgeImpl::InitializeDisabledContext(ITfContext* context) {
  Microsoft::WRL::ComPtr<ITfCompartmentMgr> compartment_mgr;
  if (FAILED(context->QueryInterface(IID_PPV_ARGS(&compartment_mgr)))) {
    DVLOG(1) << "Failed to get CompartmentMgr.";
    return false;
  }

  Microsoft::WRL::ComPtr<ITfCompartment> disabled_compartment;
  if (FAILED(compartment_mgr->GetCompartment(
          GUID_COMPARTMENT_KEYBOARD_DISABLED,
          disabled_compartment.GetAddressOf()))) {
    DVLOG(1) << "Failed to get keyboard disabled compartment.";
    return false;
  }

  base::win::ScopedVariant variant;
  variant.Set(1);
  if (FAILED(disabled_compartment->SetValue(client_id_, variant.ptr()))) {
    DVLOG(1) << "Failed to disable the DocumentMgr.";
    return false;
  }

  Microsoft::WRL::ComPtr<ITfCompartment> empty_context;
  if (FAILED(compartment_mgr->GetCompartment(GUID_COMPARTMENT_EMPTYCONTEXT,
                                             empty_context.GetAddressOf()))) {
    DVLOG(1) << "Failed to get empty context compartment.";
    return false;
  }
  base::win::ScopedVariant empty_context_variant;
  empty_context_variant.Set(static_cast<int32_t>(1));
  if (FAILED(
          empty_context->SetValue(client_id_, empty_context_variant.ptr()))) {
    DVLOG(1) << "Failed to set empty context.";
    return false;
  }

  return true;
}

bool TSFBridgeImpl::IsFocused(ITfDocumentMgr* document_manager) {
  if (!IsInitialized()) {
    // Hasn't been initialized yet. Return false.
    return false;
  }
  Microsoft::WRL::ComPtr<ITfDocumentMgr> focused_document_manager;
  if (FAILED(
          thread_manager_->GetFocus(focused_document_manager.GetAddressOf())))
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
                                  previous_focus.GetAddressOf());
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
                                  previous_focus.GetAddressOf());
}

TSFBridgeImpl::TSFDocument* TSFBridgeImpl::GetAssociatedDocument() {
  if (!client_)
    return nullptr;
  TSFDocumentMap::iterator it =
      tsf_document_map_.find(client_->GetTextInputType());
  if (it == tsf_document_map_.end()) {
    it = tsf_document_map_.find(TEXT_INPUT_TYPE_TEXT);
    // This check is necessary because it's possible that we failed to
    // initialize |tsf_document_map_| and it has no TEXT_INPUT_TYPE_TEXT.
    if (it == tsf_document_map_.end())
      return nullptr;
  }
  return &it->second;
}

void Finalize(void* data) {
  TSFBridgeImpl* delegate = static_cast<TSFBridgeImpl*>(data);
  delete delegate;
}

base::ThreadLocalStorage::Slot& TSFBridgeTLS() {
  static base::NoDestructor<base::ThreadLocalStorage::Slot> tsf_bridge_tls(
      &Finalize);
  return *tsf_bridge_tls;
}

}  // namespace

// TsfBridge  -----------------------------------------------------------------

TSFBridge::TSFBridge() {}

TSFBridge::~TSFBridge() {}

// static
void TSFBridge::Initialize() {
  if (!base::MessageLoopCurrentForUI::IsSet()) {
    DVLOG(1) << "Do not use TSFBridge without UI thread.";
    return;
  }
  TSFBridgeImpl* delegate = static_cast<TSFBridgeImpl*>(TSFBridgeTLS().Get());
  if (delegate)
    return;
  // If we aren't supporting TSF early out.
  if (!base::FeatureList::IsEnabled(features::kTSFImeSupport))
    return;
  delegate = new TSFBridgeImpl();
  TSFBridgeTLS().Set(delegate);
  delegate->Initialize();
}

// static
TSFBridge* TSFBridge::ReplaceForTesting(TSFBridge* bridge) {
  if (!base::MessageLoopCurrentForUI::IsSet()) {
    DVLOG(1) << "Do not use TSFBridge without UI thread.";
    return nullptr;
  }
  TSFBridge* old_bridge = TSFBridge::GetInstance();
  TSFBridgeTLS().Set(bridge);
  return old_bridge;
}

// static
void TSFBridge::Shutdown() {
  if (!base::MessageLoopCurrentForUI::IsSet()) {
    DVLOG(1) << "Do not use TSFBridge without UI thread.";
  }
  TSFBridgeImpl* delegate = static_cast<TSFBridgeImpl*>(TSFBridgeTLS().Get());
  TSFBridgeTLS().Set(nullptr);
  delete delegate;
}

// static
TSFBridge* TSFBridge::GetInstance() {
  if (!base::MessageLoopCurrentForUI::IsSet()) {
    DVLOG(1) << "Do not use TSFBridge without UI thread.";
    return nullptr;
  }
  TSFBridgeImpl* delegate = static_cast<TSFBridgeImpl*>(TSFBridgeTLS().Get());
  DCHECK(delegate) << "Do no call GetInstance before TSFBridge::Initialize.";
  return delegate;
}

}  // namespace ui
