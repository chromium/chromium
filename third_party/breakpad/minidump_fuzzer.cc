// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <istream>
#include <limits>
#include <memory>
#include <streambuf>

#include "base/memory/free_deleter.h"
#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/minidump.h"
#include "google_breakpad/processor/minidump_processor.h"
#include "google_breakpad/processor/process_state.h"
#include "processor/logging.h"
#include "processor/simple_symbol_supplier.h"
#include "processor/stackwalk_common.h"

namespace {

using google_breakpad::BasicSourceLineResolver;
using google_breakpad::Minidump;
using google_breakpad::MinidumpProcessor;
using google_breakpad::ProcessState;
using google_breakpad::SimpleSymbolSupplier;

struct membuf : std::streambuf {
  membuf(char* begin, char* end) { setg(begin, begin, end); }

 protected:
  virtual pos_type seekoff(off_type off,
                           std::ios_base::seekdir dir,
                           std::ios_base::openmode which = std::ios_base::in) {
    if (dir == std::ios_base::cur)
      gbump(off);
    return gptr() - eback();
  }
};

bool PrintMinidumpProcess(const uint8_t* data,
                          size_t size,
                          const std::vector<string>& symbol_paths) {
  // Signature and version number.
  static const uint8_t kHeaderPrefix[] = {'P', 'M', 'D', 'M', 0, 0, 0xa7, 0x93};
  if (size > std::numeric_limits<size_t>::max() - sizeof(kHeaderPrefix))
    return false;

  size += sizeof(kHeaderPrefix);
  std::unique_ptr<char, base::FreeDeleter> buffer(
      static_cast<char*>(malloc(size)));
  if (!buffer)
    return false;

  memcpy(buffer.get(), kHeaderPrefix, sizeof(kHeaderPrefix));
  memcpy(buffer.get() + sizeof(kHeaderPrefix), data,
         size - sizeof(kHeaderPrefix));

  membuf sbuf(buffer.get(), buffer.get() + size);
  std::istream input(&sbuf);

  std::unique_ptr<SimpleSymbolSupplier> symbol_supplier;
  if (!symbol_paths.empty())
    symbol_supplier = std::make_unique<SimpleSymbolSupplier>(symbol_paths);

  BasicSourceLineResolver resolver;
  MinidumpProcessor minidump_processor(symbol_supplier.get(), &resolver);

  // Process the minidump.
  Minidump dump(input);
  if (!dump.Read()) {
    BPLOG(ERROR) << "Minidump " << dump.path() << " could not be read";
    return false;
  }
  ProcessState process_state;
  if (minidump_processor.Process(&dump, &process_state) !=
      google_breakpad::PROCESS_OK) {
    BPLOG(ERROR) << "MinidumpProcessor::Process failed";
    return false;
  }

  PrintProcessStateMachineReadable(process_state);

  return true;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // TODO(wfh): Somehow pull symbols in.
  PrintMinidumpProcess(data, size, std::vector<string>());
  return 0;
}
