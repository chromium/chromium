// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

#include "DiagnosticConsumer.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticLex.h"
#include "clang/Basic/FileEntry.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/SaveAndRestore.h"
#include "llvm/Support/raw_ostream.h"

namespace chrome_checker {

namespace {
// NOTE: All diagnostics here require string arguments.
// If you want to add a diagnostic that takes a non-string argument, you will
// have to change HandleDiagnostic to not use ArgsFromDiagnostic().
static constexpr std::array kSupportedDiagnostics = {
    clang::diag::err_undeclared_use_of_module,
    clang::diag::err_undeclared_use_of_module_indirect,
    clang::diag::warn_use_of_private_header_outside_module};

std::vector<std::string> ArgsFromDiagnostic(
    const clang::Diagnostic& diagnostic) {
  std::vector<std::string> args;
  for (unsigned i = 0; i < diagnostic.getNumArgs(); ++i) {
    args.push_back(diagnostic.getArgStdStr(i));
  }
  return args;
}

std::optional<std::string> SourceModule(const clang::Diagnostic& diagnostic) {
  switch (diagnostic.getID()) {
    case clang::diag::err_undeclared_use_of_module:
    case clang::diag::err_undeclared_use_of_module_indirect:
      return diagnostic.getArgStdStr(0);
    default:
      return std::nullopt;
  }
}

struct IncludeRequest {
  std::string includer;
  std::string included;

  // Clang itself still uses C++17, so we cannot use C++20 default comparison
  // operators (e.g. operator<=>) here.
  bool operator<(const IncludeRequest& other) const {
    return std::tie(includer, included) <
           std::tie(other.includer, other.included);
  }
};

std::string OriginalMessage(unsigned id, const std::vector<std::string>& args) {
  switch (id) {
    case clang::diag::err_undeclared_use_of_module:
      return llvm::formatv(
                 "module {0} does not depend on a module exporting '{1}'",
                 args[0], args[1])
          .str();
    case clang::diag::err_undeclared_use_of_module_indirect:
      return llvm::formatv(
                 "module {0} does not depend on a module exporting '{1}', "
                 "which is part of indirectly used module {2}",
                 args[0], args[1], args[2])
          .str();
    case clang::diag::warn_use_of_private_header_outside_module:
      return llvm::formatv(
                 "use of private header from outside its module: '{0}'",
                 args[0])
          .str();
    default:
      llvm_unreachable(llvm::formatv("Unsupported include diagnostic {0}", id)
                           .str()
                           .c_str());
  }
}

// clang::Diagnostic doesn't play nice with lifetimes. So copying the diagnostic
// doesn't work. Instead, we store the id, location, and args, which is
// sufficient to recreate the diagnostic.
struct BadIncludeDiagnostic {
  unsigned id = 0;
  clang::DiagnosticsEngine::Level level;
  clang::SourceLocation loc;
  std::vector<std::string> args;
  IncludeRequest request;
};

}  // namespace

class StrictDepsDiagnosticConsumer
    : public chrome_checker::ForwardingDiagnosticConsumer {
 public:
  StrictDepsDiagnosticConsumer(clang::CompilerInstance* instance,
                               clang::DiagnosticConsumer* next)
      : chrome_checker::ForwardingDiagnosticConsumer(next),
        instance_(instance),
        engine_(&instance->getDiagnostics()) {
    diag_custom_error_ =
        engine_->getCustomDiagID(clang::DiagnosticsEngine::Error, "%0\n%1");
    diag_custom_warning_ =
        engine_->getCustomDiagID(clang::DiagnosticsEngine::Warning, "%0\n%1");
  }

  // Reports a diagnostic.
  void Report(clang::SourceLocation loc,
              clang::DiagnosticsEngine::Level level,
              llvm::StringRef original_diagnostic_message,
              llvm::StringRef suggestion) {
    engine_->Report(loc, level == clang::DiagnosticsEngine::Error
                             ? diag_custom_error_
                             : diag_custom_warning_)
        << original_diagnostic_message << suggestion;
  }

  // Evaluates the #include that triggered this diagnostic.
  // Returns (name, is_angled)
  std::optional<std::pair<llvm::StringRef, bool>> IncludedFile(
      clang::SourceLocation loc) {
    clang::SourceManager& sm = instance_->getSourceManager();
    if (!loc.isValid()) {
      return std::nullopt;
    }
    bool invalid = false;
    const char* begin = sm.getCharacterData(loc, &invalid);
    if (invalid || !begin || (*begin != '<' && *begin != '"')) {
      return std::nullopt;
    }
    bool is_angled = *begin == '<';
    char closing_char = is_angled ? '>' : '"';

    // Search for the closing character on the same line
    const char* end = begin + 1;
    while (*end != closing_char) {
      // Should never happen, but just in case, bail out.
      if (*end == '\n' || !*end) {
        return std::nullopt;
      }
      end++;
    }

    return std::make_pair(llvm::StringRef(begin + 1, end - begin - 1),
                          is_angled);
  }

  // Resolves the #include that triggered this diagnostic to a file path.
  std::optional<std::string> ResolveInclude(
      clang::SourceLocation includer,
      const std::pair<llvm::StringRef, bool>& included) {
    auto [header, is_angled] = included;
    clang::Preprocessor& pp = instance_->getPreprocessor();
    clang::HeaderSearch& hs = pp.getHeaderSearchInfo();

    auto file =
        hs.LookupFile(header, includer, is_angled, nullptr, nullptr, {},
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!file) {
      return std::nullopt;
    }
    return file->getName().str();
  }

  // Determines whether a path is in the sysroot. Conservatively assumes a no
  // if there was not enough information to know for sure.
  bool InSysroot(llvm::StringRef path) {
    const auto& sysroot = instance_->getHeaderSearchOpts().Sysroot;
    if (sysroot.empty()) {
      return false;
    }

    llvm::SmallString<256> p(path);
    // Poor man's starts_with (starts_with is defined in Path.cpp but not
    // exposed in Path.h).
    return llvm::sys::path::replace_path_prefix(p, sysroot, "");
  }

  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                        const clang::Diagnostic& diag) override {
    const unsigned diag_id = diag.getID();
    if (std::find(kSupportedDiagnostics.begin(), kSupportedDiagnostics.end(),
                  diag_id) == kSupportedDiagnostics.end()) {
      PassthroughDiagnostic(level, diag);
      return;
    }

    auto included = IncludedFile(diag.getLocation());
    if (!included) {
      PassthroughDiagnostic(level, diag);
      return;
    }

    auto path = ResolveInclude(diag.getLocation(), *included);
    if (!path) {
      // This shouldn't happen, so we just leave the diagnostic as-is.
      PassthroughDiagnostic(level, diag);
    } else if (InSysroot(*path)) {
      // The allowlist requires the string used for including, rather than the
      // path.
      std::string original_msg =
          OriginalMessage(diag.getID(), ArgsFromDiagnostic(diag));
      std::string allowlist_msg =
          llvm::formatv(
              "Suggestion: Add an AllowedHeader entry for '{0}' to "
              "build/modules/unified/modulemap_config.py",
              included->first)
              .str();
      Report(diag.getLocation(), level, original_msg, allowlist_msg);
    } else if (auto includer = SourceModule(diag); includer) {
      // Collect these to later report.
      // We do this because `gn suggest` is quite slow to run, so instead of
      // `gn suggest a=b` and `gn suggest c=d`, we want to either suggest or
      // run `gn suggest a=b c=d`.
      buffer_.push_back(BadIncludeDiagnostic{
          diag.getID(), level, diag.getLocation(), ArgsFromDiagnostic(diag),
          IncludeRequest{*includer, *path}});
    } else {
      // We can't handle this, so we'll just pass it on.
      PassthroughDiagnostic(level, diag);
    }
  }

