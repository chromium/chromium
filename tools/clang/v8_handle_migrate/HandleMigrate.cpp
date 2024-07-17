// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A clang tool for the migration of Handle<T> to DirectHandle<T>.
// This is only useful for the V8 code base.

#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Refactoring.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>

#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

// Command line options.

static llvm::cl::OptionCategory my_tool_category(
    "Handle migration tool options");
static llvm::cl::extrahelp common_help(CommonOptionsParser::HelpMessage);

constexpr int kVerboseNone = 0;
constexpr int kVerboseReportInterestingHandleUse = 1;
constexpr int kVerboseReportInterestingFunctionCall = 1;
constexpr int kVerboseReportImplicitHandleConversion = 2;
constexpr int kVerboseReportHandleDereference = 2;
constexpr int kVerboseReportInterestingHandleDecl = 3;
constexpr int kVerboseReportInterestingFunctions = 4;
constexpr int kVerboseWhereAreWe = 90;

static llvm::cl::opt<int> VERBOSE("verbose",
                                  llvm::cl::desc("Set verbosity level"),
                                  llvm::cl::value_desc("level"),
                                  llvm::cl::init(kVerboseNone),
                                  llvm::cl::cat(my_tool_category));

static llvm::cl::alias alias_for_VERBOSE("v",
                                         llvm::cl::desc("Alias for --verbose"),
                                         llvm::cl::aliasopt(VERBOSE),
                                         llvm::cl::cat(my_tool_category));

static llvm::cl::extrahelp verbosity_help(
    "Verbosity levels:\n"
    "\t0:\tquiet\n"
    "\t1:\tinteresting handle uses and function calls\n"
    "\t2:\timplicit handle conversions and dereferences\n"
    "\t3:\tinteresting handle declarations\n"
    "\t4:\tinteresting function declarations\n"
    "\t90:\ttrack AST source location\n"
    "\n");

static llvm::cl::opt<bool> only_in_main_file(
    "local",
    llvm::cl::desc("Process only declarations in main file(s), default false"),
    llvm::cl::init(false),
    llvm::cl::cat(my_tool_category));

// Boilerplate. (Using clang::ast_matchers naming convention for these.)
// ----------------------------------------------------------------------------

TypeMatcher relaxType(TypeMatcher t) {
  return anyOf(t, pointerType(pointee(t)), referenceType(pointee(t)));
}

StatementMatcher constructorWithOneArgument(StatementMatcher argument) {
  return cxxConstructExpr(argumentCountIs(1),
                          hasArgument(0, ignoringImplicit(argument)));
}

auto handleDecl = cxxRecordDecl(isSameOrDerivedFrom("::v8::internal::Handle"),
                                isTemplateInstantiation());
auto directHandleDecl =
    cxxRecordDecl(isSameOrDerivedFrom("::v8::internal::DirectHandle"),
                  isTemplateInstantiation());
TypeMatcher handleType = relaxType(hasDeclaration(handleDecl));
TypeMatcher directHandleType = relaxType(hasDeclaration(directHandleDecl));

// Database for storing interesting variables, functions, etc.
// ----------------------------------------------------------------------------

template <typename NodeType>
struct ASTNodeHash {
  size_t operator()(const NodeType* x) const {
    return x->getLocation().getHashValue();
  }
};

template <typename NodeType>
struct ASTNodeEquals {
  bool operator()(const NodeType* x, const NodeType* y) const {
    return x->getLocation() == y->getLocation();
  }
};

class InterestingFunction;

// A database with possibly interesting declarations of type Handle<T>.
class InterestingHandle {
 public:
  InterestingHandle(const InterestingHandle&) = delete;
  InterestingHandle(InterestingHandle&&) = default;

  static InterestingHandle* Insert(const VarDecl* decl, bool is_definition) {
    assert(decl != nullptr);
    auto it = interesting_.find(decl);
    if (it == interesting_.end()) {
      auto p =
          interesting_.emplace(decl, InterestingHandle{decl, is_definition});
      it = p.first;
    }
    return &it->second;
  }

  static InterestingHandle* Lookup(const VarDecl* decl) {
    assert(decl != nullptr);
    auto it = interesting_.find(decl);
    if (it == interesting_.end()) {
      return nullptr;
    }
    return &it->second;
  }

