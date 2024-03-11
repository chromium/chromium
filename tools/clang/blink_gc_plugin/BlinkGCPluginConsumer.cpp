// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "BlinkGCPluginConsumer.h"

#include <algorithm>
#include <set>

#include "BadPatternFinder.h"
#include "CheckDispatchVisitor.h"
#include "CheckFieldsVisitor.h"
#include "CheckFinalizerVisitor.h"
#include "CheckForbiddenFieldsVisitor.h"
#include "CheckGCRootsVisitor.h"
#include "CheckTraceVisitor.h"
#include "CollectVisitor.h"
#include "JsonWriter.h"
#include "RecordInfo.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/TimeProfiler.h"

using namespace clang;

namespace {

// Use a local RAV implementation to simply collect all FunctionDecls marked for
// late template parsing. This happens with the flag -fdelayed-template-parsing,
// which is on by default in MSVC-compatible mode.
std::set<FunctionDecl*> GetLateParsedFunctionDecls(TranslationUnitDecl* decl) {
  struct Visitor : public RecursiveASTVisitor<Visitor> {
    bool VisitFunctionDecl(FunctionDecl* function_decl) {
      if (function_decl->isLateTemplateParsed())
        late_parsed_decls.insert(function_decl);
      return true;
    }

    std::set<FunctionDecl*> late_parsed_decls;
  } v;
  v.TraverseDecl(decl);
  return v.late_parsed_decls;
}

class EmptyStmtVisitor : public RecursiveASTVisitor<EmptyStmtVisitor> {
 public:
  static bool isEmpty(Stmt* stmt) {
    EmptyStmtVisitor visitor;
    visitor.TraverseStmt(stmt);
    return visitor.empty_;
  }

  bool WalkUpFromCompoundStmt(CompoundStmt* stmt) {
    empty_ = stmt->body_empty();
    return false;
  }
  bool VisitStmt(Stmt*) {
    empty_ = false;
    return false;
  }
 private:
  EmptyStmtVisitor() : empty_(true) {}
  bool empty_;
};

const CXXRecordDecl* GetFirstTemplateArgAsCXXRecordDecl(
    const CXXRecordDecl* gc_base) {
  if (const auto* gc_base_template_id =
          dyn_cast<ClassTemplateSpecializationDecl>(gc_base)) {
    const TemplateArgumentList& gc_args =
        gc_base_template_id->getTemplateArgs();
    if (!gc_args.size() || gc_args[0].getKind() != TemplateArgument::Type)
      return nullptr;
    return gc_args[0].getAsType()->getAsCXXRecordDecl();
  }
  return nullptr;
}

}  // namespace

BlinkGCPluginConsumer::BlinkGCPluginConsumer(
    clang::CompilerInstance& instance,
    const BlinkGCPluginOptions& options)
    : instance_(instance),
      reporter_(instance),
      options_(options),
      cache_(instance),
      json_(0) {
  // Only check structures in blink, cppgc and pdfium.
  options_.checked_namespaces.insert("blink");
  options_.checked_namespaces.insert("cppgc");

  // Add Pdfium subfolders containing GCed classes.
  options_.checked_directories.push_back("fpdfsdk/");
  options_.checked_directories.push_back("fxjs/");
  options_.checked_directories.push_back("xfa/");

  // Ignore GC implementation files.
  options_.ignored_directories.push_back(
      "third_party/blink/renderer/platform/heap/collection_support/");
  options_.ignored_directories.push_back("v8/src/heap/cppgc/");
  options_.ignored_directories.push_back("v8/src/heap/cppgc-js/");
}

