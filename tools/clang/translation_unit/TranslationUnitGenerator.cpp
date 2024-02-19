// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This implements a Clang tool to generate compilation information that is
// sufficient to recompile the code with clang. For each compilation unit, all
// source files which are necessary for compiling it are determined. For each
// compilation unit, a file is created containing a list of all file paths of
// included files.

#include <assert.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <vector>

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

using clang::HeaderSearchOptions;
using clang::tooling::CommonOptionsParser;
using llvm::sys::fs::real_path;
using llvm::SmallVector;
using std::set;
using std::stack;
using std::string;
using std::vector;

namespace {
// Set of preprocessor callbacks used to record files included.
class IncludeFinderPPCallbacks : public clang::PPCallbacks {
 public:
  IncludeFinderPPCallbacks(clang::SourceManager* source_manager,
                           string* main_source_file,
                           set<string>* source_file_paths,
                           const HeaderSearchOptions* header_search_options);
  void FileChanged(clang::SourceLocation /*loc*/,
                   clang::PPCallbacks::FileChangeReason reason,
                   clang::SrcMgr::CharacteristicKind /*file_type*/,
                   clang::FileID /*prev_fid*/) override;
  void AddFile(const string& path);
  void InclusionDirective(clang::SourceLocation hash_loc,
                          const clang::Token& include_tok,
                          llvm::StringRef file_name,
                          bool is_angled,
                          clang::CharSourceRange range,
                          clang::OptionalFileEntryRef file,
                          llvm::StringRef search_path,
                          llvm::StringRef relative_path,
                          const clang::Module* SuggestedModule,
                          bool ModuleImported,
                          clang::SrcMgr::CharacteristicKind /*file_type*/
                          ) override;
  void EndOfMainFile() override;

 private:
  string DoubleSlashSystemHeaders(const string& search_path,
                                  const string& relative_path) const;

  clang::SourceManager* const source_manager_;
#if !defined(NDEBUG)
  string* const main_source_file_;
#endif
  set<string>* const source_file_paths_;
  set<string> system_header_prefixes_;
  // The path of the file that was last referenced by an inclusion directive,
  // normalized for includes that are relative to a different source file.
  string last_inclusion_directive_;
  // The stack of currently parsed files. top() gives the current file.
  stack<string> current_files_;
};

IncludeFinderPPCallbacks::IncludeFinderPPCallbacks(
    clang::SourceManager* source_manager,
    string* main_source_file,
    set<string>* source_file_paths,
    const HeaderSearchOptions* header_search_options)
    : source_manager_(source_manager),
#if !defined(NDEBUG)
      main_source_file_(main_source_file),
#endif
      source_file_paths_(source_file_paths) {
  // In practice this list seems to be empty, but add it anyway just in case.
  for (const auto& prefix : header_search_options->SystemHeaderPrefixes) {
    system_header_prefixes_.insert(prefix.Prefix);
  }

  // This list contains all the include directories of different type.  We add
  // all system headers to the set - excluding the Quoted and Angled groups
  // which are from -iquote and -I flags.
  for (const auto& entry : header_search_options->UserEntries) {
    switch (entry.Group) {
      case clang::frontend::System:
      case clang::frontend::ExternCSystem:
      case clang::frontend::CSystem:
      case clang::frontend::CXXSystem:
      case clang::frontend::ObjCSystem:
      case clang::frontend::ObjCXXSystem:
      case clang::frontend::After:
        system_header_prefixes_.insert(entry.Path);
        break;
      default:
        break;
    }
  }
}

void IncludeFinderPPCallbacks::FileChanged(
    clang::SourceLocation /*loc*/,
    clang::PPCallbacks::FileChangeReason reason,
    clang::SrcMgr::CharacteristicKind /*file_type*/,
    clang::FileID /*prev_fid*/) {
  if (reason == clang::PPCallbacks::EnterFile) {
    if (!last_inclusion_directive_.empty()) {
      current_files_.push(last_inclusion_directive_);
    } else {
      current_files_.push(std::string(
          source_manager_
              ->getFileEntryRefForID(source_manager_->getMainFileID())
              ->getName()));
    }
  } else if (reason == ExitFile) {
    current_files_.pop();
  }
  // Other reasons are not interesting for us.
}

void IncludeFinderPPCallbacks::AddFile(const string& path) {
  source_file_paths_->insert(path);
}

template <typename T>
static T* getValueOrNull(llvm::ErrorOr<T*> maybe_val) {
  if (maybe_val) {
    return *maybe_val;
  }
  return nullptr;
}

void IncludeFinderPPCallbacks::InclusionDirective(
    clang::SourceLocation hash_loc,
    const clang::Token& include_tok,
    llvm::StringRef file_name,
    bool is_angled,
    clang::CharSourceRange range,
    clang::OptionalFileEntryRef file,
    llvm::StringRef search_path,
    llvm::StringRef relative_path,
    const clang::Module* SuggestedModule,
    bool ModuleImported,
    clang::SrcMgr::CharacteristicKind /*file_type*/
) {
  if (!file)
    return;

  assert(!current_files_.top().empty());
  const clang::DirectoryEntry* const search_path_entry = getValueOrNull(
      source_manager_->getFileManager().getDirectory(search_path));
  const clang::DirectoryEntry* const current_file_parent_entry =
      (*source_manager_->getFileManager().getFile(current_files_.top().c_str()))
          ->getDir();

  // If the include file was found relatively to the current file's parent
  // directory or a search path, we need to normalize it. This is necessary
  // because llvm internalizes the path by which an inode was first accessed,
  // and always returns that path afterwards. If we do not normalize this
  // we will get an error when we replay the compilation, as the virtual
  // file system is not aware of inodes.
  if (search_path_entry == current_file_parent_entry) {
    string parent =
        llvm::sys::path::parent_path(current_files_.top().c_str()).str();

    // If the file is a top level file ("file.cc"), we normalize to a path
    // relative to "./".
    if (parent.empty() || parent == "/")
      parent = ".";

    // Otherwise we take the literal path as we stored it for the current
    // file, and append the relative path.
    last_inclusion_directive_ =
        DoubleSlashSystemHeaders(parent, relative_path.str());
  } else if (!search_path.empty()) {
    last_inclusion_directive_ =
        DoubleSlashSystemHeaders(search_path.str(), relative_path.str());
  } else {
    last_inclusion_directive_ = file_name.str();
  }
  AddFile(last_inclusion_directive_);
}

string IncludeFinderPPCallbacks::DoubleSlashSystemHeaders(
    const string& search_path,
    const string& relative_path) const {
  // We want to be able to extract the search path relative to which the
  // include statement is defined. Therefore if search_path is a system header
  // we use "//" as a separator between the search path and the relative path.
  const bool is_system_header =
      system_header_prefixes_.find(search_path) !=
      system_header_prefixes_.end();

  return search_path + (is_system_header ? "//" : "/") + relative_path;
}

void IncludeFinderPPCallbacks::EndOfMainFile() {
  clang::OptionalFileEntryRef main_file =
      source_manager_->getFileEntryRefForID(source_manager_->getMainFileID());
  assert(main_file.has_value());

  SmallVector<char, 100> main_source_file_real_path;
  SmallVector<char, 100> main_file_name_real_path;
  assert(!real_path(*main_source_file_, main_source_file_real_path));
  assert(!real_path(main_file->getName(), main_file_name_real_path));
  assert(main_source_file_real_path == main_file_name_real_path);

  AddFile(std::string(main_file->getName()));
}

class CompilationIndexerAction : public clang::PreprocessorFrontendAction {
 public:
  CompilationIndexerAction() {}
  void ExecuteAction() override;