  void Print(const SourceManager& source_manager) const {
    llvm::outs() << "    location: ";
    location_.print(llvm::outs(), source_manager);
    llvm::outs() << "\n";
    llvm::outs() << "    type: " << type_ << "\n";
  }

  void AddDependent(InterestingHandle* h) {
    // Marks `h` as a dependent of `this`: `this` is the parameter of a
    // function definition and `h` is the corresponding parameter of some
    // declaration of the same function.
    // 1. `this` is a definition and therefore must not be dependent to any
    // other definition.
    assert(dependent_to_ == nullptr);
    // 2. `h` must not be dependent to any other definition.
    assert(h->dependent_to_ == nullptr);
    // 3. `h` is in a declaration and therefore it must not have dependents.
    assert(h->list_of_dependent_.empty());
    list_of_dependent_.push_back(h);
    h->dependent_to_ = this;
  }

  void AddAsParameter(InterestingFunction* f) {
    if (function_ != nullptr && function_ != f) {
      llvm::outs()
          << "Warning: adding as a parameter to a different function\n";
    }
    function_ = f;
  }

  bool CanMigrate() const;
  Replacement GetReplacement() const { return replacement_.value(); }

  // This method registers a usage of an interesting variable. The first
  // parameter corresponds to the AST node where the variable is used,
  // whereas the second parameter advises if migration should be possible.
  // We want to disallow migration if a variable is used (at least once) in a
  // context where a `Handle<T>` is really required. When the AST is traversed
  // and a node with a variable usage is visited, the matcher callbacks will be
  // invoked consecutively for that node, in the order that they were added to
  // the match finder. These callbacks, independently from one another, may
  // invoke this method to advise whether migration should be possible.
  // Migration is prevented when this method is called for a variable's use for
  // the first time with migrate = false. If it has previously been called for
  // the same variable use with migrate = true, then migration is not prevented.
  // Evidently, the order in which the matcher callbacks are added to the finder
  // is very important. See the comment before `HandleUseVisitor` for a detailed
  // example.
  void RegisterUsage(const DeclRefExpr* use, bool migrate) {
    if (dependent_to_ != nullptr) {
      llvm::outs() << "Warning: use of handle that is marked as dependent\n";
    }
    if (use != previous_use_ && !migrate) {
      replacement_.reset();
    }
    previous_use_ = use;
  }

 private:
  InterestingHandle(const VarDecl* decl, bool is_definition)
      : is_definition_(is_definition),
        type_(decl->getType().getAsString()),
        location_(decl->getLocation()) {
    // If this is not a definition, bail out, otherwise let's be optimistic and
    // generate the replacement for migration!
    if (!is_definition_) {
      return;
    }

    // We get the |replacement_range| in a bit clumsy way, because clang docs
    // for QualifiedTypeLoc explicitly say that these objects "intentionally
    // do not provide source location for type qualifiers".
    const auto& source_manager = decl->getASTContext().getSourceManager();
    const auto& options = decl->getASTContext().getLangOpts();
    auto first_token_loc = source_manager.getSpellingLoc(decl->getBeginLoc());
    auto last_token_loc =
        source_manager.getSpellingLoc(decl->getTypeSpecEndLoc());
    auto end_loc =
        Lexer::getLocForEndOfToken(last_token_loc, 0, source_manager, options);
    auto range = CharSourceRange::getCharRange(first_token_loc, end_loc);

    auto original_text =
        Lexer::getSourceText(range, source_manager, options).str();
    std::regex re_handle("\\bHandle<");
    auto replacement_text =
        std::regex_replace(original_text, re_handle, "DirectHandle<");

    if (original_text != replacement_text) {
      replacement_.emplace(source_manager, range, replacement_text);
    }
  }

  bool is_definition_;
  std::string type_;
  SourceLocation location_;
  std::optional<Replacement> replacement_;
  const DeclRefExpr* previous_use_ = nullptr;

  // For parameters, this points to the corresponding function.
  InterestingFunction* function_ = nullptr;
  // For parameters of a function declaration, this points to the
  // corresponding parameter of the function definition.
  InterestingHandle* dependent_to_ = nullptr;
  // For parameters of a function definition, this contains a list
  // of all the corresponding parameters of function declarations (if any).
  std::vector<InterestingHandle*> list_of_dependent_;