void BlinkGCPluginConsumer::HandleTranslationUnit(ASTContext& context) {
  llvm::TimeTraceScope TimeScope(
      "BlinkGCPluginConsumer::HandleTranslationUnit");
  // Don't run the plugin if the compilation unit is already invalid.
  if (reporter_.hasErrorOccurred())
    return;

  ParseFunctionTemplates(context.getTranslationUnitDecl());

  CollectVisitor visitor;
  visitor.TraverseDecl(context.getTranslationUnitDecl());

  if (options_.dump_graph) {
    std::error_code err;
    SmallString<128> OutputFile(instance_.getFrontendOpts().OutputFile);
    llvm::sys::path::replace_extension(OutputFile, "graph.json");
    json_ = JsonWriter::from(instance_.createOutputFile(
        OutputFile,                              // OutputPath
        true,                                    // Binary
        true,                                    // RemoveFileOnSignal
        false,                                   // UseTemporary
        false));                                 // CreateMissingDirectories
    if (!err && json_) {
      json_->OpenList();
    } else {
      json_ = 0;
      llvm::errs()
          << "[blink-gc] "
          << "Failed to create an output file for the object graph.\n";
    }
  }

  for (const auto& record : visitor.record_decls())
    CheckRecord(cache_.Lookup(record));

  for (const auto& method : visitor.trace_decls())
    CheckTracingMethod(method);

  if (json_) {
    json_->CloseList();
    delete json_;
    json_ = 0;
  }

  FindBadPatterns(context, reporter_, cache_, options_);
}

void BlinkGCPluginConsumer::ParseFunctionTemplates(TranslationUnitDecl* decl) {
  if (!instance_.getLangOpts().DelayedTemplateParsing)
    return;  // Nothing to do.

  std::set<FunctionDecl*> late_parsed_decls = GetLateParsedFunctionDecls(decl);
  clang::Sema& sema = instance_.getSema();

  for (const FunctionDecl* fd : late_parsed_decls) {
    assert(fd->isLateTemplateParsed());

    if (!Config::IsTraceMethod(fd))
      continue;

    if (instance_.getSourceManager().isInSystemHeader(
            instance_.getSourceManager().getSpellingLoc(fd->getLocation())))
      continue;

    // Force parsing and AST building of the yet-uninstantiated function
    // template trace method bodies.
    clang::LateParsedTemplate* lpt = sema.LateParsedTemplateMap[fd].get();
    sema.LateTemplateParser(sema.OpaqueParser, *lpt);
  }
}

void BlinkGCPluginConsumer::CheckRecord(RecordInfo* info) {
  if (IsIgnored(info))
    return;

  CXXRecordDecl* record = info->record();

  // TODO: what should we do to check unions?
  if (record->isUnion())
    return;

  // If this is the primary template declaration, check its specializations.
  if (record->isThisDeclarationADefinition() &&
      record->getDescribedClassTemplate()) {
    ClassTemplateDecl* tmpl = record->getDescribedClassTemplate();
    for (ClassTemplateDecl::spec_iterator it = tmpl->spec_begin();
         it != tmpl->spec_end();
         ++it) {
      CheckClass(cache_.Lookup(*it));
    }
    return;
  }

  CheckClass(info);
}

void BlinkGCPluginConsumer::CheckClass(RecordInfo* info) {
  if (!info)
    return;

  if (CXXMethodDecl* trace = info->GetTraceMethod()) {
    if (info->IsStackAllocated())
      reporter_.TraceMethodForStackAllocatedClass(info, trace);
    if (trace->isPureVirtual())
      reporter_.ClassDeclaresPureVirtualTrace(info, trace);
  } else if (info->RequiresTraceMethod()) {
    reporter_.ClassRequiresTraceMethod(info);
  }

  // Check polymorphic classes that are GC-derived or have a trace method.
  if (info->record()->hasDefinition() && info->record()->isPolymorphic()) {
    // TODO: Check classes that inherit a trace method.
    CXXMethodDecl* trace = info->GetTraceMethod();
    if (trace || info->IsGCDerived())
      CheckPolymorphicClass(info, trace);
  }

  {
    CheckFieldsVisitor visitor(options_);
    if (visitor.ContainsInvalidFields(info))
      reporter_.ClassContainsInvalidFields(info, visitor.invalid_fields());
  }

  if (info->IsGCDerived()) {
    // Check that CRTP pattern for GCed classes is correctly used.
    if (auto* base_spec = info->GetDirectGCBase()) {
      // Skip the check if base_spec name is dependent. The check will occur
      // later for actual specializations.
      if (!base_spec->getType()->isDependentType()) {
        const CXXRecordDecl* base_decl =
            base_spec->getType()->getAsCXXRecordDecl();
        const CXXRecordDecl* first_arg =
            GetFirstTemplateArgAsCXXRecordDecl(base_decl);
        // The last check is for redeclaratation cases, for example, when
        // explicit instantiation declaration is followed by the corresponding
        // explicit instantiation definition.
        if (!first_arg ||
            first_arg->getFirstDecl() != info->record()->getFirstDecl()) {
          reporter_.ClassMustCRTPItself(info, base_decl, base_spec);
        }
      }
    }

    // It is illegal for a class to be both stack allocated and garbage
    // collected.
    if (info->IsStackAllocated()) {
      for (auto& base : info->GetBases()) {
        RecordInfo* base_info = base.second.info();
        if (Config::IsGCBase(base_info->name()) || base_info->IsGCDerived()) {
          reporter_.StackAllocatedDerivesGarbageCollected(info, &base.second);
        }
      }
    }

    if (!info->IsGCMixin()) {
      CheckLeftMostDerived(info);
      CheckDispatch(info);
      if (CXXMethodDecl* newop = info->DeclaresNewOperator()) {
        if (!info->IsStackAllocated() &&
            !Config::IsGCBase(newop->getParent()->getName()) &&
            !Config::IsIgnoreAnnotated(newop)) {
          reporter_.ClassOverridesNew(info, newop);
        }
      }
    }

    {
      CheckGCRootsVisitor visitor(options_);
      if (visitor.ContainsGCRoots(info))
        reporter_.ClassContainsGCRoots(info, visitor.gc_roots());
      reporter_.ClassContainsGCRootRefs(info, visitor.gc_root_refs());
    }

    CheckForbiddenFieldsVisitor visitor;
    if (visitor.ContainsForbiddenFields(info)) {
      reporter_.ClassContainsForbiddenFields(info, visitor.forbidden_fields());
    }

    if (info->NeedsFinalization())
      CheckFinalization(info);
  }

  DumpClass(info);
}

