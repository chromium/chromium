// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libxml/chromium/xml_writer.h"

#include <libxml/xmlwriter.h>

#include "third_party/libxml/chromium/libxml_utils.h"

XmlWriter::XmlWriter() : writer_(nullptr), buffer_(nullptr) {}

XmlWriter::~XmlWriter() {
  if (writer_)
    xmlFreeTextWriter(writer_);
  if (buffer_)
    xmlBufferFree(buffer_);
}

void XmlWriter::StartWriting() {
  buffer_ = xmlBufferCreate();
  writer_ = xmlNewTextWriterMemory(buffer_, 0);
  xmlTextWriterSetIndent(writer_, 1);
  xmlTextWriterStartDocument(writer_, nullptr, nullptr, nullptr);
}

void XmlWriter::StopWriting() {
  xmlTextWriterEndDocument(writer_);
  xmlFreeTextWriter(writer_);
  writer_ = nullptr;
}

void XmlWriter::StartIndenting() {
  xmlTextWriterSetIndent(writer_, 1);
}

void XmlWriter::StopIndenting() {
  xmlTextWriterSetIndent(writer_, 0);
}

bool XmlWriter::StartElement(const std::string& element_name) {
  return xmlTextWriterStartElement(writer_, BAD_CAST element_name.c_str()) >= 0;
}

bool XmlWriter::EndElement() {
  return xmlTextWriterEndElement(writer_) >= 0;
}

bool XmlWriter::AppendElementContent(const std::string& content) {
  return xmlTextWriterWriteString(writer_, BAD_CAST content.c_str()) >= 0;
}

bool XmlWriter::AddAttribute(const std::string& attribute_name,
                             const std::string& attribute_value) {
  return xmlTextWriterWriteAttribute(writer_, BAD_CAST attribute_name.c_str(),
                                     BAD_CAST attribute_value.c_str()) >= 0;
}

bool XmlWriter::WriteElement(const std::string& element_name,
                             const std::string& content) {
  return xmlTextWriterWriteElement(writer_, BAD_CAST element_name.c_str(),
                                   BAD_CAST content.c_str()) >= 0;
}

std::string XmlWriter::GetWrittenString() {
  return buffer_ ? internal::XmlStringToStdString(buffer_->content) : "";
}
