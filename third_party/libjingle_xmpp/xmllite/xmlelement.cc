/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "base/check.h"
#include "third_party/libjingle_xmpp/xmllite/qname.h"
#include "third_party/libjingle_xmpp/xmllite/xmlbuilder.h"
#include "third_party/libjingle_xmpp/xmllite/xmlconstants.h"
#include "third_party/libjingle_xmpp/xmllite/xmlparser.h"
#include "third_party/libjingle_xmpp/xmllite/xmlprinter.h"

namespace jingle_xmpp {

XmlChild::~XmlChild() {
}

bool XmlText::IsTextImpl() const {
  return true;
}

XmlElement* XmlText::AsElementImpl() const {
  return NULL;
}

XmlText* XmlText::AsTextImpl() const {
  return const_cast<XmlText *>(this);
}

void XmlText::SetText(const std::string& text) {
  text_ = text;
}

void XmlText::AddParsedText(const char* buf, int len) {
  text_.append(buf, len);
}

void XmlText::AddText(const std::string& text) {
  text_ += text;
}

XmlText::~XmlText() {
}

XmlElement::XmlElement(const QName& name) :
    name_(name),
    first_attr_(NULL),
    last_attr_(NULL),
    first_child_(NULL),
    last_child_(NULL),
    cdata_(false) {
}

XmlElement::XmlElement(const XmlElement& elt) :
    XmlChild(),
    name_(elt.name_),
    first_attr_(NULL),
    last_attr_(NULL),
    first_child_(NULL),
    last_child_(NULL),
    cdata_(false) {

  // copy attributes
  XmlAttr* attr;
  XmlAttr ** plast_attr = &first_attr_;
  XmlAttr* newAttr = NULL;
  for (attr = elt.first_attr_; attr; attr = attr->NextAttr()) {
    newAttr = new XmlAttr(*attr);
    *plast_attr = newAttr;
    plast_attr = &(newAttr->next_attr_);
  }
  last_attr_ = newAttr;

  // copy children
  XmlChild* pChild;
  XmlChild ** ppLast = &first_child_;
  XmlChild* newChild = NULL;

  for (pChild = elt.first_child_; pChild; pChild = pChild->NextChild()) {
    if (pChild->IsText()) {
      newChild = new XmlText(*(pChild->AsText()));
    } else {
      newChild = new XmlElement(*(pChild->AsElement()));
    }
    *ppLast = newChild;
    ppLast = &(newChild->next_child_);
  }
  last_child_ = newChild;

  cdata_ = elt.cdata_;
}

XmlElement::XmlElement(const QName& name, bool useDefaultNs) :
  name_(name),
  first_attr_(useDefaultNs ? new XmlAttr(QN_XMLNS, name.Namespace()) : NULL),
  last_attr_(first_attr_),
  first_child_(NULL),
  last_child_(NULL),
  cdata_(false) {
}

bool XmlElement::IsTextImpl() const {
  return false;
}

XmlElement* XmlElement::AsElementImpl() const {
  return const_cast<XmlElement *>(this);
}

XmlText* XmlElement::AsTextImpl() const {
  return NULL;
}

const std::string XmlElement::BodyText() const {
  if (first_child_ && first_child_->IsText() && last_child_ == first_child_) {
    return first_child_->AsText()->Text();
  }

  return std::string();
}

void XmlElement::SetBodyText(const std::string& text) {
  if (text.empty()) {
    ClearChildren();
  } else if (first_child_ == NULL) {
    AddText(text);
  } else if (first_child_->IsText() && last_child_ == first_child_) {
    first_child_->AsText()->SetText(text);
  } else {
    ClearChildren();
    AddText(text);
  }
}

const QName XmlElement::FirstElementName() const {
  const XmlElement* element = FirstElement();
  if (element == NULL)
    return QName();
  return element->Name();
}

XmlAttr* XmlElement::FirstAttr() {
  return first_attr_;
}

const std::string XmlElement::Attr(const StaticQName& name) const {
  XmlAttr* attr;
  for (attr = first_attr_; attr; attr = attr->next_attr_) {
    if (attr->name_ == name)
      return attr->value_;
  }
  return std::string();
}

const std::string XmlElement::Attr(const QName& name) const {
  XmlAttr* attr;
  for (attr = first_attr_; attr; attr = attr->next_attr_) {
    if (attr->name_ == name)
      return attr->value_;
  }
  return std::string();
}

bool XmlElement::HasAttr(const StaticQName& name) const {
  XmlAttr* attr;
  for (attr = first_attr_; attr; attr = attr->next_attr_) {
    if (attr->name_ == name)
      return true;
  }
  return false;
}

bool XmlElement::HasAttr(const QName& name) const {
  XmlAttr* attr;
  for (attr = first_attr_; attr; attr = attr->next_attr_) {
    if (attr->name_ == name)
      return true;
  }
  return false;
}

void XmlElement::SetAttr(const QName& name, std::string_view value) {
  XmlAttr* attr;
  for (attr = first_attr_; attr; attr = attr->next_attr_) {
    if (attr->name_ == name)
      break;
  }
  if (!attr) {
    attr = new XmlAttr(name, value);
    if (last_attr_)
      last_attr_->next_attr_ = attr;
    else
      first_attr_ = attr;
    last_attr_ = attr;
    return;
  }
  attr->value_ = value;
}

void XmlElement::ClearAttr(const QName& name) {
  XmlAttr* attr;
  XmlAttr* last_attr = NULL;
  for (attr = first_attr_; attr; attr = attr->next_attr_) {
    if (attr->name_ == name)
      break;
    last_attr = attr;
  }
  if (!attr)
    return;
  if (!last_attr)
    first_attr_ = attr->next_attr_;
  else
    last_attr->next_attr_ = attr->next_attr_;
  if (last_attr_ == attr)
    last_attr_ = last_attr;
  delete attr;
}

XmlChild* XmlElement::FirstChild() {
  return first_child_;
}

XmlElement* XmlElement::FirstElement() {
  XmlChild* pChild;
  for (pChild = first_child_; pChild; pChild = pChild->next_child_) {
    if (!pChild->IsText())
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement* XmlElement::NextElement() {
  XmlChild* pChild;
  for (pChild = next_child_; pChild; pChild = pChild->next_child_) {
    if (!pChild->IsText())
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement* XmlElement::FirstWithNamespace(const std::string& ns) {
  XmlChild* pChild;
  for (pChild = first_child_; pChild; pChild = pChild->next_child_) {
    if (!pChild->IsText() && pChild->AsElement()->Name().Namespace() == ns)
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement *
XmlElement::NextWithNamespace(const std::string& ns) {
  XmlChild* pChild;
  for (pChild = next_child_; pChild; pChild = pChild->next_child_) {
    if (!pChild->IsText() && pChild->AsElement()->Name().Namespace() == ns)
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement *
XmlElement::FirstNamed(const QName& name) {
  XmlChild* pChild;
  for (pChild = first_child_; pChild; pChild = pChild->next_child_) {
    if (!pChild->IsText() && pChild->AsElement()->Name() == name)
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement *
XmlElement::FirstNamed(const StaticQName& name) {
  XmlChild* pChild;
  for (pChild = first_child_; pChild; pChild = pChild->next_child_) {
    if (!pChild->IsText() && pChild->AsElement()->Name() == name)
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement *
XmlElement::NextNamed(const QName& name) {
  XmlChild* pChild;
  for (pChild = next_child_; pChild; pChild = pChild->next_child_) {
    if (!pChild->IsText() && pChild->AsElement()->Name() == name)
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement *
XmlElement::NextNamed(const StaticQName& name) {
  XmlChild* pChild;
  for (pChild = next_child_; pChild; pChild = pChild->next_child_) {
    if (!pChild->IsText() && pChild->AsElement()->Name() == name)
      return pChild->AsElement();
  }
  return NULL;
}

XmlElement* XmlElement::FindOrAddNamedChild(const QName& name) {
  XmlElement* child = FirstNamed(name);
  if (!child) {
    child = new XmlElement(name);
    AddElement(child);
  }

  return child;
}

const std::string XmlElement::TextNamed(const QName& name) const {
  XmlChild* pChild;
  for (pChild = first_child_; pChild; pChild = pChild->next_child_) {
    if (!pChild->IsText() && pChild->AsElement()->Name() == name)
      return pChild->AsElement()->BodyText();
  }
  return std::string();
}

void XmlElement::InsertChildAfter(XmlChild* predecessor, XmlChild* next) {
  if (predecessor == NULL) {
    next->next_child_ = first_child_;
    first_child_ = next;
  }
  else {
    next->next_child_ = predecessor->next_child_;
    predecessor->next_child_ = next;
  }
}

void XmlElement::RemoveChildAfter(XmlChild* predecessor) {
  XmlChild* next;

  if (predecessor == NULL) {
    next = first_child_;
    first_child_ = next->next_child_;
  }
  else {
    next = predecessor->next_child_;
    predecessor->next_child_ = next->next_child_;
  }

  if (last_child_ == next)
    last_child_ = predecessor;

  delete next;
}

void XmlElement::AddAttr(const QName& name, const std::string& value) {
  DCHECK(!HasAttr(name));

  XmlAttr ** pprev = last_attr_ ? &(last_attr_->next_attr_) : &first_attr_;
  last_attr_ = (*pprev = new XmlAttr(name, value));
}

void XmlElement::AddAttr(const QName& name, const std::string& value,
                         int depth) {
  XmlElement* element = this;
  while (depth--) {
    element = element->last_child_->AsElement();
  }
  element->AddAttr(name, value);
}

void XmlElement::AddParsedText(const char* cstr, int len) {
  if (len == 0)
    return;

  if (last_child_ && last_child_->IsText()) {
    last_child_->AsText()->AddParsedText(cstr, len);
    return;
  }
  XmlChild ** pprev = last_child_ ? &(last_child_->next_child_) : &first_child_;
  last_child_ = *pprev = new XmlText(cstr, len);
}

void XmlElement::AddCDATAText(const char* buf, int len) {
  cdata_ = true;
  AddParsedText(buf, len);
}

void XmlElement::AddText(const std::string& text) {
  if (text == STR_EMPTY)
    return;

  if (last_child_ && last_child_->IsText()) {
    last_child_->AsText()->AddText(text);
    return;
  }
  XmlChild ** pprev = last_child_ ? &(last_child_->next_child_) : &first_child_;
  last_child_ = *pprev = new XmlText(text);
}

void XmlElement::AddText(const std::string& text, int depth) {
  // note: the first syntax is ambigious for msvc 6
  // XmlElement* pel(this);
  XmlElement* element = this;
  while (depth--) {
    element = element->last_child_->AsElement();
  }
  element->AddText(text);
}

void XmlElement::AddElement(XmlElement *child) {
  if (child == NULL)
    return;

  XmlChild ** pprev = last_child_ ? &(last_child_->next_child_) : &first_child_;
  *pprev = child;
  last_child_ = child;
  child->next_child_ = NULL;
}

void XmlElement::AddElement(XmlElement *child, int depth) {
  XmlElement* element = this;
  while (depth--) {
    element = element->last_child_->AsElement();
  }
  element->AddElement(child);
}

void XmlElement::ClearNamedChildren(const QName& name) {
  XmlChild* prev_child = NULL;
  XmlChild* next_child;
  XmlChild* child;
  for (child = FirstChild(); child; child = next_child) {
    next_child = child->NextChild();
    if (!child->IsText() && child->AsElement()->Name() == name)
    {
      RemoveChildAfter(prev_child);
      continue;
    }
    prev_child = child;
  }
}

void XmlElement::ClearAttributes() {
  XmlAttr* attr;
  for (attr = first_attr_; attr; ) {
    XmlAttr* to_delete = attr;
    attr = attr->next_attr_;
    delete to_delete;
  }
  first_attr_ = last_attr_ = NULL;
}

void XmlElement::ClearChildren() {
  XmlChild* pchild;
  for (pchild = first_child_; pchild; ) {
    XmlChild* to_delete = pchild;
    pchild = pchild->next_child_;
    delete to_delete;
  }
  first_child_ = last_child_ = NULL;
}

std::string XmlElement::Str() const {
  std::stringstream ss;
  XmlPrinter::PrintXml(&ss, this);
  return ss.str();
}

XmlElement* XmlElement::ForStr(const std::string& str) {
  XmlBuilder builder;
  XmlParser::ParseXml(&builder, str);
  return builder.CreateElement();
}

XmlElement::~XmlElement() {
  XmlAttr* attr;
  for (attr = first_attr_; attr; ) {
    XmlAttr* to_delete = attr;
    attr = attr->next_attr_;
    delete to_delete;
  }

  XmlChild* pchild;
  for (pchild = first_child_; pchild; ) {
    XmlChild* to_delete = pchild;
    pchild = pchild->next_child_;
    delete to_delete;
  }
}

}  // namespace jingle_xmpp
