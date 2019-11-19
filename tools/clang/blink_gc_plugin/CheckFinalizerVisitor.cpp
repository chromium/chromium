// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CheckFinalizerVisitor.h"

using namespace clang;

namespace {

// Simple visitor to determine if the content of a field might be collected
// during finalization.
class MightBeCollectedVisitor : public EdgeVisitor {
 public:
  bool might_be_collected() const;

  void VisitMember(Member* edge) override;
  void VisitCollection(Collection* edge) override;

 private:
  bool might_be_collected_ = false;
};

bool MightBeCollectedVisitor::might_be_collected() const {
  return might_be_collected_;
}

void MightBeCollectedVisitor::VisitMember(Member* edge) {
  might_be_collected_ = true;
}

void MightBeCollectedVisitor::VisitCollection(Collection* edge) {
  if (edge->on_heap()) {
    might_be_collected_ = true;
  } else {
    edge->AcceptMembers(this);
  }
}

}  // namespace

CheckFinalizerVisitor::CheckFinalizerVisitor(RecordCache* cache)
    : blacklist_context_(false),
      cache_(cache) {
}

CheckFinalizerVisitor::Errors& CheckFinalizerVisitor::finalized_fields() {
  return finalized_fields_;
}

bool CheckFinalizerVisitor::WalkUpFromCXXOperatorCallExpr(
    CXXOperatorCallExpr* expr) {
  // Only continue the walk-up if the operator is a blacklisted one.
  switch (expr->getOperator()) {
    case OO_Arrow:
    case OO_Subscript:
      this->WalkUpFromCallExpr(expr);
      return true;
    default:
      return true;
  }
}

bool CheckFinalizerVisitor::WalkUpFromCallExpr(CallExpr* expr) {
  // We consider all non-operator calls to be blacklisted contexts.
  bool prev_blacklist_context = blacklist_context_;
  blacklist_context_ = true;
  for (size_t i = 0; i < expr->getNumArgs(); ++i)
    this->TraverseStmt(expr->getArg(i));
  blacklist_context_ = prev_blacklist_context;
  return true;
}

bool CheckFinalizerVisitor::VisitMemberExpr(MemberExpr* member) {
  FieldDecl* field = dyn_cast<FieldDecl>(member->getMemberDecl());
  if (!field)
    return true;

  RecordInfo* info = cache_->Lookup(field->getParent());
  if (!info)
    return true;

  RecordInfo::Fields::iterator it = info->GetFields().find(field);
  if (it == info->GetFields().end())
    return true;

  if (seen_members_.find(member) != seen_members_.end())
    return true;

  if (blacklist_context_ &&
      MightBeCollected(&it->second)) {
    finalized_fields_.push_back(
        Error(member, &it->second));
    seen_members_.insert(member);
  }
  return true;
}

bool CheckFinalizerVisitor::MightBeCollected(FieldPoint* point) {
  MightBeCollectedVisitor visitor;
  point->edge()->Accept(&visitor);
  return visitor.might_be_collected();
}