  using Container = std::unordered_map<const VarDecl*,
                                       InterestingHandle,
                                       ASTNodeHash<VarDecl>,
                                       ASTNodeEquals<VarDecl>>;
  static Container interesting_;
};

InterestingHandle::Container InterestingHandle::interesting_;

// A database with all interesting functions.
class InterestingFunction {
 public:
  InterestingFunction(const InterestingFunction&) = delete;
  InterestingFunction(InterestingFunction&&) = default;

  static InterestingFunction* Insert(const FunctionDecl* decl) {
    assert(decl != nullptr);
    auto it = interesting_.find(decl);
    if (it == interesting_.end()) {
      auto p = interesting_.emplace(decl, InterestingFunction{decl});
      it = p.first;
    }
    return &it->second;
  }

  static InterestingFunction* Lookup(const FunctionDecl* decl) {
    assert(decl != nullptr);
    auto it = interesting_.find(decl);
    if (it == interesting_.end()) {
      return nullptr;
    }
    return &it->second;
  }

  void AddOrCheckParameter(const ParmVarDecl* param, InterestingHandle* p) {
    unsigned i = param->getFunctionScopeIndex();
    if (i >= parameters_.size()) {
      llvm::outs() << "Warning: parameter " << i
                   << " does not exist, there are only " << parameters_.size()
                   << " parameters\n";
      return;
    }
    if (parameters_[i] == nullptr) {
      parameters_[i] = p;
      p->AddAsParameter(this);
    } else if (parameters_[i] != p) {
      const auto& source_manager = param->getASTContext().getSourceManager();
      llvm::outs() << "Warning: parameter " << i << " has already been added\n";
      llvm::outs() << "  previous:\n";
      parameters_[i]->Print(source_manager);
      llvm::outs() << "  current:\n";
      p->Print(source_manager);
    }
  }

  void AddLocalVariable(InterestingHandle* v) { local_vars_.push_back(v); }

  const std::vector<InterestingHandle*>& parameters() const {
    return parameters_;
  }

  const std::vector<InterestingHandle*>& local_vars() const {
    return local_vars_;
  }

  bool is_special() const { return is_special_; }

  static std::set<Replacement> GetReplacements() {
    std::set<Replacement> result;
    for (const auto& [_, f] : interesting_) {
      for (InterestingHandle* p : f.parameters()) {
        if (p != nullptr && p->CanMigrate()) {
          result.insert(p->GetReplacement());
        }
      }
      for (InterestingHandle* v : f.local_vars()) {
        if (v->CanMigrate()) {
          result.insert(v->GetReplacement());
        }
      }
    }
    return result;
  }

 private:
  explicit InterestingFunction(const FunctionDecl* decl)
      : name_(decl->getQualifiedNameAsString()),
        location_(decl->getLocation()),
        parameters_(decl->getNumParams(), nullptr) {
    if (auto* method_decl = dyn_cast<CXXMethodDecl>(decl)) {
      is_special_ = method_decl->isVirtual();
    } else {
      is_special_ = decl->isTemplateInstantiation() ||
                    decl->isFunctionTemplateSpecialization();
    }
  }

  std::string name_;
  SourceLocation location_;
  bool is_special_ = false;
  std::vector<InterestingHandle*> parameters_;
  std::vector<InterestingHandle*> local_vars_;

  using Container = std::unordered_map<const FunctionDecl*,
                                       InterestingFunction,
                                       ASTNodeHash<FunctionDecl>,
                                       ASTNodeEquals<FunctionDecl>>;
  static Container interesting_;
};

InterestingFunction::Container InterestingFunction::interesting_;

bool InterestingHandle::CanMigrate() const {
  if (dependent_to_ != nullptr) {
    return dependent_to_->CanMigrate();
  }
  if (!is_definition_) {
    return false;
  }
  if (function_ != nullptr && function_->is_special()) {
    return false;
  }
  return replacement_.has_value();
}

// Keep track of where we are, in the AST.
// This is used only for debugging purposes.
// ----------------------------------------------------------------------------
class WhereWeAreVisitor : public MatchFinder::MatchCallback {
 private:
  static DeclarationMatcher matcher() { return decl().bind("decl"); }

