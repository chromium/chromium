// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Config.h"
#include "RecordInfo.h"
#include "clang/Sema/Sema.h"

using namespace clang;
using std::string;

RecordInfo::RecordInfo(CXXRecordDecl* record, RecordCache* cache)
    : cache_(cache),
      record_(record),
      name_(record->getName()),
      fields_need_tracing_(TracingStatus::Unknown()),
      bases_(0),
      fields_(0),
      is_stack_allocated_(kNotComputed),
      is_non_newable_(kNotComputed),
      is_only_placement_newable_(kNotComputed),
      does_need_finalization_(kNotComputed),
      has_gc_mixin_methods_(kNotComputed),
      is_declaring_local_trace_(kNotComputed),
      determined_trace_methods_(false),
      trace_method_(0),
      trace_dispatch_method_(0),
      finalize_dispatch_method_(0),
      is_gc_derived_(false),
      directly_derived_gc_base_(nullptr) {}

RecordInfo::~RecordInfo() {
  delete fields_;
  delete bases_;
}

// Get |count| number of template arguments. Returns false if there
// are fewer than |count| arguments or any of the arguments are not
// of a valid Type structure. If |count| is non-positive, all
// arguments are collected.
bool RecordInfo::GetTemplateArgs(size_t count, TemplateArgs* output_args) {
  ClassTemplateSpecializationDecl* tmpl =
      dyn_cast<ClassTemplateSpecializationDecl>(record_);
  if (!tmpl)
    return false;
  const TemplateArgumentList& args = tmpl->getTemplateArgs();
  if (args.size() < count)
    return false;
  if (count <= 0)
    count = args.size();
  for (unsigned i = 0; i < count; ++i) {
    TemplateArgument arg = args[i];
    if (arg.getKind() == TemplateArgument::Type && !arg.getAsType().isNull()) {
      output_args->push_back(arg.getAsType().getTypePtr());
    } else {
      return false;
    }
  }
  return true;
}

// Test if a record is a HeapAllocated collection.
bool RecordInfo::IsHeapAllocatedCollection() {
  if (!Config::IsGCCollection(name_) && !Config::IsWTFCollection(name_))
    return false;

  TemplateArgs args;
  if (GetTemplateArgs(0, &args)) {
    for (TemplateArgs::iterator it = args.begin(); it != args.end(); ++it) {
      if (CXXRecordDecl* decl = (*it)->getAsCXXRecordDecl())
        if (decl->getName() == kHeapAllocatorName)
          return true;
    }
  }

  return Config::IsGCCollection(name_);
}

bool RecordInfo::HasOptionalFinalizer() {
  if (!IsHeapAllocatedCollection())
    return false;
  // Heap collections may have a finalizer but it is optional (i.e. may be
  // delayed until FinalizeGarbageCollectedObject() gets called), unless there
  // is an inline buffer. Vector, Deque, and ListHashSet can have an inline
  // buffer.
  if (name_ != "Vector" && name_ != "Deque" && name_ != "HeapVector" &&
      name_ != "HeapDeque")
    return true;
  ClassTemplateSpecializationDecl* tmpl =
      dyn_cast<ClassTemplateSpecializationDecl>(record_);
  // These collections require template specialization so tmpl should always be
  // non-null for valid code.
  if (!tmpl)
    return false;
  const TemplateArgumentList& args = tmpl->getTemplateArgs();
  if (args.size() < 2)
    return true;
  TemplateArgument arg = args[1];
  // The second template argument must be void or 0 so there is no inline
  // buffer.
  return (arg.getKind() == TemplateArgument::Type &&
          arg.getAsType()->isVoidType()) ||
         (arg.getKind() == TemplateArgument::Integral &&
          arg.getAsIntegral().getExtValue() == 0);
}

// Test if a record is derived from a garbage collected base.
bool RecordInfo::IsGCDerived() {
  // If already computed, return the known result.
  if (gc_base_names_.size())
    return is_gc_derived_;

  if (!record_->hasDefinition())
    return false;

  // The base classes are not themselves considered garbage collected objects.
  if (Config::IsGCBase(name_))
    return false;

  // Walk the inheritance tree to find GC base classes.
  walkBases();
  return is_gc_derived_;
}

