// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Util.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Pragma.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"

namespace chrome_checker {

enum Disposition {
  kSkip = 0,  // Do not check for any unsafe operations.
  kSkipLibc,  // Check for unsafe buffers but not unsafe libc calls.
  kCheck,     // Check for both unsafe buffers and unsafe libc calls.
};

// Stores whether the filename (key) should be checked for errors.
// If the filename is not present, the choice is up to the plugin to
// determine from the path prefixes control file.
llvm::StringMap<Disposition> g_checked_files_cache;

struct CheckFilePrefixes {
  // `buffer` owns the memory for the strings in `prefix_map`.
  std::unique_ptr<llvm::MemoryBuffer> buffer;
  std::map<llvm::StringRef, char> prefix_map;
  bool check_buffers = true;
  bool check_libc_calls = false;
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
        check_file_prefixes_(std::move(check_file_prefixes)),
        diag_note_link_(engine_->getCustomDiagID(
            clang::DiagnosticsEngine::Level::Note,
            "See //docs/unsafe_buffers.md for help.")) {}
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

    // Drop the note saying "pass -fsafe-buffer-usage-suggestions to receive
    // code hardening suggestions" since that's not simple for Chrome devs to
    // do anyway. We can provide a GN variable in the future and point to that
    // if needed, or just turn it on always in this plugin, if desired.
    if (diag_id == clang::diag::note_safe_buffer_usage_suggestions_disabled) {
      return;
    }

    const bool is_buffers_diagnostic =
        diag_id == clang::diag::warn_unsafe_buffer_variable ||
        diag_id == clang::diag::warn_unsafe_buffer_operation ||
        diag_id == clang::diag::note_unsafe_buffer_operation ||
        diag_id == clang::diag::note_unsafe_buffer_variable_fixit_group ||
        diag_id == clang::diag::note_unsafe_buffer_variable_fixit_together ||
        diag_id == clang::diag::note_safe_buffer_debug_mode;

    const bool is_libc_diagnostic =
        diag_id == clang::diag::warn_unsafe_buffer_libc_call ||
        diag_id == clang::diag::note_unsafe_buffer_printf_call;

    const bool ignore_diagnostic =
        (is_buffers_diagnostic && !check_file_prefixes_.check_buffers) ||
        (is_libc_diagnostic && !check_file_prefixes_.check_libc_calls);

    if (ignore_diagnostic) {
      return;
    }

    const bool handle_diagnostic =
        (is_buffers_diagnostic && check_file_prefixes_.check_buffers) ||
        (is_libc_diagnostic && check_file_prefixes_.check_libc_calls);

    if (!handle_diagnostic) {
      return PassthroughDiagnostic(level, diag);
    }

    // Note that we promote from Remark directly to Error, rather than to
    // Warning, as -Werror will not get applied to whatever we choose here.
    const auto elevated_level =
        (is_libc_diagnostic ||
         diag_id == clang::diag::warn_unsafe_buffer_variable ||
         diag_id == clang::diag::warn_unsafe_buffer_operation)
            ? (engine_->getWarningsAsErrors()
                   ? clang::DiagnosticsEngine::Level::Error
                   : clang::DiagnosticsEngine::Level::Warning)
            : clang::DiagnosticsEngine::Level::Note;

    const clang::SourceManager& sm = instance_->getSourceManager();
    const clang::SourceLocation loc = diag.getLocation();

    // -Wunsage-buffer-usage errors are omitted conditionally based on what file
    // they are coming from.
    auto disposition = FileHasSafeBuffersWarnings(sm, loc);
    if (disposition == kSkip ||
        (is_libc_diagnostic && disposition == kSkipLibc)) {
      return;
    }

