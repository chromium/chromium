// Copyright 2016 The Chromium Authors
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

const char kClassContainsGCRootRef[] =
    "[blink-gc] Class %0 contains a reference to a GC root in field %1. Avoid "
    "holding references to GC roots. This should generally not be needed.";

const char kFinalizerAccessesFinalizedField[] =
    "[blink-gc] Finalizer %0 accesses potentially finalized field %1.";

const char kRawPtrToGCManagedClassNote[] =
    "[blink-gc] Raw pointer field %0 to a GC managed class declared here:";

const char kRefPtrToGCManagedClassNote[] =
    "[blink-gc] scoped_refptr field %0 to a GC managed class declared here:";

const char kReferencePtrToGCManagedClassNote[] =
    "[blink-gc] Reference pointer field %0 to a GC managed class"
    " declared here:";

const char kUniquePtrToGCManagedClassNote[] =
    "[blink-gc] std::unique_ptr field %0 to a GC managed class declared here:";

const char kRawPtrToTraceableClassNote[] =
    "[blink-gc] Raw pointer field %0 to a traceable class declared here:";

const char kRefPtrToTraceableClassNote[] =
    "[blink-gc] scoped_refptr field %0 to a traceable class declared here:";

const char kReferencePtrToTraceableClassNote[] =
    "[blink-gc] Reference pointer field %0 to a traceable class"
    " declared here:";

const char kUniquePtrToTraceableClassNote[] =
    "[blink-gc] std::unique_ptr field %0 to a traceable class declared here:";

const char kWeakPtrToGCManagedClass[] =
    "[blink-gc] WeakPtr or WeakPtrFactory field %0 to a GC managed class %1 "
    "declared here (use WeakCell or WeakCellFactory instead):";

const char kGCedField[] =
    "[blink-gc] Using GC managed class %1 as field %0 is not allowed (Allocate "
    "with MakeGarbageCollected and use Member or Persistent instead):";

const char kGCedVar[] =
    "[blink-gc] Using GC managed class %1 as variable %0 is not allowed "
    "(Allocate with MakeGarbageCollected and use raw pointer instead):";

const char kTaskRunnerInGCManagedClassNote[] =
    "[blink-gc] TaskRunnerTimer field %0 used within a garbage collected "
    "context. "
    "Consider using HeapTaskRunnerTimer instead.";

const char kMojoRemoteInGCManagedClassNote[] =
    "[blink-gc] mojo::Remote field %0 used within a garbage collected "
    "context. "
    "Consider using blink::HeapMojoRemote instead.";

const char kMojoReceiverInGCManagedClassNote[] =
    "[blink-gc] mojo::Receiver field %0 used within a garbage collected "
    "context. "
    "Consider using blink::HeapMojoAssociatedRemote instead.";

const char kMojoAssociatedRemoteInGCManagedClassNote[] =
    "[blink-gc] mojo::AssociatedRemote field %0 used within a garbage "
    "collected context. "
    "Consider using blink::HeapMojoAssociatedReceiver instead.";

const char kMojoAssociatedReceiverInGCManagedClassNote[] =
    "[blink-gc] mojo::AssociatedReceiver field %0 used within a garbage "
    "collected context. "
    "Consider using blink::HeapMojoReceiver instead.";

const char kForbiddenFieldPartObjectClassNote[] =
    "[blink-gc] From part object field %0 here:";

const char kMemberToGCUnmanagedClassNote[] =
    "[blink-gc] Member field %0 to non-GC managed class declared here:";

const char kStackAllocatedFieldNote[] =
    "[blink-gc] Stack-allocated field %0 declared here:";

const char kMemberInUnmanagedClassNote[] =
    "[blink-gc] Member field %0 in unmanaged class declared here:";

const char kPtrToMemberInUnmanagedClassNote[] =
    "[blink-gc] Pointer to Member field %0 in unmanaged class declared here:";

const char kPartObjectToGCDerivedClassNote[] =
    "[blink-gc] Part-object field %0 to a GC derived class declared here:";

const char kPartObjectContainsGCRootNote[] =
    "[blink-gc] Field %0 with embedded GC root in %1 declared here:";