 public:
  explicit WhereWeAreVisitor(MatchFinder& finder) {
    finder.addMatcher(matcher(), this);
  }

  void run(MatchFinder::MatchResult const& Result) override {
    auto* decl = Result.Nodes.getNodeAs<Decl>("decl");
    assert(decl != nullptr);

    if (VERBOSE >= kVerboseWhereAreWe) {
      ASTContext* context = Result.Context;
      auto loc = decl->getBeginLoc();
      llvm::outs() << "At: " << decl->getDeclKindName() << " ";
      loc.print(llvm::outs(), context->getSourceManager());
      llvm::outs() << "\n";
    }
  }
};

// Find and record interesting functions:
// - some parameter is a Handle<T>, or
// - the result is a Handle<T>, or
// - is a method of Handle<T>, because the object pointed to by `this` is a
// `Handle<T>`.
// ----------------------------------------------------------------------------
class InterestingFunctionVisitor : public MatchFinder::MatchCallback {
 private:
  static DeclarationMatcher matcher() {
    return functionDecl(anyOf(hasAnyParameter(parmVarDecl(hasType(handleType))),
                              returns(handleType),
                              cxxMethodDecl(ofClass(handleDecl))))
        .bind("interesting-function");
  }

 public:
  explicit InterestingFunctionVisitor(MatchFinder& finder,
                                      bool only_in_main_file = false)
      : only_in_main_file_(only_in_main_file) {
    finder.addMatcher(matcher(), this);
  }

  void run(MatchFinder::MatchResult const& Result) override {
    auto* decl = Result.Nodes.getNodeAs<FunctionDecl>("interesting-function");
    assert(decl != nullptr);

    if (only_in_main_file_) {
      ASTContext* context = Result.Context;
      if (!context->getSourceManager().isWrittenInMainFile(
              decl->getBeginLoc())) {
        return;
      }
    }

    InterestingFunction::Insert(decl);

    if (VERBOSE >= kVerboseReportInterestingFunctions) {
      ASTContext* context = Result.Context;
      auto loc = decl->getBeginLoc();
      llvm::outs() << "Func: ";
      loc.print(llvm::outs(), context->getSourceManager());
      llvm::outs() << "\n";
      llvm::outs() << "  name " << decl->getQualifiedNameAsString() << "\n";
      for (const auto& param : decl->parameters()) {
        auto type = param->getOriginalType();
        llvm::outs() << "  param " << param->getFunctionScopeIndex()
                     << " of type " << type.getAsString() << "\n";
      }
      auto result_type = decl->getCallResultType();
      llvm::outs() << "  result " << result_type.getAsString() << "\n";
      llvm::outs() << "  templated kind " << decl->getTemplatedKind() << "\n";
    }
  }

 private:
  bool only_in_main_file_;
};

// Find and record interesting variables.
// - of type Handle<T>; and
// - local variables or parameters.
// ----------------------------------------------------------------------------
class HandleDeclVisitor : public MatchFinder::MatchCallback {
 private:
  static DeclarationMatcher matcher() {
    return anyOf(
        varDecl(allOf(hasLocalStorage(), hasType(handleType),
                      hasAncestor(functionDecl().bind("func-decl"))))
            .bind("var-decl"),
        bindingDecl(allOf(hasType(handleType),
                          hasAncestor(functionDecl().bind("func-decl"))))
            .bind("binding-decl"));
  }

 public:
  explicit HandleDeclVisitor(MatchFinder& finder,
                             bool only_in_main_file = false)
      : only_in_main_file_(only_in_main_file) {
    finder.addMatcher(matcher(), this);
  }

