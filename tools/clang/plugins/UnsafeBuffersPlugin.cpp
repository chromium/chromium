// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"

#include "Util.h"

namespace chrome_checker {

struct CheckFilePrefixes {
  // Owns the memory holding the strings.
  std::unique_ptr<llvm::MemoryBuffer> buffer;
  // Pointers into the `buffer`, in sorted order.
  std::vector<llvm::StringRef> prefixes;
};

class UnsafeBuffersDiagnosticConsumer : public clang::DiagnosticConsumer {
 public:
  UnsafeBuffersDiagnosticConsumer(clang::DiagnosticsEngine* engine,
                                  clang::DiagnosticConsumer* next,
                                  clang::CompilerInstance* instance,
                                  CheckFilePrefixes check_file_prefixes)
      : engine_(engine),
        next_(next),
        instance_(instance),
        check_file_prefixes_(std::move(check_file_prefixes)) {}
  ~UnsafeBuffersDiagnosticConsumer() override = default;

  void clear() override {
    if (next_) {
      next_->clear();
      NumErrors = next_->getNumErrors();
      NumWarnings = next_->getNumWarnings();
    }
  }

  void BeginSourceFile(const clang::LangOptions& opts,
                       const clang::Preprocessor* pp) override {
    if (next_) {
      next_->BeginSourceFile(opts, pp);
      NumErrors = next_->getNumErrors();
      NumWarnings = next_->getNumWarnings();
    }
  }

  void EndSourceFile() override {
    if (next_) {
      next_->EndSourceFile();
      NumErrors = next_->getNumErrors();
      NumWarnings = next_->getNumWarnings();
    }
  }

  void finish() override {
    if (next_) {
      next_->finish();
      NumErrors = next_->getNumErrors();
      NumWarnings = next_->getNumWarnings();
    }
  }

  bool IncludeInDiagnosticCounts() const override {
    return next_ && next_->IncludeInDiagnosticCounts();
  }

  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                        const clang::Diagnostic& diag) override {
    const unsigned diag_id = diag.getID();

    if (inside_handle_diagnostic_) {
      // Avoid handling the diagnostics which we emit in here.
      return PassthroughDiagnostic(level, diag);
    }

    // The `-Runsafe-buffer-usage-in-container` warning gets enabled along with
    // `-Runsafe-buffer-usage`, but it's a hardcoded warning about std::span
    // constructor. We don't want to emit these, we instead want the span ctor
    // (and our own base::span ctor) to be marked [[clang::unsafe_buffer_usage]]
    // and have that work: https://github.com/llvm/llvm-project/issues/80482
    if (diag_id == clang::diag::warn_unsafe_buffer_usage_in_container) {
      return;
    }

    if (!(diag_id == clang::diag::warn_unsafe_buffer_variable ||
          diag_id == clang::diag::warn_unsafe_buffer_operation ||
          diag_id == clang::diag::note_unsafe_buffer_operation ||
          diag_id == clang::diag::note_unsafe_buffer_variable_fixit_group ||
          diag_id == clang::diag::note_unsafe_buffer_variable_fixit_together ||
          diag_id == clang::diag::note_safe_buffer_usage_suggestions_disabled ||
          diag_id == clang::diag::note_safe_buffer_debug_mode)) {
      return PassthroughDiagnostic(level, diag);
    }

    // Note that we promote from Remark directly to Error, rather than to
    // Warning, as -Werror will not get applied to whatever we choose here.
    const auto elevated_level =
        (diag_id == clang::diag::warn_unsafe_buffer_variable ||
         diag_id == clang::diag::warn_unsafe_buffer_operation)
            ? (engine_->getWarningsAsErrors()
                   ? clang::DiagnosticsEngine::Level::Error
                   : clang::DiagnosticsEngine::Level::Warning)
            : clang::DiagnosticsEngine::Level::Note;

    const clang::SourceManager& sm = instance_->getSourceManager();
    const clang::SourceLocation loc = diag.getLocation();

    // -Wunsage-buffer-usage errors are omitted conditionally based on what file
    // they are coming from.
    if (FileHasSafeBuffersWarnings(sm, loc)) {
      // Elevate the Remark to a Warning, and pass along its Notes without
      // changing them. Otherwise, do nothing, and the Remark (and its notes)
      // will not be displayed.
      //
      // We don't count warnings/errors in this DiagnosticConsumer, so we don't
      // call up to the base class here. Instead, whenever we pass through to
      // the `next_` DiagnosticConsumer, we record its counts.
      //
      // Construct the StoredDiagnostic before Clear() or we get bad data from
      // `diag`.
      auto stored = clang::StoredDiagnostic(elevated_level, diag);
      engine_->Clear();
      inside_handle_diagnostic_ = true;
      engine_->Report(stored);
      inside_handle_diagnostic_ = false;
    }
  }

