/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/libjingle_xmpp/xmpp/jid.h"

#include <ctype.h>

#include <algorithm>
#include <string>

#include "base/check.h"
#include "base/logging.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"

namespace jingle_xmpp {

Jid::Jid() {
}

Jid::Jid(const std::string& jid_string) {
  if (jid_string.empty())
    return;

  // First find the slash and slice off that part
  size_t slash = jid_string.find('/');
  resource_name_ = (slash == std::string::npos ? STR_EMPTY :
                    jid_string.substr(slash + 1));

  // Now look for the node
  size_t at = jid_string.find('@');
  size_t domain_begin;
  if (at < slash && at != std::string::npos) {
    node_name_ = jid_string.substr(0, at);
    domain_begin = at + 1;
  } else {
    domain_begin = 0;
  }

  // Now take what is left as the domain
  size_t domain_length = (slash == std::string::npos) ?
      (jid_string.length() - domain_begin) : (slash - domain_begin);
  domain_name_ = jid_string.substr(domain_begin, domain_length);

  ValidateOrReset();
}

Jid::Jid(const std::string& node_name,
         const std::string& domain_name,
         const std::string& resource_name)
    :  node_name_(node_name),
       domain_name_(domain_name),
       resource_name_(resource_name) {
  ValidateOrReset();
}

void Jid::ValidateOrReset() {
  bool valid_node;
  bool valid_domain;
  bool valid_resource;

  node_name_ = PrepNode(node_name_, &valid_node);
  domain_name_ = PrepDomain(domain_name_, &valid_domain);
  resource_name_ = PrepResource(resource_name_, &valid_resource);

  if (!valid_node || !valid_domain || !valid_resource) {
    node_name_.clear();
    domain_name_.clear();
    resource_name_.clear();
  }
}

std::string Jid::Str() const {
  if (!IsValid())
    return STR_EMPTY;

  std::string ret;

  if (!node_name_.empty())
    ret = node_name_ + "@";

  DCHECK(domain_name_ != STR_EMPTY);
  ret += domain_name_;

  if (!resource_name_.empty())
    ret += "/" + resource_name_;

  return ret;
}

Jid::~Jid() {
}

bool Jid::IsEmpty() const {
  return (node_name_.empty() && domain_name_.empty() &&
          resource_name_.empty());
}

bool Jid::IsValid() const {
  return !domain_name_.empty();
}

bool Jid::IsBare() const {
  if (IsEmpty()) {
    DVLOG(1) << "Warning: Calling IsBare() on the empty jid.";
    return true;
  }
  return IsValid() && resource_name_.empty();
}

bool Jid::IsFull() const {
  return IsValid() && !resource_name_.empty();
}

Jid Jid::BareJid() const {
  if (!IsValid())
    return Jid();
  if (!IsFull())
    return *this;
  return Jid(node_name_, domain_name_, STR_EMPTY);
}

bool Jid::BareEquals(const Jid& other) const {
  return other.node_name_ == node_name_ &&
      other.domain_name_ == domain_name_;
}

void Jid::CopyFrom(const Jid& jid) {
  this->node_name_ = jid.node_name_;
  this->domain_name_ = jid.domain_name_;
  this->resource_name_ = jid.resource_name_;
}

bool Jid::operator==(const Jid& other) const {
  return other.node_name_ == node_name_ &&
      other.domain_name_ == domain_name_ &&
      other.resource_name_ == resource_name_;
}

int Jid::Compare(const Jid& other) const {
  int compare_result;
  compare_result = node_name_.compare(other.node_name_);
  if (0 != compare_result)
    return compare_result;
  compare_result = domain_name_.compare(other.domain_name_);
  if (0 != compare_result)
    return compare_result;
  compare_result = resource_name_.compare(other.resource_name_);
  return compare_result;
}

// --- JID parsing code: ---

// Checks and normalizes the node part of a JID.
std::string Jid::PrepNode(const std::string& node, bool* valid) {
  *valid = false;
  std::string result;

  for (std::string::const_iterator i = node.begin(); i < node.end(); ++i) {
    bool char_valid = true;
    unsigned char ch = *i;
    if (ch <= 0x7F) {
      result += PrepNodeAscii(ch, &char_valid);
    }
    else {
      // TODO: implement the correct stringprep protocol for these
      result += tolower(ch);
    }
    if (!char_valid) {
      return STR_EMPTY;
    }
  }

  if (result.length() > 1023) {
    return STR_EMPTY;
  }
  *valid = true;
  return result;
}


// Returns the appropriate mapping for an ASCII character in a node.
char Jid::PrepNodeAscii(char ch, bool* valid) {
  *valid = true;
  switch (ch) {
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
    case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
    case 'V': case 'W': case 'X': case 'Y': case 'Z':
      return (char)(ch + ('a' - 'A'));

    case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
    case 0x06: case 0x07: case 0x08: case 0x09: case 0x0A: case 0x0B:
    case 0x0C: case 0x0D: case 0x0E: case 0x0F: case 0x10: case 0x11:
    case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
    case ' ': case '&': case '/': case ':': case '<': case '>': case '@':
    case '\"': case '\'':
    case 0x7F:
      *valid = false;
      return 0;

    default:
      return ch;
  }
}


// Checks and normalizes the resource part of a JID.
std::string Jid::PrepResource(const std::string& resource, bool* valid) {
  *valid = false;
  std::string result;

  for (std::string::const_iterator i = resource.begin();
       i < resource.end(); ++i) {
    bool char_valid = true;
    unsigned char ch = *i;
    if (ch <= 0x7F) {
      result += PrepResourceAscii(ch, &char_valid);
    }
    else {
      // TODO: implement the correct stringprep protocol for these
      result += ch;
    }
  }

  if (result.length() > 1023) {
    return STR_EMPTY;
  }
  *valid = true;
  return result;
}

// Returns the appropriate mapping for an ASCII character in a resource.
char Jid::PrepResourceAscii(char ch, bool* valid) {
  *valid = true;
  switch (ch) {
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
    case 0x06: case 0x07: case 0x08: case 0x09: case 0x0A: case 0x0B:
    case 0x0C: case 0x0D: case 0x0E: case 0x0F: case 0x10: case 0x11:
    case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
    case 0x7F:
      *valid = false;
      return 0;

    default:
      return ch;
  }
}

// Checks and normalizes the domain part of a JID.
std::string Jid::PrepDomain(const std::string& domain, bool* valid) {
  *valid = false;
  std::string result;

  // TODO: if the domain contains a ':', then we should parse it
  // as an IPv6 address rather than giving an error about illegal domain.
  PrepDomain(domain, &result, valid);
  if (!*valid) {
    return STR_EMPTY;
  }

  if (result.length() > 1023) {
    return STR_EMPTY;
  }
  *valid = true;
  return result;
}


// Checks and normalizes an IDNA domain.
void Jid::PrepDomain(const std::string& domain, std::string* buf, bool* valid) {
  *valid = false;
  std::string::const_iterator last = domain.begin();
  for (std::string::const_iterator i = domain.begin(); i < domain.end(); ++i) {
    bool label_valid = true;
    char ch = *i;
    switch (ch) {
      case 0x002E:
#if 0 // FIX: This isn't UTF-8-aware.
      case 0x3002:
      case 0xFF0E:
      case 0xFF61:
#endif
        PrepDomainLabel(last, i, buf, &label_valid);
        *buf += '.';
        last = i + 1;
        break;
    }
    if (!label_valid) {
      return;
    }
  }
  PrepDomainLabel(last, domain.end(), buf, valid);
}

// Checks and normalizes a domain label.
void Jid::PrepDomainLabel(
    std::string::const_iterator start, std::string::const_iterator end,
    std::string* buf, bool* valid) {
  *valid = false;

  int start_len = static_cast<int>(buf->length());
  for (std::string::const_iterator i = start; i < end; ++i) {
    bool char_valid = true;
    unsigned char ch = *i;
    if (ch <= 0x7F) {
      *buf += PrepDomainLabelAscii(ch, &char_valid);
    }
    else {
      // TODO: implement ToASCII for these
      *buf += ch;
    }
    if (!char_valid) {
      return;
    }
  }

  int count = static_cast<int>(buf->length() - start_len);
  if (count == 0) {
    return;
  }
  else if (count > 63) {
    return;
  }

  // Is this check needed? See comment in PrepDomainLabelAscii.
  if ((*buf)[start_len] == '-') {
    return;
  }
  if ((*buf)[buf->length() - 1] == '-') {
    return;
  }
  *valid = true;
}


// Returns the appropriate mapping for an ASCII character in a domain label.
char Jid::PrepDomainLabelAscii(char ch, bool* valid) {
  *valid = true;
  // TODO: A literal reading of the spec seems to say that we do
  // not need to check for these illegal characters (an "internationalized
  // domain label" runs ToASCII with UseSTD3... set to false).  But that
  // can't be right.  We should at least be checking that there are no '/'
  // or '@' characters in the domain.  Perhaps we should see what others
  // do in this case.

  switch (ch) {
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
    case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
    case 'V': case 'W': case 'X': case 'Y': case 'Z':
      return (char)(ch + ('a' - 'A'));

    case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
    case 0x06: case 0x07: case 0x08: case 0x09: case 0x0A: case 0x0B:
    case 0x0C: case 0x0D: case 0x0E: case 0x0F: case 0x10: case 0x11:
    case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
    case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D:
    case 0x1E: case 0x1F: case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x27: case 0x28: case 0x29:
    case 0x2A: case 0x2B: case 0x2C: case 0x2E: case 0x2F: case 0x3A:
    case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F: case 0x40:
    case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F: case 0x60:
    case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
      *valid = false;
      return 0;

    default:
      return ch;
  }
}

}  // namespace jingle_xmpp