  void run(MatchFinder::MatchResult const& Result) override {
    auto* decl = Result.Nodes.getNodeAs<VarDecl>("var-decl");
    if (decl == nullptr) {
      auto* binding = Result.Nodes.getNodeAs<BindingDecl>("binding-decl");
      assert(binding != nullptr);
      decl = binding->getHoldingVar();
      assert(decl != nullptr);
    }

    if (only_in_main_file_) {
      ASTContext* context = Result.Context;
      if (!context->getSourceManager().isWrittenInMainFile(
              decl->getBeginLoc())) {
        return;
      }
    }

    auto* func_decl = Result.Nodes.getNodeAs<FunctionDecl>("func-decl");
    assert(func_decl != nullptr);

    if (auto* param = dyn_cast<ParmVarDecl>(decl)) {
      auto* ctxt = param->getDeclContext();
      assert(ctxt != nullptr);
      auto type = param->getType();
      auto* func = dyn_cast<FunctionDecl>(ctxt);
      if (func == nullptr) {
        // TODO(42203211): This may happen, for example, if a handle parameter
        // is part of some other parameter's type, e.g.
        //
        //     void f(std::function<void(Handle<HeapObject>)> g);
        //
        // Migrating higher-order functions is out of the scope of this tool
        // right now. For migrating the definition of `f` here, we would need to
        // check all its call sites and see what the actual function passed as
        // the `g` parameter is. If the actual parameter can be migrated in all
        // cases to a function expecting a `DirectHandle`, then `f` can be
        // migrated, otherwise it cannot.
        //
        // Such cases are ignored now and we expect that they be migrated
        // manually.
        llvm::outs() << "Warning: this parameter does not lead to function "
                        "declaration\n";
        ASTContext* context = Result.Context;
        auto loc = decl->getBeginLoc();
        llvm::outs() << "Decl parm: ";
        loc.print(llvm::outs(), context->getSourceManager());
        llvm::outs() << "\n";
        llvm::outs() << "  type " << type.getAsString() << "\n";
        llvm::outs() << "  index " << param->getFunctionScopeIndex() << "\n";
        return;
      }
      if (func != func_decl) {
        llvm::outs() << "Warning: function declaration mismatch\n";
        llvm::outs() << "  func: " << *func << "\n";
        llvm::outs() << "  func_decl: " << *func_decl << "\n";
      }
      auto* f = InterestingFunction::Lookup(func_decl);
      if (f != nullptr && !func_decl->isDefaulted() &&
          !func_decl->isTemplateInstantiation()) {
        auto* p = InterestingHandle::Lookup(param);
        if (p == nullptr) {
          bool is_definition = func_decl->hasBody();
          p = InterestingHandle::Insert(param, is_definition);
          // If there's no function definition, nothing to be done yet.
          if (auto* func_def = func_decl->getDefinition()) {
            if (func_def == func_decl) {
              // If this is the function definition.
              assert(is_definition);
              // Look for all registered declarations of this function.
              for (const auto& prev_decl : func_decl->redecls()) {
                if (auto* d = InterestingFunction::Lookup(prev_decl)) {
                  // Mark the corresponding parameter of the declaration as
                  // dependent to this entry.
                  auto* q = d->parameters()[param->getFunctionScopeIndex()];
                  if (q != nullptr) {
                    p->AddDependent(q);
                  }
                }
              }
            } else if (auto* d = InterestingFunction::Lookup(func_def)) {
              // If there is a registered function definition, mark this entry
              // as dependent to the corresponding parameter of the function
              // definition.
              auto* q = d->parameters()[param->getFunctionScopeIndex()];
              assert(q != nullptr);
              q->AddDependent(p);
            }
          }
        }
        f->AddOrCheckParameter(param, p);
      }

      if (VERBOSE >= kVerboseReportInterestingHandleDecl) {
        ASTContext* context = Result.Context;
        auto loc = decl->getBeginLoc();
        llvm::outs() << "Decl parm: ";
        loc.print(llvm::outs(), context->getSourceManager());
        llvm::outs() << "\n";
        llvm::outs() << "  type " << type.getAsString() << "\n";
        llvm::outs() << "  index " << param->getFunctionScopeIndex() << "\n";
        llvm::outs() << "  func " << func->getQualifiedNameAsString() << "\n";
      }
    } else {
      auto* p = InterestingHandle::Insert(decl, true);
      auto* f = InterestingFunction::Lookup(func_decl);
      if (!func_decl->isDefaulted() && !func_decl->isTemplateInstantiation()) {
        if (f == nullptr) {
          assert(func_decl->hasBody());
          f = InterestingFunction::Insert(func_decl);
        }
        assert(f != nullptr);
        f->AddLocalVariable(p);
      }

      if (VERBOSE >= kVerboseReportInterestingHandleDecl) {
        ASTContext* context = Result.Context;
        auto loc = decl->getBeginLoc();
        llvm::outs() << "Decl var: ";
        loc.print(llvm::outs(), context->getSourceManager());
        llvm::outs() << "\n"
                     << "  type " << decl->getType().getAsString() << "\n";
      }
    }
  }