  // Runs the preprocessor over the translation unit.
  // This triggers the PPCallbacks we register to intercept all required
  // files for the compilation.
  void Preprocess();
  void EndSourceFileAction() override;

 private:
  // Set up the state extracted during the compilation, and run Clang over the
  // input.
  string main_source_file_;
  // Maps file names to their contents as read by Clang's source manager.
  set<string> source_file_paths_;
};

void CompilationIndexerAction::ExecuteAction() {
  auto inputs = getCompilerInstance().getFrontendOpts().Inputs;
  assert(inputs.size() == 1);
  main_source_file_ = std::string(inputs[0].getFile());

  Preprocess();
}

void CompilationIndexerAction::Preprocess() {
  clang::Preprocessor& preprocessor = getCompilerInstance().getPreprocessor();
  preprocessor.addPPCallbacks(std::make_unique<IncludeFinderPPCallbacks>(
      &getCompilerInstance().getSourceManager(), &main_source_file_,
      &source_file_paths_, &getCompilerInstance().getHeaderSearchOpts()));
  preprocessor.getDiagnostics().setIgnoreAllWarnings(true);
  preprocessor.SetSuppressIncludeNotFoundError(true);
  preprocessor.EnterMainSourceFile();
  clang::Token token;
  do {
    preprocessor.Lex(token);
  } while (token.isNot(clang::tok::eof));
}

void CompilationIndexerAction::EndSourceFileAction() {
  std::ofstream out(main_source_file_ + ".filepaths");
  for (const string& path : source_file_paths_) {
    out << path << std::endl;
  }
}
}  // namespace

static llvm::cl::extrahelp common_help(CommonOptionsParser::HelpMessage);

int main(int argc, const char* argv[]) {
  llvm::cl::OptionCategory category("TranslationUnitGenerator Tool");
  auto ExpectedParser = CommonOptionsParser::create(
      argc, argv, category, llvm::cl::OneOrMore, nullptr);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser& options = ExpectedParser.get();
  std::unique_ptr<clang::tooling::FrontendActionFactory> frontend_factory =
      clang::tooling::newFrontendActionFactory<CompilationIndexerAction>();
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());
  return tool.run(frontend_factory.get());
}
