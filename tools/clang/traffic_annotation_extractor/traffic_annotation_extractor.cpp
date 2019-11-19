// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This clang tool does the following three tasks:
// 1) Finds all instances of the following functions and extracts the location
//    info and content of annotation tags:
//      - net::DefineNetworkTrafficAnnotation
//      - net::DefinePartialNetworkTrafficAnnotation
//      - net::CompleteNetworkTrafficAnnotation
//      - net::BranchedCompleteNetworkTrafficAnnotation
// 2) Extracts all calls of the following network request creation functions
//    and returns their source location and availability of a
//    net::[Partial]NetworkTrafficAnnotation parameter in them:
//     - URLFetcher::Create
//     - URLRequestContext::CreateRequest
// 3) Finds all instances of initializing any of the following classes with list
//    expressions or assignment of a value to |unique_id_hash_code| of the
//    mutable ones, outside traffic annotation API functions:
//     - net::NetworkTrafficAnnotationTag
//     - net::PartialNetworkTrafficAnnotationTag
//     - net::MutableNetworkTrafficAnnotationTag
//     - net::MutablePartialNetworkTrafficAnnotationTag
// All outputs are written to to llvm::outs.
// Please refer to README.md for build and usage instructions.

#include <memory>
#include <vector>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/ArgumentsAdjusters.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"

using namespace clang::ast_matchers;

namespace {

// Information about location of a line of code.
struct Location {
  std::string file_path;
  int line_number = -1;
};

// An instance of a call to either of the 4 network traffic annotation
// definition functions.
struct NetworkAnnotationInstance {
  // Annotation content. These are the arguments of the call to either of the 4
  // network traffic annotation definition functions.
  struct Annotation {
    std::string unique_id;
    std::string text;

    // |extra_id| will have |completing_id| for
    // net::DefinePartialNetworkTrafficAnnotation and |group_id| for
    // net::BranchedCompleteNetworkTrafficAnnotation. It will be empty in other
    // cases.
    std::string extra_id;
  };

  Location location;
  Annotation annotation;

  // Specifying the function type.
  enum FunctionType {
    kDefinition,         // net::DefineNetworkTrafficAnnotation
    kPartial,            // net::DefinePartialNetworkTrafficAnnotation
    kCompleting,         // net::CompleteNetworkTrafficAnnotation
    kBranchedCompleting  // net::BranchedCompleteNetworkTrafficAnnotation
  };

  FunctionType function_type;

  const char* GetTypeName() const {
    switch (function_type) {
      case kDefinition:
        return "Definition";
      case kPartial:
        return "Partial";
      case kCompleting:
        return "Completing";
      case kBranchedCompleting:
        return "BranchedCompleting";
    }
    assert(false);
    return "";
  }
};

// An instance of a call to one of the monitored function.
struct CallInstance {
  // Location of the call.
  Location location;

  // Whether the function is annotated.
  bool has_annotation = false;

  // Name of the called function.
  std::string called_function_name;
};

// A structure to keep detected annotation and call instances, and all code
// locations that include a direct value assignment to annotations using list
// expression constructors or mutable annotations' |unique_id_hash_code|.
struct Collector {
  std::vector<NetworkAnnotationInstance> annotations;
  std::vector<CallInstance> calls;
  std::vector<Location> assignments;
};

// This class implements the call back functions for AST Matchers. The matchers
// are defined in RunMatchers function. When a pattern is found there,
// the run function in this class is called back with information on the matched
// location and description of the matched pattern.
class NetworkAnnotationTagCallback : public MatchFinder::MatchCallback {
 public:
  explicit NetworkAnnotationTagCallback(Collector* collector)
      : collector_(collector) {}
  ~NetworkAnnotationTagCallback() override = default;