CXXRecordDecl* BlinkGCPluginConsumer::GetDependentTemplatedDecl(
    const Type& type) {
  const TemplateSpecializationType* tmpl_type =
      type.getAs<TemplateSpecializationType>();
  if (!tmpl_type)
    return 0;

  TemplateDecl* tmpl_decl = tmpl_type->getTemplateName().getAsTemplateDecl();
  if (!tmpl_decl)
    return 0;

  return dyn_cast<CXXRecordDecl>(tmpl_decl->getTemplatedDecl());
}

// The GC infrastructure assumes that if the vtable of a polymorphic
// base-class is not initialized for a given object (ie, it is partially
// initialized) then the object does not need to be traced. Thus, we must
// ensure that any polymorphic class with a trace method does not have any
// tractable fields that are initialized before we are sure that the vtable
// and the trace method are both defined.  There are two cases that need to
// hold to satisfy that assumption:
//
// 1. If trace is virtual, then it must be defined in the left-most base.
// This ensures that if the vtable is initialized then it contains a pointer
// to the trace method.
//
// 2. If trace is non-virtual, then the trace method is defined and we must
// ensure that the left-most base defines a vtable. This ensures that the
// first thing to be initialized when constructing the object is the vtable
// itself.
void BlinkGCPluginConsumer::CheckPolymorphicClass(
    RecordInfo* info,
    CXXMethodDecl* trace) {
  CXXRecordDecl* left_most = info->record();
  CXXRecordDecl::base_class_iterator it = left_most->bases_begin();
  CXXRecordDecl* left_most_base = 0;
  while (it != left_most->bases_end()) {
    left_most_base = it->getType()->getAsCXXRecordDecl();
    if (!left_most_base && it->getType()->isDependentType())
      left_most_base = RecordInfo::GetDependentTemplatedDecl(*it->getType());

    // TODO: Find a way to correctly check actual instantiations
    // for dependent types. The escape below will be hit, eg, when
    // we have a primary template with no definition and
    // specializations for each case (such as SupplementBase) in
    // which case we don't succeed in checking the required
    // properties.
    if (!left_most_base || !left_most_base->hasDefinition())
      return;

    StringRef name = left_most_base->getName();
    // We know GCMixin base defines virtual trace.
    if (Config::IsGCMixinBase(name))
      return;

    // Stop with the left-most prior to a safe polymorphic base (a safe base
    // is non-polymorphic and contains no fields).
    if (Config::IsSafePolymorphicBase(name))
      break;

    left_most = left_most_base;
    it = left_most->bases_begin();
  }

  if (RecordInfo* left_most_info = cache_.Lookup(left_most)) {
    // Check condition (1):
    if (trace && trace->isVirtual()) {
      if (CXXMethodDecl* trace = left_most_info->GetTraceMethod()) {
        if (trace->isVirtual())
          return;
      }
      reporter_.BaseClassMustDeclareVirtualTrace(info, left_most);
      return;
    }

    // Check condition (2):
    if (DeclaresVirtualMethods(left_most))
      return;
    if (left_most_base) {
      // Get the base next to the "safe polymorphic base"
      if (it != left_most->bases_end())
        ++it;
      if (it != left_most->bases_end()) {
        if (CXXRecordDecl* next_base = it->getType()->getAsCXXRecordDecl()) {
          if (CXXRecordDecl* next_left_most = GetLeftMostBase(next_base)) {
            if (DeclaresVirtualMethods(next_left_most))
              return;
            reporter_.LeftMostBaseMustBePolymorphic(info, next_left_most);
            return;
          }
        }
      }
    }
    reporter_.LeftMostBaseMustBePolymorphic(info, left_most);
  }
}