// Test if a record is directly derived from a garbage collected base.
bool RecordInfo::IsGCDirectlyDerived() {
  // If already computed, return the known result.
  if (directly_derived_gc_base_)
    return true;

  if (!record_->hasDefinition())
    return false;

  // The base classes are not themselves considered garbage collected objects.
  if (Config::IsGCBase(name_))
    return false;

  for (const auto& it : record()->bases()) {
    const CXXRecordDecl* base = it.getType()->getAsCXXRecordDecl();
    if (!base)
      continue;

    const std::string& name = base->getName();
    if (Config::IsGCSimpleBase(name)) {
      directly_derived_gc_base_ = &it;
      break;
    }
  }

  return directly_derived_gc_base_;
}

CXXRecordDecl* RecordInfo::GetDependentTemplatedDecl(const Type& type) {
  const TemplateSpecializationType* tmpl_type =
      type.getAs<TemplateSpecializationType>();
  if (!tmpl_type)
    return 0;

  TemplateDecl* tmpl_decl = tmpl_type->getTemplateName().getAsTemplateDecl();
  if (!tmpl_decl)
    return 0;

  return dyn_cast_or_null<CXXRecordDecl>(tmpl_decl->getTemplatedDecl());
}

void RecordInfo::walkBases() {
  // This traversal is akin to CXXRecordDecl::forallBases()'s,
  // but without stepping over dependent bases -- these might also
  // have a "GC base name", so are to be included and considered.
  SmallVector<const CXXRecordDecl*, 8> queue;

  const CXXRecordDecl* base_record = record();
  while (true) {
    for (const auto& it : base_record->bases()) {
      const RecordType* type = it.getType()->getAs<RecordType>();
      CXXRecordDecl* base;
      if (!type)
        base = GetDependentTemplatedDecl(*it.getType());
      else {
        base = cast_or_null<CXXRecordDecl>(type->getDecl()->getDefinition());
        if (base)
          queue.push_back(base);
      }
      if (!base)
        continue;

      const std::string& name = base->getName();
      if (Config::IsGCBase(name)) {
        gc_base_names_.push_back(name);
        is_gc_derived_ = true;
      }
    }

    if (queue.empty())
      break;
    base_record = queue.pop_back_val(); // not actually a queue.
  }
}

// A GC mixin is a class that inherits from a GC mixin base and has
// not yet been "mixed in" with another GC base class.
bool RecordInfo::IsGCMixin() {
  if (!IsGCDerived() || !gc_base_names_.size())
    return false;
  for (const auto& gc_base : gc_base_names_) {
      // If it is not a mixin base we are done.
      if (!Config::IsGCMixinBase(gc_base))
          return false;
  }
  // This is a mixin if all GC bases are mixins.
  return true;
}

// Test if a record is allocated on the managed heap.
bool RecordInfo::IsGCAllocated() {
  return IsGCDerived() || IsHeapAllocatedCollection();
}

bool RecordInfo::HasDefinition() {
  return record_->hasDefinition();
}

RecordInfo* RecordCache::Lookup(CXXRecordDecl* record) {
  // Ignore classes annotated with the GC_PLUGIN_IGNORE macro.
  if (!record || Config::IsIgnoreAnnotated(record))
    return 0;
  Cache::iterator it = cache_.find(record);
  if (it != cache_.end())
    return &it->second;
  return &cache_.insert(std::make_pair(record, RecordInfo(record, this)))
              .first->second;
}

bool RecordInfo::IsStackAllocated() {
  if (is_stack_allocated_ == kNotComputed) {
    is_stack_allocated_ = kFalse;
    for (Bases::iterator it = GetBases().begin();
         it != GetBases().end();
         ++it) {
      if (it->second.info()->IsStackAllocated()) {
        is_stack_allocated_ = kTrue;
        return is_stack_allocated_;
      }
    }
    for (CXXRecordDecl::method_iterator it = record_->method_begin();
         it != record_->method_end();
         ++it) {
      if (it->getNameAsString() == kNewOperatorName &&
          it->isDeleted() &&
          Config::IsStackAnnotated(*it)) {
        is_stack_allocated_ = kTrue;
        return is_stack_allocated_;
      }
    }
  }
  return is_stack_allocated_;
}

