// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    composite \
//    damage \
//    dpms \
//    dri2 \
//    dri3 \
//    ge \
//    glx \
//    present \
//    randr \
//    record \
//    render \
//    res \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xc_misc \
//    xevie \
//    xf86dri \
//    xf86vidmode \
//    xfixes \
//    xinerama \
//    xinput \
//    xkb \
//    xprint \
//    xproto \
//    xselinux \
//    xtest \
//    xv \
//    xvmc

#include "xprint.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

XPrint::XPrint(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<XPrint::NotifyEvent>(XPrint::NotifyEvent* event_,
                                    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& detail = (*event_).detail;
  auto& sequence = (*event_).sequence;
  auto& context = (*event_).context;
  auto& cancel = (*event_).cancel;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // detail
  Read(&detail, &buf);

  // sequence
  Read(&sequence, &buf);

  // context
  Read(&context, &buf);

  // cancel
  Read(&cancel, &buf);

  DUMP_WILL_BE_CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<XPrint::AttributNotifyEvent>(XPrint::AttributNotifyEvent* event_,
                                            ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& detail = (*event_).detail;
  auto& sequence = (*event_).sequence;
  auto& context = (*event_).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // detail
  Read(&detail, &buf);

  // sequence
  Read(&sequence, &buf);

  // context
  Read(&context, &buf);

  DUMP_WILL_BE_CHECK_LE(buf.offset, 32ul);
}

std::string XPrint::BadContextError::ToString() const {
  std::stringstream ss_;
  ss_ << "XPrint::BadContextError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<XPrint::BadContextError>(XPrint::BadContextError* error_,
                                        ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  DUMP_WILL_BE_CHECK_LE(buf.offset, 32ul);
}
std::string XPrint::BadSequenceError::ToString() const {
  std::stringstream ss_;
  ss_ << "XPrint::BadSequenceError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<XPrint::BadSequenceError>(XPrint::BadSequenceError* error_,
                                         ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  DUMP_WILL_BE_CHECK_LE(buf.offset, 32ul);
}
Future<XPrint::PrintQueryVersionReply> XPrint::PrintQueryVersion(
    const XPrint::PrintQueryVersionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 0;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<XPrint::PrintQueryVersionReply>(
      &buf, "XPrint::PrintQueryVersion", false);
}

Future<XPrint::PrintQueryVersionReply> XPrint::PrintQueryVersion() {
  return XPrint::PrintQueryVersion(XPrint::PrintQueryVersionRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XPrint::PrintQueryVersionReply> detail::ReadReply<
    XPrint::PrintQueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XPrint::PrintQueryVersionReply>();

  auto& sequence = (*reply).sequence;
  auto& major_version = (*reply).major_version;
  auto& minor_version = (*reply).minor_version;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // major_version
  Read(&major_version, &buf);

  // minor_version
  Read(&minor_version, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XPrint::PrintGetPrinterListReply> XPrint::PrintGetPrinterList(
    const XPrint::PrintGetPrinterListRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  uint32_t printerNameLen{};
  uint32_t localeLen{};
  auto& printer_name = request.printer_name;
  size_t printer_name_len = printer_name.size();
  auto& locale = request.locale;
  size_t locale_len = locale.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // printerNameLen
  printerNameLen = printer_name.size();
  buf.Write(&printerNameLen);

  // localeLen
  localeLen = locale.size();
  buf.Write(&localeLen);

  // printer_name
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(printerNameLen),
                        printer_name.size());
  for (auto& printer_name_elem : printer_name) {
    // printer_name_elem
    buf.Write(&printer_name_elem);
  }

  // pad0
  Align(&buf, 4);

  // locale
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(localeLen), locale.size());
  for (auto& locale_elem : locale) {
    // locale_elem
    buf.Write(&locale_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<XPrint::PrintGetPrinterListReply>(
      &buf, "XPrint::PrintGetPrinterList", false);
}

Future<XPrint::PrintGetPrinterListReply> XPrint::PrintGetPrinterList(
    const std::vector<String8>& printer_name,
    const std::vector<String8>& locale) {
  return XPrint::PrintGetPrinterList(
      XPrint::PrintGetPrinterListRequest{printer_name, locale});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XPrint::PrintGetPrinterListReply> detail::ReadReply<
    XPrint::PrintGetPrinterListReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XPrint::PrintGetPrinterListReply>();

  auto& sequence = (*reply).sequence;
  uint32_t listCount{};
  auto& printers = (*reply).printers;
  size_t printers_len = printers.size();

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // listCount
  Read(&listCount, &buf);

  // pad1
  Pad(&buf, 20);

  // printers
  printers.resize(listCount);
  for (auto& printers_elem : printers) {
    // printers_elem
    {
      uint32_t nameLen{};
      auto& name = printers_elem.name;
      size_t name_len = name.size();
      uint32_t descLen{};
      auto& description = printers_elem.description;
      size_t description_len = description.size();

      // nameLen
      Read(&nameLen, &buf);

      // name
      name.resize(nameLen);
      for (auto& name_elem : name) {
        // name_elem
        Read(&name_elem, &buf);
      }

      // pad0
      Align(&buf, 4);

      // descLen
      Read(&descLen, &buf);

      // description
      description.resize(descLen);
      for (auto& description_elem : description) {
        // description_elem
        Read(&description_elem, &buf);
      }

      // pad1
      Align(&buf, 4);
    }
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XPrint::PrintRehashPrinterList(
    const XPrint::PrintRehashPrinterListRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 20;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::PrintRehashPrinterList",
                                        false);
}

Future<void> XPrint::PrintRehashPrinterList() {
  return XPrint::PrintRehashPrinterList(
      XPrint::PrintRehashPrinterListRequest{});
}

Future<void> XPrint::CreateContext(
    const XPrint::CreateContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context_id = request.context_id;
  uint32_t printerNameLen{};
  uint32_t localeLen{};
  auto& printerName = request.printerName;
  size_t printerName_len = printerName.size();
  auto& locale = request.locale;
  size_t locale_len = locale.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context_id
  buf.Write(&context_id);

  // printerNameLen
  printerNameLen = printerName.size();
  buf.Write(&printerNameLen);

  // localeLen
  localeLen = locale.size();
  buf.Write(&localeLen);

  // printerName
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(printerNameLen),
                        printerName.size());
  for (auto& printerName_elem : printerName) {
    // printerName_elem
    buf.Write(&printerName_elem);
  }

  // pad0
  Align(&buf, 4);

  // locale
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(localeLen), locale.size());
  for (auto& locale_elem : locale) {
    // locale_elem
    buf.Write(&locale_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::CreateContext", false);
}

Future<void> XPrint::CreateContext(const uint32_t& context_id,
                                   const std::vector<String8>& printerName,
                                   const std::vector<String8>& locale) {
  return XPrint::CreateContext(
      XPrint::CreateContextRequest{context_id, printerName, locale});
}

Future<void> XPrint::PrintSetContext(
    const XPrint::PrintSetContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::PrintSetContext", false);
}

Future<void> XPrint::PrintSetContext(const uint32_t& context) {
  return XPrint::PrintSetContext(XPrint::PrintSetContextRequest{context});
}

Future<XPrint::PrintGetContextReply> XPrint::PrintGetContext(
    const XPrint::PrintGetContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<XPrint::PrintGetContextReply>(
      &buf, "XPrint::PrintGetContext", false);
}

Future<XPrint::PrintGetContextReply> XPrint::PrintGetContext() {
  return XPrint::PrintGetContext(XPrint::PrintGetContextRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XPrint::PrintGetContextReply> detail::ReadReply<
    XPrint::PrintGetContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XPrint::PrintGetContextReply>();

  auto& sequence = (*reply).sequence;
  auto& context = (*reply).context;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // context
  Read(&context, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XPrint::PrintDestroyContext(
    const XPrint::PrintDestroyContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::PrintDestroyContext",
                                        false);
}

Future<void> XPrint::PrintDestroyContext(const uint32_t& context) {
  return XPrint::PrintDestroyContext(
      XPrint::PrintDestroyContextRequest{context});
}

Future<XPrint::PrintGetScreenOfContextReply> XPrint::PrintGetScreenOfContext(
    const XPrint::PrintGetScreenOfContextRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<XPrint::PrintGetScreenOfContextReply>(
      &buf, "XPrint::PrintGetScreenOfContext", false);
}

Future<XPrint::PrintGetScreenOfContextReply> XPrint::PrintGetScreenOfContext() {
  return XPrint::PrintGetScreenOfContext(
      XPrint::PrintGetScreenOfContextRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XPrint::PrintGetScreenOfContextReply> detail::ReadReply<
    XPrint::PrintGetScreenOfContextReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XPrint::PrintGetScreenOfContextReply>();

  auto& sequence = (*reply).sequence;
  auto& root = (*reply).root;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // root
  Read(&root, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XPrint::PrintStartJob(
    const XPrint::PrintStartJobRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& output_mode = request.output_mode;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // output_mode
  buf.Write(&output_mode);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::PrintStartJob", false);
}

Future<void> XPrint::PrintStartJob(const uint8_t& output_mode) {
  return XPrint::PrintStartJob(XPrint::PrintStartJobRequest{output_mode});
}

Future<void> XPrint::PrintEndJob(const XPrint::PrintEndJobRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& cancel = request.cancel;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cancel
  buf.Write(&cancel);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::PrintEndJob", false);
}

Future<void> XPrint::PrintEndJob(const uint8_t& cancel) {
  return XPrint::PrintEndJob(XPrint::PrintEndJobRequest{cancel});
}

Future<void> XPrint::PrintStartDoc(
    const XPrint::PrintStartDocRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& driver_mode = request.driver_mode;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 9;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // driver_mode
  buf.Write(&driver_mode);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::PrintStartDoc", false);
}

Future<void> XPrint::PrintStartDoc(const uint8_t& driver_mode) {
  return XPrint::PrintStartDoc(XPrint::PrintStartDocRequest{driver_mode});
}

Future<void> XPrint::PrintEndDoc(const XPrint::PrintEndDocRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& cancel = request.cancel;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 10;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cancel
  buf.Write(&cancel);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::PrintEndDoc", false);
}

Future<void> XPrint::PrintEndDoc(const uint8_t& cancel) {
  return XPrint::PrintEndDoc(XPrint::PrintEndDocRequest{cancel});
}

Future<void> XPrint::PrintPutDocumentData(
    const XPrint::PrintPutDocumentDataRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  uint32_t len_data{};
  uint16_t len_fmt{};
  uint16_t len_options{};
  auto& data = request.data;
  size_t data_len = data.size();
  auto& doc_format = request.doc_format;
  size_t doc_format_len = doc_format.size();
  auto& options = request.options;
  size_t options_len = options.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // len_data
  len_data = data.size();
  buf.Write(&len_data);

  // len_fmt
  len_fmt = doc_format.size();
  buf.Write(&len_fmt);

  // len_options
  len_options = options.size();
  buf.Write(&len_options);

  // data
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(len_data), data.size());
  for (auto& data_elem : data) {
    // data_elem
    buf.Write(&data_elem);
  }

  // pad0
  Align(&buf, 4);

  // doc_format
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(len_fmt), doc_format.size());
  for (auto& doc_format_elem : doc_format) {
    // doc_format_elem
    buf.Write(&doc_format_elem);
  }

  // pad1
  Align(&buf, 4);

  // options
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(len_options), options.size());
  for (auto& options_elem : options) {
    // options_elem
    buf.Write(&options_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::PrintPutDocumentData",
                                        false);
}

Future<void> XPrint::PrintPutDocumentData(
    const Drawable& drawable,
    const std::vector<uint8_t>& data,
    const std::vector<String8>& doc_format,
    const std::vector<String8>& options) {
  return XPrint::PrintPutDocumentData(
      XPrint::PrintPutDocumentDataRequest{drawable, data, doc_format, options});
}

Future<XPrint::PrintGetDocumentDataReply> XPrint::PrintGetDocumentData(
    const XPrint::PrintGetDocumentDataRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;
  auto& max_bytes = request.max_bytes;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 12;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  // max_bytes
  buf.Write(&max_bytes);

  Align(&buf, 4);

  return connection_->SendRequest<XPrint::PrintGetDocumentDataReply>(
      &buf, "XPrint::PrintGetDocumentData", false);
}

Future<XPrint::PrintGetDocumentDataReply> XPrint::PrintGetDocumentData(
    const PContext& context,
    const uint32_t& max_bytes) {
  return XPrint::PrintGetDocumentData(
      XPrint::PrintGetDocumentDataRequest{context, max_bytes});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XPrint::PrintGetDocumentDataReply> detail::ReadReply<
    XPrint::PrintGetDocumentDataReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XPrint::PrintGetDocumentDataReply>();

  auto& sequence = (*reply).sequence;
  auto& status_code = (*reply).status_code;
  auto& finished_flag = (*reply).finished_flag;
  uint32_t dataLen{};
  auto& data = (*reply).data;
  size_t data_len = data.size();

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // status_code
  Read(&status_code, &buf);

  // finished_flag
  Read(&finished_flag, &buf);

  // dataLen
  Read(&dataLen, &buf);

  // pad1
  Pad(&buf, 12);

  // data
  data.resize(dataLen);
  for (auto& data_elem : data) {
    // data_elem
    Read(&data_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XPrint::PrintStartPage(
    const XPrint::PrintStartPageRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 13;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::PrintStartPage", false);
}

Future<void> XPrint::PrintStartPage(const Window& window) {
  return XPrint::PrintStartPage(XPrint::PrintStartPageRequest{window});
}

Future<void> XPrint::PrintEndPage(const XPrint::PrintEndPageRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& cancel = request.cancel;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 14;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // cancel
  buf.Write(&cancel);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::PrintEndPage", false);
}

Future<void> XPrint::PrintEndPage(const uint8_t& cancel) {
  return XPrint::PrintEndPage(XPrint::PrintEndPageRequest{cancel});
}

Future<void> XPrint::PrintSelectInput(
    const XPrint::PrintSelectInputRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;
  auto& event_mask = request.event_mask;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 15;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  // event_mask
  buf.Write(&event_mask);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::PrintSelectInput",
                                        false);
}

Future<void> XPrint::PrintSelectInput(const PContext& context,
                                      const uint32_t& event_mask) {
  return XPrint::PrintSelectInput(
      XPrint::PrintSelectInputRequest{context, event_mask});
}

Future<XPrint::PrintInputSelectedReply> XPrint::PrintInputSelected(
    const XPrint::PrintInputSelectedRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 16;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<XPrint::PrintInputSelectedReply>(
      &buf, "XPrint::PrintInputSelected", false);
}

Future<XPrint::PrintInputSelectedReply> XPrint::PrintInputSelected(
    const PContext& context) {
  return XPrint::PrintInputSelected(XPrint::PrintInputSelectedRequest{context});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XPrint::PrintInputSelectedReply> detail::ReadReply<
    XPrint::PrintInputSelectedReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XPrint::PrintInputSelectedReply>();

  auto& sequence = (*reply).sequence;
  auto& event_mask = (*reply).event_mask;
  auto& all_events_mask = (*reply).all_events_mask;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_mask
  Read(&event_mask, &buf);

  // all_events_mask
  Read(&all_events_mask, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XPrint::PrintGetAttributesReply> XPrint::PrintGetAttributes(
    const XPrint::PrintGetAttributesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;
  auto& pool = request.pool;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 17;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  // pool
  buf.Write(&pool);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<XPrint::PrintGetAttributesReply>(
      &buf, "XPrint::PrintGetAttributes", false);
}

Future<XPrint::PrintGetAttributesReply> XPrint::PrintGetAttributes(
    const PContext& context,
    const uint8_t& pool) {
  return XPrint::PrintGetAttributes(
      XPrint::PrintGetAttributesRequest{context, pool});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XPrint::PrintGetAttributesReply> detail::ReadReply<
    XPrint::PrintGetAttributesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XPrint::PrintGetAttributesReply>();

  auto& sequence = (*reply).sequence;
  uint32_t stringLen{};
  auto& attributes = (*reply).attributes;
  size_t attributes_len = attributes.size();

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // stringLen
  Read(&stringLen, &buf);

  // pad1
  Pad(&buf, 20);

  // attributes
  attributes.resize(stringLen);
  for (auto& attributes_elem : attributes) {
    // attributes_elem
    Read(&attributes_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XPrint::PrintGetOneAttributesReply> XPrint::PrintGetOneAttributes(
    const XPrint::PrintGetOneAttributesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;
  uint32_t nameLen{};
  auto& pool = request.pool;
  auto& name = request.name;
  size_t name_len = name.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 19;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  // nameLen
  nameLen = name.size();
  buf.Write(&nameLen);

  // pool
  buf.Write(&pool);

  // pad0
  Pad(&buf, 3);

  // name
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(nameLen), name.size());
  for (auto& name_elem : name) {
    // name_elem
    buf.Write(&name_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<XPrint::PrintGetOneAttributesReply>(
      &buf, "XPrint::PrintGetOneAttributes", false);
}

Future<XPrint::PrintGetOneAttributesReply> XPrint::PrintGetOneAttributes(
    const PContext& context,
    const uint8_t& pool,
    const std::vector<String8>& name) {
  return XPrint::PrintGetOneAttributes(
      XPrint::PrintGetOneAttributesRequest{context, pool, name});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XPrint::PrintGetOneAttributesReply> detail::ReadReply<
    XPrint::PrintGetOneAttributesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XPrint::PrintGetOneAttributesReply>();

  auto& sequence = (*reply).sequence;
  uint32_t valueLen{};
  auto& value = (*reply).value;
  size_t value_len = value.size();

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // valueLen
  Read(&valueLen, &buf);

  // pad1
  Pad(&buf, 20);

  // value
  value.resize(valueLen);
  for (auto& value_elem : value) {
    // value_elem
    Read(&value_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> XPrint::PrintSetAttributes(
    const XPrint::PrintSetAttributesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;
  auto& stringLen = request.stringLen;
  auto& pool = request.pool;
  auto& rule = request.rule;
  auto& attributes = request.attributes;
  size_t attributes_len = attributes.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 18;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  // stringLen
  buf.Write(&stringLen);

  // pool
  buf.Write(&pool);

  // rule
  buf.Write(&rule);

  // pad0
  Pad(&buf, 2);

  // attributes
  DUMP_WILL_BE_CHECK_EQ(static_cast<size_t>(attributes_len), attributes.size());
  for (auto& attributes_elem : attributes) {
    // attributes_elem
    buf.Write(&attributes_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "XPrint::PrintSetAttributes",
                                        false);
}

Future<void> XPrint::PrintSetAttributes(
    const PContext& context,
    const uint32_t& stringLen,
    const uint8_t& pool,
    const uint8_t& rule,
    const std::vector<String8>& attributes) {
  return XPrint::PrintSetAttributes(XPrint::PrintSetAttributesRequest{
      context, stringLen, pool, rule, attributes});
}

Future<XPrint::PrintGetPageDimensionsReply> XPrint::PrintGetPageDimensions(
    const XPrint::PrintGetPageDimensionsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 21;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<XPrint::PrintGetPageDimensionsReply>(
      &buf, "XPrint::PrintGetPageDimensions", false);
}

Future<XPrint::PrintGetPageDimensionsReply> XPrint::PrintGetPageDimensions(
    const PContext& context) {
  return XPrint::PrintGetPageDimensions(
      XPrint::PrintGetPageDimensionsRequest{context});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XPrint::PrintGetPageDimensionsReply> detail::ReadReply<
    XPrint::PrintGetPageDimensionsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XPrint::PrintGetPageDimensionsReply>();

  auto& sequence = (*reply).sequence;
  auto& width = (*reply).width;
  auto& height = (*reply).height;
  auto& offset_x = (*reply).offset_x;
  auto& offset_y = (*reply).offset_y;
  auto& reproducible_width = (*reply).reproducible_width;
  auto& reproducible_height = (*reply).reproducible_height;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // width
  Read(&width, &buf);

  // height
  Read(&height, &buf);

  // offset_x
  Read(&offset_x, &buf);

  // offset_y
  Read(&offset_y, &buf);

  // reproducible_width
  Read(&reproducible_width, &buf);

  // reproducible_height
  Read(&reproducible_height, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XPrint::PrintQueryScreensReply> XPrint::PrintQueryScreens(
    const XPrint::PrintQueryScreensRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 22;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<XPrint::PrintQueryScreensReply>(
      &buf, "XPrint::PrintQueryScreens", false);
}

Future<XPrint::PrintQueryScreensReply> XPrint::PrintQueryScreens() {
  return XPrint::PrintQueryScreens(XPrint::PrintQueryScreensRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XPrint::PrintQueryScreensReply> detail::ReadReply<
    XPrint::PrintQueryScreensReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XPrint::PrintQueryScreensReply>();

  auto& sequence = (*reply).sequence;
  uint32_t listCount{};
  auto& roots = (*reply).roots;
  size_t roots_len = roots.size();

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // listCount
  Read(&listCount, &buf);

  // pad1
  Pad(&buf, 20);

  // roots
  roots.resize(listCount);
  for (auto& roots_elem : roots) {
    // roots_elem
    Read(&roots_elem, &buf);
  }

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XPrint::PrintSetImageResolutionReply> XPrint::PrintSetImageResolution(
    const XPrint::PrintSetImageResolutionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;
  auto& image_resolution = request.image_resolution;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 23;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  // image_resolution
  buf.Write(&image_resolution);

  Align(&buf, 4);

  return connection_->SendRequest<XPrint::PrintSetImageResolutionReply>(
      &buf, "XPrint::PrintSetImageResolution", false);
}

Future<XPrint::PrintSetImageResolutionReply> XPrint::PrintSetImageResolution(
    const PContext& context,
    const uint16_t& image_resolution) {
  return XPrint::PrintSetImageResolution(
      XPrint::PrintSetImageResolutionRequest{context, image_resolution});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XPrint::PrintSetImageResolutionReply> detail::ReadReply<
    XPrint::PrintSetImageResolutionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XPrint::PrintSetImageResolutionReply>();

  auto& status = (*reply).status;
  auto& sequence = (*reply).sequence;
  auto& previous_resolutions = (*reply).previous_resolutions;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // status
  Read(&status, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // previous_resolutions
  Read(&previous_resolutions, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<XPrint::PrintGetImageResolutionReply> XPrint::PrintGetImageResolution(
    const XPrint::PrintGetImageResolutionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& context = request.context;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 24;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // context
  buf.Write(&context);

  Align(&buf, 4);

  return connection_->SendRequest<XPrint::PrintGetImageResolutionReply>(
      &buf, "XPrint::PrintGetImageResolution", false);
}

Future<XPrint::PrintGetImageResolutionReply> XPrint::PrintGetImageResolution(
    const PContext& context) {
  return XPrint::PrintGetImageResolution(
      XPrint::PrintGetImageResolutionRequest{context});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<XPrint::PrintGetImageResolutionReply> detail::ReadReply<
    XPrint::PrintGetImageResolutionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<XPrint::PrintGetImageResolutionReply>();

  auto& sequence = (*reply).sequence;
  auto& image_resolution = (*reply).image_resolution;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // image_resolution
  Read(&image_resolution, &buf);

  Align(&buf, 4);
  DUMP_WILL_BE_CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