    // More selectively filter the libc calls we enforce.
    if (is_libc_diagnostic && IsIgnoredLibcFunction(diag)) {
      return;
    }

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
    inside_handle_diagnostic_ = true;
    engine_->Report(stored);
    if (elevated_level != clang::DiagnosticsEngine::Level::Note) {
      // For each warning, we inject our own Note as well, pointing to docs.
      engine_->Report(loc, diag_note_link_);
    }
    inside_handle_diagnostic_ = false;
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
  Disposition FileHasSafeBuffersWarnings(const clang::SourceManager& sm,
                                         clang::SourceLocation loc) {
    // ClassifySourceLocation() does not report kMacro as the location unless it
    // happens to be inside a scratch buffer, which not all macro use does. For
    // the unsafe-buffers warning, we want the SourceLocation where the macro is
    // expanded to always be the decider about whether to fire a warning or not.
    //
    // The reason we do this is that the expansion site should be wrapped in
    // UNSAFE_BUFFERS() if the unsafety is warranted. It can be done inside the
    // macro itself too (in which case the warning will not fire), but the
    // finest control is always at each expansion site.
    while (loc.isMacroID()) {
      loc = sm.getExpansionLoc(loc);
    }

    // TODO(crbug.com/40284755): Expand this diagnostic to more code. It should
    // include everything except kSystem eventually.
    LocationClassification loc_class =
        ClassifySourceLocation(instance_->getHeaderSearchOpts(), sm, loc);
    switch (loc_class) {
      case LocationClassification::kSystem:
        return kSkip;
      case LocationClassification::kGenerated:
        return kSkip;
      case LocationClassification::kThirdParty:
      case LocationClassification::kChromiumThirdParty:
      case LocationClassification::kFirstParty:
      case LocationClassification::kBlink:
      case LocationClassification::kMacro:
        break;
    }

    // We default to everything opting into checks (except categories that early
    // out above) unless it is removed by the paths control file or by pragma.

    // TODO(danakj): It would be an optimization to find a way to avoid creating
    // a std::string here.
    std::string filename = GetFilename(sm, loc, FilenameLocationType::kExactLoc,
                                       FilenamesFollowPresumed::kNo);

    // Avoid searching `check_file_prefixes_` more than once for a file.
    auto cache_it = g_checked_files_cache.find(filename);
    if (cache_it != g_checked_files_cache.end()) {
      return cache_it->second;
    }

    llvm::StringRef cmp_filename = filename;

    // If the path is absolute, drop the prefix up to the current working
    // directory. Some mac machines are passing absolute paths to source files,
    // but it's the absolute path to the build directory (the current working
    // directory here) then a relative path from there.
    llvm::SmallVector<char> cwd;
    if (llvm::sys::fs::current_path(cwd).value() == 0) {
      if (cmp_filename.consume_front(llvm::StringRef(cwd.data(), cwd.size()))) {
        cmp_filename.consume_front("/");
      }
    }

    // Drop the ../ prefixes.
    while (cmp_filename.consume_front("./") ||
           cmp_filename.consume_front("../"))
      continue;

    Disposition should_check = kCheck;
    while (!cmp_filename.empty()) {
      auto it = check_file_prefixes_.prefix_map.find(cmp_filename);
      if (it != check_file_prefixes_.prefix_map.end()) {
        should_check = it->second == '+' ? kCheck : kSkip;
        break;
      }
      cmp_filename = llvm::sys::path::parent_path(cmp_filename);
    }
    g_checked_files_cache.insert({filename, should_check});
    return should_check;
  }

  bool IsIgnoredLibcFunction(const clang::Diagnostic& diag) const {
    // The unsafe libc calls warning is a wee bit overzealous about
    // functions which might result in a OOB read only.
    if (diag.getNumArgs() < 1) {
      return false;
    }
    if (diag.getArgKind(0) !=
        clang::DiagnosticsEngine::ArgumentKind::ak_nameddecl) {
      return false;
    }
    auto* decl = reinterpret_cast<clang::NamedDecl*>(diag.getRawArg(0));
    llvm::StringRef name = decl->getName();
    return name == "strlen" || name == "wcslen" || name == "atoi" ||
           name == "atof";
  }

  // Used to prevent recursing into HandleDiagnostic() when we're emitting a
  // diagnostic from that function.
  bool inside_handle_diagnostic_ = false;
  clang::DiagnosticsEngine* engine_;
  clang::DiagnosticConsumer* next_;
  clang::CompilerInstance* instance_;
  CheckFilePrefixes check_file_prefixes_;
  unsigned diag_note_link_;
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

