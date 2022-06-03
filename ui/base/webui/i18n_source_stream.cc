// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/webui/i18n_source_stream.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "net/base/io_buffer.h"

namespace ui {

I18nSourceStream::~I18nSourceStream() {}

std::unique_ptr<I18nSourceStream> I18nSourceStream::Create(
    std::unique_ptr<SourceStream> upstream,
    SourceStream::SourceType type,
    const TemplateReplacements* replacements) {
  DCHECK(replacements);
  std::unique_ptr<I18nSourceStream> source(
      new I18nSourceStream(std::move(upstream), type, replacements));
  return source;
}

I18nSourceStream::I18nSourceStream(std::unique_ptr<SourceStream> upstream,
                                   SourceStream::SourceType type,
                                   const TemplateReplacements* replacements)
    : FilterSourceStream(type, std::move(upstream)),
      replacements_(replacements) {}

std::string I18nSourceStream::GetTypeAsString() const {
  return "i18n";
}

int I18nSourceStream::FilterData(net::IOBuffer* output_buffer,
                                 int output_buffer_size,
                                 net::IOBuffer* input_buffer,
                                 int input_buffer_size,
                                 int* consumed_bytes,
                                 bool upstream_end_reached) {
  // |input_| is often empty (or it may have something from the prior call).
  input_.append(input_buffer->data(), input_buffer_size);
  *consumed_bytes = input_buffer_size;

  // The replacement tag starts with '$' and ends with '}'. The white-space
  // characters are an optimization that looks for characters that are invalid
  // within $i18n{} tags.
  size_t pos = input_.find_last_of("$} \t\r\n");
  std::string to_process;
  if (!upstream_end_reached && pos != std::string::npos && input_[pos] == '$') {
    // If there is a trailing '$' then split the |input_| at that point. Process
    // the first part; save the second part for the next call to FilterData().
    to_process.assign(input_, 0, pos);
    input_.erase(0, pos);
  } else {
    // There is no risk of a split key, process the whole input.
    to_process.swap(input_);
  }

  output_.append(ReplaceTemplateExpressions(to_process, *replacements_));
  int bytes_out =
      std::min(output_.size(), static_cast<size_t>(output_buffer_size));
  output_.copy(output_buffer->data(), bytes_out);
  output_.erase(0, bytes_out);
  return bytes_out;
}

}  // namespace ui
