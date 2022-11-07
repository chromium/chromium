// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "FindBadRawPtrPatterns.h"

#include "Util.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Attr.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/TypeLoc.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/LineIterator.h"

using namespace clang;
using namespace clang::ast_matchers;

namespace chrome_checker {

const char kBadCastSignature[] =
    "[chromium-raw-ptr-cast] Casting raw_ptr<T>* to another type is not "
    "allowed as it may cause BRP ref count mismatch and bypass security "
    "checks.";

class BadCastMatcher : public MatchFinder::MatchCallback {
 public:
  explicit BadCastMatcher(clang::CompilerInstance& compiler)
      : compiler_(compiler) {
    error_bad_raw_ptr_cast_signature_ =
        compiler_.getDiagnostics().getCustomDiagID(
            clang::DiagnosticsEngine::Error, kBadCastSignature);
  }

  void Register(MatchFinder& match_finder) {
    // TODO(keishi): Also find casts to and from classes that contain raw_ptr.
    auto cast_matcher =
        castExpr(
            allOf(hasSourceExpression(hasType(pointerType(pointee(
                      hasUnqualifiedDesugaredType(recordType(hasDeclaration(
                          cxxRecordDecl(classTemplateSpecializationDecl(
                              hasName("base::raw_ptr")))))))))),
                  hasCastKind(CK_BitCast)))
            .bind("castExpr");
    match_finder.addMatcher(cast_matcher, this);
  }

  void run(const MatchFinder::MatchResult& result) override {
    const clang::CastExpr* cast_expr =
        result.Nodes.getNodeAs<clang::CastExpr>("castExpr");
    assert(cast_expr && "matcher should bind 'castExpr'");

    const clang::SourceManager& source_manager = *result.SourceManager;
    clang::SourceLocation loc = cast_expr->getSourceRange().getBegin();
    std::string file_path = GetFilename(source_manager, loc);

    // Using raw_ptr<T> in a stdlib collection will cause a cast.
    // e.g.
    // https://source.chromium.org/chromium/chromium/src/+/main:components/feed/core/v2/xsurface_datastore.h;drc=a0ff03edcace35ec020edd235f4d9e9735fc9690;l=107
    if (file_path.find("buildtools/third_party/libc++") != std::string::npos)
      return;
    // CHECK(raw_ptr<T>) will cause a cast.
    // e.g.
    // https://source.chromium.org/chromium/chromium/src/+/main:base/task/sequence_manager/thread_controller_with_message_pump_impl.cc;drc=c49b7434a9d4a61c49fc0123e904a6c5e7162731;l=121
    if (file_path.find("base/check_op.h") != std::string::npos)
      return;
    // raw_ptr<T>* is cast to ui::metadata::PropertyKey
    // https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.cc;drc=a0ff03edcace35ec020edd235f4d9e9735fc9690;l=2417
    if (file_path.find("ui/views/controls/table/table_view.cc") !=
        std::string::npos)
      return;
    // XdgActivation::activation_queue_ is a base::queue<raw_ptr> which causes a
    // cast in VectorBuffer and circular_deque.
    if (file_path.find("base/containers/vector_buffer.h") != std::string::npos)
      return;
    if (file_path.find("base/containers/circular_deque.h") != std::string::npos)
      return;

    compiler_.getDiagnostics().Report(cast_expr->getEndLoc(),
                                      error_bad_raw_ptr_cast_signature_);
  }

 private:
  clang::CompilerInstance& compiler_;
  unsigned error_bad_raw_ptr_cast_signature_;
};

void FindBadRawPtrPatterns(Options options,
                           clang::ASTContext& ast_context,
                           clang::CompilerInstance& compiler) {
  if (!options.check_bad_raw_ptr_cast)
    return;
  MatchFinder match_finder;

  BadCastMatcher bad_cast_matcher(compiler);
  bad_cast_matcher.Register(match_finder);

  match_finder.matchAST(ast_context);
}

}  // namespace chrome_checker