 private:
  void PassthroughDiagnostic(clang::DiagnosticsEngine::Level level,
                             const clang::Diagnostic& diag) {
    if (next_) {
      next_->HandleDiagnostic(level, diag);
      NumErrors = next_->getNumErrors();
      NumWarnings = next_->getNumWarnings();
    }
  }

  // Depending on where the diagnostic is coming from, we may ignore it or
  // cause it to generate a warning.
  bool FileHasSafeBuffersWarnings(const clang::SourceManager& sm,
                                  clang::SourceLocation loc) {
    // TODO(crbug.com/40284755): Expand this diagnostic to more code. It should
    // include everything except kThirdParty and kSystem eventually.
    LocationClassification loc_class = ClassifySourceLocation(sm, loc);
    switch (loc_class) {
      case LocationClassification::kThirdParty:
        return false;
      case LocationClassification::kSystem:
        return false;
      case LocationClassification::kGenerated:
        return false;
      case LocationClassification::kChromiumThirdParty:
        return false;
      case LocationClassification::kMacro:
        break;
      case LocationClassification::kFirstParty:
        break;
      case LocationClassification::kBlink:
        break;
    }

    // TODO(crbug.com/40284755): Currently we default to everything being
    // known-bad except for a list of clean files. Eventually this should become
    // default known-good with a list of bad files (which should become empty in
    // time).
    //
    // TODO(danakj): It would be an optimization to find a way to avoid creating
    // a std::string here.
    std::string filename = GetFilename(sm, loc);

    // Avoid searching `check_file_prefixes_` more than once for a file.
    auto cache_it = checked_files_cache_.find(filename);
    if (cache_it != checked_files_cache_.end()) {
      return cache_it->second;
    }

    // Drop the ../ prefixes.
    llvm::StringRef cmp_filename = filename;
    while (cmp_filename.consume_front("./") ||
           cmp_filename.consume_front("../"))
      ;
    if (cmp_filename.empty()) {
      return false;
    }

    // Look for prefix match (whether any of `check_file_prefixes_` is a prefix
    // of the filename).
    if (!check_file_prefixes_.prefixes.empty()) {
      const auto begin = check_file_prefixes_.prefixes.begin();
      const auto end = check_file_prefixes_.prefixes.end();
      auto it = std::upper_bound(begin, end, cmp_filename);
      if (it != begin) {
        --it;  // Now `it` will be either the exact or prefix match.
        if (*it == cmp_filename.take_front(it->size())) {
          checked_files_cache_.insert({filename, true});
          return true;
        }
      }
    }
    checked_files_cache_.insert({filename, false});
    return false;
  }

  // Used to prevent recursing into HandleDiagnostic() when we're emitting a
  // diagnostic from that function.
  bool inside_handle_diagnostic_ = false;
  clang::DiagnosticsEngine* engine_;
  clang::DiagnosticConsumer* next_;
  clang::CompilerInstance* instance_;
  CheckFilePrefixes check_file_prefixes_;
  // Stores `true` if the filename (key) matches against the
  // check_file_prefixes_, and `false` if it does not. Used as a shortcut to
  // avoid looking through `check_file_prefixes_` for any file in this map.
  //
  // TODO(danakj): Another form of optimization here would be to replace this
  // and the `check_file_prefixes_` vector with a string-prefix-matching data
  // structure.
  llvm::StringMap<bool> checked_files_cache_;
};

class UnsafeBuffersASTConsumer : public clang::ASTConsumer {
 public:
  UnsafeBuffersASTConsumer(clang::CompilerInstance* instance,
                           CheckFilePrefixes check_file_prefixes)
      : instance_(instance) {
    // Replace the DiagnosticConsumer with our own that sniffs diagnostics and
    // can omit them.
    clang::DiagnosticsEngine& engine = instance_->getDiagnostics();
    old_client_ = engine.getClient();
    old_owned_client_ = engine.takeClient();
    engine.setClient(
        new UnsafeBuffersDiagnosticConsumer(&engine, old_client_, instance_,
                                            std::move(check_file_prefixes)),
        /*owned=*/true);

    // Enable the -Wunsafe-buffer-usage warning as a remark. This prevents it
    // from stopping compilation, even with -Werror. If we see the remark go by,
    // we can re-emit it as a warning for the files we want to include in the
    // check.
    engine.setSeverityForGroup(clang::diag::Flavor::WarningOrError,
                               "unsafe-buffer-usage",
                               clang::diag::Severity::Remark);
  }

