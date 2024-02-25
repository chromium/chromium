/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlAttr;
using jingle_xmpp::XmlChild;
using jingle_xmpp::XmlElement;

std::ostream& operator<<(std::ostream& os, const QName& name) {
  os << name.Namespace() << ":" << name.LocalPart();
  return os;
}

TEST(XmlElementTest, TestConstructors) {
  XmlElement elt(QName("google:test", "first"));
  EXPECT_EQ("<test:first xmlns:test=\"google:test\"/>", elt.Str());

  XmlElement elt2(QName("google:test", "first"), true);
  EXPECT_EQ("<first xmlns=\"google:test\"/>", elt2.Str());
}

TEST(XmlElementTest, TestAdd) {
  XmlElement elt(QName("google:test", "root"), true);
  elt.AddElement(new XmlElement(QName("google:test", "first")));
  elt.AddElement(new XmlElement(QName("google:test", "nested")), 1);
  elt.AddText("nested-value", 2);
  elt.AddText("between-", 1);
  elt.AddText("value", 1);
  elt.AddElement(new XmlElement(QName("google:test", "nested2")), 1);
  elt.AddElement(new XmlElement(QName("google:test", "second")));
  elt.AddText("init-value", 1);
  elt.AddElement(new XmlElement(QName("google:test", "nested3")), 1);
  elt.AddText("trailing-value", 1);

  // make sure it looks ok overall
  EXPECT_EQ("<root xmlns=\"google:test\">"
        "<first><nested>nested-value</nested>between-value<nested2/></first>"
        "<second>init-value<nested3/>trailing-value</second></root>",
        elt.Str());

  // make sure text was concatenated
  XmlChild * pchild =
    elt.FirstChild()->AsElement()->FirstChild()->NextChild();
  EXPECT_TRUE(pchild->IsText());
  EXPECT_EQ("between-value", pchild->AsText()->Text());
}

TEST(XmlElementTest, TestAttrs) {
  XmlElement elt(QName("", "root"));
  elt.SetAttr(QName("", "a"), "avalue");
  EXPECT_EQ("<root a=\"avalue\"/>", elt.Str());

  elt.SetAttr(QName("", "b"), "bvalue");
  EXPECT_EQ("<root a=\"avalue\" b=\"bvalue\"/>", elt.Str());

  elt.SetAttr(QName("", "a"), "avalue2");
  EXPECT_EQ("<root a=\"avalue2\" b=\"bvalue\"/>", elt.Str());

  // Make sure that `SetAttr` can also be called with an explicit
  // `std::string_view` object.
  elt.SetAttr(QName("", "b"), std::string_view("bvalue2"));
  EXPECT_EQ("<root a=\"avalue2\" b=\"bvalue2\"/>", elt.Str());

  elt.SetAttr(QName("", "c"), "cvalue");
  EXPECT_EQ("<root a=\"avalue2\" b=\"bvalue2\" c=\"cvalue\"/>", elt.Str());

  XmlAttr * patt = elt.FirstAttr();
  EXPECT_EQ(QName("", "a"), patt->Name());
  EXPECT_EQ("avalue2", patt->Value());

  patt = patt->NextAttr();
  EXPECT_EQ(QName("", "b"), patt->Name());
  EXPECT_EQ("bvalue2", patt->Value());

  patt = patt->NextAttr();
  EXPECT_EQ(QName("", "c"), patt->Name());
  EXPECT_EQ("cvalue", patt->Value());

  patt = patt->NextAttr();
  EXPECT_TRUE(NULL == patt);

  EXPECT_TRUE(elt.HasAttr(QName("", "a")));
  EXPECT_TRUE(elt.HasAttr(QName("", "b")));
  EXPECT_TRUE(elt.HasAttr(QName("", "c")));
  EXPECT_FALSE(elt.HasAttr(QName("", "d")));

  elt.SetAttr(QName("", "d"), "dvalue");
  EXPECT_EQ("<root a=\"avalue2\" b=\"bvalue2\" c=\"cvalue\" d=\"dvalue\"/>",
      elt.Str());
  EXPECT_TRUE(elt.HasAttr(QName("", "d")));

  elt.ClearAttr(QName("", "z"));  // not found, no effect
  EXPECT_EQ("<root a=\"avalue2\" b=\"bvalue2\" c=\"cvalue\" d=\"dvalue\"/>",
      elt.Str());

  elt.ClearAttr(QName("", "b"));
  EXPECT_EQ("<root a=\"avalue2\" c=\"cvalue\" d=\"dvalue\"/>", elt.Str());

  elt.ClearAttr(QName("", "a"));
  EXPECT_EQ("<root c=\"cvalue\" d=\"dvalue\"/>", elt.Str());

  elt.ClearAttr(QName("", "d"));
  EXPECT_EQ("<root c=\"cvalue\"/>", elt.Str());

  elt.ClearAttr(QName("", "c"));
  EXPECT_EQ("<root/>", elt.Str());
}

