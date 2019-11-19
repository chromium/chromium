// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "DiagnosticsReporter.h"

#include "llvm/Support/ErrorHandling.h"

using namespace clang;

namespace {

const char kClassMustLeftMostlyDeriveGC[] =
    "[blink-gc] Class %0 must derive from GarbageCollected in the left-most position.";

const char kClassRequiresTraceMethod[] =
    "[blink-gc] Class %0 requires a trace method.";

const char kBaseRequiresTracing[] =
    "[blink-gc] Base class %0 of derived class %1 requires tracing.";

const char kBaseRequiresTracingNote[] =
    "[blink-gc] Untraced base class %0 declared here:";

const char kFieldsRequireTracing[] =
    "[blink-gc] Class %0 has untraced fields that require tracing.";

const char kFieldsImproperlyTraced[] =
    "[blink-gc] Class %0 has untraced or not traceable fields.";

const char kFieldRequiresTracingNote[] =
    "[blink-gc] Untraced field %0 declared here:";

const char kFieldShouldNotBeTracedNote[] =
    "[blink-gc] Untraceable field %0 declared here:";

const char kClassContainsInvalidFields[] =
    "[blink-gc] Class %0 contains invalid fields.";

const char kClassContainsGCRoot[] =
    "[blink-gc] Class %0 contains GC root in field %1.";

const char kFinalizerAccessesFinalizedField[] =
    "[blink-gc] Finalizer %0 accesses potentially finalized field %1.";

const char kRawPtrToGCManagedClassNote[] =
    "[blink-gc] Raw pointer field %0 to a GC managed class declared here:";

const char kRefPtrToGCManagedClassNote[] =
    "[blink-gc] RefPtr field %0 to a GC managed class declared here:";

const char kReferencePtrToGCManagedClassNote[] =
    "[blink-gc] Reference pointer field %0 to a GC managed class"
    " declared here:";

const char kUniquePtrToGCManagedClassNote[] =
    "[blink-gc] std::unique_ptr field %0 to a GC managed class declared here:";

const char kMemberToGCUnmanagedClassNote[] =
    "[blink-gc] Member field %0 to non-GC managed class declared here:";

const char kStackAllocatedFieldNote[] =
    "[blink-gc] Stack-allocated field %0 declared here:";

const char kMemberInUnmanagedClassNote[] =
    "[blink-gc] Member field %0 in unmanaged class declared here:";

const char kPartObjectToGCDerivedClassNote[] =
    "[blink-gc] Part-object field %0 to a GC derived class declared here:";

const char kPartObjectContainsGCRootNote[] =
    "[blink-gc] Field %0 with embedded GC root in %1 declared here:";

const char kFieldContainsGCRootNote[] =
    "[blink-gc] Field %0 defining a GC root declared here:";

const char kOverriddenNonVirtualTrace[] =
    "[blink-gc] Class %0 overrides non-virtual trace of base class %1.";

const char kOverriddenNonVirtualTraceNote[] =
    "[blink-gc] Non-virtual trace method declared here:";

const char kMissingTraceDispatchMethod[] =
    "[blink-gc] Class %0 is missing manual trace dispatch.";

const char kVirtualAndManualDispatch[] =
    "[blink-gc] Class %0 contains or inherits virtual methods"
    " but implements manual dispatching.";

const char kMissingTraceDispatch[] =
    "[blink-gc] Missing dispatch to class %0 in manual trace dispatch.";

const char kMissingFinalizeDispatch[] =
    "[blink-gc] Missing dispatch to class %0 in manual finalize dispatch.";

const char kFinalizedFieldNote[] =
    "[blink-gc] Potentially finalized field %0 declared here:";

const char kManualDispatchMethodNote[] =
    "[blink-gc] Manual dispatch %0 declared here:";

const char kStackAllocatedDerivesGarbageCollected[] =
    "[blink-gc] Stack-allocated class %0 derives class %1"
    " which is garbage collected.";

const char kClassOverridesNew[] =
    "[blink-gc] Garbage collected class %0"
    " is not permitted to override its new operator.";

const char kClassDeclaresPureVirtualTrace[] =
    "[blink-gc] Garbage collected class %0"
    " is not permitted to declare a pure-virtual trace method.";

const char kLeftMostBaseMustBePolymorphic[] =
    "[blink-gc] Left-most base class %0 of derived class %1"
    " must be polymorphic.";

const char kBaseClassMustDeclareVirtualTrace[] =
    "[blink-gc] Left-most base class %0 of derived class %1"
    " must define a virtual trace method.";

const char kClassMustCRTPItself[] =
    "[blink-gc] GC base class %0 must be specialized with the derived class "
    "%1.";

const char kIteratorToGCManagedCollectionNote[] =
    "[blink-gc] Iterator field %0 to a GC managed collection declared here:";

const char kTraceMethodOfStackAllocatedParentNote[] =
    "[blink-gc] The stack allocated class %0 provides an unnecessary "
    "trace method:";

const char kUniquePtrUsedWithGC[] =
    "[blink-gc] Disallowed use of %0 found; %1 is a garbage-collected type. "
    "std::unique_ptr cannot hold garbage-collected objects.";

const char kOptionalUsedWithGC[] =
    "[blink-gc] Disallowed construction of %0 found; %1 is a garbage-collected "
    "type. optional cannot hold garbage-collected objects.";

const char kMissingMixinMarker[] =
    "[blink-gc] Garbage-collected class %0 derives mixin class %1. "
    "You must add USING_GARBAGE_COLLECTED_MIXIN(%2).";

const char kMissingMixinMarkerNote[] =
    "[blink-gc] Mixin base class derived here:";

} // namespace

