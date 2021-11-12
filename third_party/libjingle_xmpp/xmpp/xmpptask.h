/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef THIRD_PARTY_LIBJINGLE_XMPP_XMPP_XMPPTASK_H_
#define THIRD_PARTY_LIBJINGLE_XMPP_XMPP_XMPPTASK_H_

#include <deque>
#include <memory>
#include <string>

#include "third_party/libjingle_xmpp/task_runner/task.h"
#include "third_party/libjingle_xmpp/task_runner/taskparent.h"
#include "third_party/libjingle_xmpp/xmpp/xmppengine.h"
#include "third_party/webrtc/rtc_base/third_party/sigslot/sigslot.h"

namespace jingle_xmpp {

/////////////////////////////////////////////////////////////////////
//
// XMPPTASK
//
/////////////////////////////////////////////////////////////////////
//
// See Task and XmppClient first.
//
// XmppTask is a task that is designed to go underneath XmppClient and be
// useful there.  It has a way of finding its XmppClient parent so you
// can have it nested arbitrarily deep under an XmppClient and it can
// still find the XMPP services.
//
// Tasks register themselves to listen to particular kinds of stanzas
// that are sent out by the client.  Rather than processing stanzas
// right away, they should decide if they own the sent stanza,
// and if so, queue it and Wake() the task, or if a stanza does not belong
// to you, return false right away so the next XmppTask can take a crack.
// This technique (synchronous recognize, but asynchronous processing)
// allows you to have arbitrary logic for recognizing stanzas yet still,
// for example, disconnect a client while processing a stanza -
// without reentrancy problems.
//
/////////////////////////////////////////////////////////////////////

class XmppTask;

// XmppClientInterface is an abstract interface for sending and
// handling stanzas.  It can be implemented for unit tests or
// different network environments.  It will usually be implemented by
// XmppClient.
class XmppClientInterface {
 public:
  XmppClientInterface();

  XmppClientInterface(const XmppClientInterface&) = delete;
  XmppClientInterface& operator=(const XmppClientInterface&) = delete;

  virtual ~XmppClientInterface();

  virtual XmppEngine::State GetState() const = 0;
  virtual const Jid& jid() const = 0;
  virtual std::string NextId() = 0;
  virtual XmppReturnStatus SendStanza(const XmlElement* stanza) = 0;
  virtual XmppReturnStatus SendStanzaError(const XmlElement* original_stanza,
                                           XmppStanzaError error_code,
                                           const std::string& message) = 0;
  virtual void AddXmppTask(XmppTask* task, XmppEngine::HandlerLevel level) = 0;
  virtual void RemoveXmppTask(XmppTask* task) = 0;
  sigslot::signal0<> SignalDisconnected;
};

// XmppTaskParentInterface is the interface require for any parent of
// an XmppTask.  It needs, for example, a way to get an
// XmppClientInterface.

// We really ought to inherit from a TaskParentInterface, but we tried
// that and it's way too complicated to change
// Task/TaskParent/TaskRunner.  For now, this works.
class XmppTaskParentInterface : public jingle_xmpp::Task {
 public:
  explicit XmppTaskParentInterface(jingle_xmpp::TaskParent* parent)
      : Task(parent) {
  }

  XmppTaskParentInterface(const XmppTaskParentInterface&) = delete;
  XmppTaskParentInterface& operator=(const XmppTaskParentInterface&) = delete;

  virtual ~XmppTaskParentInterface() {}

  virtual XmppClientInterface* GetClient() = 0;
};

class XmppTaskBase : public XmppTaskParentInterface {
 public:
  explicit XmppTaskBase(XmppTaskParentInterface* parent)
      : XmppTaskParentInterface(parent),
        parent_(parent) {
  }

  XmppTaskBase(const XmppTaskBase&) = delete;
  XmppTaskBase& operator=(const XmppTaskBase&) = delete;

  virtual ~XmppTaskBase() {}

  virtual XmppClientInterface* GetClient() {
    return parent_->GetClient();
  }

 protected:
  XmppTaskParentInterface* parent_;
};

class XmppTask : public XmppTaskBase,
                 public XmppStanzaHandler,
                 public sigslot::has_slots<>
{
 public:
  XmppTask(XmppTaskParentInterface* parent,
           XmppEngine::HandlerLevel level = XmppEngine::HL_NONE);
  virtual ~XmppTask();

  std::string task_id() const { return id_; }
  void set_task_id(std::string id) { id_ = id; }

#if !defined(NDEBUG)
  void set_debug_force_timeout(const bool f) { debug_force_timeout_ = f; }
#endif

  virtual bool HandleStanza(const XmlElement* stanza) { return false; }

 protected:
  XmppReturnStatus SendStanza(const XmlElement* stanza);
  XmppReturnStatus SetResult(const std::string& code);
  XmppReturnStatus SendStanzaError(const XmlElement* element_original,
                                   XmppStanzaError code,
                                   const std::string& text);

  virtual void Stop();
  virtual void OnDisconnect();

  virtual void QueueStanza(const XmlElement* stanza);
  const XmlElement* NextStanza();

  bool MatchStanzaFrom(const XmlElement* stanza, const Jid& match_jid);

  bool MatchResponseIq(const XmlElement* stanza, const Jid& to,
                       const std::string& task_id);

  static bool MatchRequestIq(const XmlElement* stanza, const std::string& type,
                             const QName& qn);
  static XmlElement *MakeIqResult(const XmlElement* query);
  static XmlElement *MakeIq(const std::string& type,
                            const Jid& to, const std::string& task_id);

  // Returns true if the task is under the specified rate limit and updates the
  // rate limit accordingly
  bool VerifyTaskRateLimit(const std::string task_name, int max_count,
                           int per_x_seconds);

private:
  void StopImpl();

  bool stopped_;
  std::deque<XmlElement*> stanza_queue_;
  std::unique_ptr<XmlElement> next_stanza_;
  std::string id_;

#if !defined(NDEBUG)
  bool debug_force_timeout_;
#endif
};

}  // namespace jingle_xmpp

#endif // THIRD_PARTY_LIBJINGLE_XMPP_XMPP_XMPPTASK_H_