const char kPartObjectContainsGCRootRefNote[] =
    "[blink-gc] Field %0 with embedded reference to a GC root in %1 declared "
    "here:";

const char kFieldContainsGCRootNote[] =
    "[blink-gc] Field %0 defining a GC root declared here:";

const char kFieldContainsGCRootRefNote[] =
    "[blink-gc] Field %0 defining reference to a GC root declared here:";

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

const char kMemberInStackAllocated[] =
    "[blink-gc] Member field %0 in stack allocated class declared here (use "
    "raw pointer or reference instead):";

const char kMemberOnStack[] =
    "[blink-gc] Member variable %0 declared on stack here (use raw pointer or "
    "reference instead):";

const char kAdditionalPadding[] =
    "[blink-gc] Additional padding causes the sizeof(%0) to grow by %1. "
    "Consider reordering fields.";

const char kTraceablePartObjectInUnmanaged[] =
    "[blink-gc] Traceable part object field %0 found in unmanaged class:";

const char kUniquePtrUsedWithGC[] =
    "[blink-gc] Disallowed use of %0 found; %1 is a garbage-collected type. "
    "std::unique_ptr cannot hold garbage-collected objects.";

const char kOptionalDeclUsedWithGC[] =
    "[blink-gc] Disallowed optional field or variable of type %0 found; %1 is "
    "a "
    "garbage-collected or traceable "
    "type. Optional fields and variables cannot hold garbage-collected or "
    "traceable objects.";

const char kOptionalNewExprUsedWithGC[] =
    "[blink-gc] Disallowed new-expression of %0 found; %1 is a "
    "garbage-collected or traceable "
    "type. Optional fields cannot hold garbage-collected or traceable objects.";

const char kOptionalDeclUsedWithMember[] =
    "[blink-gc] Disallowed optional field of type %0 found; %1 is "
    "a Member/WeakMember type. Optional fields and variables cannot hold "
    "Members.";

const char kOptionalNewExprUsedWithMember[] =
    "[blink-gc] Disallowed new-expression of %0 found; %1 is a "
    "Member/WeakMember type. Optional fields cannot hold Members.";

const char kRawPtrOrRefDeclUsedWithGC[] =
    "[blink-gc] Disallowed raw_ptr or raw_ref field or variable of type %0 "
    "found; %1 is a "
    "garbage-collected or traceable "
    "type. Raw_ptr and raw_ref field and variable cannot hold "
    "garbage-collected or "
    "traceable objects.";

const char kRawPtrOrRefNewExprUsedWithGC[] =
    "[blink-gc] Disallowed new-expression of %0 found; %1 is a "
    "garbage-collected or traceable "
    "type. Raw_ptr and raw_ref fields cannot hold garbage-collected or "
    "traceable objects.";

const char kVariantUsedWithGC[] =
    "[blink-gc] Disallowed construction of %0 found; %1 is a garbage-collected "
    "type. Variant cannot hold garbage-collected objects.";

const char kCollectionOfGced[] =
    "[blink-gc] Disallowed collection %0 found; %1 is a "
    "garbage-collected "
    "type. Use heap collections to hold garbage-collected objects.";