DiagnosticBuilder DiagnosticsReporter::ReportDiagnostic(
    SourceLocation location,
    unsigned diag_id) {
  SourceManager& manager = instance_.getSourceManager();
  FullSourceLoc full_loc(location, manager);
  return diagnostic_.Report(full_loc, diag_id);
}

DiagnosticsReporter::DiagnosticsReporter(
    clang::CompilerInstance& instance)
    : instance_(instance),
      diagnostic_(instance.getDiagnostics())
{
  // Register warning/error messages.
  diag_class_must_left_mostly_derive_gc_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kClassMustLeftMostlyDeriveGC);
  diag_class_requires_trace_method_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kClassRequiresTraceMethod);
  diag_base_requires_tracing_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kBaseRequiresTracing);
  diag_fields_require_tracing_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kFieldsRequireTracing);
  diag_fields_improperly_traced_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kFieldsImproperlyTraced);
  diag_class_contains_invalid_fields_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kClassContainsInvalidFields);
  diag_class_contains_gc_root_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kClassContainsGCRoot);
  diag_finalizer_accesses_finalized_field_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kFinalizerAccessesFinalizedField);
  diag_overridden_non_virtual_trace_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kOverriddenNonVirtualTrace);
  diag_missing_trace_dispatch_method_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kMissingTraceDispatchMethod);
  diag_virtual_and_manual_dispatch_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kVirtualAndManualDispatch);
  diag_missing_trace_dispatch_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kMissingTraceDispatch);
  diag_missing_finalize_dispatch_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kMissingFinalizeDispatch);
  diag_stack_allocated_derives_gc_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kStackAllocatedDerivesGarbageCollected);
  diag_class_overrides_new_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kClassOverridesNew);
  diag_class_declares_pure_virtual_trace_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kClassDeclaresPureVirtualTrace);
  diag_left_most_base_must_be_polymorphic_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kLeftMostBaseMustBePolymorphic);
  diag_base_class_must_declare_virtual_trace_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kBaseClassMustDeclareVirtualTrace);
  diag_class_must_crtp_itself_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kClassMustCRTPItself);
  diag_iterator_to_gc_managed_collection_note_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kIteratorToGCManagedCollectionNote);
  diag_trace_method_of_stack_allocated_parent_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kTraceMethodOfStackAllocatedParentNote);

  // Register note messages.
  diag_base_requires_tracing_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kBaseRequiresTracingNote);
  diag_field_requires_tracing_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kFieldRequiresTracingNote);
  diag_field_should_not_be_traced_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kFieldShouldNotBeTracedNote);
  diag_raw_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kRawPtrToGCManagedClassNote);
  diag_ref_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kRefPtrToGCManagedClassNote);
  diag_reference_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kReferencePtrToGCManagedClassNote);
  diag_unique_ptr_to_gc_managed_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kUniquePtrToGCManagedClassNote);
  diag_member_to_gc_unmanaged_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kMemberToGCUnmanagedClassNote);
  diag_stack_allocated_field_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kStackAllocatedFieldNote);
  diag_member_in_unmanaged_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kMemberInUnmanagedClassNote);
  diag_part_object_to_gc_derived_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kPartObjectToGCDerivedClassNote);
  diag_part_object_contains_gc_root_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kPartObjectContainsGCRootNote);
  diag_field_contains_gc_root_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kFieldContainsGCRootNote);
  diag_finalized_field_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kFinalizedFieldNote);
  diag_overridden_non_virtual_trace_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kOverriddenNonVirtualTraceNote);
  diag_manual_dispatch_method_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kManualDispatchMethodNote);

  diag_unique_ptr_used_with_gc_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kUniquePtrUsedWithGC);
  diag_optional_used_with_gc_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kOptionalUsedWithGC);
  diag_missing_mixin_marker_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kMissingMixinMarker);
  diag_missing_mixin_marker_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kMissingMixinMarkerNote);
}