bool RecordInfo::IsNonNewable() {
  if (is_non_newable_ == kNotComputed) {
    bool deleted = false;
    bool all_deleted = true;
    for (CXXRecordDecl::method_iterator it = record_->method_begin();
         it != record_->method_end();
         ++it) {
      if (it->getNameAsString() == kNewOperatorName) {
        deleted = it->isDeleted();
        all_deleted = all_deleted && deleted;
      }
    }
    is_non_newable_ = (deleted && all_deleted) ? kTrue : kFalse;
  }
  return is_non_newable_;
}

bool RecordInfo::IsOnlyPlacementNewable() {
  if (is_only_placement_newable_ == kNotComputed) {
    bool placement = false;
    bool new_deleted = false;
    for (CXXRecordDecl::method_iterator it = record_->method_begin();
         it != record_->method_end();
         ++it) {
      if (it->getNameAsString() == kNewOperatorName) {
        if (it->getNumParams() == 1) {
          new_deleted = it->isDeleted();
        } else if (it->getNumParams() == 2) {
          placement = !it->isDeleted();
        }
      }
    }
    is_only_placement_newable_ = (placement && new_deleted) ? kTrue : kFalse;
  }
  return is_only_placement_newable_;
}

CXXMethodDecl* RecordInfo::DeclaresNewOperator() {
  for (CXXRecordDecl::method_iterator it = record_->method_begin();
       it != record_->method_end();
       ++it) {
    if (it->getNameAsString() == kNewOperatorName && it->getNumParams() == 1)
      return *it;
  }
  return 0;
}

// An object requires a tracing method if it has any fields that need tracing
// or if it inherits from multiple bases that need tracing.
bool RecordInfo::RequiresTraceMethod() {
  if (IsStackAllocated())
    return false;
  unsigned bases_with_trace = 0;
  for (Bases::iterator it = GetBases().begin(); it != GetBases().end(); ++it) {
    if (it->second.NeedsTracing().IsNeeded())
      ++bases_with_trace;
  }
  if (bases_with_trace > 1)
    return true;
  GetFields();
  return fields_need_tracing_.IsNeeded();
}

// Get the actual tracing method (ie, can be traceAfterDispatch if there is a
// dispatch method).
CXXMethodDecl* RecordInfo::GetTraceMethod() {
  DetermineTracingMethods();
  return trace_method_;
}

// Get the static trace dispatch method.
CXXMethodDecl* RecordInfo::GetTraceDispatchMethod() {
  DetermineTracingMethods();
  return trace_dispatch_method_;
}

CXXMethodDecl* RecordInfo::GetFinalizeDispatchMethod() {
  DetermineTracingMethods();
  return finalize_dispatch_method_;
}

const CXXBaseSpecifier* RecordInfo::GetDirectGCBase() {
  if (!IsGCDirectlyDerived())
    return nullptr;
  return directly_derived_gc_base_;
}

RecordInfo::Bases& RecordInfo::GetBases() {
  if (!bases_)
    bases_ = CollectBases();
  return *bases_;
}

bool RecordInfo::InheritsTrace() {
  if (GetTraceMethod())
    return true;
  for (Bases::iterator it = GetBases().begin(); it != GetBases().end(); ++it) {
    if (it->second.info()->InheritsTrace())
      return true;
  }
  return false;
}

CXXMethodDecl* RecordInfo::InheritsNonVirtualTrace() {
  if (CXXMethodDecl* trace = GetTraceMethod())
    return trace->isVirtual() ? 0 : trace;
  for (Bases::iterator it = GetBases().begin(); it != GetBases().end(); ++it) {
    if (CXXMethodDecl* trace = it->second.info()->InheritsNonVirtualTrace())
      return trace;
  }
  return 0;
}

bool RecordInfo::DeclaresGCMixinMethods() {
  DetermineTracingMethods();
  return has_gc_mixin_methods_;
}