TEST(XmlElementTest, TestBodyText) {
  XmlElement elt(QName("", "root"));
  EXPECT_EQ("", elt.BodyText());

  elt.AddText("body value text");

  EXPECT_EQ("body value text", elt.BodyText());

  elt.ClearChildren();
  elt.AddText("more value ");
  elt.AddText("text");

  EXPECT_EQ("more value text", elt.BodyText());

  elt.ClearChildren();
  elt.AddText("decoy");
  elt.AddElement(new XmlElement(QName("", "dummy")));
  EXPECT_EQ("", elt.BodyText());

  elt.SetBodyText("replacement");
  EXPECT_EQ("replacement", elt.BodyText());

  elt.SetBodyText("");
  EXPECT_TRUE(NULL == elt.FirstChild());

  elt.SetBodyText("goodbye");
  EXPECT_EQ("goodbye", elt.FirstChild()->AsText()->Text());
  EXPECT_EQ("goodbye", elt.BodyText());
}

TEST(XmlElementTest, TestCopyConstructor) {
  XmlElement * element = XmlElement::ForStr(
      "<root xmlns='test-foo'>This is a <em a='avalue' b='bvalue'>"
      "little <b>little</b></em> test</root>");

  XmlElement * pelCopy = new XmlElement(*element);
  EXPECT_EQ("<root xmlns=\"test-foo\">This is a <em a=\"avalue\" b=\"bvalue\">"
      "little <b>little</b></em> test</root>", pelCopy->Str());
  delete pelCopy;

  pelCopy = new XmlElement(*(element->FirstChild()->NextChild()->AsElement()));
  EXPECT_EQ("<foo:em a=\"avalue\" b=\"bvalue\" xmlns:foo=\"test-foo\">"
      "little <foo:b>little</foo:b></foo:em>", pelCopy->Str());

  XmlAttr * patt = pelCopy->FirstAttr();
  EXPECT_EQ(QName("", "a"), patt->Name());
  EXPECT_EQ("avalue", patt->Value());

  patt = patt->NextAttr();
  EXPECT_EQ(QName("", "b"), patt->Name());
  EXPECT_EQ("bvalue", patt->Value());

  patt = patt->NextAttr();
  EXPECT_TRUE(NULL == patt);
  delete pelCopy;
  delete element;
}

TEST(XmlElementTest, TestNameSearch) {
  XmlElement * element = XmlElement::ForStr(
    "<root xmlns='test-foo'>"
      "<firstname>George</firstname>"
      "<middlename>X.</middlename>"
      "some text"
      "<lastname>Harrison</lastname>"
      "<firstname>John</firstname>"
      "<middlename>Y.</middlename>"
      "<lastname>Lennon</lastname>"
    "</root>");
  EXPECT_TRUE(NULL ==
      element->FirstNamed(QName("", "firstname")));
  EXPECT_EQ(element->FirstChild(),
      element->FirstNamed(QName("test-foo", "firstname")));
  EXPECT_EQ(element->FirstChild()->NextChild(),
      element->FirstNamed(QName("test-foo", "middlename")));
  EXPECT_EQ(element->FirstElement()->NextElement(),
      element->FirstNamed(QName("test-foo", "middlename")));
  EXPECT_EQ("Harrison",
      element->TextNamed(QName("test-foo", "lastname")));
  EXPECT_EQ(element->FirstElement()->NextElement()->NextElement(),
      element->FirstNamed(QName("test-foo", "lastname")));
  EXPECT_EQ("John", element->FirstNamed(QName("test-foo", "firstname"))->
      NextNamed(QName("test-foo", "firstname"))->BodyText());
  EXPECT_EQ("Y.", element->FirstNamed(QName("test-foo", "middlename"))->
      NextNamed(QName("test-foo", "middlename"))->BodyText());
  EXPECT_EQ("Lennon", element->FirstNamed(QName("test-foo", "lastname"))->
      NextNamed(QName("test-foo", "lastname"))->BodyText());
  EXPECT_TRUE(NULL == element->FirstNamed(QName("test-foo", "firstname"))->
      NextNamed(QName("test-foo", "firstname"))->
      NextNamed(QName("test-foo", "firstname")));

  delete element;
}
