// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CheckForbiddenFieldsVisitor.h"
#include "BlinkGCPluginOptions.h"

#include <string_view>

#include "llvm/ADT/STLExtras.h"

CheckForbiddenFieldsVisitor::CheckForbiddenFieldsVisitor(
    const BlinkGCPluginOptions& options) {}

CheckForbiddenFieldsVisitor::Errors&
CheckForbiddenFieldsVisitor::forbidden_fields() {
  return forbidden_fields_;
}

bool CheckForbiddenFieldsVisitor::ContainsForbiddenFields(RecordInfo* info) {
  bool managed_host = info->IsStackAllocated() || info->IsGCAllocated() ||
                      info->IsNewDisallowed();
  if (!managed_host)
    return false;

  return ContainsForbiddenFieldsInternal(info);
}

bool CheckForbiddenFieldsVisitor::ContainsForbiddenFieldsInternal(
    RecordInfo* info) {
  for (auto& field : info->GetFields()) {
    current_.push_back(&field.second);
    field.second.edge()->Accept(this);
    current_.pop_back();
  }
  return !forbidden_fields_.empty();
}

void CheckForbiddenFieldsVisitor::VisitValue(Value* edge) {
  // TODO: what should we do to check unions?
  if (edge->value()->record()->isUnion())
    return;

  // Prevent infinite regress for cyclic embedded objects.
  if (visiting_set_.find(edge->value()) != visiting_set_.end())
    return;

  visiting_set_.insert(edge->value());

  // We want to keep recursing into the current field if we did not encounter
  // something else than a collection during our recursion. However, in case of
  // pointers, we still want to check whether their template specializations
  // are forbidden classes, and then stop the recursion.
  bool keep_recursing = true;
  bool check_for_forbidden_fields = true;
  for (Edge* e : llvm::reverse(context())) {
    if (!e->IsCollection()) {
      keep_recursing = false;
      check_for_forbidden_fields = false;
      if (e->IsRawPtr() || e->IsRefPtr() || e->IsUniquePtr()) {
        check_for_forbidden_fields = true;
      }
    }
  }

  if (check_for_forbidden_fields && ContainsInvalidFieldTypes(edge)) {
    visiting_set_.erase(edge->value());
    return;
  }

  if (keep_recursing) {
    ContainsForbiddenFieldsInternal(edge->value());
  }

  visiting_set_.erase(edge->value());
}

void CheckForbiddenFieldsVisitor::VisitArrayEdge(ArrayEdge* edge) {
  if (edge->element()->IsValue()) {
    edge->element()->Accept(this);
  }
}

bool CheckForbiddenFieldsVisitor::ContainsInvalidFieldTypes(Value* edge) {
  constexpr std::array<std::pair<std::string_view, Error>, 3> kErrors = {{
      {"blink::TaskRunnerTimer", Error::kTaskRunnerInGCManaged},
      {"mojo::Receiver", Error::kMojoReceiverInGCManaged},
      {"mojo::Remote", Error::kMojoRemoteInGCManaged},
  }};

  auto* decl = edge->value()->record()->getDefinition();
  if (!decl) {
    return false;
  }

  auto type_name = decl->getQualifiedNameAsString();
  auto it = std::find_if(
      kErrors.begin(), kErrors.end(),
      [&type_name](const auto& val) { return val.first == type_name; });

  if (it == kErrors.end()) {
    return false;
  }

  forbidden_fields_.push_back(std::make_pair(current_, it->second));
  return true;
}
