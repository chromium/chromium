// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OutputHelper.h"

void OutputHelper::Delete(const clang::CharSourceRange& replacement_range,
                          const clang::SourceManager& source_manager,
                          const clang::LangOptions& lang_opts) {
  Replace(replacement_range, "", source_manager, lang_opts);
}

// Replaces `replacement_range` with `replacement_text`.
void OutputHelper::Replace(const clang::CharSourceRange& replacement_range,
                           std::string replacement_text,
                           const clang::SourceManager& source_manager,
                           const clang::LangOptions& lang_opts) {
  clang::tooling::Replacement replacement(source_manager, replacement_range,
                                          std::move(replacement_text),
                                          lang_opts);

  llvm::StringRef file_path = replacement.getFilePath();
  if (file_path.empty()) {
    return;
  }

  PrintReplacement(file_path, replacement.getOffset(), replacement.getLength(),
                   replacement.getReplacementText());
}
// Inserts `lhs` and `rhs` to the left and right of `replacement_range`.
void OutputHelper::Wrap(const clang::CharSourceRange& replacement_range,
                        std::string_view lhs,
                        std::string_view rhs,
                        const clang::SourceManager& source_manager,
                        const clang::LangOptions& lang_opts) {
  clang::tooling::Replacement replacement(source_manager, replacement_range, "",
                                          lang_opts);

  llvm::StringRef file_path = replacement.getFilePath();
  if (file_path.empty()) {
    return;
  }

  PrintReplacement(file_path, replacement.getOffset(), 0, lhs);
  PrintReplacement(file_path, replacement.getOffset() + replacement.getLength(),
                   0, rhs);
}

// This is run automatically when the tool is first invoked
bool OutputHelper::handleBeginSource(clang::CompilerInstance& compiler) {
  const clang::FrontendOptions& frontend_options = compiler.getFrontendOpts();

  // Validate our expectations about how this tool should be used
  assert((frontend_options.Inputs.size() == 1) &&
         "run_tool.py should invoke the rewriter one file at a time");
  const clang::FrontendInputFile& input_file = frontend_options.Inputs[0];
  assert(input_file.isFile() &&
         "run_tool.py should invoke the rewriter on actual files");

  current_language_ = input_file.getKind().getLanguage();

  // Report that we succeeded
  return true;
}

// This is run automatically at the end of the file.
void OutputHelper::handleEndSource() {
  for (auto& file : files_replaced_in_) {
    for (auto& header : headers_to_add_) {
      llvm::outs() << "include-user-header:::" << file.getKey()
                   << ":::-1:::-1:::" << header.getKey() << "\n";
    }
  }
}