CXXRecordDecl* BlinkGCPluginConsumer::GetLeftMostBase(
    CXXRecordDecl* left_most) {
  CXXRecordDecl::base_class_iterator it = left_most->bases_begin();
  while (it != left_most->bases_end()) {
    if (it->getType()->isDependentType())
      left_most = RecordInfo::GetDependentTemplatedDecl(*it->getType());
    else
      left_most = it->getType()->getAsCXXRecordDecl();
    if (!left_most || !left_most->hasDefinition())
      return 0;
    it = left_most->bases_begin();
  }
  return left_most;
}

bool BlinkGCPluginConsumer::DeclaresVirtualMethods(CXXRecordDecl* decl) {
  CXXRecordDecl::method_iterator it = decl->method_begin();
  for (; it != decl->method_end(); ++it)
    if (it->isVirtual() && !it->isPureVirtual())
      return true;
  return false;
}

void BlinkGCPluginConsumer::CheckLeftMostDerived(RecordInfo* info) {
  CXXRecordDecl* left_most = GetLeftMostBase(info->record());
  if (!left_most)
    return;
  if (!Config::IsGCBase(left_most->getName()) || Config::IsGCMixinBase(left_most->getName()))
    reporter_.ClassMustLeftMostlyDeriveGC(info);
}

void BlinkGCPluginConsumer::CheckDispatch(RecordInfo* info) {
  CXXMethodDecl* trace_dispatch = info->GetTraceDispatchMethod();
  CXXMethodDecl* finalize_dispatch = info->GetFinalizeDispatchMethod();
  if (!trace_dispatch && !finalize_dispatch)
    return;

  CXXRecordDecl* base = trace_dispatch ? trace_dispatch->getParent()
                                       : finalize_dispatch->getParent();

  // Check that dispatch methods are defined at the base.
  if (base == info->record()) {
    if (!trace_dispatch)
      reporter_.MissingTraceDispatchMethod(info);
  }

  // Check that classes implementing manual dispatch do not have vtables.
  if (info->record()->isPolymorphic()) {
    reporter_.VirtualAndManualDispatch(
        info, trace_dispatch ? trace_dispatch : finalize_dispatch);
  }

  // If this is a non-abstract class check that it is dispatched to.
  // TODO: Create a global variant of this local check. We can only check if
  // the dispatch body is known in this compilation unit.
  if (info->IsConsideredAbstract())
    return;

  const FunctionDecl* defn;

  if (trace_dispatch && trace_dispatch->isDefined(defn)) {
    CheckDispatchVisitor visitor(info);
    visitor.TraverseStmt(defn->getBody());
    if (!visitor.dispatched_to_receiver())
      reporter_.MissingTraceDispatch(defn, info);
  }

  if (finalize_dispatch && finalize_dispatch->isDefined(defn)) {
    CheckDispatchVisitor visitor(info);
    visitor.TraverseStmt(defn->getBody());
    if (!visitor.dispatched_to_receiver())
      reporter_.MissingFinalizeDispatch(defn, info);
  }
}