 private:
  bool only_in_main_file_;
};

// Find and process uses of handle parameters or variables.
//
// Tracking such uses is important, because we want to disallow the migration of
// a variable's type from `Handle<T>` to `DirectHandle<T>` if the variable is
// used (at least once) in a context where a `Handle<T>` is really required.
// In general, if a variable of type `Handle<T>` is used, we need to prevent the
// migration of a variable. This is the purpose of `HandleUseVisitor`. However,
// there are cases when such a variable is used in a manner that is compatible
// with a `DirectHandle<T>`, e.g., when the handle is dereferenced, or
// implicitly converted to a direct handle. In these cases there is no need to
// prevent the migration and this is the purpose of more specific visitors, such
// as `HandleDereferenceVisitor` or `ImplicitHandleToDirectHandleVisitor` below.
//
// As mentioned in the comment before `InterestingHandle::RegisterUsage`, the
// order in which visitors are executed is important. For a given variable
// usage, migration is prevented if the first executed visitor decides to
// prevent it.
//
// Consider the following example:
//
//     void consume_direct(DirectHandle<HeapObject> o);  /* line: 1 */
//     Handle<HeapObject> h;                             /* line: 2 */
//     consume_direct(h);                                /* line: 3 */
//     Tagged<Map> map = h->map();                       /* line: 4 */
//
// The use of variable `h` in line 3 will be processed by two visitors:
//
// 1. `HandleUseVisitor` (this one), which will claim that the use of this
//     variable is reason enough for disallowing its migration.
// 2. `ImplicitHandleToDirectHandleVisitor` (below), which will realize that
//    the handle is implicitly converted to a direct handle, therefore we can
//    allow its migration.
//
// Because visitor 1 is added last to the match finder (we rely on this),
// visitor 2 will run before visitor 1 for this node, thus not preventing the
// migration.
//
// Similarly, the use of variable `h` in line 4 will be processed by two
// visitors: first `HandleDereferenceVisitor` and then `HandleUseVisitor`, in
// this order, and migration will again not be prevented. As none of the
// variable's uses has prevented migration, the type of variable `h` in line 2
// will be migrated from `Handle<HeapObject>` to `DirectHandle<HeapObject>`.
// ----------------------------------------------------------------------------
class HandleUseVisitor : public MatchFinder::MatchCallback {
 public:
  static StatementMatcher matcher() {
    return declRefExpr(
               allOf(to(varDecl().bind("var-decl")), hasType(handleType),
                     hasAncestor(
                         functionDecl(allOf(isDefinition(), hasBody(stmt())))
                             .bind("func-def"))))
        .bind("handle-use");
  }

 public:
  explicit HandleUseVisitor(MatchFinder& finder, bool only_in_main_file = false)
      : only_in_main_file_(only_in_main_file) {
    finder.addMatcher(matcher(), this);
  }

  void run(MatchFinder::MatchResult const& Result) override {
    auto* use = Result.Nodes.getNodeAs<DeclRefExpr>("handle-use");
    assert(use != nullptr);
    auto* decl = Result.Nodes.getNodeAs<VarDecl>("var-decl");
    assert(decl != nullptr);
    auto* func = Result.Nodes.getNodeAs<FunctionDecl>("func-def");
    assert(func != nullptr);

    if (only_in_main_file_) {
      ASTContext* context = Result.Context;
      if (!context->getSourceManager().isWrittenInMainFile(
              use->getBeginLoc())) {
        return;
      }
    }

    auto* h = InterestingHandle::Lookup(decl);
    assert(h != nullptr || func->isDefaulted() ||
           func->isTemplateInstantiation());

    if (VERBOSE >= kVerboseReportInterestingHandleUse) {
      auto type = decl->getType();
      ASTContext* context = Result.Context;
      auto loc = use->getBeginLoc();
      llvm::outs() << "Use var: ";
      loc.print(llvm::outs(), context->getSourceManager());
      llvm::outs() << "\n";
      llvm::outs() << "  var of type " << type.getAsString() << "\n";
    }

    if (h != nullptr) {
      // This will disallow migration, unless some other more specific visitor
      // has already run and explicitly allowed migration for the same variable
      // usage. Here, we rely on the fact that `HandleUseVisitor` is added last
      // to the match finder, therefore it will run last for any given AST node.
      h->RegisterUsage(use, false);
    }
  }

