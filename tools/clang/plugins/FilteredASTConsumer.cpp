// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "FilteredASTConsumer.h"

#include <algorithm>
#include <memory>

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/MacroInfo.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/ADT/DenseMap.h"

// Records the locations in system headers where code from non-system files
// shows up: expansions of macros that are defined in a non-system file. (Files
// that are included by a system header are system headers too.)
class FilteredASTConsumer::NonSystemCodeInSystemHeaderFinder
    : public clang::PPCallbacks {
 public:
  explicit NonSystemCodeInSystemHeaderFinder(
      const clang::SourceManager& source_manager)
      : source_manager_(source_manager) {}

  void MacroExpands(const clang::Token& macro_name_token,
                    const clang::MacroDefinition& definition,
                    clang::SourceRange range,
                    const clang::MacroArgs* args) override {
    const clang::MacroInfo* macro_info = definition.getMacroInfo();
    if (!macro_info) {
      return;
    }
    // This runs for every macro expansion, so only classify every macro once.
    auto [it, inserted] = is_non_system_macro_.try_emplace(macro_info, false);
    if (inserted) {
      // Builtin macros and macros from the command line are not under the
      // control of the author of the code, and the plugins don't report
      // anything for code spelled there.
      clang::SourceLocation loc = macro_info->getDefinitionLoc();
      it->second = loc.isValid() && !source_manager_.isInSystemHeader(loc) &&
                   !source_manager_.isWrittenInBuiltinFile(loc) &&
                   !source_manager_.isWrittenInCommandLineFile(loc) &&
                   !source_manager_.isWrittenInScratchSpace(loc);
    }
    if (it->second) {
      clang::SourceLocation loc =
          source_manager_.getExpansionLoc(range.getBegin());
      if (source_manager_.isInSystemHeader(loc)) {
        // Nested macro arguments can be expanded before rescanning the macro
        // body, so callbacks can revisit earlier locations even without
        // modules. Expansions nested in the same macro invocation also share
        // its outermost expansion location. Keep unique locations sorted for
        // HasNonSystemCodeIn().
        if (locations_.empty() ||
            source_manager_.isBeforeInTranslationUnit(locations_.back(), loc)) {
          locations_.push_back(loc);
        } else if (locations_.back() != loc) {
          auto it =
              std::lower_bound(locations_.begin(), locations_.end(), loc,
                               IsBeforeInTranslationUnit(source_manager_));
          if (it == locations_.end() || *it != loc) {
            locations_.insert(it, loc);
          }
        }
      }
    }
  }

  // Returns true if code from a non-system file shows up in `range`, which has
  // to consist of file locations.
  bool HasNonSystemCodeIn(clang::SourceRange range) const {
    // `locations_` is sorted (see MacroExpands()), so find the first recorded
    // location that is not before `range`'s start...
    auto it =
        std::lower_bound(locations_.begin(), locations_.end(), range.getBegin(),
                         IsBeforeInTranslationUnit(source_manager_));
    // ...and check whether it also falls at or before `range`'s end. Any
    // later locations can't be in `range` either, since they're sorted.
    return it != locations_.end() &&
           !source_manager_.isBeforeInTranslationUnit(range.getEnd(), *it);
  }

 private:
  // Functor comparing source locations by isBeforeInTranslationUnit(), for
  // use with std::lower_bound() on `locations_`.
  struct IsBeforeInTranslationUnit {
    explicit IsBeforeInTranslationUnit(
        const clang::SourceManager& source_manager)
        : source_manager(source_manager) {}
    bool operator()(clang::SourceLocation a, clang::SourceLocation b) const {
      return source_manager.isBeforeInTranslationUnit(a, b);
    }
    const clang::SourceManager& source_manager;
  };

  const clang::SourceManager& source_manager_;
  llvm::DenseMap<const clang::MacroInfo*, bool> is_non_system_macro_;
  // This is almost always empty. Kept sorted and unique; see MacroExpands().
  std::vector<clang::SourceLocation> locations_;
};

void FilteredASTConsumer::SkipSystemHeaders(clang::Preprocessor& preprocessor) {
  auto finder = std::make_unique<NonSystemCodeInSystemHeaderFinder>(
      preprocessor.getSourceManager());
  non_system_code_finder_ = finder.get();
  preprocessor.addPPCallbacks(std::move(finder));
}

bool FilteredASTConsumer::IsFilteredOutSystemHeaderDecl(
    const clang::Decl* decl) const {
  if (!non_system_code_finder_) {
    return false;
  }
  const clang::SourceManager& source_manager =
      decl->getASTContext().getSourceManager();
  clang::SourceRange range = decl->getSourceRange();
  if (range.isInvalid()) {
    return false;
  }
  range = clang::SourceRange(source_manager.getExpansionLoc(range.getBegin()),
                             source_manager.getExpansionLoc(range.getEnd()));
  return source_manager.isInSystemHeader(range.getBegin()) &&
         source_manager.isInSystemHeader(range.getEnd()) &&
         !non_system_code_finder_->HasNonSystemCodeIn(range);
}

bool FilteredASTConsumer::HandleTopLevelDecl(clang::DeclGroupRef d) {
  // Since HandleTopLevelDecl is only called when a part of the AST is actually
  // read, this allows us to skip checking any parts of the AST that weren't
  // used.
  // eg. With modules, we have a large AST, but only read parts which were
  // actually used.

  for (clang::Decl* decl : d) {
    // If I write code like the following:
    // template <typename T> void foo()
    // foo<int>();
    // Then foo appears twice in the top-level decls - once as a template, and
    // once as a template instantiation.
    // We want to skip the template instantiation for 2 reasons:
    // 1) Performance
    // 2) Coding style should be based on written code, not generated code. For
    //    example, `auto foo = T()` is good code, but if we use the template
    //    instantiotion of `T=int*`, then it would look like
    //    `auto foo = int*()`, which is incorrect (it should be `auto* foo`).
    auto kind = clang::TemplateSpecializationKind::TSK_Undeclared;
    if (auto* fd = clang::dyn_cast<clang::FunctionDecl>(decl)) {
      kind = fd->getTemplateSpecializationKind();
    } else if (auto* crd = clang::dyn_cast<clang::CXXRecordDecl>(decl)) {
      kind = crd->getTemplateSpecializationKind();
    }
    if (!isTemplateInstantiation(kind) &&
        !IsFilteredOutSystemHeaderDecl(decl)) {
      top_level_decls_.push_back(decl);
    }
  }
  return true;
}

void FilteredASTConsumer::ApplyFilter(clang::ASTContext& context) {
  // The traversal scope defaults to the whole AST. When using clang modules,
  // this includes parts of the AST that were available but never read due to
  // PCM files being lazy.

  // Note: This can only run once all top-level decls are complete.
  // TODO(crbug.com/425542181): One optimization we could consider making in
  // the future is to determine the main cc file and main header file, and
  // filter to only scan those.
  context.setTraversalScope(top_level_decls_);
}