bool RecordInfo::DeclaresLocalTraceMethod() {
  if (is_declaring_local_trace_ != kNotComputed)
    return is_declaring_local_trace_;
  DetermineTracingMethods();
  is_declaring_local_trace_ = trace_method_ ? kTrue : kFalse;
  if (is_declaring_local_trace_) {
    for (auto it = record_->method_begin();
         it != record_->method_end(); ++it) {
      if (*it == trace_method_) {
        is_declaring_local_trace_ = kTrue;
        break;
      }
    }
  }
  return is_declaring_local_trace_;
}

// A (non-virtual) class is considered abstract in Blink if it has
// no public constructors and no create methods.
bool RecordInfo::IsConsideredAbstract() {
  for (CXXRecordDecl::ctor_iterator it = record_->ctor_begin();
       it != record_->ctor_end();
       ++it) {
    if (!it->isCopyOrMoveConstructor() && it->getAccess() == AS_public)
      return false;
  }
  for (CXXRecordDecl::method_iterator it = record_->method_begin();
       it != record_->method_end();
       ++it) {
    if (it->getNameAsString() == kCreateName)
      return false;
  }
  return true;
}

RecordInfo::Bases* RecordInfo::CollectBases() {
  // Compute the collection locally to avoid inconsistent states.
  Bases* bases = new Bases;
  if (!record_->hasDefinition())
    return bases;
  for (CXXRecordDecl::base_class_iterator it = record_->bases_begin();
       it != record_->bases_end();
       ++it) {
    const CXXBaseSpecifier& spec = *it;
    RecordInfo* info = cache_->Lookup(spec.getType());
    if (!info)
      continue;
    CXXRecordDecl* base = info->record();
    TracingStatus status = info->InheritsTrace()
                               ? TracingStatus::Needed()
                               : TracingStatus::Unneeded();
    bases->push_back(std::make_pair(base, BasePoint(spec, info, status)));
  }
  return bases;
}

RecordInfo::Fields& RecordInfo::GetFields() {
  if (!fields_)
    fields_ = CollectFields();
  return *fields_;
}

RecordInfo::Fields* RecordInfo::CollectFields() {
  // Compute the collection locally to avoid inconsistent states.
  Fields* fields = new Fields;
  if (!record_->hasDefinition())
    return fields;
  TracingStatus fields_status = TracingStatus::Unneeded();
  for (RecordDecl::field_iterator it = record_->field_begin();
       it != record_->field_end();
       ++it) {
    FieldDecl* field = *it;
    // Ignore fields annotated with the GC_PLUGIN_IGNORE macro.
    if (Config::IsIgnoreAnnotated(field))
      continue;
    // Check if the unexpanded type should be recorded; needed
    // to track iterator aliases only
    const Type* unexpandedType = field->getType().getSplitUnqualifiedType().Ty;
    Edge* edge = CreateEdgeFromOriginalType(unexpandedType);
    if (!edge)
      edge = CreateEdge(field->getType().getTypePtrOrNull());
    if (edge) {
      fields_status = fields_status.LUB(edge->NeedsTracing(Edge::kRecursive));
      fields->insert(std::make_pair(field, FieldPoint(field, edge)));
    }
  }
  fields_need_tracing_ = fields_status;
  return fields;
}

