// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the names used by GC infrastructure.

// TODO: Restructure the name determination to use fully qualified names (ala,
// blink::Foo) so that the plugin can be enabled for all of chromium. Doing so
// would allow us to catch errors with structures outside of blink that might
// have unsafe pointers to GC allocated blink structures.

#ifndef TOOLS_BLINK_GC_PLUGIN_CONFIG_H_
#define TOOLS_BLINK_GC_PLUGIN_CONFIG_H_

#include <cassert>

#include "clang/AST/AST.h"
#include "clang/AST/Attr.h"

extern const char kNewOperatorName[];
extern const char kCreateName[];
extern const char kTraceName[];
extern const char kFinalizeName[];
extern const char kTraceAfterDispatchName[];
extern const char kRegisterWeakMembersName[];
extern const char kHeapAllocatorName[];
extern const char kTraceIfNeededName[];
extern const char kVisitorDispatcherName[];
extern const char kVisitorVarName[];
extern const char kAdjustAndMarkName[];
extern const char kIsHeapObjectAliveName[];
extern const char kConstIteratorName[];
extern const char kIteratorName[];
extern const char kConstReverseIteratorName[];
extern const char kReverseIteratorName[];

class Config {
 public:
  static bool IsMember(const std::string& name) {
    return name == "Member";
  }

  static bool IsWeakMember(const std::string& name) {
    return name == "WeakMember";
  }

  static bool IsMemberHandle(const std::string& name) {
    return IsMember(name) ||
           IsWeakMember(name);
  }

  static bool IsPersistent(const std::string& name) {
    return name == "Persistent" ||
           name == "WeakPersistent" ;
  }

  static bool IsCrossThreadPersistent(const std::string& name) {
    return name == "CrossThreadPersistent" ||
           name == "CrossThreadWeakPersistent" ;
  }

  static bool IsRefPtr(const std::string& name) {
    return name == "RefPtr";
  }

  static bool IsUniquePtr(const std::string& name) {
    return name == "unique_ptr";
  }

  static bool IsTraceWrapperV8Reference(const std::string& name) {
    return name == "TraceWrapperV8Reference";
  }

  static bool IsWTFCollection(const std::string& name) {
    return name == "Vector" ||
           name == "Deque" ||
           name == "HashSet" ||
           name == "ListHashSet" ||
           name == "LinkedHashSet" ||
           name == "HashCountedSet" ||
           name == "HashMap";
  }

  static bool IsGCCollection(const std::string& name) {
    return name == "HeapVector" || name == "HeapDeque" ||
           name == "HeapHashSet" || name == "HeapListHashSet" ||
           name == "HeapLinkedHashSet" || name == "HeapHashCountedSet" ||
           name == "HeapHashMap";
  }

  static bool IsGCCollectionWithUnsafeIterator(const std::string& name) {
    if (!IsGCCollection(name))
      return false;
    // The list hash set iterators refer to the set, not the
    // backing store and are consequently safe.
    if (name == "HeapListHashSet" || name == "PersistentHeapListHashSet")
      return false;
    return true;
  }

  static bool IsHashMap(const std::string& name) {
    return name == "HashMap" ||
           name == "HeapHashMap" ||
           name == "PersistentHeapHashMap";
  }

  // Assumes name is a valid collection name.
  static size_t CollectionDimension(const std::string& name) {
    return (IsHashMap(name) || name == "pair") ? 2 : 1;
  }

  static bool IsRefCountedBase(const std::string& name) {
    return name == "RefCounted" ||
           name == "ThreadSafeRefCounted";
  }

  static bool IsGCSimpleBase(const std::string& name) {
    return name == "GarbageCollected";
  }

  static bool IsGCMixinBase(const std::string& name) {
    return name == "GarbageCollectedMixin";
  }

  static bool IsGCBase(const std::string& name) {
    return IsGCSimpleBase(name) || IsGCMixinBase(name);
  }