 private:
  bool only_in_main_file_;
};

// Find and process calls to interesting functions.
// Currently, this does nothing interesting except for logging.
// ----------------------------------------------------------------------------
class CallExprWithHandleVisitor : public MatchFinder::MatchCallback {
 private:
  static StatementMatcher matcher() {
    return callExpr(hasAnyArgument(hasType(handleType))).bind("call-expr");
  }

 public:
  explicit CallExprWithHandleVisitor(MatchFinder& finder,
                                     bool only_in_main_file = false)
      : only_in_main_file_(only_in_main_file) {
    finder.addMatcher(matcher(), this);
  }

  void run(MatchFinder::MatchResult const& Result) override {
    auto* call = Result.Nodes.getNodeAs<CallExpr>("call-expr");
    assert(call != nullptr);

    if (only_in_main_file_) {
      ASTContext* context = Result.Context;
      if (!context->getSourceManager().isWrittenInMainFile(
              call->getBeginLoc())) {
        return;
      }
    }

    auto* decl = call->getCalleeDecl();
    // This happens when we have a call with an unresolved expression in a
    // template definition.
    if (decl == nullptr) {
      return;
    }
    auto* func = dyn_cast<FunctionDecl>(decl);
    if (func == nullptr) {
      llvm::outs() << "Warning: This is not a FunctionDecl but a "
                   << decl->getDeclKindName() << "\n";
      return;
    }

    if (VERBOSE >= kVerboseReportInterestingFunctionCall) {
      ASTContext* context = Result.Context;
      auto loc = call->getBeginLoc();
      llvm::outs() << "Call: ";
      loc.print(llvm::outs(), context->getSourceManager());
      llvm::outs() << "\n";
      llvm::outs() << "  callee " << func->getQualifiedNameAsString() << "\n";

      int i = 0;
      for (const auto& arg : call->arguments()) {
        auto type = arg->getType();
        llvm::outs() << "  param " << i << " of type " << type.getAsString()
                     << "\n";
        ++i;
      }
    }
  }

 private:
  bool only_in_main_file_;
};

// Implicit Handle<T> -> DirectHandle<T> conversions.
// They do not prevent handle migration.
// ----------------------------------------------------------------------------
class ImplicitHandleToDirectHandleVisitor : public MatchFinder::MatchCallback {
 private:
  static StatementMatcher matcher() {
    return implicitCastExpr(
               allOf(hasImplicitDestinationType(directHandleType),
                     hasCastKind(CK_ConstructorConversion),
                     hasSourceExpression(
                         constructorWithOneArgument(constructorWithOneArgument(
                             HandleUseVisitor::matcher())))))
        .bind("implicit-cast");
  }

 public:
  explicit ImplicitHandleToDirectHandleVisitor(MatchFinder& finder,
                                               bool only_in_main_file = false)
      : only_in_main_file_(only_in_main_file) {
    finder.addMatcher(matcher(), this);
  }

  void run(MatchFinder::MatchResult const& Result) override {
    auto* expr = Result.Nodes.getNodeAs<ImplicitCastExpr>("implicit-cast");
    assert(expr != nullptr);

    if (only_in_main_file_) {
      ASTContext* context = Result.Context;
      if (!context->getSourceManager().isWrittenInMainFile(
              expr->getBeginLoc())) {
        return;
      }
    }

    if (VERBOSE >= kVerboseReportImplicitHandleConversion) {
      ASTContext* context = Result.Context;
      auto loc = expr->getBeginLoc();
      llvm::outs() << "H->DH: ";
      loc.print(llvm::outs(), context->getSourceManager());
      llvm::outs() << "\n";
    }

    auto* use = Result.Nodes.getNodeAs<DeclRefExpr>("handle-use");
    assert(use != nullptr);
    auto* decl = Result.Nodes.getNodeAs<VarDecl>("var-decl");
    assert(decl != nullptr);

    auto* h = InterestingHandle::Lookup(decl);
    if (h != nullptr) {
      // This will allow migration for this variable usage.
      h->RegisterUsage(use, true);
    }
  }