    // Enable the -Wunsafe-buffer-usage-in-libc-call warning as a remark. This
    // prevents it from stopping compilation, even with -Werror. If we see the
    // remark go by, we can re-emit it as a warning for the files we want to
    // include in the check.
    engine.setSeverityForGroup(clang::diag::Flavor::WarningOrError,
                               "unsafe-buffer-usage-in-libc-call",
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
    for (const auto& arg : args) {
      // Nothing should follow the unsafe buffers path positional argument.
      if (found_file_arg) {
        llvm::errs()
            << "[unsafe-buffers] Extra argument to unsafe-buffers plugin: '"
            << arg << ". Usage: [SWITCHES] PATH_TO_CHECK_FILE'\n";
        return false;
      }

      // Switches, if any, would go here.

      // Anything not recognized as a switch is the unsafe buffer paths file.
      found_file_arg = true;
      if (!LoadCheckFilePrefixes(arg)) {
        llvm::errs() << "[unsafe-buffers] Failed to load paths from file '"
                     << arg << "'\n";
        return false;
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

    // Parse out the paths into `check_file_prefixes_`.
    //
    // The file format is as follows:
    // * `#` introduces a comment until the end of the line.
    // * Empty lines are ignored.
    // * A line beginning with a `.` lists diagnostics to enable. These
    //   are comma-separated and currently allow: `buffers`, `libc`.
    // * Every other line is a path prefix from the source tree root using
    //   unix-style delimiters.
    //   * Each line either removes a path from checks or adds a path to checks.
    //   * If the line starts with `+` paths matching the line will be added.
    //   * If the line starts with `-` paths matching the line will removed.
    //   * Other starting characters are not allowed.
    //   * Paths naming directories match the entire sub-directory. For instance
    //     `+a/b/` will match the file at `//a/b/c.h` but will *not* match
    //     `//other/a/b/c.h`.
    //   * Paths naming files match the single file and look like `+a/b/c.h`.
    //   * Trailing slashes for directories are recommended, but not enforced.
    // * The longest (most specific) match takes precedence.
    // * Files that do not match any of the prefixes file will be checked.
    // * Duplicate entries are not allowed and produce compilation errors.
    //
    // Example:
    // ```
    // # A file of path prefixes.
    // # Matches anything under the directory //foo/bar, opting them into
    // # checks.
    // +foo/bar/
    // # Avoids checks in the //my directory.
    // -my/
    // # Matches a specific file at //my/file.cc, overriding the `-my/` above
    // # for this one file.
    // +my/file.cc
    //
    llvm::StringRef string = check_file_prefixes_.buffer->getBuffer();
    while (!string.empty()) {
      auto [line, remainder] = string.split('\n');
      string = remainder;
      auto [active, comment] = line.split('#');
      active = active.trim();
      if (active.empty()) {
        continue;
      }
      char symbol = active[0u];
      if (symbol == '.') {
        // A "dot" line contains directives to enable.
        if (active.contains("buffers")) {
          check_file_prefixes_.check_buffers = true;
        }
        if (active.contains("libc")) {
          check_file_prefixes_.check_libc_calls = true;
        }
        continue;
      }
      if (symbol != '+' && symbol != '-') {
        llvm::errs() << "[unsafe-buffers] Invalid line in paths file, must "
                     << "start with +/-: '" << line << "'\n";
        return false;
      }
      llvm::StringRef prefix = active.substr(1u).rtrim('/');
      if (prefix.empty()) {
        llvm::errs() << "[unsafe-buffers] Invalid line in paths file, path "
                     << "must immediately follow +/-: '" << line << "'\n";
        return false;
      }
      auto [ignore, was_inserted] =
          check_file_prefixes_.prefix_map.insert({prefix, symbol});
      if (!was_inserted) {
        llvm::errs() << "[unsafe-buffers] Duplicate entry in paths file "
                        "for '"
                     << line << "'\n";
        return false;
      }
    }
    return true;
  }

 private:
  CheckFilePrefixes check_file_prefixes_;
  bool moved_prefixes_ = false;
};

class AllowUnsafeBuffersPragmaHandler : public clang::PragmaHandler {
 public:
  static constexpr char kName[] = "allow_unsafe_buffers";

  AllowUnsafeBuffersPragmaHandler() : clang::PragmaHandler(kName) {}

  void HandlePragma(clang::Preprocessor& preprocessor,
                    clang::PragmaIntroducer introducer,
                    clang::Token& token) override {
    // TODO(danakj): It would be an optimization to find a way to avoid creating
    // a std::string here.
    std::string filename =
        GetFilename(preprocessor.getSourceManager(), introducer.Loc,
                    FilenameLocationType::kExpansionLoc);
    // The pragma opts the file out of checks.
    g_checked_files_cache.insert({filename, kSkip});
  }
};

class AllowUnsafeLibcPragmaHandler : public clang::PragmaHandler {
 public:
  static constexpr char kName[] = "allow_unsafe_libc_calls";

  AllowUnsafeLibcPragmaHandler() : clang::PragmaHandler(kName) {}

  void HandlePragma(clang::Preprocessor& preprocessor,
                    clang::PragmaIntroducer introducer,
                    clang::Token& token) override {
    // TODO(danakj): It would be an optimization to find a way to avoid creating
    // a std::string here.
    std::string filename =
        GetFilename(preprocessor.getSourceManager(), introducer.Loc,
                    FilenameLocationType::kExpansionLoc);
    // The pragma opts the file into checks.
    g_checked_files_cache.insert({filename, kSkipLibc});
  }
};

static clang::FrontendPluginRegistry::Add<UnsafeBuffersASTAction> X1(
    "unsafe-buffers",
    "Enforces -Wunsafe-buffer-usage during incremental rollout");

static clang::PragmaHandlerRegistry::Add<AllowUnsafeBuffersPragmaHandler> X2(
    AllowUnsafeBuffersPragmaHandler::kName,
    "Avoid reporting unsafe-buffer-usage warnings in the file");

static clang::PragmaHandlerRegistry::Add<AllowUnsafeLibcPragmaHandler> X3(
    AllowUnsafeLibcPragmaHandler::kName,
    "Avoid reporting unsafe-libc-call warnings in the file");

}  // namespace chrome_checker
