// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included

// Note that this class separates message receipt (OnMessage) from message
// dispatch (DispatchMessage). By default, OnMessage() simply forwards to
// DispatchMessage(), but the split allows subclasses to override this behavior,
// for example to defer dispatch in some cases.
//
// In any case, all raw transport messages are fully validated and deserialized
// before hitting OnMessage().
#define IPCZ_MSG_BEGIN_INTERFACE(name)                             \
  class name##MessageListener : public DriverTransport::Listener { \
   public:                                                         \
    virtual ~name##MessageListener() = default;                    \
    virtual bool OnMessage(Message& message);                      \
                                                                   \
   protected:                                                      \
    virtual bool DispatchMessage(Message& message);

#define IPCZ_MSG_END_INTERFACE()                                      \
 private:                                                             \
  bool OnTransportMessage(const DriverTransport::RawMessage& message, \
                          const DriverTransport& transport) final;    \
  void OnTransportError() override {}                                 \
  }                                                                   \
  ;

#define IPCZ_MSG_ID(x)
#define IPCZ_MSG_VERSION(x)

#define IPCZ_MSG_BEGIN(name, id_decl, version_decl) \
  virtual bool On##name(name&) { return false; }

#define IPCZ_MSG_END()

#define IPCZ_MSG_PARAM(type, name)
#define IPCZ_MSG_PARAM_ARRAY(type, name)
#define IPCZ_MSG_PARAM_DRIVER_OBJECT(name)
#define IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY(name)