void RecordInfo::DetermineTracingMethods() {
  if (determined_trace_methods_)
    return;
  determined_trace_methods_ = true;
  if (Config::IsGCBase(name_))
    return;
  CXXMethodDecl* trace = nullptr;
  CXXMethodDecl* trace_after_dispatch = nullptr;
  bool has_adjust_and_mark = false;
  bool has_is_heap_object_alive = false;
  for (Decl* decl : record_->decls()) {
    CXXMethodDecl* method = dyn_cast<CXXMethodDecl>(decl);
    if (!method) {
      if (FunctionTemplateDecl* func_template =
          dyn_cast<FunctionTemplateDecl>(decl))
        method = dyn_cast<CXXMethodDecl>(func_template->getTemplatedDecl());
    }
    if (!method)
      continue;

    switch (Config::GetTraceMethodType(method)) {
      case Config::TRACE_METHOD:
        trace = method;
        break;
      case Config::TRACE_AFTER_DISPATCH_METHOD:
        trace_after_dispatch = method;
        break;
      case Config::NOT_TRACE_METHOD:
        if (method->getNameAsString() == kFinalizeName) {
          finalize_dispatch_method_ = method;
        } else if (method->getNameAsString() == kAdjustAndMarkName) {
          has_adjust_and_mark = true;
        } else if (method->getNameAsString() == kIsHeapObjectAliveName) {
          has_is_heap_object_alive = true;
        }
        break;
    }
  }

  // Record if class defines the two GCMixin methods.
  has_gc_mixin_methods_ =
      has_adjust_and_mark && has_is_heap_object_alive ? kTrue : kFalse;
  if (trace_after_dispatch) {
    trace_method_ = trace_after_dispatch;
    trace_dispatch_method_ = trace;
  } else {
    // TODO: Can we never have a dispatch method called trace without the same
    // class defining a traceAfterDispatch method?
    trace_method_ = trace;
    trace_dispatch_method_ = nullptr;
  }
  if (trace_dispatch_method_ && finalize_dispatch_method_)
    return;
  // If this class does not define dispatching methods inherit them.
  for (Bases::iterator it = GetBases().begin(); it != GetBases().end(); ++it) {
    // TODO: Does it make sense to inherit multiple dispatch methods?
    if (CXXMethodDecl* dispatch = it->second.info()->GetTraceDispatchMethod()) {
      assert(!trace_dispatch_method_ && "Multiple trace dispatching methods");
      trace_dispatch_method_ = dispatch;
    }
    if (CXXMethodDecl* dispatch =
            it->second.info()->GetFinalizeDispatchMethod()) {
      assert(!finalize_dispatch_method_ &&
             "Multiple finalize dispatching methods");
      finalize_dispatch_method_ = dispatch;
    }
  }
}

// TODO: Add classes with a finalize() method that specialize FinalizerTrait.
bool RecordInfo::NeedsFinalization() {
  if (does_need_finalization_ == kNotComputed) {
    if (HasOptionalFinalizer()) {
      does_need_finalization_ = kFalse;
      return does_need_finalization_;
    }

    // Rely on hasNonTrivialDestructor(), but if the only
    // identifiable reason for it being true is the presence
    // of a safely ignorable class as a direct base,
    // or we're processing such an 'ignorable' class, then it does
    // not need finalization.
    does_need_finalization_ =
        record_->hasNonTrivialDestructor() ? kTrue : kFalse;
    if (!does_need_finalization_)
      return does_need_finalization_;

    CXXDestructorDecl* dtor = record_->getDestructor();
    if (dtor && dtor->isUserProvided())
      return does_need_finalization_;
    for (Fields::iterator it = GetFields().begin();
         it != GetFields().end();
         ++it) {
      if (it->second.edge()->NeedsFinalization())
        return does_need_finalization_;
    }

    for (Bases::iterator it = GetBases().begin();
         it != GetBases().end();
         ++it) {
      if (it->second.info()->NeedsFinalization())
        return does_need_finalization_;
    }
    // Destructor was non-trivial due to bases with destructors that
    // can be safely ignored. Hence, no need for finalization.
    does_need_finalization_ = kFalse;
  }
  return does_need_finalization_;
}

// A class needs tracing if:
// - it is allocated on the managed heap,
// - it is derived from a class that needs tracing, or
// - it contains fields that need tracing.
//
TracingStatus RecordInfo::NeedsTracing(Edge::NeedsTracingOption option) {
  if (IsGCAllocated())
    return TracingStatus::Needed();

  if (IsStackAllocated())
    return TracingStatus::Unneeded();

  for (Bases::iterator it = GetBases().begin(); it != GetBases().end(); ++it) {
    if (it->second.info()->NeedsTracing(option).IsNeeded())
      return TracingStatus::Needed();
  }

  if (option == Edge::kRecursive)
    GetFields();

  return fields_need_tracing_;
}

static bool isInStdNamespace(clang::Sema& sema, NamespaceDecl* ns)
{
  while (ns) {
    if (sema.getStdNamespace()->InEnclosingNamespaceSetOf(ns))
      return true;
    ns = dyn_cast<NamespaceDecl>(ns->getParent());
  }
  return false;
}