  ~UnsafeBuffersASTConsumer() {
    // Restore the original DiagnosticConsumer that we replaced with our own.
    clang::DiagnosticsEngine& engine = instance_->getDiagnostics();
    if (old_owned_client_) {
      engine.setClient(old_owned_client_.release(),
                       /*owned=*/true);
    } else {
      engine.setClient(old_client_, /*owned=*/false);
    }
  }

 private:
  clang::CompilerInstance* instance_;
  clang::DiagnosticConsumer* old_client_;
  std::unique_ptr<clang::DiagnosticConsumer> old_owned_client_;
};

class UnsafeBuffersASTAction : public clang::PluginASTAction {
 public:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& instance,
      llvm::StringRef ref) override {
    assert(!moved_prefixes_);  // This would mean we move the prefixes twice.
    moved_prefixes_ = true;

    // The ASTConsumer can outlive `this`, so we can't give it references to
    // members here and must move the `check_file_prefixes_` vector instead.
    return std::make_unique<UnsafeBuffersASTConsumer>(
        &instance, std::move(check_file_prefixes_));
  }

  bool ParseArgs(const clang::CompilerInstance& instance,
                 const std::vector<std::string>& args) override {
    bool found_file_arg = false;
    for (size_t i = 0u; i < args.size(); ++i) {
      // Look for any switches first (there are currently none).

      if (found_file_arg) {
        llvm::errs()
            << "[unsafe-buffers] Extra argument to unsafe-buffers plugin: '"
            << args[i] << ". Usage: [SWITCHES] PATH_TO_CHECK_FILE'\n";
        return false;
      } else {
        found_file_arg = true;
        if (!LoadCheckFilePrefixes(args[i])) {
          llvm::errs() << "[unsafe-buffers] Failed to load paths from file '"
                       << args[i] << "'\n";
        }
      }
    }
    return true;
  }

  bool LoadCheckFilePrefixes(std::string_view path) {
    if (auto buffer = llvm::MemoryBuffer::getFileAsStream(path)) {
      check_file_prefixes_.buffer = std::move(buffer.get());
    } else {
      llvm::errs() << "[unsafe-buffers] Error reading file: '"
                   << buffer.getError().message() << "'\n";
      return false;
    }

    // Parse out the paths into `check_file_prefixes_.prefixes`.
    //
    // The file format is as follows:
    // * Lines that begin with `#` are comments are are ignored.
    // * Empty lines are ignored.
    // * Every other line is a path prefix from the source tree root using
    //   unix-style delimiters.
    //   * For instance `a/b` will match the file at `//a/b/c.h` but will *not*
    //     match `//other/a/b/c.h`.
    // * Exact file paths look like `a/b/c.h` and directory prefixes should end
    //   with a `/` such as `a/b/`.
    //
    // Example:
    // ```
    // # A file of path prefixes.
    // # Matches anything under the directory //foo/bar.
    // foo/bar/
    // # Matches a specific file at //my/file.cc.
    // my/file.cc

    llvm::StringRef string = check_file_prefixes_.buffer->getBuffer();
    while (!string.empty()) {
      auto [lhs, rhs] = string.split('\n');
      string = rhs;
      bool keep_lhs = false;
      for (char c : lhs) {
        if (c != ' ' && c != '#') {
          keep_lhs = true;
          break;
        }
      }
      if (keep_lhs) {
        check_file_prefixes_.prefixes.push_back(lhs);
      }
    }

    // TODO(danakj): Use std::ranges::sort when Clang is build with C++20.
    std::sort(check_file_prefixes_.prefixes.begin(),
              check_file_prefixes_.prefixes.end());
    return true;
  }

 private:
  CheckFilePrefixes check_file_prefixes_;
  bool moved_prefixes_ = false;
};

static clang::FrontendPluginRegistry::Add<UnsafeBuffersASTAction> X(
    "unsafe-buffers",
    "Enforces -Wunsafe-buffer-usage during incremental rollout");

}  // namespace chrome_checker