  // Is called on any pattern found by ASTMathers that are defined in RunMathers
  // function.
  virtual void run(const MatchFinder::MatchResult& result) override {
    if (const clang::CallExpr* call_expr =
            result.Nodes.getNodeAs<clang::CallExpr>("monitored_function")) {
      AddFunction(call_expr, result);
    } else if (const clang::CXXConstructExpr* constructor_expr =
                   result.Nodes.getNodeAs<clang::CXXConstructExpr>(
                       "annotation_constructor")) {
      AddConstructor(constructor_expr, result);
    } else if (const clang::MemberExpr* member_expr =
                   result.Nodes.getNodeAs<clang::MemberExpr>(
                       "direct_assignment")) {
      AddAssignment(member_expr, result);
    } else {
      AddAnnotation(result);
    }
  }

  void GetInstanceLocation(const MatchFinder::MatchResult& result,
                           const clang::Expr* expr,
                           Location* location) {
    clang::SourceLocation source_location = expr->getBeginLoc();
    if (source_location.isMacroID())
      source_location = result.SourceManager->getExpansionLoc(source_location);
    location->file_path = result.SourceManager->getFilename(source_location);
    location->line_number =
        result.SourceManager->getSpellingLineNumber(source_location);

    std::replace(location->file_path.begin(), location->file_path.end(), '\\',
                 '/');

    // Trim leading "../"s from file path.
    while (location->file_path.length() > 3 &&
           location->file_path.substr(0, 3) == "../") {
      location->file_path =
          location->file_path.substr(3, location->file_path.length() - 3);
    }
  }

  // Stores a function call that should be monitored.
  void AddFunction(const clang::CallExpr* call_expr,
                   const MatchFinder::MatchResult& result) {
    CallInstance instance;

    GetInstanceLocation(result, call_expr, &instance.location);
    instance.called_function_name =
        call_expr->getDirectCallee()->getQualifiedNameAsString();
    instance.has_annotation =
        (result.Nodes.getNodeAs<clang::RecordDecl>("annotation") != nullptr);
    collector_->calls.push_back(instance);
  }

  // Stores an annotation constructor called with list expression.
  void AddConstructor(const clang::CXXConstructExpr* constructor_expr,
                      const MatchFinder::MatchResult& result) {
    Location instance;
    GetInstanceLocation(result, constructor_expr, &instance);
    collector_->assignments.push_back(instance);
  }

  // Stores a value assignment to |unique_id_hash_code| of a mutable annotaton.
  void AddAssignment(const clang::MemberExpr* member_expr,
                     const MatchFinder::MatchResult& result) {
    Location instance;
    GetInstanceLocation(result, member_expr, &instance);
    collector_->assignments.push_back(instance);
  }

  // Stores an annotation.
  void AddAnnotation(const MatchFinder::MatchResult& result) {
    NetworkAnnotationInstance instance;

    const clang::StringLiteral* unique_id =
        result.Nodes.getNodeAs<clang::StringLiteral>("unique_id");
    const clang::StringLiteral* annotation_text =
        result.Nodes.getNodeAs<clang::StringLiteral>("annotation_text");
    const clang::StringLiteral* group_id =
        result.Nodes.getNodeAs<clang::StringLiteral>("group_id");
    const clang::StringLiteral* completing_id =
        result.Nodes.getNodeAs<clang::StringLiteral>("completing_id");

    const clang::CallExpr* call_expr = nullptr;
    if ((call_expr =
             result.Nodes.getNodeAs<clang::CallExpr>("definition_function"))) {
      instance.function_type = NetworkAnnotationInstance::kDefinition;
    } else if ((call_expr = result.Nodes.getNodeAs<clang::CallExpr>(
                    "partial_function"))) {
      instance.function_type = NetworkAnnotationInstance::kPartial;
      assert(completing_id);
      instance.annotation.extra_id = completing_id->getString();
    } else if ((call_expr = result.Nodes.getNodeAs<clang::CallExpr>(
                    "completing_function"))) {
      instance.function_type = NetworkAnnotationInstance::kCompleting;
    } else if ((call_expr = result.Nodes.getNodeAs<clang::CallExpr>(
                    "branched_completing_function"))) {
      instance.function_type = NetworkAnnotationInstance::kBranchedCompleting;
      assert(group_id);
      instance.annotation.extra_id = group_id->getString();
    } else {
      assert(false);
    }

    assert(unique_id && annotation_text);
    instance.annotation.unique_id = unique_id->getString();
    instance.annotation.text = annotation_text->getString();

    GetInstanceLocation(result, call_expr, &instance.location);

    collector_->annotations.push_back(instance);
  }