bool DiagnosticsReporter::hasErrorOccurred() const
{
  return diagnostic_.hasErrorOccurred();
}

DiagnosticsEngine::Level DiagnosticsReporter::getErrorLevel() const {
  return diagnostic_.getWarningsAsErrors() ? DiagnosticsEngine::Error
                                           : DiagnosticsEngine::Warning;
}

void DiagnosticsReporter::ClassMustLeftMostlyDeriveGC(
    RecordInfo* info) {
  ReportDiagnostic(info->record()->getInnerLocStart(),
                   diag_class_must_left_mostly_derive_gc_)
      << info->record();
}

void DiagnosticsReporter::ClassRequiresTraceMethod(RecordInfo* info) {
  ReportDiagnostic(info->record()->getInnerLocStart(),
                   diag_class_requires_trace_method_)
      << info->record();

  for (auto& base : info->GetBases())
    if (base.second.NeedsTracing().IsNeeded())
      NoteBaseRequiresTracing(&base.second);

  for (auto& field : info->GetFields())
    if (!field.second.IsProperlyTraced())
      NoteFieldRequiresTracing(info, field.first);
}

void DiagnosticsReporter::BaseRequiresTracing(
    RecordInfo* derived,
    CXXMethodDecl* trace,
    CXXRecordDecl* base) {
  ReportDiagnostic(trace->getBeginLoc(), diag_base_requires_tracing_)
      << base << derived->record();
}

void DiagnosticsReporter::FieldsImproperlyTraced(
    RecordInfo* info,
    CXXMethodDecl* trace) {
  // Only mention untraceable in header diagnostic if they appear.
  unsigned diag = diag_fields_require_tracing_;
  for (auto& field : info->GetFields()) {
    if (field.second.IsInproperlyTraced()) {
      diag = diag_fields_improperly_traced_;
      break;
    }
  }
  ReportDiagnostic(trace->getBeginLoc(), diag) << info->record();
  for (auto& field : info->GetFields()) {
    if (!field.second.IsProperlyTraced())
      NoteFieldRequiresTracing(info, field.first);
    if (field.second.IsInproperlyTraced())
      NoteFieldShouldNotBeTraced(info, field.first);
  }
}

void DiagnosticsReporter::ClassContainsInvalidFields(
    RecordInfo* info,
    const CheckFieldsVisitor::Errors& errors) {
  ReportDiagnostic(info->record()->getBeginLoc(),
                   diag_class_contains_invalid_fields_)
      << info->record();

  for (auto& error : errors) {
    unsigned note;
    if (error.second == CheckFieldsVisitor::kRawPtrToGCManaged) {
      note = diag_raw_ptr_to_gc_managed_class_note_;
    } else if (error.second == CheckFieldsVisitor::kRefPtrToGCManaged) {
      note = diag_ref_ptr_to_gc_managed_class_note_;
    } else if (error.second == CheckFieldsVisitor::kReferencePtrToGCManaged) {
      note = diag_reference_ptr_to_gc_managed_class_note_;
    } else if (error.second == CheckFieldsVisitor::kUniquePtrToGCManaged) {
      note = diag_unique_ptr_to_gc_managed_class_note_;
    } else if (error.second == CheckFieldsVisitor::kMemberToGCUnmanaged) {
      note = diag_member_to_gc_unmanaged_class_note_;
    } else if (error.second == CheckFieldsVisitor::kMemberInUnmanaged) {
      note = diag_member_in_unmanaged_class_note_;
    } else if (error.second == CheckFieldsVisitor::kPtrFromHeapToStack) {
      note = diag_stack_allocated_field_note_;
    } else if (error.second == CheckFieldsVisitor::kGCDerivedPartObject) {
      note = diag_part_object_to_gc_derived_class_note_;
    } else if (error.second == CheckFieldsVisitor::kIteratorToGCManaged) {
      note = diag_iterator_to_gc_managed_collection_note_;
    } else {
      llvm_unreachable("Unknown field error.");
    }
    NoteField(error.first, note);
  }
}