  static bool IsIterator(const std::string& name) {
    return name == kIteratorName || name == kConstIteratorName ||
           name == kReverseIteratorName || name == kConstReverseIteratorName;
  }

  // Returns true of the base classes that do not need a vtable entry for trace
  // because they cannot possibly initiate a GC during construction.
  static bool IsSafePolymorphicBase(const std::string& name) {
    return IsGCBase(name) || IsRefCountedBase(name);
  }

  static bool IsAnnotated(clang::Decl* decl, const std::string& anno) {
    clang::AnnotateAttr* attr = decl->getAttr<clang::AnnotateAttr>();
    return attr && (attr->getAnnotation() == anno);
  }

  static bool IsStackAnnotated(clang::Decl* decl) {
    return IsAnnotated(decl, "blink_stack_allocated");
  }

  static bool IsIgnoreAnnotated(clang::Decl* decl) {
    return IsAnnotated(decl, "blink_gc_plugin_ignore");
  }

  static bool IsIgnoreCycleAnnotated(clang::Decl* decl) {
    return IsAnnotated(decl, "blink_gc_plugin_ignore_cycle") ||
           IsIgnoreAnnotated(decl);
  }

  static bool IsVisitor(const std::string& name) {
    return name == "Visitor" || name == "VisitorHelper";
  }

  static bool IsVisitorPtrType(const clang::QualType& formal_type) {
    if (!formal_type->isPointerType())
      return false;

    clang::CXXRecordDecl* pointee_type =
        formal_type->getPointeeType()->getAsCXXRecordDecl();
    if (!pointee_type)
      return false;

    if (!IsVisitor(pointee_type->getName()))
      return false;

    return true;
  }

  static bool IsVisitorDispatcherType(const clang::QualType& formal_type) {
    if (const clang::SubstTemplateTypeParmType* subst_type =
            clang::dyn_cast<clang::SubstTemplateTypeParmType>(
                formal_type.getTypePtr())) {
      if (IsVisitorPtrType(subst_type->getReplacementType())) {
        // VisitorDispatcher template parameter substituted to Visitor*.
        return true;
      }
    } else if (const clang::TemplateTypeParmType* parm_type =
                   clang::dyn_cast<clang::TemplateTypeParmType>(
                       formal_type.getTypePtr())) {
      if (parm_type->getDecl()->getName() == kVisitorDispatcherName) {
        // Unresolved, but its parameter name is VisitorDispatcher.
        return true;
      }
    }

    return IsVisitorPtrType(formal_type);
  }

  enum TraceMethodType {
    NOT_TRACE_METHOD,
    TRACE_METHOD,
    TRACE_AFTER_DISPATCH_METHOD,
  };

  static TraceMethodType GetTraceMethodType(const clang::FunctionDecl* method) {
    if (method->getNumParams() != 1)
      return NOT_TRACE_METHOD;

    const std::string& name = method->getNameAsString();
    if (name != kTraceName && name != kTraceAfterDispatchName)
      return NOT_TRACE_METHOD;

    const clang::QualType& formal_type = method->getParamDecl(0)->getType();
    if (!IsVisitorPtrType(formal_type)) {
      return NOT_TRACE_METHOD;
    }

    if (name == kTraceName)
      return TRACE_METHOD;
    if (name == kTraceAfterDispatchName)
      return TRACE_AFTER_DISPATCH_METHOD;

    assert(false && "Should not reach here");
    return NOT_TRACE_METHOD;
  }

  static bool IsTraceMethod(const clang::FunctionDecl* method) {
    return GetTraceMethodType(method) != NOT_TRACE_METHOD;
  }

  static bool IsTraceWrappersMethod(const clang::FunctionDecl* method);

  static bool StartsWith(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size())
      return false;
    return str.compare(0, prefix.size(), prefix) == 0;
  }

  static bool EndsWith(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size())
      return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  // Test if a template specialization is an instantiation.
  static bool IsTemplateInstantiation(clang::CXXRecordDecl* record);
};

#endif  // TOOLS_BLINK_GC_PLUGIN_CONFIG_H_
