/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/libjingle_xmpp/xmpp/xmppclient.h"

#include "base/logging.h"
#include "net/base/host_port_pair.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"
#include "third_party/libjingle_xmpp/xmpp/plainsaslhandler.h"
#include "third_party/libjingle_xmpp/xmpp/prexmppauth.h"
#include "third_party/libjingle_xmpp/xmpp/saslplainmechanism.h"
#include "third_party/webrtc/rtc_base/third_party/sigslot/sigslot.h"
#include "xmpptask.h"

namespace jingle_xmpp {

class XmppClient::Private :
    public sigslot::has_slots<>,
    public XmppSessionHandler,
    public XmppOutputHandler {
public:

  explicit Private(XmppClient* client) :
    client_(client),
    socket_(),
    engine_(),
    pre_engine_error_(XmppEngine::ERROR_NONE),
    pre_engine_subcode_(0),
    signal_closed_(false),
    allow_plain_(false) {}

  virtual ~Private() {
    // We need to disconnect from socket_ before engine_ is destructed (by
    // the auto-generated destructor code).
    ResetSocket();
  }

  // the owner
  XmppClient* const client_;

  // the two main objects
  std::unique_ptr<AsyncSocket> socket_;
  std::unique_ptr<XmppEngine> engine_;
  std::unique_ptr<PreXmppAuth> pre_auth_;
  std::string pass_;
  std::string auth_mechanism_;
  std::string auth_token_;
  net::HostPortPair server_;
  XmppEngine::Error pre_engine_error_;
  int pre_engine_subcode_;
  CaptchaChallenge captcha_challenge_;
  bool signal_closed_;
  bool allow_plain_;

  void ResetSocket() {
    if (socket_) {
      socket_->SignalConnected.disconnect(this);
      socket_->SignalRead.disconnect(this);
      socket_->SignalClosed.disconnect(this);
      socket_.reset(NULL);
    }
  }

  // implementations of interfaces
  void OnStateChange(int state);
  void WriteOutput(const char* bytes, size_t len);
  void StartTls(const std::string& domainname);
  void CloseConnection();

  // slots for socket signals
  void OnSocketConnected();
  void OnSocketRead();
  void OnSocketClosed();
};

XmppReturnStatus XmppClient::Connect(
    const XmppClientSettings& settings,
    const std::string& lang, AsyncSocket* socket, PreXmppAuth* pre_auth) {
  if (socket == NULL)
    return XMPP_RETURN_BADARGUMENT;
  if (d_->socket_)
    return XMPP_RETURN_BADSTATE;

  d_->socket_.reset(socket);

  d_->socket_->SignalConnected.connect(d_.get(), &Private::OnSocketConnected);
  d_->socket_->SignalRead.connect(d_.get(), &Private::OnSocketRead);
  d_->socket_->SignalClosed.connect(d_.get(), &Private::OnSocketClosed);

  d_->engine_.reset(XmppEngine::Create());
  d_->engine_->SetSessionHandler(d_.get());
  d_->engine_->SetOutputHandler(d_.get());
  if (!settings.resource().empty()) {
    d_->engine_->SetRequestedResource(settings.resource());
  }
  d_->engine_->SetTls(settings.use_tls());

  // The talk.google.com server returns a certificate with common-name:
  //   CN="gmail.com" for @gmail.com accounts,
  //   CN="googlemail.com" for @googlemail.com accounts,
  //   CN="talk.google.com" for other accounts (such as @example.com),
  // so we tweak the tls server setting for those other accounts to match the
  // returned certificate CN of "talk.google.com".
  // For other servers, we leave the strings empty, which causes the jid's
  // domain to be used.  We do the same for gmail.com and googlemail.com as the
  // returned CN matches the account domain in those cases.
  std::string server_name = settings.server().host();
  if (server_name == jingle_xmpp::STR_TALK_GOOGLE_COM ||
      server_name == jingle_xmpp::STR_TALKX_L_GOOGLE_COM ||
      server_name == jingle_xmpp::STR_XMPP_GOOGLE_COM ||
      server_name == jingle_xmpp::STR_XMPPX_L_GOOGLE_COM) {
    if (settings.host() != STR_GMAIL_COM &&
        settings.host() != STR_GOOGLEMAIL_COM) {
      d_->engine_->SetTlsServer("", STR_TALK_GOOGLE_COM);
    }
  }

  // Set language
  d_->engine_->SetLanguage(lang);

  d_->engine_->SetUser(jingle_xmpp::Jid(settings.user(), settings.host(), STR_EMPTY));

  d_->pass_ = settings.pass();
  d_->auth_mechanism_ = settings.auth_mechanism();
  d_->auth_token_ = settings.auth_token();
  d_->server_ = settings.server();
  d_->allow_plain_ = settings.allow_plain();
  d_->pre_auth_.reset(pre_auth);

  return XMPP_RETURN_OK;
}

XmppEngine::State XmppClient::GetState() const {
  if (!d_->engine_)
    return XmppEngine::STATE_NONE;
  return d_->engine_->GetState();
}

XmppEngine::Error XmppClient::GetError(int* subcode) {
  if (subcode) {
    *subcode = 0;
  }
  if (!d_->engine_)
    return XmppEngine::ERROR_NONE;
  if (d_->pre_engine_error_ != XmppEngine::ERROR_NONE) {
    if (subcode) {
      *subcode = d_->pre_engine_subcode_;
    }
    return d_->pre_engine_error_;
  }
  return d_->engine_->GetError(subcode);
}

const XmlElement* XmppClient::GetStreamError() {
  if (!d_->engine_) {
    return NULL;
  }
  return d_->engine_->GetStreamError();
}

CaptchaChallenge XmppClient::GetCaptchaChallenge() {
  if (!d_->engine_)
    return CaptchaChallenge();
  return d_->captcha_challenge_;
}

std::string XmppClient::GetAuthMechanism() {
  if (!d_->engine_)
    return "";
  return d_->auth_mechanism_;
}

std::string XmppClient::GetAuthToken() {
  if (!d_->engine_)
    return "";
  return d_->auth_token_;
}

int XmppClient::ProcessStart() {
  // Should not happen, but was observed in crash reports
  if (!d_->socket_) {
    DVLOG(1) << "socket_ already reset";
    return STATE_DONE;
  }

  if (d_->pre_auth_) {
    d_->pre_auth_->SignalAuthDone.connect(this, &XmppClient::OnAuthDone);
    d_->pre_auth_->StartPreXmppAuth(
        d_->engine_->GetUser(), d_->pass_,
        d_->auth_mechanism_, d_->auth_token_);
    d_->pass_.clear(); // done with this;
    return STATE_PRE_XMPP_LOGIN;
  }
  else {
    d_->engine_->SetSaslHandler(new PlainSaslHandler(
              d_->engine_->GetUser(), d_->pass_, d_->allow_plain_));
    d_->pass_.clear(); // done with this;
    return STATE_START_XMPP_LOGIN;
  }
}

void XmppClient::OnAuthDone() {
  Wake();
}

int XmppClient::ProcessTokenLogin() {
  // Should not happen, but was observed in crash reports
  if (!d_->socket_) {
    DVLOG(1) << "socket_ already reset";
    return STATE_DONE;
  }

  // Don't know how this could happen, but crash reports show it as NULL
  if (!d_->pre_auth_) {
    d_->pre_engine_error_ = XmppEngine::ERROR_AUTH;
    EnsureClosed();
    return STATE_ERROR;
  }

  // Wait until pre authentication is done is done
  if (!d_->pre_auth_->IsAuthDone())
    return STATE_BLOCKED;

  if (!d_->pre_auth_->IsAuthorized()) {
    // maybe split out a case when gaia is down?
    if (d_->pre_auth_->HadError()) {
      d_->pre_engine_error_ = XmppEngine::ERROR_AUTH;
      d_->pre_engine_subcode_ = d_->pre_auth_->GetError();
    }
    else {
      d_->pre_engine_error_ = XmppEngine::ERROR_UNAUTHORIZED;
      d_->pre_engine_subcode_ = 0;
      d_->captcha_challenge_ = d_->pre_auth_->GetCaptchaChallenge();
    }
    d_->pre_auth_.reset(NULL); // done with this
    EnsureClosed();
    return STATE_ERROR;
  }

  // Save auth token as a result

  d_->auth_mechanism_ = d_->pre_auth_->GetAuthMechanism();
  d_->auth_token_ = d_->pre_auth_->GetAuthToken();

  // transfer ownership of pre_auth_ to engine
  d_->engine_->SetSaslHandler(d_->pre_auth_.release());
  return STATE_START_XMPP_LOGIN;
}

int XmppClient::ProcessStartXmppLogin() {
  // Should not happen, but was observed in crash reports
  if (!d_->socket_) {
    DVLOG(1) << "socket_ already reset";
    return STATE_DONE;
  }

  // Done with pre-connect tasks - connect!
  if (!d_->socket_->Connect(d_->server_)) {
    EnsureClosed();
    return STATE_ERROR;
  }

  return STATE_RESPONSE;
}

int XmppClient::ProcessResponse() {
  // Hang around while we are connected.
  if (!delivering_signal_ &&
      (!d_->engine_ || d_->engine_->GetState() == XmppEngine::STATE_CLOSED))
    return STATE_DONE;
  return STATE_BLOCKED;
}

XmppReturnStatus XmppClient::Disconnect() {
  if (!d_->socket_)
    return XMPP_RETURN_BADSTATE;
  Abort();
  d_->engine_->Disconnect();
  d_->ResetSocket();
  return XMPP_RETURN_OK;
}

XmppClient::XmppClient(TaskParent* parent)
    : XmppTaskParentInterface(parent),
      delivering_signal_(false),
      valid_(false) {
  d_.reset(new Private(this));
  valid_ = true;
}

XmppClient::~XmppClient() {
  valid_ = false;
}

const Jid& XmppClient::jid() const {
  return d_->engine_->FullJid();
}


std::string XmppClient::NextId() {
  return d_->engine_->NextId();
}

XmppReturnStatus XmppClient::SendStanza(const XmlElement* stanza) {
  return d_->engine_->SendStanza(stanza);
}

XmppReturnStatus XmppClient::SendStanzaError(
    const XmlElement* old_stanza, XmppStanzaError xse,
    const std::string& message) {
  return d_->engine_->SendStanzaError(old_stanza, xse, message);
}

XmppReturnStatus XmppClient::SendRaw(const std::string& text) {
  return d_->engine_->SendRaw(text);
}

XmppEngine* XmppClient::engine() {
  return d_->engine_.get();
}

void XmppClient::Private::OnSocketConnected() {
  engine_->Connect();
}

void XmppClient::Private::OnSocketRead() {
  char bytes[4096];
  size_t bytes_read;
  for (;;) {
    // Should not happen, but was observed in crash reports
    if (!socket_) {
      DVLOG(1) << "socket_ already reset";
      return;
    }

    if (!socket_->Read(bytes, sizeof(bytes), &bytes_read)) {
      // TODO: deal with error information
      return;
    }

    if (bytes_read == 0)
      return;

//#if !defined(NDEBUG)
    client_->SignalLogInput(bytes, static_cast<int>(bytes_read));
//#endif

    engine_->HandleInput(bytes, bytes_read);
  }
}

void XmppClient::Private::OnSocketClosed() {
  int code = socket_->GetError();
  engine_->ConnectionClosed(code);
}

void XmppClient::Private::OnStateChange(int state) {
  if (state == XmppEngine::STATE_CLOSED) {
    client_->EnsureClosed();
  }
  else {
    client_->SignalStateChange((XmppEngine::State)state);
  }
  client_->Wake();
}

void XmppClient::Private::WriteOutput(const char* bytes, size_t len) {
//#if !defined(NDEBUG)
  client_->SignalLogOutput(bytes, static_cast<int>(len));
//#endif

  socket_->Write(bytes, len);
  // TODO: deal with error information
}

void XmppClient::Private::StartTls(const std::string& domain) {
  socket_->StartTls(domain);
}

void XmppClient::Private::CloseConnection() {
  socket_->Close();
}

void XmppClient::AddXmppTask(XmppTask* task, XmppEngine::HandlerLevel level) {
  d_->engine_->AddStanzaHandler(task, level);
}

void XmppClient::RemoveXmppTask(XmppTask* task) {
  d_->engine_->RemoveStanzaHandler(task);
}

void XmppClient::EnsureClosed() {
  if (!d_->signal_closed_) {
    d_->signal_closed_ = true;
    delivering_signal_ = true;
    SignalStateChange(XmppEngine::STATE_CLOSED);
    delivering_signal_ = false;
  }
}

}  // namespace jingle_xmpp