void DiagnosticsReporter::ClassContainsGCRoots(
    RecordInfo* info,
    const CheckGCRootsVisitor::Errors& errors) {
  for (auto& error : errors) {
    FieldPoint* point = nullptr;
    for (FieldPoint* path : error) {
      if (!point) {
        point = path;
        ReportDiagnostic(info->record()->getBeginLoc(),
                         diag_class_contains_gc_root_)
            << info->record() << point->field();
        continue;
      }
      NotePartObjectContainsGCRoot(point);
      point = path;
    }
    NoteFieldContainsGCRoot(point);
  }
}

void DiagnosticsReporter::FinalizerAccessesFinalizedFields(
    CXXMethodDecl* dtor,
    const CheckFinalizerVisitor::Errors& errors) {
  for (auto& error : errors) {
    ReportDiagnostic(error.member->getBeginLoc(),
        diag_finalizer_accesses_finalized_field_)
        << dtor << error.field->field();
    NoteField(error.field, diag_finalized_field_note_);
  }
}

void DiagnosticsReporter::OverriddenNonVirtualTrace(
    RecordInfo* info,
    CXXMethodDecl* trace,
    CXXMethodDecl* overridden) {
  ReportDiagnostic(trace->getBeginLoc(), diag_overridden_non_virtual_trace_)
      << info->record() << overridden->getParent();
  NoteOverriddenNonVirtualTrace(overridden);
}

void DiagnosticsReporter::MissingTraceDispatchMethod(RecordInfo* info) {
  ReportMissingDispatchMethod(info, diag_missing_trace_dispatch_method_);
}

void DiagnosticsReporter::ReportMissingDispatchMethod(
    RecordInfo* info,
    unsigned error) {
  ReportDiagnostic(info->record()->getInnerLocStart(), error)
      << info->record();
}

void DiagnosticsReporter::VirtualAndManualDispatch(
    RecordInfo* info,
    CXXMethodDecl* dispatch) {
  ReportDiagnostic(info->record()->getInnerLocStart(),
                   diag_virtual_and_manual_dispatch_)
      << info->record();
  NoteManualDispatchMethod(dispatch);
}

void DiagnosticsReporter::MissingTraceDispatch(
    const FunctionDecl* dispatch,
    RecordInfo* receiver) {
  ReportMissingDispatch(dispatch, receiver, diag_missing_trace_dispatch_);
}

void DiagnosticsReporter::MissingFinalizeDispatch(
    const FunctionDecl* dispatch,
    RecordInfo* receiver) {
  ReportMissingDispatch(dispatch, receiver, diag_missing_finalize_dispatch_);
}

void DiagnosticsReporter::ReportMissingDispatch(
    const FunctionDecl* dispatch,
    RecordInfo* receiver,
    unsigned error) {
  ReportDiagnostic(dispatch->getBeginLoc(), error) << receiver->record();
}

void DiagnosticsReporter::StackAllocatedDerivesGarbageCollected(
    RecordInfo* info,
    BasePoint* base) {
  ReportDiagnostic(base->spec().getBeginLoc(), diag_stack_allocated_derives_gc_)
      << info->record() << base->info()->record();
}

void DiagnosticsReporter::ClassOverridesNew(
    RecordInfo* info,
    CXXMethodDecl* newop) {
  ReportDiagnostic(newop->getBeginLoc(), diag_class_overrides_new_)
      << info->record();
}

void DiagnosticsReporter::ClassDeclaresPureVirtualTrace(
    RecordInfo* info,
    CXXMethodDecl* trace) {
  ReportDiagnostic(trace->getBeginLoc(),
                   diag_class_declares_pure_virtual_trace_)
      << info->record();
}