 private:
  Collector* collector_;
};

// Sets up an ASTMatcher and runs clang tool to populate collector. Returns the
// result of running the clang tool.
int RunMatchers(clang::tooling::ClangTool* clang_tool, Collector* collector) {
  NetworkAnnotationTagCallback callback(collector);
  MatchFinder match_finder;

  // Set up patterns to find network traffic annotation definition functions,
  // their arguments, and their ancestor function (when possible).
  auto bind_function_context_if_present =
      anyOf(hasAncestor(functionDecl().bind("function_context")),
            unless(hasAncestor(functionDecl())));
  auto has_annotation_parameter = anyOf(
      hasAnyParameter(hasType(
          recordDecl(anyOf(hasName("net::NetworkTrafficAnnotationTag"),
                           hasName("net::PartialNetworkTrafficAnnotationTag")))
              .bind("annotation"))),
      unless(hasAnyParameter(hasType(recordDecl(
          anyOf(hasName("net::NetworkTrafficAnnotationTag"),
                hasName("net::PartialNetworkTrafficAnnotationTag")))))));
  match_finder.addMatcher(
      callExpr(hasDeclaration(functionDecl(
                   anyOf(hasName("DefineNetworkTrafficAnnotation"),
                         hasName("net::DefineNetworkTrafficAnnotation")))),
               hasArgument(0, stringLiteral().bind("unique_id")),
               hasArgument(1, stringLiteral().bind("annotation_text")),
               bind_function_context_if_present)
          .bind("definition_function"),
      &callback);
  match_finder.addMatcher(
      callExpr(hasDeclaration(functionDecl(anyOf(
                   hasName("DefinePartialNetworkTrafficAnnotation"),
                   hasName("net::DefinePartialNetworkTrafficAnnotation")))),
               hasArgument(0, stringLiteral().bind("unique_id")),
               hasArgument(1, stringLiteral().bind("completing_id")),
               hasArgument(2, stringLiteral().bind("annotation_text")),
               bind_function_context_if_present)
          .bind("partial_function"),
      &callback);
  match_finder.addMatcher(
      callExpr(hasDeclaration(functionDecl(
                   anyOf(hasName("CompleteNetworkTrafficAnnotation"),
                         hasName("net::CompleteNetworkTrafficAnnotation")))),
               hasArgument(0, stringLiteral().bind("unique_id")),
               hasArgument(2, stringLiteral().bind("annotation_text")),
               bind_function_context_if_present)
          .bind("completing_function"),
      &callback);
  match_finder.addMatcher(
      callExpr(hasDeclaration(functionDecl(anyOf(
                   hasName("BranchedCompleteNetworkTrafficAnnotation"),
                   hasName("net::BranchedCompleteNetworkTrafficAnnotation")))),
               hasArgument(0, stringLiteral().bind("unique_id")),
               hasArgument(1, stringLiteral().bind("group_id")),
               hasArgument(3, stringLiteral().bind("annotation_text")),
               bind_function_context_if_present)
          .bind("branched_completing_function"),
      &callback);

  // Setup patterns to find functions that should be monitored.
  match_finder.addMatcher(
      callExpr(hasDeclaration(functionDecl(
                   anyOf(hasName("URLFetcher::Create"),
                         hasName("URLRequestContext::CreateRequest")),
                   has_annotation_parameter)),
               bind_function_context_if_present)
          .bind("monitored_function"),
      &callback);

  // Setup patterns to find constructors of different network traffic annotation
  // tags that are initialized by list expressions.
  match_finder.addMatcher(
      cxxConstructExpr(
          hasDeclaration(functionDecl(
              anyOf(hasName("net::NetworkTrafficAnnotationTag::"
                            "NetworkTrafficAnnotationTag"),
                    hasName("net::PartialNetworkTrafficAnnotationTag::"
                            "PartialNetworkTrafficAnnotationTag"),
                    hasName("net::MutableNetworkTrafficAnnotationTag::"
                            "MutableNetworkTrafficAnnotationTag"),
                    hasName("net::MutablePartialNetworkTrafficAnnotationTag::"
                            "MutablePartialNetworkTrafficAnnotationTag")))),
          hasDescendant(initListExpr()), bind_function_context_if_present)
          .bind("annotation_constructor"),
      &callback);

  // Setup pattern to find direct assignment of value to |unique_id_hash_code|
  // of net::MutableNetworkTrafficAnnotationTag or
  // net::MutablePartialNetworkTrafficAnnotationTag.
  match_finder.addMatcher(
      memberExpr(
          member(hasName("unique_id_hash_code")),
          hasObjectExpression(hasType(cxxRecordDecl(anyOf(
              hasName("net::MutableNetworkTrafficAnnotationTag"),
              hasName("net::MutablePartialNetworkTrafficAnnotationTag"))))),
          hasParent(binaryOperator(hasOperatorName("="))),
          bind_function_context_if_present)
          .bind("direct_assignment"),
      &callback);

  std::unique_ptr<clang::tooling::FrontendActionFactory> frontend_factory =
      clang::tooling::newFrontendActionFactory(&match_finder);
  return clang_tool->run(frontend_factory.get());
}

}  // namespace

