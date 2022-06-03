/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/libjingle_xmpp/xmpp/xmppengineimpl.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "base/check.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmllite/xmlprinter.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"
#include "third_party/libjingle_xmpp/xmpp/saslhandler.h"
#include "third_party/libjingle_xmpp/xmpp/xmpplogintask.h"

namespace jingle_xmpp {

XmppEngine* XmppEngine::Create() {
  return new XmppEngineImpl();
}


XmppEngineImpl::XmppEngineImpl()
    : stanza_parse_handler_(this),
      stanza_parser_(&stanza_parse_handler_),
      engine_entered_(0),
      password_(),
      requested_resource_(STR_EMPTY),
      tls_option_(jingle_xmpp::TLS_REQUIRED),
      login_task_(new XmppLoginTask(this)),
      next_id_(0),
      state_(STATE_START),
      encrypted_(false),
      error_code_(ERROR_NONE),
      subcode_(0),
      stream_error_(),
      raised_reset_(false),
      output_handler_(NULL),
      session_handler_(NULL),
      iq_entries_(new IqEntryVector()),
      sasl_handler_(),
      output_(new std::stringstream()) {
  for (int i = 0; i < HL_COUNT; i+= 1) {
    stanza_handlers_[i].reset(new StanzaHandlerVector());
  }

  // Add XMPP namespaces to XML namespaces stack.
  xmlns_stack_.AddXmlns("stream", "http://etherx.jabber.org/streams");
  xmlns_stack_.AddXmlns("", "jabber:client");
}

XmppEngineImpl::~XmppEngineImpl() {
  DeleteIqCookies();
}

XmppReturnStatus XmppEngineImpl::SetOutputHandler(
    XmppOutputHandler* output_handler) {
  if (state_ != STATE_START)
    return XMPP_RETURN_BADSTATE;

  output_handler_ = output_handler;

  return XMPP_RETURN_OK;
}

XmppReturnStatus XmppEngineImpl::SetSessionHandler(
    XmppSessionHandler* session_handler) {
  if (state_ != STATE_START)
    return XMPP_RETURN_BADSTATE;

  session_handler_ = session_handler;

  return XMPP_RETURN_OK;
}

XmppReturnStatus XmppEngineImpl::HandleInput(
    const char* bytes, size_t len) {
  if (state_ < STATE_OPENING || state_ > STATE_OPEN)
    return XMPP_RETURN_BADSTATE;

  EnterExit ee(this);

  // TODO: The return value of the xml parser is not checked.
  stanza_parser_.Parse(bytes, len, false);

  return XMPP_RETURN_OK;
}

XmppReturnStatus XmppEngineImpl::ConnectionClosed(int subcode) {
  if (state_ != STATE_CLOSED) {
    EnterExit ee(this);
    // If told that connection closed and not already closed,
    // then connection was unpexectedly dropped.
    if (subcode) {
      SignalError(ERROR_SOCKET, subcode);
    } else {
      SignalError(ERROR_CONNECTION_CLOSED, 0);  // no subcode
    }
  }
  return XMPP_RETURN_OK;
}

XmppReturnStatus XmppEngineImpl::SetTls(TlsOptions use_tls) {
  if (state_ != STATE_START)
    return XMPP_RETURN_BADSTATE;
  tls_option_ = use_tls;
  return XMPP_RETURN_OK;
}

XmppReturnStatus XmppEngineImpl::SetTlsServer(
    const std::string& tls_server_hostname,
    const std::string& tls_server_domain) {
  if (state_ != STATE_START)
    return XMPP_RETURN_BADSTATE;

  tls_server_hostname_ = tls_server_hostname;
  tls_server_domain_= tls_server_domain;

  return XMPP_RETURN_OK;
}

TlsOptions XmppEngineImpl::GetTls() {
  return tls_option_;
}

XmppReturnStatus XmppEngineImpl::SetUser(const Jid& jid) {
  if (state_ != STATE_START)
    return XMPP_RETURN_BADSTATE;

  user_jid_ = jid;

  return XMPP_RETURN_OK;
}

const Jid& XmppEngineImpl::GetUser() {
  return user_jid_;
}

XmppReturnStatus XmppEngineImpl::SetSaslHandler(SaslHandler* sasl_handler) {
  if (state_ != STATE_START)
    return XMPP_RETURN_BADSTATE;

  sasl_handler_.reset(sasl_handler);
  return XMPP_RETURN_OK;
}

XmppReturnStatus XmppEngineImpl::SetRequestedResource(
    const std::string& resource) {
  if (state_ != STATE_START)
    return XMPP_RETURN_BADSTATE;

  requested_resource_ = resource;

  return XMPP_RETURN_OK;
}

const std::string& XmppEngineImpl::GetRequestedResource() {
  return requested_resource_;
}

XmppReturnStatus XmppEngineImpl::AddStanzaHandler(
    XmppStanzaHandler* stanza_handler,
    XmppEngine::HandlerLevel level) {
  if (state_ == STATE_CLOSED)
    return XMPP_RETURN_BADSTATE;

  stanza_handlers_[level]->push_back(stanza_handler);

  return XMPP_RETURN_OK;
}

XmppReturnStatus XmppEngineImpl::RemoveStanzaHandler(
    XmppStanzaHandler* stanza_handler) {
  bool found = false;

  for (int level = 0; level < HL_COUNT; level += 1) {
    StanzaHandlerVector::iterator new_end =
      std::remove(stanza_handlers_[level]->begin(),
      stanza_handlers_[level]->end(),
      stanza_handler);

    if (new_end != stanza_handlers_[level]->end()) {
      stanza_handlers_[level]->erase(new_end, stanza_handlers_[level]->end());
      found = true;
    }
  }

  if (!found)
    return XMPP_RETURN_BADARGUMENT;

  return XMPP_RETURN_OK;
}

XmppReturnStatus XmppEngineImpl::Connect() {
  if (state_ != STATE_START)
    return XMPP_RETURN_BADSTATE;

  EnterExit ee(this);

  // get the login task started
  state_ = STATE_OPENING;
  if (login_task_) {
    login_task_->IncomingStanza(NULL, false);
    if (login_task_->IsDone())
      login_task_.reset();
  }

  return XMPP_RETURN_OK;
}

XmppReturnStatus XmppEngineImpl::SendStanza(const XmlElement* element) {
  if (state_ == STATE_CLOSED)
    return XMPP_RETURN_BADSTATE;

  EnterExit ee(this);

  if (login_task_) {
    // still handshaking - then outbound stanzas are queued
    login_task_->OutgoingStanza(element);
  } else {
    // handshake done - send straight through
    InternalSendStanza(element);
  }

  return XMPP_RETURN_OK;
}

XmppReturnStatus XmppEngineImpl::SendRaw(const std::string& text) {
  if (state_ == STATE_CLOSED || login_task_)
    return XMPP_RETURN_BADSTATE;

  EnterExit ee(this);

  (*output_) << text;

  return XMPP_RETURN_OK;
}

std::string XmppEngineImpl::NextId() {
  std::stringstream ss;
  ss << next_id_++;
  return ss.str();
}

XmppReturnStatus XmppEngineImpl::Disconnect() {
  if (state_ != STATE_CLOSED) {
    EnterExit ee(this);
    if (state_ == STATE_OPEN)
      *output_ << "</stream:stream>";
    state_ = STATE_CLOSED;
  }

  return XMPP_RETURN_OK;
}

void XmppEngineImpl::IncomingStart(const XmlElement* start) {
  if (HasError() || raised_reset_)
    return;

  if (login_task_) {
    // start-stream should go to login task
    login_task_->IncomingStanza(start, true);
    if (login_task_->IsDone())
      login_task_.reset();
  }
  else {
    // if not logging in, it's an error to see a start
    SignalError(ERROR_XML, 0);
  }
}

void XmppEngineImpl::IncomingStanza(const XmlElement* stanza) {
  if (HasError() || raised_reset_)
    return;

  if (stanza->Name() == QN_STREAM_ERROR) {
    // Explicit XMPP stream error
    SignalStreamError(stanza);
  } else if (login_task_) {
    // Handle login handshake
    login_task_->IncomingStanza(stanza, false);
    if (login_task_->IsDone())
      login_task_.reset();
  } else if (HandleIqResponse(stanza)) {
    // iq is handled by above call
  } else {
    // give every "peek" handler a shot at all stanzas
    for (size_t i = 0; i < stanza_handlers_[HL_PEEK]->size(); i += 1) {
      (*stanza_handlers_[HL_PEEK])[i]->HandleStanza(stanza);
    }

    // give other handlers a shot in precedence order, stopping after handled
    for (int level = HL_SINGLE; level <= HL_ALL; level += 1) {
      for (size_t i = 0; i < stanza_handlers_[level]->size(); i += 1) {
        if ((*stanza_handlers_[level])[i]->HandleStanza(stanza))
          return;
      }
    }

    // If nobody wants to handle a stanza then send back an error.
    // Only do this for IQ stanzas as messages should probably just be dropped
    // and presence stanzas should certainly be dropped.
    std::string type = stanza->Attr(QN_TYPE);
    if (stanza->Name() == QN_IQ &&
        !(type == "error" || type == "result")) {
      SendStanzaError(stanza, XSE_FEATURE_NOT_IMPLEMENTED, STR_EMPTY);
    }
  }
}

void XmppEngineImpl::IncomingEnd(bool isError) {
  if (HasError() || raised_reset_)
    return;

  SignalError(isError ? ERROR_XML : ERROR_DOCUMENT_CLOSED, 0);
}

void XmppEngineImpl::InternalSendStart(const std::string& to) {
  std::string hostname = tls_server_hostname_;
  if (hostname.empty())
    hostname = to;

  // If not language is specified, the spec says use *
  std::string lang = lang_;
  if (lang.length() == 0)
    lang = "*";

  // send stream-beginning
  // note, we put a \r\n at tne end fo the first line to cause non-XMPP
  // line-oriented servers (e.g., Apache) to reveal themselves more quickly.
  *output_ << "<stream:stream to=\"" << hostname << "\" "
           << "xml:lang=\"" << lang << "\" "
           << "version=\"1.0\" "
           << "xmlns:stream=\"http://etherx.jabber.org/streams\" "
           << "xmlns=\"jabber:client\">\r\n";
}

void XmppEngineImpl::InternalSendStanza(const XmlElement* element) {
  // It should really never be necessary to set a FROM attribute on a stanza.
  // It is implied by the bind on the stream and if you get it wrong
  // (by flipping from/to on a message?) the server will close the stream.
  DCHECK(!element->HasAttr(QN_FROM));

  XmlPrinter::PrintXml(output_.get(), element, &xmlns_stack_);
}

std::string XmppEngineImpl::ChooseBestSaslMechanism(
    const std::vector<std::string>& mechanisms, bool encrypted) {
  return sasl_handler_->ChooseBestSaslMechanism(mechanisms, encrypted);
}

SaslMechanism* XmppEngineImpl::GetSaslMechanism(const std::string& name) {
  return sasl_handler_->CreateSaslMechanism(name);
}

void XmppEngineImpl::SignalBound(const Jid& fullJid) {
  if (state_ == STATE_OPENING) {
    bound_jid_ = fullJid;
    state_ = STATE_OPEN;
  }
}

void XmppEngineImpl::SignalStreamError(const XmlElement* stream_error) {
  if (state_ != STATE_CLOSED) {
    stream_error_.reset(new XmlElement(*stream_error));
    SignalError(ERROR_STREAM, 0);
  }
}

void XmppEngineImpl::SignalError(Error error_code, int sub_code) {
  if (state_ != STATE_CLOSED) {
    error_code_ = error_code;
    subcode_ = sub_code;
    state_ = STATE_CLOSED;
  }
}

bool XmppEngineImpl::HasError() {
  return error_code_ != ERROR_NONE;
}

void XmppEngineImpl::StartTls(const std::string& domain) {
  if (output_handler_) {
    // As substitute for the real (login jid's) domain, we permit
    // verifying a tls_server_domain_ instead, if one was passed.
    // This allows us to avoid running a proxy that needs to handle
    // valuable certificates.
    output_handler_->StartTls(
      tls_server_domain_.empty() ? domain : tls_server_domain_);
    encrypted_ = true;
  }
}

XmppEngineImpl::EnterExit::EnterExit(XmppEngineImpl* engine)
    : engine_(engine),
      state_(engine->state_) {
  engine->engine_entered_ += 1;
}

XmppEngineImpl::EnterExit::~EnterExit()  {
 XmppEngineImpl* engine = engine_;

 engine->engine_entered_ -= 1;

 bool closing = (engine->state_ != state_ &&
       engine->state_ == STATE_CLOSED);
 bool flushing = closing || (engine->engine_entered_ == 0);

 if (engine->output_handler_ && flushing) {
   std::string output = engine->output_->str();
   if (output.length() > 0)
     engine->output_handler_->WriteOutput(output.c_str(), output.length());
   engine->output_->str("");

   if (closing) {
     engine->output_handler_->CloseConnection();
     engine->output_handler_ = 0;
   }
 }

 if (engine->engine_entered_)
   return;

 if (engine->raised_reset_) {
   engine->stanza_parser_.Reset();
   engine->raised_reset_ = false;
 }

 if (engine->session_handler_) {
   if (engine->state_ != state_)
     engine->session_handler_->OnStateChange(engine->state_);
   // Note: Handling of OnStateChange(CLOSED) should allow for the
   // deletion of the engine, so no members should be accessed
   // after this line.
 }
}

}  // namespace jingle_xmpp