const char kCollectionOfMembers[] =
    "[blink-gc] Disallowed collection %0 found; %1 is a "
    "Member type. Use heap collections to hold Members.";

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
  diag_class_contains_gc_root_ref_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kClassContainsGCRootRef);
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
  diag_member_in_stack_allocated_class_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kMemberInStackAllocated);
  diag_member_on_stack_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kMemberOnStack);
  diag_additional_padding_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kAdditionalPadding);
  diag_part_object_in_unmanaged_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kTraceablePartObjectInUnmanaged);
  diag_weak_ptr_to_gc_managed_class_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kWeakPtrToGCManagedClass);
  diag_gced_field_ = diagnostic_.getCustomDiagID(getErrorLevel(), kGCedField);
  diag_gced_var_ = diagnostic_.getCustomDiagID(getErrorLevel(), kGCedVar);
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
  diag_task_runner_timer_in_gc_class_note = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kTaskRunnerInGCManagedClassNote);
  diag_mojo_remote_in_gc_class_note = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kMojoRemoteInGCManagedClassNote);
  diag_mojo_receiver_in_gc_class_note = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kMojoReceiverInGCManagedClassNote);
  diag_mojo_associated_remote_in_gc_class_note = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kMojoAssociatedRemoteInGCManagedClassNote);
  diag_mojo_associated_receiver_in_gc_class_note = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kMojoAssociatedReceiverInGCManagedClassNote);
  diag_forbidden_field_part_object_class_note = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kForbiddenFieldPartObjectClassNote);
  diag_member_to_gc_unmanaged_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kMemberToGCUnmanagedClassNote);
  diag_stack_allocated_field_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kStackAllocatedFieldNote);
  diag_member_in_unmanaged_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kMemberInUnmanagedClassNote);
  diag_ptr_to_member_in_unmanaged_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kPtrToMemberInUnmanagedClassNote);
  diag_part_object_to_gc_derived_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kPartObjectToGCDerivedClassNote);
  diag_part_object_contains_gc_root_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kPartObjectContainsGCRootNote);
  diag_part_object_contains_gc_root_ref_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kPartObjectContainsGCRootRefNote);
  diag_field_contains_gc_root_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kFieldContainsGCRootNote);
  diag_field_contains_gc_root_ref_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kFieldContainsGCRootRefNote);
  diag_finalized_field_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kFinalizedFieldNote);
  diag_overridden_non_virtual_trace_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kOverriddenNonVirtualTraceNote);
  diag_manual_dispatch_method_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kManualDispatchMethodNote);
  diag_raw_ptr_to_traceable_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kRawPtrToTraceableClassNote);
  diag_ref_ptr_to_traceable_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kRefPtrToTraceableClassNote);
  diag_reference_ptr_to_traceable_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kReferencePtrToTraceableClassNote);
  diag_unique_ptr_to_traceable_class_note_ = diagnostic_.getCustomDiagID(
      DiagnosticsEngine::Note, kUniquePtrToTraceableClassNote);

  diag_unique_ptr_used_with_gc_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kUniquePtrUsedWithGC);
  diag_optional_decl_used_with_gc_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kOptionalDeclUsedWithGC);
  diag_optional_new_expr_used_with_gc_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kOptionalNewExprUsedWithGC);
  diag_optional_decl_used_with_member_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kOptionalDeclUsedWithMember);
  diag_optional_new_expr_used_with_member_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kOptionalNewExprUsedWithMember);
  diag_raw_ptr_or_ref_decl_used_with_gc_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kRawPtrOrRefDeclUsedWithGC);
  diag_raw_ptr_or_ref_new_expr_used_with_gc_ = diagnostic_.getCustomDiagID(
      getErrorLevel(), kRawPtrOrRefNewExprUsedWithGC);
  diag_variant_used_with_gc_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kVariantUsedWithGC);
  diag_collection_of_gced_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kCollectionOfGced);
  diag_collection_of_members_ =
      diagnostic_.getCustomDiagID(getErrorLevel(), kCollectionOfMembers);
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
    } else if (error.second == CheckFieldsVisitor::kPtrToMemberInUnmanaged) {
      note = diag_ptr_to_member_in_unmanaged_class_note_;
    } else if (error.second == CheckFieldsVisitor::kPtrFromHeapToStack) {
      note = diag_stack_allocated_field_note_;
    } else if (error.second == CheckFieldsVisitor::kGCDerivedPartObject) {
      note = diag_part_object_to_gc_derived_class_note_;
    } else if (error.second == CheckFieldsVisitor::kIteratorToGCManaged) {
      note = diag_iterator_to_gc_managed_collection_note_;
    } else if (error.second == CheckFieldsVisitor::kMemberInStackAllocated) {
      note = diag_member_in_stack_allocated_class_;
    } else if (error.second ==
               CheckFieldsVisitor::kTraceablePartObjectInUnmanaged) {
      note = diag_part_object_in_unmanaged_;
    } else if (error.second == CheckFieldsVisitor::kRawPtrToTraceable) {
      note = diag_raw_ptr_to_traceable_class_note_;
    } else if (error.second == CheckFieldsVisitor::kRefPtrToTraceable) {
      note = diag_ref_ptr_to_traceable_class_note_;
    } else if (error.second == CheckFieldsVisitor::kReferencePtrToTraceable) {
      note = diag_reference_ptr_to_traceable_class_note_;
    } else if (error.second == CheckFieldsVisitor::kUniquePtrToTraceable) {
      note = diag_unique_ptr_to_traceable_class_note_;
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

void DiagnosticsReporter::ClassContainsGCRootRefs(
    RecordInfo* info,
    const CheckGCRootsVisitor::Errors& errors) {
  for (auto& error : errors) {
    FieldPoint* point = nullptr;
    for (FieldPoint* path : error) {
      if (!point) {
        point = path;
        ReportDiagnostic(info->record()->getBeginLoc(),
                         diag_class_contains_gc_root_ref_)
            << info->record() << point->field();
        continue;
      }
      NotePartObjectContainsGCRootRef(point);
      point = path;
    }
    NoteFieldContainsGCRootRef(point);
  }
}

void DiagnosticsReporter::ClassContainsForbiddenFields(
    RecordInfo* info,
    const CheckForbiddenFieldsVisitor::Errors& errors) {
  ReportDiagnostic(info->record()->getBeginLoc(),
                   diag_class_contains_invalid_fields_)
      << info->record();
  for (const auto& error : errors) {
    for (FieldPoint* field : error.first) {
      if (field == error.first.back()) {
        break;
      }
      NoteField(field, diag_forbidden_field_part_object_class_note);
    }
    unsigned note;
    if (error.second ==
        CheckForbiddenFieldsVisitor::Error::kTaskRunnerInGCManaged) {
      note = diag_task_runner_timer_in_gc_class_note;
    } else if (error.second ==
               CheckForbiddenFieldsVisitor::Error::kMojoRemoteInGCManaged) {
      note = diag_mojo_remote_in_gc_class_note;
    } else if (error.second ==
               CheckForbiddenFieldsVisitor::Error::kMojoReceiverInGCManaged) {
      note = diag_mojo_receiver_in_gc_class_note;
    } else if (error.second == CheckForbiddenFieldsVisitor::Error::
                                   kMojoAssociatedRemoteInGCManaged) {
      note = diag_mojo_associated_remote_in_gc_class_note;
    } else if (error.second == CheckForbiddenFieldsVisitor::Error::
                                   kMojoAssociatedReceiverInGCManaged) {
      note = diag_mojo_associated_receiver_in_gc_class_note;
    } else {
      llvm_unreachable("Unknown field error.");
    }
    NoteField(error.first.back(), note);
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

void DiagnosticsReporter::NotePartObjectContainsGCRootRef(FieldPoint* point) {
  FieldDecl* field = point->field();
  ReportDiagnostic(field->getBeginLoc(),
                   diag_part_object_contains_gc_root_ref_note_)
      << field << field->getParent();
}

void DiagnosticsReporter::NoteFieldContainsGCRoot(FieldPoint* point) {
  NoteField(point, diag_field_contains_gc_root_note_);
}

void DiagnosticsReporter::NoteFieldContainsGCRootRef(FieldPoint* point) {
  NoteField(point, diag_field_contains_gc_root_ref_note_);
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

void DiagnosticsReporter::OptionalDeclUsedWithGC(
    const clang::Decl* decl,
    const clang::CXXRecordDecl* optional,
    const clang::CXXRecordDecl* gc_type) {
  ReportDiagnostic(decl->getBeginLoc(), diag_optional_decl_used_with_gc_)
      << optional << gc_type << decl->getSourceRange();
}

void DiagnosticsReporter::OptionalNewExprUsedWithGC(
    const clang::Expr* expr,
    const clang::CXXRecordDecl* optional,
    const clang::CXXRecordDecl* gc_type) {
  ReportDiagnostic(expr->getBeginLoc(), diag_optional_new_expr_used_with_gc_)
      << optional << gc_type << expr->getSourceRange();
}

void DiagnosticsReporter::OptionalDeclUsedWithMember(
    const clang::Decl* decl,
    const clang::CXXRecordDecl* optional,
    const clang::CXXRecordDecl* member) {
  ReportDiagnostic(decl->getBeginLoc(), diag_optional_decl_used_with_member_)
      << optional << member << decl->getSourceRange();
}

void DiagnosticsReporter::OptionalNewExprUsedWithMember(
    const clang::Expr* expr,
    const clang::CXXRecordDecl* optional,
    const clang::CXXRecordDecl* member) {
  ReportDiagnostic(expr->getBeginLoc(),
                   diag_optional_new_expr_used_with_member_)
      << optional << member << expr->getSourceRange();
}

void DiagnosticsReporter::RawPtrOrRefDeclUsedWithGC(
    const clang::Decl* decl,
    const clang::CXXRecordDecl* optional,
    const clang::CXXRecordDecl* gc_type) {
  ReportDiagnostic(decl->getBeginLoc(), diag_raw_ptr_or_ref_decl_used_with_gc_)
      << optional << gc_type << decl->getSourceRange();
}

void DiagnosticsReporter::RawPtrOrRefNewExprUsedWithGC(
    const clang::Expr* expr,
    const clang::CXXRecordDecl* optional,
    const clang::CXXRecordDecl* gc_type) {
  ReportDiagnostic(expr->getBeginLoc(),
                   diag_raw_ptr_or_ref_new_expr_used_with_gc_)
      << optional << gc_type << expr->getSourceRange();
}

void DiagnosticsReporter::VariantUsedWithGC(
    const clang::Expr* expr,
    const clang::CXXRecordDecl* variant,
    const clang::CXXRecordDecl* gc_type) {
  ReportDiagnostic(expr->getBeginLoc(), diag_variant_used_with_gc_)
      << variant << gc_type << expr->getSourceRange();
}

void DiagnosticsReporter::CollectionOfGCed(
    const clang::Decl* decl,
    const clang::CXXRecordDecl* collection,
    const clang::CXXRecordDecl* gc_type) {
  ReportDiagnostic(decl->getBeginLoc(), diag_collection_of_gced_)
      << collection << gc_type << decl->getSourceRange();
}

void DiagnosticsReporter::CollectionOfGCed(
    const clang::Expr* expr,
    const clang::CXXRecordDecl* collection,
    const clang::CXXRecordDecl* gc_type) {
  ReportDiagnostic(expr->getBeginLoc(), diag_collection_of_gced_)
      << collection << gc_type << expr->getSourceRange();
}

void DiagnosticsReporter::CollectionOfMembers(
    const clang::Decl* decl,
    const clang::CXXRecordDecl* collection,
    const clang::CXXRecordDecl* member) {
  ReportDiagnostic(decl->getBeginLoc(), diag_collection_of_members_)
      << collection << member << decl->getSourceRange();
}

void DiagnosticsReporter::CollectionOfMembers(
    const clang::Expr* expr,
    const clang::CXXRecordDecl* collection,
    const clang::CXXRecordDecl* member) {
  ReportDiagnostic(expr->getBeginLoc(), diag_collection_of_members_)
      << collection << member << expr->getSourceRange();
}

void DiagnosticsReporter::MemberOnStack(const clang::VarDecl* var) {
  ReportDiagnostic(var->getBeginLoc(), diag_member_on_stack_)
      << var->getName() << var->getSourceRange();
}

void DiagnosticsReporter::AdditionalPadding(const clang::RecordDecl* record,
                                            size_t padding_size) {
  ReportDiagnostic(record->getBeginLoc(), diag_additional_padding_)
      << record->getName() << padding_size << record->getSourceRange();
}

void DiagnosticsReporter::WeakPtrToGCed(const clang::Decl* decl,
                                        const clang::CXXRecordDecl* weak_ptr,
                                        const clang::CXXRecordDecl* gc_type) {
  ReportDiagnostic(decl->getBeginLoc(), diag_weak_ptr_to_gc_managed_class_)
      << weak_ptr << gc_type << decl->getSourceRange();
}

void DiagnosticsReporter::GCedField(const clang::FieldDecl* field,
                                    const clang::CXXRecordDecl* gctype) {
  ReportDiagnostic(field->getBeginLoc(), diag_gced_field_)
      << field << gctype << field->getSourceRange();
}
void DiagnosticsReporter::GCedVar(const clang::VarDecl* var,
                                  const clang::CXXRecordDecl* gctype) {
  ReportDiagnostic(var->getBeginLoc(), diag_gced_var_)
      << var << gctype << var->getSourceRange();
}