  void EndSourceFile() override {
    // We intentionally give the user a `gn suggest` command that has all
    // requests. This is because it's *much* more performant to batch these
    // into a single invocation.
    std::string suggestion = "For potential fixes, run `gn suggest $OUT_DIR";
    std::set<IncludeRequest> requests;
    for (const auto& diag : buffer_) {
      if (requests.insert(diag.request).second) {
        suggestion += llvm::formatv(" {0}={1}", diag.request.includer,
                                    diag.request.included)
                          .str();
      }
    }
    suggestion += "`";

    for (const auto& diag : buffer_) {
      Report(diag.loc, diag.level, OriginalMessage(diag.id, diag.args),
             suggestion);
    }

    ForwardingDiagnosticConsumer::EndSourceFile();
  }

 private:
  clang::CompilerInstance* instance_;
  clang::DiagnosticsEngine* engine_;
  unsigned diag_custom_error_;
  unsigned diag_custom_warning_;
  std::vector<BadIncludeDiagnostic> buffer_;
};

class StrictDepsASTConsumer : public clang::ASTConsumer {
 public:
  StrictDepsASTConsumer(clang::CompilerInstance* instance)
      : instance_(instance) {
    clang::DiagnosticsEngine& engine = instance_->getDiagnostics();
    old_client_ = engine.getClient();
    old_owned_client_ = engine.takeClient();
    engine.setClient(new StrictDepsDiagnosticConsumer(instance_, old_client_),
                     /*owned=*/true);
  }

  ~StrictDepsASTConsumer() override {
    clang::DiagnosticsEngine& engine = instance_->getDiagnostics();
    if (old_owned_client_) {
      engine.setClient(old_owned_client_.release(), /*owned=*/true);
    } else {
      engine.setClient(old_client_, /*owned=*/false);
    }
  }

 private:
  clang::CompilerInstance* instance_;
  clang::DiagnosticConsumer* old_client_;
  std::unique_ptr<clang::DiagnosticConsumer> old_owned_client_;
};

class StrictDepsASTAction : public clang::PluginASTAction {
 public:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& instance,
      llvm::StringRef ref) override {
    return std::make_unique<StrictDepsASTConsumer>(&instance);
  }

  bool ParseArgs(const clang::CompilerInstance& instance,
                 const std::vector<std::string>& args) override {
    return true;
  }
};

static clang::FrontendPluginRegistry::Add<StrictDepsASTAction> X(
    "strict-deps",
    "Checks for strict dependencies in modules");

}  // namespace chrome_checker