void DiagnosticsReporter::LeftMostBaseMustBePolymorphic(
    RecordInfo* derived,
    CXXRecordDecl* base) {
  ReportDiagnostic(base->getBeginLoc(),
                   diag_left_most_base_must_be_polymorphic_)
      << base << derived->record();
}

void DiagnosticsReporter::BaseClassMustDeclareVirtualTrace(
    RecordInfo* derived,
    CXXRecordDecl* base) {
  ReportDiagnostic(base->getBeginLoc(),
                   diag_base_class_must_declare_virtual_trace_)
      << base << derived->record();
}

void DiagnosticsReporter::ClassMustCRTPItself(
    const RecordInfo* derived,
    const CXXRecordDecl* base,
    const CXXBaseSpecifier* base_spec) {
  ReportDiagnostic(base_spec->getBeginLoc(), diag_class_must_crtp_itself_)
      << base << derived->record();
}

void DiagnosticsReporter::TraceMethodForStackAllocatedClass(
    RecordInfo* info,
    CXXMethodDecl* trace) {
  ReportDiagnostic(trace->getBeginLoc(),
                   diag_trace_method_of_stack_allocated_parent_)
      << info->record();
}

void DiagnosticsReporter::NoteManualDispatchMethod(CXXMethodDecl* dispatch) {
  ReportDiagnostic(dispatch->getBeginLoc(), diag_manual_dispatch_method_note_)
      << dispatch;
}

void DiagnosticsReporter::NoteBaseRequiresTracing(BasePoint* base) {
  ReportDiagnostic(base->spec().getBeginLoc(), diag_base_requires_tracing_note_)
      << base->info()->record();
}

void DiagnosticsReporter::NoteFieldRequiresTracing(
    RecordInfo* holder,
    FieldDecl* field) {
  NoteField(field, diag_field_requires_tracing_note_);
}

void DiagnosticsReporter::NoteFieldShouldNotBeTraced(
    RecordInfo* holder,
    FieldDecl* field) {
  NoteField(field, diag_field_should_not_be_traced_note_);
}

void DiagnosticsReporter::NotePartObjectContainsGCRoot(FieldPoint* point) {
  FieldDecl* field = point->field();
  ReportDiagnostic(field->getBeginLoc(),
                   diag_part_object_contains_gc_root_note_)
      << field << field->getParent();
}

void DiagnosticsReporter::NoteFieldContainsGCRoot(FieldPoint* point) {
  NoteField(point, diag_field_contains_gc_root_note_);
}

void DiagnosticsReporter::NoteField(FieldPoint* point, unsigned note) {
  NoteField(point->field(), note);
}

void DiagnosticsReporter::NoteField(FieldDecl* field, unsigned note) {
  ReportDiagnostic(field->getBeginLoc(), note) << field;
}

void DiagnosticsReporter::NoteOverriddenNonVirtualTrace(
    CXXMethodDecl* overridden) {
  ReportDiagnostic(overridden->getBeginLoc(),
                   diag_overridden_non_virtual_trace_note_)
      << overridden;
}

void DiagnosticsReporter::UniquePtrUsedWithGC(
    const clang::Expr* expr,
    const clang::FunctionDecl* bad_function,
    const clang::CXXRecordDecl* gc_type) {
  ReportDiagnostic(expr->getBeginLoc(), diag_unique_ptr_used_with_gc_)
      << bad_function << gc_type << expr->getSourceRange();
}

void DiagnosticsReporter::OptionalUsedWithGC(
    const clang::Expr* expr,
    const clang::CXXRecordDecl* optional,
    const clang::CXXRecordDecl* gc_type) {
  ReportDiagnostic(expr->getBeginLoc(), diag_optional_used_with_gc_)
      << optional << gc_type << expr->getSourceRange();
}

void DiagnosticsReporter::MissingMixinMarker(
    const clang::CXXRecordDecl* bad_class,
    const clang::CXXRecordDecl* mixin_class,
    const clang::CXXBaseSpecifier* first_base) {
  ReportDiagnostic(first_base->getBaseTypeLoc(), diag_missing_mixin_marker_)
      << bad_class << mixin_class << bad_class->getName()
      << first_base->getSourceRange();
}

void DiagnosticsReporter::MissingMixinMarkerNote(
    const clang::CXXBaseSpecifier* base) {
  ReportDiagnostic(base->getBaseTypeLoc(), diag_missing_mixin_marker_note_)
      << base->getSourceRange();
}