// TODO: Should we collect destructors similar to trace methods?
void BlinkGCPluginConsumer::CheckFinalization(RecordInfo* info) {
  CXXDestructorDecl* dtor = info->record()->getDestructor();
  if (!dtor || !dtor->hasBody())
    return;

  CheckFinalizerVisitor visitor(&cache_);
  visitor.TraverseCXXMethodDecl(dtor);
  if (!visitor.finalized_fields().empty()) {
    reporter_.FinalizerAccessesFinalizedFields(dtor,
                                               visitor.finalized_fields());
  }
}

void BlinkGCPluginConsumer::CheckTracingMethod(CXXMethodDecl* method) {
  RecordInfo* parent = cache_.Lookup(method->getParent());
  if (IsIgnored(parent))
    return;

  // Check templated tracing methods by checking the template instantiations.
  // Specialized templates are handled as ordinary classes.
  if (ClassTemplateDecl* tmpl =
      parent->record()->getDescribedClassTemplate()) {
    for (ClassTemplateDecl::spec_iterator it = tmpl->spec_begin();
         it != tmpl->spec_end();
         ++it) {
      // Check trace using each template instantiation as the holder.
      if (Config::IsTemplateInstantiation(*it))
        CheckTraceOrDispatchMethod(cache_.Lookup(*it), method);
    }
    return;
  }

  CheckTraceOrDispatchMethod(parent, method);
}

void BlinkGCPluginConsumer::CheckTraceOrDispatchMethod(
    RecordInfo* parent,
    CXXMethodDecl* method) {
  Config::TraceMethodType trace_type = Config::GetTraceMethodType(method);
  if (trace_type == Config::TRACE_AFTER_DISPATCH_METHOD ||
      !parent->GetTraceDispatchMethod()) {
    CheckTraceMethod(parent, method, trace_type);
  }
  // Dispatch methods are checked when we identify subclasses.
}

void BlinkGCPluginConsumer::CheckTraceMethod(
    RecordInfo* parent,
    CXXMethodDecl* trace,
    Config::TraceMethodType trace_type) {
  // A trace method must not override any non-virtual trace methods.
  if (trace_type == Config::TRACE_METHOD) {
    for (auto& base : parent->GetBases())
      if (CXXMethodDecl* other = base.second.info()->InheritsNonVirtualTrace())
        reporter_.OverriddenNonVirtualTrace(parent, trace, other);
  }

  CheckTraceVisitor visitor(trace, parent, &cache_);
  visitor.TraverseCXXMethodDecl(trace);

  for (auto& base : parent->GetBases())
    if (!base.second.IsProperlyTraced())
      reporter_.BaseRequiresTracing(parent, trace, base.first);

  for (auto& field : parent->GetFields()) {
    if (!field.second.IsProperlyTraced() ||
        field.second.IsInproperlyTraced()) {
      // Report one or more tracing-related field errors.
      reporter_.FieldsImproperlyTraced(parent, trace);
      break;
    }
  }
}

void BlinkGCPluginConsumer::DumpClass(RecordInfo* info) {
  if (!json_)
    return;

  json_->OpenObject();
  json_->Write("name", info->record()->getQualifiedNameAsString());
  json_->Write("loc", GetLocString(info->record()->getBeginLoc()));
  json_->CloseObject();

  class DumpEdgeVisitor : public RecursiveEdgeVisitor {
   public:
    DumpEdgeVisitor(JsonWriter* json) : json_(json) {}
    void DumpEdge(RecordInfo* src,
                  RecordInfo* dst,
                  const std::string& lbl,
                  const Edge::LivenessKind& kind,
                  const std::string& loc) {
      json_->OpenObject();
      json_->Write("src", src->record()->getQualifiedNameAsString());
      json_->Write("dst", dst->record()->getQualifiedNameAsString());
      json_->Write("lbl", lbl);
      json_->Write("kind", kind);
      json_->Write("loc", loc);
      json_->Write("ptr",
                   !Parent() ? "val" :
                   Parent()->IsRawPtr() ?
                       (static_cast<RawPtr*>(Parent())->HasReferenceType() ?
                        "reference" : "raw") :
                   Parent()->IsRefPtr() ? "ref" :
                   Parent()->IsUniquePtr() ? "unique" :
                   (Parent()->IsMember() || Parent()->IsWeakMember()) ? "mem" :
                   "val");
      json_->CloseObject();
    }

    void DumpField(RecordInfo* src, FieldPoint* point, const std::string& loc) {
      src_ = src;
      point_ = point;
      loc_ = loc;
      point_->edge()->Accept(this);
    }

    void AtValue(Value* e) override {
      // The liveness kind of a path from the point to this value
      // is given by the innermost place that is non-strong.
      Edge::LivenessKind kind = Edge::kStrong;
      for (Context::iterator it = context().begin(); it != context().end();
           ++it) {
        Edge::LivenessKind pointer_kind = (*it)->Kind();
        if (pointer_kind != Edge::kStrong) {
          kind = pointer_kind;
          break;
        }
      }
      DumpEdge(
          src_, e->value(), point_->field()->getNameAsString(), kind, loc_);
    }

   private:
    JsonWriter* json_;
    RecordInfo* src_;
    FieldPoint* point_;
    std::string loc_;
  };

  DumpEdgeVisitor visitor(json_);

  for (auto& base : info->GetBases())
    visitor.DumpEdge(info, base.second.info(), "<super>", Edge::kStrong,
                     GetLocString(base.second.spec().getBeginLoc()));

  for (auto& field : info->GetFields())
    visitor.DumpField(info, &field.second,
                      GetLocString(field.second.field()->getBeginLoc()));
}