static llvm::cl::OptionCategory ToolCategory(
    "traffic_annotation_extractor: Extract traffic annotation texts");
static llvm::cl::extrahelp CommonHelp(
    clang::tooling::CommonOptionsParser::HelpMessage);

int main(int argc, const char* argv[]) {
  clang::tooling::CommonOptionsParser options(argc, argv, ToolCategory);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());
  tool.appendArgumentsAdjuster(clang::tooling::getStripPluginsAdjuster());
  Collector collector;

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  int result = RunMatchers(&tool, &collector);

  if (result != 0)
    return result;

  // For each call to any of the functions that define a network traffic
  // annotation, write annotation text and relevant meta data into llvm::outs().
  for (const NetworkAnnotationInstance& instance : collector.annotations) {
    llvm::outs() << "==== NEW ANNOTATION ====\n";
    llvm::outs() << instance.location.file_path << "\n";
    llvm::outs() << instance.location.line_number << "\n";
    llvm::outs() << instance.GetTypeName() << "\n";
    llvm::outs() << instance.annotation.unique_id << "\n";
    llvm::outs() << instance.annotation.extra_id << "\n";
    llvm::outs() << instance.annotation.text << "\n";
    llvm::outs() << "==== ANNOTATION ENDS ====\n";
  }

  // For each call, write annotation text and relevant meta data.
  for (const CallInstance& instance : collector.calls) {
    llvm::outs() << "==== NEW CALL ====\n";
    llvm::outs() << instance.location.file_path << "\n";
    llvm::outs() << instance.location.line_number << "\n";
    llvm::outs() << instance.called_function_name << "\n";
    llvm::outs() << instance.has_annotation << "\n";
    llvm::outs() << "==== CALL ENDS ====\n";
  }

  // For each assignment, write relevant meta data.
  for (const Location& instance : collector.assignments) {
    llvm::outs() << "==== NEW ASSIGNMENT ====\n";
    llvm::outs() << instance.file_path << "\n";
    llvm::outs() << instance.line_number << "\n";
    llvm::outs() << "==== ASSIGNMENT ENDS ====\n";
  }

  return 0;
}