Edge* RecordInfo::CreateEdgeFromOriginalType(const Type* type) {
  if (!type)
    return nullptr;

  // look for "typedef ... iterator;"
  if (!isa<ElaboratedType>(type))
    return nullptr;
  const ElaboratedType* elaboratedType = cast<ElaboratedType>(type);
  if (!isa<TypedefType>(elaboratedType->getNamedType()))
    return nullptr;
  const TypedefType* typedefType =
      cast<TypedefType>(elaboratedType->getNamedType());
  std::string typeName = typedefType->getDecl()->getNameAsString();
  if (!Config::IsIterator(typeName))
    return nullptr;
  RecordInfo* info =
      cache_->Lookup(elaboratedType->getQualifier()->getAsType());

  bool on_heap = false;
  bool is_unsafe = false;
  // Silently handle unknown types; the on-heap collection types will
  // have to be in scope for the declaration to compile, though.
  if (info) {
    is_unsafe = Config::IsGCCollectionWithUnsafeIterator(info->name());
    // Don't mark iterator as being on the heap if it is not supported.
    on_heap = !is_unsafe && Config::IsGCCollection(info->name());
  }
  return new Iterator(info, on_heap, is_unsafe);
}

Edge* RecordInfo::CreateEdge(const Type* type) {
  if (!type) {
    return 0;
  }

  if (type->isPointerType() || type->isReferenceType()) {
    if (Edge* ptr = CreateEdge(type->getPointeeType().getTypePtrOrNull()))
      return new RawPtr(ptr, type->isReferenceType());
    return 0;
  }

  RecordInfo* info = cache_->Lookup(type);

  // If the type is neither a pointer or a C++ record we ignore it.
  if (!info) {
    return 0;
  }

  TemplateArgs args;

  if (Config::IsRefPtr(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new RefPtr(ptr);
    return 0;
  }

  if (Config::IsUniquePtr(info->name()) && info->GetTemplateArgs(1, &args)) {
    // Check that this is std::unique_ptr
    NamespaceDecl* ns =
        dyn_cast<NamespaceDecl>(info->record()->getDeclContext());
    clang::Sema& sema = cache_->instance().getSema();
    if (!isInStdNamespace(sema, ns))
      return 0;
    if (Edge* ptr = CreateEdge(args[0]))
      return new UniquePtr(ptr);
    return 0;
  }

  if (Config::IsMember(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new Member(ptr);
    return 0;
  }

  if (Config::IsWeakMember(info->name()) && info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new WeakMember(ptr);
    return 0;
  }

  bool is_persistent = Config::IsPersistent(info->name());
  if (is_persistent || Config::IsCrossThreadPersistent(info->name())) {
    // Persistent might refer to v8::Persistent, so check the name space.
    // TODO: Consider using a more canonical identification than names.
    NamespaceDecl* ns =
        dyn_cast<NamespaceDecl>(info->record()->getDeclContext());
    if (!ns || ns->getName() != "blink")
      return 0;
    if (!info->GetTemplateArgs(1, &args))
      return 0;
    if (Edge* ptr = CreateEdge(args[0])) {
      if (is_persistent)
        return new Persistent(ptr);
      else
        return new CrossThreadPersistent(ptr);
    }
    return 0;
  }

  if (Config::IsGCCollection(info->name()) ||
      Config::IsWTFCollection(info->name())) {
    bool on_heap = info->IsHeapAllocatedCollection();
    size_t count = Config::CollectionDimension(info->name());
    if (!info->GetTemplateArgs(count, &args))
      return 0;
    Collection* edge = new Collection(info, on_heap);
    for (TemplateArgs::iterator it = args.begin(); it != args.end(); ++it) {
      if (Edge* member = CreateEdge(*it)) {
        edge->members().push_back(member);
      }
      // TODO: Handle the case where we fail to create an edge (eg, if the
      // argument is a primitive type or just not fully known yet).
    }
    return edge;
  }

  if (Config::IsTraceWrapperV8Reference(info->name()) &&
      info->GetTemplateArgs(1, &args)) {
    if (Edge* ptr = CreateEdge(args[0]))
      return new TraceWrapperV8Reference(ptr);
    return 0;
  }

  return new Value(info);
}