std::string BlinkGCPluginConsumer::GetLocString(SourceLocation loc) {
  const SourceManager& source_manager = instance_.getSourceManager();
  PresumedLoc ploc = source_manager.getPresumedLoc(loc);
  if (ploc.isInvalid())
    return "";
  std::string loc_str;
  llvm::raw_string_ostream os(loc_str);
  os << ploc.getFilename()
     << ":" << ploc.getLine()
     << ":" << ploc.getColumn();
  return os.str();
}

bool BlinkGCPluginConsumer::IsIgnored(RecordInfo* record) {
  return (!record || !InCheckedNamespaceOrDirectory(record) ||
          IsIgnoredClass(record) || InIgnoredDirectory(record));
}

bool BlinkGCPluginConsumer::IsIgnoredClass(RecordInfo* info) {
  // Ignore any class prefixed by SameSizeAs. These are used in
  // Blink to verify class sizes and don't need checking.
  const std::string SameSizeAs = "SameSizeAs";
  if (info->name().compare(0, SameSizeAs.size(), SameSizeAs) == 0)
    return true;
  return (options_.ignored_classes.find(info->name()) !=
          options_.ignored_classes.end());
}

bool BlinkGCPluginConsumer::InIgnoredDirectory(RecordInfo* info) {
  std::string filename;
  if (!GetFilename(info->record()->getBeginLoc(), &filename))
    return false;  // TODO: should we ignore non-existing file locations?
#if defined(_WIN32)
  std::replace(filename.begin(), filename.end(), '\\', '/');
#endif
  for (const auto& ignored_dir : options_.ignored_directories)
    if (filename.find(ignored_dir) != std::string::npos) {
      return true;
    }
  return false;
}

bool BlinkGCPluginConsumer::InCheckedNamespaceOrDirectory(RecordInfo* info) {
  if (!info)
    return false;
  for (DeclContext* context = info->record()->getDeclContext();
       !context->isTranslationUnit();
       context = context->getParent()) {
    if (NamespaceDecl* decl = dyn_cast<NamespaceDecl>(context)) {
      if (decl->isAnonymousNamespace())
        return true;
      if (options_.checked_namespaces.find(decl->getNameAsString()) !=
          options_.checked_namespaces.end()) {
        return true;
      }
    }
  }
  std::string filename;
  if (!GetFilename(info->record()->getBeginLoc(), &filename)) {
    return false;
  }
#if defined(_WIN32)
  std::replace(filename.begin(), filename.end(), '\\', '/');
#endif
  for (const auto& checked_dir : options_.checked_directories) {
    if (filename.find(checked_dir) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool BlinkGCPluginConsumer::GetFilename(SourceLocation loc,
                                        std::string* filename) {
  const SourceManager& source_manager = instance_.getSourceManager();
  SourceLocation spelling_location = source_manager.getSpellingLoc(loc);
  PresumedLoc ploc = source_manager.getPresumedLoc(spelling_location);
  if (ploc.isInvalid()) {
    // If we're in an invalid location, we're looking at things that aren't
    // actually stated in the source.
    return false;
  }
  *filename = ploc.getFilename();
  return true;
}
