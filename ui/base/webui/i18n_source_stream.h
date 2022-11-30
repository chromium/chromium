// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WEBUI_I18N_SOURCE_STREAM_H_
#define UI_BASE_WEBUI_I18N_SOURCE_STREAM_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "net/filter/filter_source_stream.h"
#include "ui/base/template_expressions.h"

namespace ui {

class COMPONENT_EXPORT(UI_BASE) I18nSourceStream
    : public net::FilterSourceStream {
 public:
  I18nSourceStream(const I18nSourceStream&) = delete;
  I18nSourceStream& operator=(const I18nSourceStream&) = delete;

  ~I18nSourceStream() override;

  // Factory function to create an I18nSourceStream.
  // |replacements| is a dictionary of i18n replacements.
  static std::unique_ptr<I18nSourceStream> Create(
      std::unique_ptr<SourceStream> previous,
      SourceStream::SourceType type,
      const ui::TemplateReplacements* replacements);

 private:
  I18nSourceStream(std::unique_ptr<SourceStream> previous,
                   SourceStream::SourceType type,
                   const TemplateReplacements* replacements);

  // SourceStream implementation.
  std::string GetTypeAsString() const override;
  base::expected<size_t, net::Error> FilterData(
      net::IOBuffer* output_buffer,
      size_t output_buffer_size,
      net::IOBuffer* input_buffer,
      size_t input_buffer_size,
      size_t* consumed_bytes,
      bool upstream_end_reached) override;

  // Keep split $i18n tags (wait for the whole tag). This is expected to vary
  // in size from 0 to a few KB and should never be larger than the input file
  // (in the worst case).
  std::string input_;

  // Keep excess that didn't fit in the output buffer. This is expected to vary
  // in size from 0 to a few KB and should never get much larger than the input
  // file (in the worst case).
  std::string output_;

  // A map of i18n replacement keys and translations.
  raw_ptr<const TemplateReplacements> replacements_;
};

}  // namespace ui

#endif  // UI_BASE_WEBUI_I18N_SOURCE_STREAM_H_