 private:
  bool only_in_main_file_;
};

// Handle<T>::operator* and Handle<T>::operator->
// They do not prevent handle migration.
// ----------------------------------------------------------------------------
class HandleDereferenceVisitor : public MatchFinder::MatchCallback {
 private:
  static StatementMatcher matcher() {
    return cxxOperatorCallExpr(
               hasAnyOverloadedOperatorName("*", "->"),
               hasAnyArgument(ignoringImplicit(HandleUseVisitor::matcher())))
        .bind("handle-deref");
  }

 public:
  explicit HandleDereferenceVisitor(MatchFinder& finder,
                                    bool only_in_main_file = false)
      : only_in_main_file_(only_in_main_file) {
    finder.addMatcher(matcher(), this);
  }

  void run(MatchFinder::MatchResult const& Result) override {
    auto* expr = Result.Nodes.getNodeAs<CXXOperatorCallExpr>("handle-deref");
    assert(expr != nullptr);

    if (only_in_main_file_) {
      ASTContext* context = Result.Context;
      if (!context->getSourceManager().isWrittenInMainFile(
              expr->getBeginLoc())) {
        return;
      }
    }

    if (VERBOSE >= kVerboseReportHandleDereference) {
      ASTContext* context = Result.Context;
      auto loc = expr->getBeginLoc();
      llvm::outs() << "Handle deref: ";
      loc.print(llvm::outs(), context->getSourceManager());
      llvm::outs() << "\n";
    }

    auto* use = Result.Nodes.getNodeAs<DeclRefExpr>("handle-use");
    assert(use != nullptr);
    auto* decl = Result.Nodes.getNodeAs<VarDecl>("var-decl");
    assert(decl != nullptr);

    auto* h = InterestingHandle::Lookup(decl);
    if (h != nullptr) {
      // This will allow migration for this variable usage.
      h->RegisterUsage(use, true);
    }
  }

 private:
  bool only_in_main_file_;
};

// Main program.
// ----------------------------------------------------------------------------

int main(int argc, const char* argv[]) {
  auto expected_parser =
      CommonOptionsParser::create(argc, argv, my_tool_category);
  if (!expected_parser) {
    // Fail gracefully for unsupported options.
    llvm::errs() << expected_parser.takeError();
    return 1;
  }
  CommonOptionsParser& options_parser = expected_parser.get();
  ClangTool Tool(options_parser.getCompilations(),
                 options_parser.getSourcePathList());

  MatchFinder finder;
  std::optional<WhereWeAreVisitor> where_we_are_visitor;
  if (VERBOSE >= kVerboseWhereAreWe) {
    where_we_are_visitor.emplace(finder);
  }

  // These populate the database of functions and handle declarations.
  InterestingFunctionVisitor interesting_function_visitor(finder,
                                                          only_in_main_file);
  HandleDeclVisitor handle_decl_visitor(finder, only_in_main_file);
  // These allow migration in some special cases.
  HandleDereferenceVisitor handle_deref_callback(finder, only_in_main_file);
  ImplicitHandleToDirectHandleVisitor implicit_conversion_visitor(
      finder, only_in_main_file);
  // This is not used, except for logging.
  std::optional<CallExprWithHandleVisitor> call_expr_with_handle_visitor;
  if (VERBOSE >= kVerboseReportInterestingFunctionCall) {
    call_expr_with_handle_visitor.emplace(finder, only_in_main_file);
  }
  // This needs to be last, to disallow migration in all other cases.
  HandleUseVisitor handle_use_callback(finder, only_in_main_file);

  int error_code = Tool.run(newFrontendActionFactory(&finder).get());
  if (error_code) {
    return error_code;
  }

  std::set<Replacement> replacements = InterestingFunction::GetReplacements();
  if (replacements.empty()) {
    return 0;
  }

  // Serialization format is documented in tools/clang/scripts/run_tool.py
  llvm::outs() << "==== BEGIN EDITS ====\n";
  for (const auto& r : replacements) {
    std::string replacement_text = r.getReplacementText().str();
    std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');
    llvm::outs() << "r:::" << r.getFilePath() << ":::" << r.getOffset()
                 << ":::" << r.getLength() << ":::" << replacement_text << "\n";
  }
  llvm::outs() << "==== END EDITS ====\n";

  return 0;
}
