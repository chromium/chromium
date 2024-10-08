// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_base.h"

#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom-shared-internal.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/compute_attributes.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

namespace {

using OnNotifyEventCallbackMap =
    std::map<ax::mojom::Event,
             // A function to call when focus changes, for testing only.
             base::RepeatingClosure>;

OnNotifyEventCallbackMap& GetOnNotifyEventCallbackMap() {
  static base::NoDestructor<OnNotifyEventCallbackMap>
      on_notify_event_for_testing;
  return *on_notify_event_for_testing;
}

// Check for descendant comment, using limited depth first search.
bool FindDescendantRoleWithMaxDepth(const AXPlatformNodeBase* node,
                                    ax::mojom::Role descendant_role,
                                    size_t max_depth,
                                    size_t max_children_to_check) {
  if (node->GetRole() == descendant_role)
    return true;
  if (max_depth <= 1)
    return false;

  size_t num_children_to_check =
      std::min(node->GetChildCount(), max_children_to_check);
  for (size_t index = 0; index < num_children_to_check; index++) {
    auto* child = static_cast<AXPlatformNodeBase*>(
        AXPlatformNode::FromNativeViewAccessible(node->ChildAtIndex(index)));
    if (child &&
        FindDescendantRoleWithMaxDepth(child, descendant_role, max_depth - 1,
                                       max_children_to_check)) {
      return true;
    }
  }

  return false;
}

// Map from each AXPlatformNode's unique id to its instance.
using UniqueIdMap =
    std::unordered_map<int32_t, raw_ptr<AXPlatformNode, CtnExperimental>>;
base::LazyInstance<UniqueIdMap>::Leaky g_unique_id_map =
    LAZY_INSTANCE_INITIALIZER;

// Adds process-wide statistics about accessibility objects to traces.
class AXPlatformNodeMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  AXPlatformNodeMemoryDumpProvider(const AXPlatformNodeMemoryDumpProvider&) =
      delete;
  AXPlatformNodeMemoryDumpProvider& operator=(
      const AXPlatformNodeMemoryDumpProvider&) = delete;

  // base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend class base::NoDestructor<AXPlatformNodeMemoryDumpProvider>;

  explicit AXPlatformNodeMemoryDumpProvider(const UniqueIdMap& id_to_node);
  ~AXPlatformNodeMemoryDumpProvider() override = default;

  const raw_ref<const UniqueIdMap> id_to_node_;
};

bool AXPlatformNodeMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  auto* const dump = pmd->CreateAllocatorDump("accessibility/ax_platform_node");
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  id_to_node_->size());
  return true;
}

AXPlatformNodeMemoryDumpProvider::AXPlatformNodeMemoryDumpProvider(
    const UniqueIdMap& id_to_node)
    : id_to_node_(id_to_node) {
  // Skip this in tests that don't set up a task runner on the main thread.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "AXPlatformNode",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

}  // namespace

const char16_t AXPlatformNodeBase::kEmbeddedCharacter = u'\xfffc';

// TODO(fxbug.dev/91030): Remove the !BUILDFLAG(IS_FUCHSIA) condition once
// fuchsia has native accessibility.
#if !BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY() && !BUILDFLAG(IS_FUCHSIA)
// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  AXPlatformNodeBase* node = new AXPlatformNodeBase();
  node->Init(delegate);
  return node;
}
#endif

// static
AXPlatformNode* AXPlatformNodeBase::GetFromUniqueId(int32_t unique_id) {
  UniqueIdMap* unique_ids = g_unique_id_map.Pointer();
  auto iter = unique_ids->find(unique_id);
  if (iter != unique_ids->end())
    return iter->second;

  return nullptr;
}

// static
size_t AXPlatformNodeBase::GetInstanceCountForTesting() {
  return g_unique_id_map.Get().size();
}

// static
void AXPlatformNodeBase::SetOnNotifyEventCallbackForTesting(
    ax::mojom::Event event_type,
    base::RepeatingClosure callback) {
  OnNotifyEventCallbackMap& callback_map = GetOnNotifyEventCallbackMap();
  callback_map[event_type] = std::move(callback);
}

AXPlatformNodeBase::AXPlatformNodeBase() = default;

AXPlatformNodeBase::~AXPlatformNodeBase() = default;

void AXPlatformNodeBase::Init(AXPlatformNodeDelegate* delegate) {
  delegate_ = delegate;

  // This must be called after assigning our delegate.
  g_unique_id_map.Get()[GetUniqueId()] = this;

  static base::NoDestructor<AXPlatformNodeMemoryDumpProvider> dump_provider(
      g_unique_id_map.Get());
}

const AXNodeData& AXPlatformNodeBase::GetData() const {
  static const base::NoDestructor<AXNodeData> empty_data;
  if (delegate_)
    return delegate_->GetData();
  return *empty_data;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetFocus() const {
  if (delegate_)
    return delegate_->GetFocus();
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetParent() const {
  if (delegate_)
    return delegate_->GetParent();
  return nullptr;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetPlatformParent() const {
  if (delegate_)
    return FromNativeViewAccessible(delegate_->GetParent());
  return nullptr;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetPlatformTextFieldAncestor() const {
  if (delegate_)
    return FromNativeViewAccessible(delegate_->GetTextFieldAncestor());
  return nullptr;
}

size_t AXPlatformNodeBase::GetChildCount() const {
  if (delegate_)
    return delegate_->GetChildCount();
  return 0;
}

gfx::NativeViewAccessible AXPlatformNodeBase::ChildAtIndex(size_t index) const {
  if (delegate_)
    return delegate_->ChildAtIndex(index);
  return nullptr;
}

std::string AXPlatformNodeBase::GetName() const {
  if (delegate_) {
    std::string name = delegate_->GetName();

    // Compute extra name based on the image annotation (generated alt text)
    // results.
    std::string extra_text;
    ax::mojom::ImageAnnotationStatus status =
        GetData().GetImageAnnotationStatus();
    switch (status) {
      case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
      case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
      case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
      case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
        extra_text = base::UTF16ToUTF8(
            delegate_->GetLocalizedStringForImageAnnotationStatus(status));
        break;

      case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
        extra_text =
            GetStringAttribute(ax::mojom::StringAttribute::kImageAnnotation);
        break;

      case ax::mojom::ImageAnnotationStatus::kNone:
      case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
      case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
      case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
        break;
    }

    if (!extra_text.empty()) {
      if (!name.empty())
        name += ". ";
      name += extra_text;
    }

    DCHECK(base::IsStringUTF8AllowingNoncharacters(name)) << "Invalid UTF8";
    return name;
  }
  return std::string();
}

std::optional<size_t> AXPlatformNodeBase::GetIndexInParent() {
  AXPlatformNodeBase* parent = FromNativeViewAccessible(GetParent());
  if (!parent)
    return std::nullopt;

  // If this is the webview, it is not in the child in the list of its parent's
  // child.
  // TODO(jkim): Check if we could remove this after making WebView ignored.
  if (delegate_ &&
      delegate_->GetNativeViewAccessible() != GetNativeViewAccessible()) {
    return std::nullopt;
  }

  size_t child_count = parent->GetChildCount();
  if (child_count == 0) {
    // |child_count| could be 0 if the parent is IsLeaf.
    DCHECK(parent->IsLeaf());
    return std::nullopt;
  }

  // Ask the delegate for the index in parent, and return it if it's plausible.
  //
  // Delegates are allowed to not implement this (ViewsAXPlatformNodeDelegate
  // returns -1). Also, delegates may not know the correct answer if this
  // node is the root of a tree that's embedded in another tree, in which
  // case the delegate should return -1 and we'll compute it.
  auto index = delegate_ ? delegate_->GetIndexInParent() : std::nullopt;
  if (index.has_value() && index.value() < child_count)
    return index;

  // Otherwise, search the parent's children.
  gfx::NativeViewAccessible current = GetNativeViewAccessible();
  for (size_t i = 0; i < child_count; i++) {
    if (parent->ChildAtIndex(i) == current)
      return i;
  }

  // If the parent has a modal dialog, it doesn't count other children.
  if (parent->delegate_ && parent->delegate_->HasModalDialog())
    return std::nullopt;

  DCHECK(false)
      << "Unable to find the child in the list of its parent's children.";
  return std::nullopt;
}

base::stack<gfx::NativeViewAccessible> AXPlatformNodeBase::GetAncestors() {
  base::stack<gfx::NativeViewAccessible> ancestors;
  gfx::NativeViewAccessible current_node = GetNativeViewAccessible();
  while (current_node) {
    ancestors.push(current_node);
    current_node = FromNativeViewAccessible(current_node)->GetParent();
  }

  return ancestors;
}

std::optional<int> AXPlatformNodeBase::CompareTo(AXPlatformNodeBase& other) {
  // We define two node's relative positions in the following way:
  // 1. this->CompareTo(other) == 0:
  //  - |this| and |other| are the same node.
  // 2. this->CompareTo(other) < 0:
  //  - |this| is an ancestor of |other|.
  //  - |this|'s first uncommon ancestor comes before |other|'s first uncommon
  //    ancestor. The first uncommon ancestor is defined as the immediate child
  //    of the lowest common anestor of the two nodes. The first uncommon
  //    ancestor of |this| and |other| share the same parent (i.e. lowest common
  //    ancestor), so we can just compare the first uncommon ancestors' child
  //    indices to determine their relative positions.
  // 3. this->CompareTo(other) == nullopt:
  //  - |this| and |other| are not comparable. E.g. they do not have a common
  //    ancestor.
  //
  // Another way to look at the nodes' relative positions/logical orders is that
  // they are equivalent to pre-order traversal of the tree. If we pre-order
  // traverse from the root, the node that we visited earlier is always going to
  // be before (logically less) the node we visit later.

  if (this == &other)
    return std::optional<int>(0);

  // Compute the ancestor stacks of both positions and traverse them from the
  // top most ancestor down, so we can discover the first uncommon ancestors.
  // The first uncommon ancestor is the immediate child of the lowest common
  // ancestor.
  gfx::NativeViewAccessible common_ancestor = nullptr;
  base::stack<gfx::NativeViewAccessible> our_ancestors = GetAncestors();
  base::stack<gfx::NativeViewAccessible> other_ancestors = other.GetAncestors();

  // Start at the root and traverse down. Keep going until the |this|'s ancestor
  // chain and |other|'s ancestor chain disagree. The last node before they
  // disagree is the lowest common ancestor.
  while (!our_ancestors.empty() && !other_ancestors.empty() &&
         our_ancestors.top() == other_ancestors.top()) {
    common_ancestor = our_ancestors.top();
    our_ancestors.pop();
    other_ancestors.pop();
  }

  // Nodes do not have a common ancestor, they are not comparable.
  if (!common_ancestor)
    return std::nullopt;

  // Compute the logical order when the common ancestor is |this| or |other|.
  auto* common_ancestor_platform_node =
      FromNativeViewAccessible(common_ancestor);
  if (common_ancestor_platform_node == this)
    return std::optional<int>(-1);
  if (common_ancestor_platform_node == &other)
    return std::optional<int>(1);

  // Compute the logical order of |this| and |other| by using their first
  // uncommon ancestors.
  if (!our_ancestors.empty() && !other_ancestors.empty()) {
    std::optional<int> this_index_in_parent =
        FromNativeViewAccessible(our_ancestors.top())->GetIndexInParent();
    std::optional<int> other_index_in_parent =
        FromNativeViewAccessible(other_ancestors.top())->GetIndexInParent();

    if (!this_index_in_parent || !other_index_in_parent)
      return std::nullopt;

    int this_uncommon_ancestor_index = this_index_in_parent.value();
    int other_uncommon_ancestor_index = other_index_in_parent.value();
    DCHECK_NE(this_uncommon_ancestor_index, other_uncommon_ancestor_index)
        << "Deepest uncommon ancestors should truly be uncommon, i.e. not "
           "the same.";

    return std::optional<int>(this_uncommon_ancestor_index -
                              other_uncommon_ancestor_index);
  }

  return std::nullopt;
}

AXNodeID AXPlatformNodeBase::GetNodeId() const {
  if (!delegate_)
    return kInvalidAXNodeID;

  return delegate_->GetData().id;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetActiveDescendant() const {
  if (!delegate_)
    return nullptr;

  AXNodeID active_descendant_id;
  AXPlatformNodeBase* active_descendant = nullptr;
  if (GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                      &active_descendant_id)) {
    active_descendant = static_cast<AXPlatformNodeBase*>(
        delegate_->GetFromNodeID(active_descendant_id));
  }

  if (GetRole() == ax::mojom::Role::kComboBoxSelect) {
    AXPlatformNodeBase* child = GetFirstChild();
    if (child && child->GetRole() == ax::mojom::Role::kMenuListPopup &&
        !child->IsInvisibleOrIgnored()) {
      // The active descendant is found on the menu list popup, i.e. on the
      // actual list and not on the button that opens it.
      // If there is no active descendant, focus should stay on the button so
      // that Windows screen readers would enable their virtual cursor.
      // Do not expose an activedescendant in a hidden/collapsed list, as
      // screen readers expect the focus event to go to the button itself.
      // Note that the AX hierarchy in this case is strange -- the active
      // option is the only visible option, and is inside an invisible list.
      if (child->GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                 &active_descendant_id)) {
        active_descendant = static_cast<AXPlatformNodeBase*>(
            child->delegate_->GetFromNodeID(active_descendant_id));
      }
    }
  }

  if (active_descendant && !active_descendant->IsInvisibleOrIgnored())
    return active_descendant;

  return nullptr;
}

// AXPlatformNode overrides.

void AXPlatformNodeBase::Destroy() {
  g_unique_id_map.Get().erase(GetUniqueId());
  AXPlatformNode::Destroy();
  delegate_ = nullptr;
  Dispose();
}

void AXPlatformNodeBase::Dispose() {
  delete this;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetNativeViewAccessible() {
  return nullptr;
}

void AXPlatformNodeBase::NotifyAccessibilityEvent(ax::mojom::Event event_type) {
  if (event_type == ax::mojom::Event::kAlert) {
    CHECK(IsAlert(GetRole()))
        << "On some platforms, the alert event does not work correctly unless "
           "it is fired on an object with an alert role. Role was "
        << GetRole();
  }

  OnNotifyEventCallbackMap& callback_map = GetOnNotifyEventCallbackMap();
  if (callback_map.find(event_type) != callback_map.end() &&
      callback_map[event_type]) {
    callback_map[event_type].Run();
  }
}

#if BUILDFLAG(IS_APPLE)
void AXPlatformNodeBase::AnnounceTextAs(const std::u16string& text,
                                        AnnouncementType announcement_type) {}
#endif

AXPlatformNodeDelegate* AXPlatformNodeBase::GetDelegate() const {
  return delegate_;
}

bool AXPlatformNodeBase::IsDescendantOf(AXPlatformNode* ancestor) const {
  if (!ancestor)
    return false;

  if (this == ancestor)
    return true;

  AXPlatformNodeBase* parent = FromNativeViewAccessible(GetParent());
  if (!parent)
    return false;

  return parent->IsDescendantOf(ancestor);
}

AXPlatformNodeBase::AXPlatformNodeChildIterator
AXPlatformNodeBase::AXPlatformNodeChildrenBegin() const {
  return AXPlatformNodeChildIterator(this, GetFirstChild());
}

AXPlatformNodeBase::AXPlatformNodeChildIterator
AXPlatformNodeBase::AXPlatformNodeChildrenEnd() const {
  return AXPlatformNodeChildIterator(this, nullptr);
}
// Helpers.

AXPlatformNodeBase* AXPlatformNodeBase::GetPreviousSibling() const {
  if (!delegate_)
    return nullptr;
  return FromNativeViewAccessible(delegate_->GetPreviousSibling());
}

AXPlatformNodeBase* AXPlatformNodeBase::GetNextSibling() const {
  if (!delegate_)
    return nullptr;
  return FromNativeViewAccessible(delegate_->GetNextSibling());
}

AXPlatformNodeBase* AXPlatformNodeBase::GetFirstChild() const {
  if (!delegate_)
    return nullptr;
  return FromNativeViewAccessible(delegate_->GetFirstChild());
}

AXPlatformNodeBase* AXPlatformNodeBase::GetLastChild() const {
  if (!delegate_)
    return nullptr;
  return FromNativeViewAccessible(delegate_->GetLastChild());
}

bool AXPlatformNodeBase::IsDescendant(AXPlatformNodeBase* node) {
  if (!delegate_)
    return false;
  if (!node)
    return false;
  if (node == this)
    return true;
  gfx::NativeViewAccessible native_parent = node->GetParent();
  if (!native_parent)
    return false;
  AXPlatformNodeBase* parent = FromNativeViewAccessible(native_parent);
  return IsDescendant(parent);
}

ax::mojom::Role AXPlatformNodeBase::GetRole() const {
  if (!delegate_)
    return ax::mojom::Role::kUnknown;
  return delegate_->GetRole();
}

bool AXPlatformNodeBase::HasBoolAttribute(
    ax::mojom::BoolAttribute attribute) const {
  if (!delegate_)
    return false;
  return delegate_->HasBoolAttribute(attribute);
}

bool AXPlatformNodeBase::GetBoolAttribute(
    ax::mojom::BoolAttribute attribute) const {
  if (!delegate_)
    return false;
  return delegate_->GetBoolAttribute(attribute);
}

bool AXPlatformNodeBase::GetBoolAttribute(ax::mojom::BoolAttribute attribute,
                                          bool* value) const {
  if (!delegate_)
    return false;
  return delegate_->GetBoolAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasFloatAttribute(
    ax::mojom::FloatAttribute attribute) const {
  if (!delegate_)
    return false;
  return delegate_->HasFloatAttribute(attribute);
}

float AXPlatformNodeBase::GetFloatAttribute(
    ax::mojom::FloatAttribute attribute) const {
  if (!delegate_)
    return 0.0f;
  return delegate_->GetFloatAttribute(attribute);
}

bool AXPlatformNodeBase::GetFloatAttribute(ax::mojom::FloatAttribute attribute,
                                           float* value) const {
  if (!delegate_)
    return false;
  return delegate_->GetFloatAttribute(attribute, value);
}

const std::vector<std::pair<ax::mojom::IntAttribute, int32_t>>&
AXPlatformNodeBase::GetIntAttributes() const {
  static const base::NoDestructor<
      const std::vector<std::pair<ax::mojom::IntAttribute, int32_t>>>
      empty_data;
  if (!delegate_)
    return *empty_data;
  return delegate_->GetIntAttributes();
}

bool AXPlatformNodeBase::HasIntAttribute(
    ax::mojom::IntAttribute attribute) const {
  if (!delegate_)
    return false;
  return delegate_->HasIntAttribute(attribute);
}

int AXPlatformNodeBase::GetIntAttribute(
    ax::mojom::IntAttribute attribute) const {
  if (!delegate_)
    return 0;
  return delegate_->GetIntAttribute(attribute);
}

bool AXPlatformNodeBase::GetIntAttribute(ax::mojom::IntAttribute attribute,
                                         int* value) const {
  if (!delegate_)
    return false;
  return delegate_->GetIntAttribute(attribute, value);
}

const std::vector<std::pair<ax::mojom::StringAttribute, std::string>>&
AXPlatformNodeBase::GetStringAttributes() const {
  static const base::NoDestructor<
      const std::vector<std::pair<ax::mojom::StringAttribute, std::string>>>
      empty_data;
  if (!delegate_)
    return *empty_data;
  return delegate_->GetStringAttributes();
}

bool AXPlatformNodeBase::HasStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  if (!delegate_)
    return false;
  return delegate_->HasStringAttribute(attribute);
}

const std::string& AXPlatformNodeBase::GetStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  if (!delegate_)
    return base::EmptyString();
  return delegate_->GetStringAttribute(attribute);
}

bool AXPlatformNodeBase::GetStringAttribute(
    ax::mojom::StringAttribute attribute,
    std::string* value) const {
  if (!delegate_)
    return false;
  return delegate_->GetStringAttribute(attribute, value);
}

std::u16string AXPlatformNodeBase::GetString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  if (!delegate_)
    return std::u16string();
  return delegate_->GetString16Attribute(attribute);
}

bool AXPlatformNodeBase::GetString16Attribute(
    ax::mojom::StringAttribute attribute,
    std::u16string* value) const {
  if (!delegate_)
    return false;
  return delegate_->GetString16Attribute(attribute, value);
}

bool AXPlatformNodeBase::HasInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  const AXPlatformNodeBase* current_node = this;

  do {
    if (!current_node->delegate_) {
      return false;
    }

    if (current_node->HasStringAttribute(attribute)) {
      return true;
    }

    current_node = FromNativeViewAccessible(current_node->GetParent());
  } while (current_node);

  return false;
}

const std::string& AXPlatformNodeBase::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  // TODO(nektar): Switch to using `AXNode::GetInheritedStringAttribute` after
  // it has been modified to cross tree boundaries.
  const AXPlatformNodeBase* current_node = this;

  do {
    if (!current_node->delegate_) {
      return base::EmptyString();
    }

    if (current_node->HasStringAttribute(attribute)) {
      return current_node->GetStringAttribute(attribute);
    }

    current_node = FromNativeViewAccessible(current_node->GetParent());
  } while (current_node);

  return base::EmptyString();
}

bool AXPlatformNodeBase::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute,
    std::string* value) const {
  // TODO(nektar): Switch to using `AXNode::GetInheritedStringAttribute` after
  // it has been modified to cross tree boundaries.
  const AXPlatformNodeBase* current_node = this;

  do {
    if (!current_node->delegate_) {
      return false;
    }

    if (current_node->GetStringAttribute(attribute, value)) {
      return true;
    }

    current_node = FromNativeViewAccessible(current_node->GetParent());
  } while (current_node);

  return false;
}

std::u16string AXPlatformNodeBase::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  // TODO(nektar): Switch to using `AXNode::GetInheritedString16Attribute` after
  // it has been modified to cross tree boundaries.
  return base::UTF8ToUTF16(GetInheritedStringAttribute(attribute));
}

bool AXPlatformNodeBase::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute,
    std::u16string* value) const {
  // TODO(nektar): Switch to using `AXNode::GetInheritedString16Attribute` after
  // it has been modified to cross tree boundaries.
  std::string value_utf8;
  if (!GetInheritedStringAttribute(attribute, &value_utf8))
    return false;
  *value = base::UTF8ToUTF16(value_utf8);
  return true;
}

const std::vector<std::pair<ax::mojom::IntListAttribute, std::vector<int32_t>>>&
AXPlatformNodeBase::GetIntListAttributes() const {
  static const base::NoDestructor<const std::vector<
      std::pair<ax::mojom::IntListAttribute, std::vector<int32_t>>>>
      empty_data;
  if (!delegate_)
    return *empty_data;
  return delegate_->GetIntListAttributes();
}

bool AXPlatformNodeBase::HasIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  if (!delegate_)
    return false;
  return delegate_->HasIntListAttribute(attribute);
}

const std::vector<int32_t>& AXPlatformNodeBase::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  static const base::NoDestructor<std::vector<int32_t>> empty_data;
  if (!delegate_)
    return *empty_data;
  return delegate_->GetIntListAttribute(attribute);
}

bool AXPlatformNodeBase::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute,
    std::vector<int32_t>* value) const {
  if (!delegate_)
    return false;
  return delegate_->GetIntListAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasStringListAttribute(
    ax::mojom::StringListAttribute attribute) const {
  if (!delegate_)
    return false;
  return delegate_->HasStringListAttribute(attribute);
}

const std::vector<std::string>& AXPlatformNodeBase::GetStringListAttribute(
    ax::mojom::StringListAttribute attribute) const {
  static const base::NoDestructor<std::vector<std::string>> empty_data;
  if (!delegate_)
    return *empty_data;
  return delegate_->GetStringListAttribute(attribute);
}

bool AXPlatformNodeBase::GetStringListAttribute(
    ax::mojom::StringListAttribute attribute,
    std::vector<std::string>* value) const {
  if (!delegate_)
    return false;
  return delegate_->GetStringListAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasHtmlAttribute(const char* attribute) const {
  if (!delegate_)
    return false;
  return delegate_->HasHtmlAttribute(attribute);
}

const base::StringPairs& AXPlatformNodeBase::GetHtmlAttributes() const {
  static const base::NoDestructor<base::StringPairs> empty_data;
  if (!delegate_)
    return *empty_data;
  return delegate_->GetHtmlAttributes();
}

bool AXPlatformNodeBase::GetHtmlAttribute(const char* attribute,
                                          std::string* value) const {
  if (!delegate_)
    return false;
  return delegate_->GetHtmlAttribute(attribute, value);
}

bool AXPlatformNodeBase::GetHtmlAttribute(const char* attribute,
                                          std::u16string* value) const {
  if (!delegate_)
    return false;
  return delegate_->GetHtmlAttribute(attribute, value);
}

AXTextAttributes AXPlatformNodeBase::GetTextAttributes() const {
  if (!delegate_)
    return AXTextAttributes();
  return delegate_->GetTextAttributes();
}

bool AXPlatformNodeBase::HasState(ax::mojom::State state) const {
  if (!delegate_)
    return false;
  return delegate_->HasState(state);
}

ax::mojom::State AXPlatformNodeBase::GetState() const {
  if (!delegate_)
    return ax::mojom::State::kNone;
  return delegate_->GetState();
}

bool AXPlatformNodeBase::HasAction(ax::mojom::Action action) const {
  if (!delegate_)
    return false;
  return delegate_->HasAction(action);
}

bool AXPlatformNodeBase::HasTextStyle(ax::mojom::TextStyle text_style) const {
  if (!delegate_)
    return false;
  return delegate_->HasTextStyle(text_style);
}

ax::mojom::NameFrom AXPlatformNodeBase::GetNameFrom() const {
  if (!delegate_)
    return ax::mojom::NameFrom::kNone;
  return delegate_->GetNameFrom();
}

bool AXPlatformNodeBase::HasNameFromOtherElement() const {
  ax::mojom::NameFrom nameFrom = GetNameFrom();
  return nameFrom == ax::mojom::NameFrom::kCaption ||
         nameFrom == ax::mojom::NameFrom::kRelatedElement;
}

// static
AXPlatformNodeBase* AXPlatformNodeBase::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(accessible));
}

bool AXPlatformNodeBase::SetHypertextSelection(int start_offset,
                                               int end_offset) {
  if (!delegate_)
    return false;
  return delegate_->SetHypertextSelection(start_offset, end_offset);
}

bool AXPlatformNodeBase::IsPlatformDocument() const {
  return delegate_ && delegate_->IsPlatformDocument();
}

bool AXPlatformNodeBase::IsStructuredAnnotation() const {
  // The node represents a structured annotation if it can trace back to a
  // target node that is being annotated.
  std::vector<AXPlatformNode*> reverse_relations =
      GetDelegate()->GetSourceNodesForReverseRelations(
          ax::mojom::IntListAttribute::kDetailsIds);

  return !reverse_relations.empty();
}

bool AXPlatformNodeBase::IsTextField() const {
  return GetData().IsTextField();
}

bool AXPlatformNodeBase::IsAtomicTextField() const {
  return GetData().IsAtomicTextField();
}

bool AXPlatformNodeBase::IsNonAtomicTextField() const {
  return GetData().IsNonAtomicTextField();
}

bool AXPlatformNodeBase::IsText() const {
  return delegate_ && delegate_->IsText();
}

std::u16string AXPlatformNodeBase::GetHypertext() const {
  if (!delegate_)
    return std::u16string();

  // Hypertext of platform leaves, which internally are composite objects, are
  // represented with the text content of the internal composite object. These
  // don't exist on non-web content.
  if (IsChildOfLeaf())
    return GetTextContentUTF16();

  if (hypertext_.needs_update)
    UpdateComputedHypertext();
  return hypertext_.hypertext;
}

std::u16string AXPlatformNodeBase::GetTextContentUTF16() const {
  if (!delegate_)
    return std::u16string();
  return delegate_->GetTextContentUTF16();
}

std::u16string
AXPlatformNodeBase::GetRoleDescriptionFromImageAnnotationStatusOrFromAttribute()
    const {
  if (GetRole() == ax::mojom::Role::kImage &&
      (GetData().GetImageAnnotationStatus() ==
           ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation ||
       GetData().GetImageAnnotationStatus() ==
           ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation)) {
    return GetDelegate()->GetLocalizedRoleDescriptionForUnlabeledImage();
  }

  return GetString16Attribute(ax::mojom::StringAttribute::kRoleDescription);
}

std::u16string AXPlatformNodeBase::GetRoleDescription() const {
  std::u16string role_description =
      GetRoleDescriptionFromImageAnnotationStatusOrFromAttribute();

  if (!role_description.empty()) {
    return role_description;
  }

  return GetDelegate()->GetLocalizedStringForRoleDescription();
}

bool AXPlatformNodeBase::IsImageWithMap() const {
  DCHECK_EQ(GetRole(), ax::mojom::Role::kImage)
      << "Only call IsImageWithMap() on an image";
  return GetChildCount();
}

AXPlatformNodeBase* AXPlatformNodeBase::GetSelectionContainer() const {
  if (!delegate_)
    return nullptr;
  return FromNativeViewAccessible(delegate_->GetSelectionContainer());
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTable() const {
  if (!delegate_)
    return nullptr;
  return FromNativeViewAccessible(delegate_->GetTableAncestor());
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTableCaption() const {
  if (!delegate_)
    return nullptr;
  return static_cast<AXPlatformNodeBase*>(delegate_->GetTableCaption());
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTableCell(int index) const {
  if (!delegate_)
    return nullptr;

  std::optional<int32_t> cell_id = delegate_->CellIndexToId(index);
  if (!cell_id)
    return nullptr;

  return static_cast<AXPlatformNodeBase*>(delegate_->GetFromNodeID(*cell_id));
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTableCell(int row,
                                                     int column) const {
  if (!delegate_) {
    return nullptr;
  }

  std::optional<int32_t> cell_id = delegate_->GetCellId(row, column);
  if (!cell_id)
    return nullptr;

  return static_cast<AXPlatformNodeBase*>(delegate_->GetFromNodeID(*cell_id));
}

AXPlatformNodeBase* AXPlatformNodeBase::GetAriaTableCell(int aria_row,
                                                         int aria_column) const {
  if (!delegate_) {
    return nullptr;
  }

  std::optional<int32_t> cell_id =
      delegate_->GetCellIdAriaCoords(aria_row, aria_column);
  if (!cell_id) {
    return nullptr;
  }
  return static_cast<AXPlatformNodeBase*>(delegate_->GetFromNodeID(*cell_id));
}

std::optional<int> AXPlatformNodeBase::GetTableCellIndex() const {
  if (!delegate_)
    return std::nullopt;
  return delegate_->GetTableCellIndex();
}

std::optional<int> AXPlatformNodeBase::GetTableColumn() const {
  if (!delegate_)
    return std::nullopt;
  return delegate_->GetTableCellColIndex();
}

std::optional<int> AXPlatformNodeBase::GetTableColumnCount() const {
  if (!delegate_)
    return std::nullopt;
  return delegate_->GetTableColCount();
}

std::optional<int> AXPlatformNodeBase::GetTableAriaColumnCount() const {
  if (!delegate_)
    return std::nullopt;
  return delegate_->GetTableAriaColCount();
}

std::optional<int> AXPlatformNodeBase::GetTableColumnSpan() const {
  if (!delegate_)
    return std::nullopt;
  return delegate_->GetTableCellColSpan();
}

std::optional<int> AXPlatformNodeBase::GetTableRow() const {
  if (!delegate_)
    return std::nullopt;
  if (delegate_->IsTableRow())
    return delegate_->GetTableRowRowIndex();
  if (delegate_->IsTableCellOrHeader())
    return delegate_->GetTableCellRowIndex();
  return std::nullopt;
}

std::optional<int> AXPlatformNodeBase::GetTableRowCount() const {
  if (!delegate_)
    return std::nullopt;
  return delegate_->GetTableRowCount();
}

std::optional<int> AXPlatformNodeBase::GetTableAriaRowCount() const {
  if (!delegate_)
    return std::nullopt;
  return delegate_->GetTableAriaRowCount();
}

std::optional<int> AXPlatformNodeBase::GetTableRowSpan() const {
  if (!delegate_)
    return std::nullopt;
  return delegate_->GetTableCellRowSpan();
}

std::optional<float> AXPlatformNodeBase::GetFontSizeInPoints() const {
  float font_size;
  // Attribute has no default value.
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kFontSize, &font_size)) {
    // The IA2 Spec requires the value to be in pt, not in pixels.
    // There are 72 points per inch.
    // We assume that there are 96 pixels per inch on a standard display.
    // TODO(nektar): Figure out the current value of pixels per inch.
    float points = font_size * 72.0 / 96.0;

    // Round to the nearest 0.5 points.
    points = std::round(points * 2.0) / 2.0;
    return points;
  }
  return std::nullopt;
}

bool AXPlatformNodeBase::HasVisibleCaretOrSelection() const {
  return delegate_ && delegate_->HasVisibleCaretOrSelection();
}

bool AXPlatformNodeBase::IsLeaf() const {
  return delegate_ && delegate_->IsLeaf();
}

bool AXPlatformNodeBase::IsChildOfLeaf() const {
  return delegate_ && delegate_->IsChildOfLeaf();
}

bool AXPlatformNodeBase::IsInvisibleOrIgnored() const {
  if (!GetData().IsInvisibleOrIgnored())
    return false;

  // Never marked a focused node as invisible or ignored, otherwise screen
  // reader users will not hear an announcement for it when it receives focus.
  if (IsFocused())
    return false;

  return !HasVisibleCaretOrSelection();
}

bool AXPlatformNodeBase::IsFocused() const {
  return delegate_ && FromNativeViewAccessible(delegate_->GetFocus()) == this;
}

bool AXPlatformNodeBase::IsFocusable() const {
  return delegate_ && delegate_->IsFocusable();
}

bool AXPlatformNodeBase::IsScrollable() const {
  return (HasIntAttribute(ax::mojom::IntAttribute::kScrollXMin) &&
          HasIntAttribute(ax::mojom::IntAttribute::kScrollXMax) &&
          HasIntAttribute(ax::mojom::IntAttribute::kScrollX)) ||
         (HasIntAttribute(ax::mojom::IntAttribute::kScrollYMin) &&
          HasIntAttribute(ax::mojom::IntAttribute::kScrollYMax) &&
          HasIntAttribute(ax::mojom::IntAttribute::kScrollY));
}

bool AXPlatformNodeBase::IsHorizontallyScrollable() const {
  DCHECK_GE(GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin), 0)
      << "Pixel sizes should be non-negative.";
  DCHECK_GE(GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax), 0)
      << "Pixel sizes should be non-negative.";
  return IsScrollable() &&
         GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin) <
             GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
}

bool AXPlatformNodeBase::IsVerticallyScrollable() const {
  DCHECK_GE(GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin), 0)
      << "Pixel sizes should be non-negative.";
  DCHECK_GE(GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax), 0)
      << "Pixel sizes should be non-negative.";
  return IsScrollable() &&
         GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin) <
             GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);
}

std::u16string AXPlatformNodeBase::GetValueForControl() const {
  if (!delegate_)
    return std::u16string();
  return delegate_->GetValueForControl();
}

void AXPlatformNodeBase::ComputeAttributes(PlatformAttributeList* attributes) {
  DCHECK(delegate_) << "Many attributes need to be retrieved from our "
                       "AXPlatformNodeDelegate.";
  // Expose some HTML and ARIA attributes in the IAccessible2 attributes string
  // "display", "tag", and "xml-roles" have somewhat unusual names for
  // historical reasons. Aside from that, virtually every ARIA attribute
  // is exposed in a really straightforward way, i.e. "aria-foo" is exposed
  // as "foo".
  AddAttributeToList(ax::mojom::StringAttribute::kDisplay, "display",
                     attributes);
  AddAttributeToList(ax::mojom::StringAttribute::kHtmlTag, "tag", attributes);
  AddAttributeToList(ax::mojom::StringAttribute::kRole, "xml-roles",
                     attributes);
  AddAttributeToList(ax::mojom::StringAttribute::kPlaceholder, "placeholder",
                     attributes);

  AddAttributeToList(ax::mojom::StringAttribute::kAutoComplete, "autocomplete",
                     attributes);
  if (!HasStringAttribute(ax::mojom::StringAttribute::kAutoComplete) &&
      HasState(ax::mojom::State::kAutofillAvailable)) {
    AddAttributeToList("autocomplete", "list", attributes);
  }

  std::u16string role_description =
      GetRoleDescriptionFromImageAnnotationStatusOrFromAttribute();
  if (!role_description.empty() ||
      HasStringAttribute(ax::mojom::StringAttribute::kRoleDescription)) {
    AddAttributeToList("roledescription", base::UTF16ToUTF8(role_description),
                       attributes);
  }

  // Expose description-from and description.
  int desc_from;
  if (GetIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom, &desc_from)) {
    std::string from;
    switch (static_cast<ax::mojom::DescriptionFrom>(desc_from)) {
      case ax::mojom::DescriptionFrom::kAriaDescription:
        // Descriptions are exposed via each platform's usual description field.
        // Also, only aria-description is exposed via the "description" object
        // attribute, in order to match Firefox.
        AddAttributeToList(ax::mojom::StringAttribute::kDescription,
                           "description", attributes);
        from = "aria-description";
        break;
      case ax::mojom::DescriptionFrom::kButtonLabel:
        from = "button-label";
        break;
      case ax::mojom::DescriptionFrom::kProhibitedNameRepair:
        from = "prohibited-name-repair";
        break;
      case ax::mojom::DescriptionFrom::kRelatedElement:
        // aria-describedby=tooltip is mapped to "tooltip".
        from = IsDescribedByTooltip() ? "tooltip" : "aria-describedby";
        break;
      case ax::mojom::DescriptionFrom::kRubyAnnotation:
        from = "ruby-annotation";
        break;
      case ax::mojom::DescriptionFrom::kSummary:
        from = "summary";
        break;
      case ax::mojom::DescriptionFrom::kSvgDescElement:
        from = "svg-desc-element";
        break;
      case ax::mojom::DescriptionFrom::kTableCaption:
        from = "table-caption";
        break;
      case ax::mojom::DescriptionFrom::kTitle:
      case ax::mojom::DescriptionFrom::kPopoverAttribute:
        // The following types of markup are mapped to "tooltip":
        // * The title attribute.
        // * A popover=something related via the `popovertarget` attribute.
        // * A tooltip related via aria-describedby (see kRelatedElement above).
        from = "tooltip";
        break;
      case ax::mojom::DescriptionFrom::kNone:
      case ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty:
        break;
    }
    if (!from.empty()) {
      AddAttributeToList("description-from", from, attributes);
    }
  }

  AddAttributeToList(ax::mojom::StringAttribute::kAriaBrailleLabel,
                     "braillelabel", attributes);
  AddAttributeToList(ax::mojom::StringAttribute::kAriaBrailleRoleDescription,
                     "brailleroledescription", attributes);

  AddAttributeToList(ax::mojom::StringAttribute::kKeyShortcuts, "keyshortcuts",
                     attributes);
  AddAttributeToList(ax::mojom::IntAttribute::kHierarchicalLevel, "level",
                     attributes);
  AddAttributeToList(ax::mojom::IntAttribute::kSetSize, "setsize", attributes);
  AddAttributeToList(ax::mojom::IntAttribute::kPosInSet, "posinset",
                     attributes);

  if (IsPlatformCheckable())
    AddAttributeToList("checkable", "true", attributes);

  if (IsInvisibleOrIgnored())  // Note: NVDA prefers this over INVISIBLE state.
    AddAttributeToList("hidden", "true", attributes);

  // Expose live region attributes.
  AddAttributeToList(ax::mojom::StringAttribute::kLiveStatus, "live",
                     attributes);
  AddAttributeToList(ax::mojom::StringAttribute::kLiveRelevant, "relevant",
                     attributes);
  AddAttributeToList(ax::mojom::BoolAttribute::kLiveAtomic, "atomic",
                     attributes);
  // Busy is usually associated with live regions but can occur anywhere:
  AddAttributeToList(ax::mojom::BoolAttribute::kBusy, "busy", attributes);

  // Expose container live region attributes.
  AddAttributeToList(ax::mojom::StringAttribute::kContainerLiveStatus,
                     "container-live", attributes);
  AddAttributeToList(ax::mojom::StringAttribute::kContainerLiveRelevant,
                     "container-relevant", attributes);
  AddAttributeToList(ax::mojom::BoolAttribute::kContainerLiveAtomic,
                     "container-atomic", attributes);
  AddAttributeToList(ax::mojom::BoolAttribute::kContainerLiveBusy,
                     "container-busy", attributes);

  // Expose name-from.
  ax::mojom::NameFrom name_from = GetNameFrom();
  std::string from;
  bool is_explicit_name = true;
  switch (static_cast<ax::mojom::NameFrom>(name_from)) {
    case ax::mojom::NameFrom::kAttribute:
      from = "attribute";
      DCHECK(!GetName().empty());
      break;
    case ax::mojom::NameFrom::kCaption:
      from = "caption";
      DCHECK(!GetName().empty());
      break;
    case ax::mojom::NameFrom::kContents:
      is_explicit_name = false;
      from = "contents";
      DCHECK(!GetName().empty());
      break;
    case ax::mojom::NameFrom::kCssAltText:
      from = "CSS alt text";
      DCHECK(!GetName().empty());
      break;
    case ax::mojom::NameFrom::kPlaceholder:
      from = "placeholder";
      DCHECK(!GetName().empty());
      break;
    case ax::mojom::NameFrom::kProhibited:
    case ax::mojom::NameFrom::kProhibitedAndRedundant:
      is_explicit_name = false;
      from = "prohibited";
      DCHECK(GetName().empty());
      break;
    case ax::mojom::NameFrom::kRelatedElement:
      from = "related-element";
      DCHECK(!GetName().empty());
      break;
    case ax::mojom::NameFrom::kPopoverAttribute:
    case ax::mojom::NameFrom::kTitle:
      from = "tooltip";
      DCHECK(!GetName().empty());
      break;
    case ax::mojom::NameFrom::kValue:
      from = "value";
      DCHECK(!GetName().empty());
      break;
    case ax::mojom::NameFrom::kAttributeExplicitlyEmpty:
      break;
    case ax::mojom::NameFrom::kNone:
      is_explicit_name = false;
      break;  // Not exposed.
  }
  if (!from.empty()) {
    AddAttributeToList("name-from", from, attributes);
  }
  // Expose the non-standard explicit-name IA2 attribute.
  if (is_explicit_name) {
    AddAttributeToList("explicit-name", "true", attributes);
  }

  // Expose the aria-haspopup attribute.
  int32_t has_popup;
  if (GetIntAttribute(ax::mojom::IntAttribute::kHasPopup, &has_popup)) {
    switch (static_cast<ax::mojom::HasPopup>(has_popup)) {
      case ax::mojom::HasPopup::kFalse:
        break;
      case ax::mojom::HasPopup::kTrue:
        AddAttributeToList("haspopup", "true", attributes);
        break;
      case ax::mojom::HasPopup::kMenu:
        AddAttributeToList("haspopup", "menu", attributes);
        break;
      case ax::mojom::HasPopup::kListbox:
        AddAttributeToList("haspopup", "listbox", attributes);
        break;
      case ax::mojom::HasPopup::kTree:
        AddAttributeToList("haspopup", "tree", attributes);
        break;
      case ax::mojom::HasPopup::kGrid:
        AddAttributeToList("haspopup", "grid", attributes);
        break;
      case ax::mojom::HasPopup::kDialog:
        AddAttributeToList("haspopup", "dialog", attributes);
        break;
    }
  } else if (HasState(ax::mojom::State::kAutofillAvailable)) {
    AddAttributeToList("haspopup", "menu", attributes);
  }

  // Expose the aria-ispopup attribute.
  int32_t is_popup;
  if (GetIntAttribute(ax::mojom::IntAttribute::kIsPopup, &is_popup)) {
    switch (static_cast<ax::mojom::IsPopup>(is_popup)) {
      case ax::mojom::IsPopup::kNone:
        break;
      case ax::mojom::IsPopup::kManual:
        AddAttributeToList("ispopup", "manual", attributes);
        break;
      case ax::mojom::IsPopup::kAuto:
        AddAttributeToList("ispopup", "auto", attributes);
        break;
      case ax::mojom::IsPopup::kHint:
        AddAttributeToList("ispopup", "hint", attributes);
        break;
    }
  }

  // Expose the aria-current attribute.
  int32_t aria_current_state;
  if (GetIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState,
                      &aria_current_state)) {
    switch (static_cast<ax::mojom::AriaCurrentState>(aria_current_state)) {
      case ax::mojom::AriaCurrentState::kNone:
        break;
      case ax::mojom::AriaCurrentState::kFalse:
        AddAttributeToList("current", "false", attributes);
        break;
      case ax::mojom::AriaCurrentState::kTrue:
        AddAttributeToList("current", "true", attributes);
        break;
      case ax::mojom::AriaCurrentState::kPage:
        AddAttributeToList("current", "page", attributes);
        break;
      case ax::mojom::AriaCurrentState::kStep:
        AddAttributeToList("current", "step", attributes);
        break;
      case ax::mojom::AriaCurrentState::kLocation:
        AddAttributeToList("current", "location", attributes);
        break;
      case ax::mojom::AriaCurrentState::kDate:
        AddAttributeToList("current", "date", attributes);
        break;
      case ax::mojom::AriaCurrentState::kTime:
        AddAttributeToList("current", "time", attributes);
        break;
    }
  }

  // Expose table cell index.
  if (IsCellOrTableHeader(GetRole())) {
    std::optional<int> index = delegate_->GetTableCellIndex();
    if (index) {
      std::string str_index(base::NumberToString(*index));
      AddAttributeToList("table-cell-index", str_index, attributes);
    }
  }
  if (GetRole() == ax::mojom::Role::kLayoutTable)
    AddAttributeToList("layout-guess", "true", attributes);

  // Expose aria-colcount and aria-rowcount in a table, grid or treegrid if they
  // are different from its physical dimensions.
  if (IsTableLike(GetRole()) &&
      (delegate_->GetTableAriaRowCount() != delegate_->GetTableRowCount() ||
       delegate_->GetTableAriaColCount() != delegate_->GetTableColCount())) {
    AddAttributeToList(ax::mojom::IntAttribute::kAriaColumnCount, "colcount",
                       attributes);
    AddAttributeToList(ax::mojom::IntAttribute::kAriaRowCount, "rowcount",
                       attributes);
  }

  if (IsCellOrTableHeader(GetRole()) || IsTableRow(GetRole())) {
    // Expose aria-colindex and aria-rowindex in a cell or row only if they are
    // different from the table's physical coordinates.
    // Note: aria-col/rowindex is 1 based where as table's physical coordinates
    // are 0 based, so we subtract aria-col/rowindex by 1 to compare with
    // table's physical coordinates.
    std::optional<int> aria_rowindex = delegate_->GetTableCellAriaRowIndex();
    std::optional<int> physical_rowindex = delegate_->GetTableCellRowIndex();
    std::optional<int> aria_colindex = delegate_->GetTableCellAriaColIndex();
    std::optional<int> physical_colindex = delegate_->GetTableCellColIndex();

    if (aria_rowindex && physical_rowindex &&
        aria_rowindex.value() - 1 != physical_rowindex.value()) {
      std::string str_value = base::NumberToString(*aria_rowindex);
      AddAttributeToList("rowindex", str_value, attributes);
    }

    if (!IsTableRow(GetRole()) && aria_colindex && physical_colindex &&
        aria_colindex.value() - 1 != physical_colindex.value()) {
      AddAttributeToList(ax::mojom::IntAttribute::kAriaCellColumnIndex,
                         "colindex", attributes);
    }
  }

  // Expose row or column header sort direction.
  int32_t sort_direction;
  if (IsTableHeader(GetRole()) &&
      GetIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                      &sort_direction)) {
    switch (static_cast<ax::mojom::SortDirection>(sort_direction)) {
      case ax::mojom::SortDirection::kNone:
        break;
      case ax::mojom::SortDirection::kUnsorted:
        AddAttributeToList("sort", "none", attributes);
        break;
      case ax::mojom::SortDirection::kAscending:
        AddAttributeToList("sort", "ascending", attributes);
        break;
      case ax::mojom::SortDirection::kDescending:
        AddAttributeToList("sort", "descending", attributes);
        break;
      case ax::mojom::SortDirection::kOther:
        AddAttributeToList("sort", "other", attributes);
        break;
    }
  }

  if (IsCellOrTableHeader(GetRole())) {
    // These are the older, backwards compatible names that work with JAWS/NVDA:
    AddAttributeToList(ax::mojom::StringAttribute::kAriaCellColumnIndexText,
                       "coltext", attributes);
    AddAttributeToList(ax::mojom::StringAttribute::kAriaCellRowIndexText,
                       "rowtext", attributes);
    // These newer names are consistent with the ARIA attribute names:
    AddAttributeToList(ax::mojom::StringAttribute::kAriaCellColumnIndexText,
                       "colindextext", attributes);
    AddAttributeToList(ax::mojom::StringAttribute::kAriaCellRowIndexText,
                       "rowindextext", attributes);

    AddAttributeToList(ax::mojom::IntAttribute::kAriaCellColumnSpan, "colspan",
                       attributes);
    AddAttributeToList(ax::mojom::IntAttribute::kAriaCellRowSpan, "rowspan",
                       attributes);
  }

  // Expose the value of a progress bar, slider, scroll bar or <select> element.
  if (GetData().IsRangeValueSupported() ||
      GetRole() == ax::mojom::Role::kComboBoxMenuButton) {
    std::string value = base::UTF16ToUTF8(GetValueForControl());
    if (!value.empty())
      AddAttributeToList("valuetext", value, attributes);
  }

  // Expose dropeffect attribute.
  // aria-dropeffect is deprecated in WAI-ARIA 1.1.
  if (delegate_->HasIntAttribute(
          ax::mojom::IntAttribute::kDropeffectDeprecated)) {
    NOTREACHED_IN_MIGRATION();
  }

  // Expose class attribute.
  std::string class_attr;
  if (delegate_->GetStringAttribute(ax::mojom::StringAttribute::kClassName,
                                    &class_attr)) {
    AddAttributeToList("class", class_attr, attributes);
  }

  // Expose datetime attribute.
  std::string datetime;
  if (GetRole() == ax::mojom::Role::kTime &&
      GetHtmlAttribute("datetime", &datetime)) {
    AddAttributeToList("datetime", datetime, attributes);
  }

  std::string id;
  if (delegate_->GetStringAttribute(ax::mojom::StringAttribute::kHtmlId, &id)) {
    AddAttributeToList("id", id, attributes);
  }

  std::string src;
  if (IsImage(GetRole()) &&
      GetStringAttribute(ax::mojom::StringAttribute::kUrl, &src)) {
    AddAttributeToList("src", src, attributes);
  }

  if (delegate_->HasIntAttribute(ax::mojom::IntAttribute::kTextAlign)) {
    auto text_align = static_cast<ax::mojom::TextAlign>(
        delegate_->GetIntAttribute(ax::mojom::IntAttribute::kTextAlign));
    switch (text_align) {
      case ax::mojom::TextAlign::kNone:
        break;
      case ax::mojom::TextAlign::kLeft:
        AddAttributeToList("text-align", "left", attributes);
        break;
      case ax::mojom::TextAlign::kRight:
        AddAttributeToList("text-align", "right", attributes);
        break;
      case ax::mojom::TextAlign::kCenter:
        AddAttributeToList("text-align", "center", attributes);
        break;
      case ax::mojom::TextAlign::kJustify:
        AddAttributeToList("text-align", "justify", attributes);
        break;
    }
  }

  float text_indent;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kTextIndent, &text_indent) !=
      0.0f) {
    // Round value to two decimal places.
    std::stringstream value;
    value << std::fixed << std::setprecision(2) << text_indent << "mm";
    AddAttributeToList("text-indent", value.str(), attributes);
  }

  // Text fields need to report the attribute "text-model:a1" to instruct
  // screen readers to use IAccessible2 APIs to handle text editing in this
  // object (as opposed to treating it like a native Windows text box).
  // The text-model:a1 attribute is documented here:
  // http://www.linuxfoundation.org/collaborate/workgroups/accessibility/ia2/ia2_implementation_guide
  if (IsTextField())
    AddAttributeToList("text-model", "a1", attributes);

  // Expose input-text type attribute.
  if (IsAtomicTextField() || IsDateOrTimeInput(GetRole())) {
    AddAttributeToList(ax::mojom::StringAttribute::kInputType,
                       "text-input-type", attributes);
  }

  // Expose details-from.
  int details_from;
  if (GetIntAttribute(ax::mojom::IntAttribute::kDetailsFrom, &details_from)) {
    switch (static_cast<ax::mojom::DetailsFrom>(details_from)) {
      case ax::mojom::DetailsFrom::kAriaDetails:
        AddAttributeToList("details-from", "aria-details", attributes);
        break;
      case ax::mojom::DetailsFrom::kCssAnchor:
        AddAttributeToList("details-from", "css-anchor", attributes);
        break;
      case ax::mojom::DetailsFrom::kPopoverAttribute:
        AddAttributeToList("details-from", "popover-attribute", attributes);
        break;
    }
  }

  std::string details_roles = ComputeDetailsRoles();
  if (!details_roles.empty())
    AddAttributeToList("details-roles", details_roles, attributes);

  if (IsLink(GetRole())) {
    AddAttributeToList(ax::mojom::StringAttribute::kLinkTarget, "link-target",
                       attributes);
  }
}

void AXPlatformNodeBase::AddAttributeToList(
    const ax::mojom::StringAttribute attribute,
    const char* name,
    PlatformAttributeList* attributes) {
  DCHECK(attributes);
  std::string value;
  if (GetStringAttribute(attribute, &value)) {
    AddAttributeToList(name, value, attributes);
  }
}

void AXPlatformNodeBase::AddAttributeToList(
    const ax::mojom::BoolAttribute attribute,
    const char* name,
    PlatformAttributeList* attributes) {
  DCHECK(attributes);
  bool value;
  if (GetBoolAttribute(attribute, &value)) {
    AddAttributeToList(name, value ? "true" : "false", attributes);
  }
}

void AXPlatformNodeBase::AddAttributeToList(
    const ax::mojom::IntAttribute attribute,
    const char* name,
    PlatformAttributeList* attributes) {
  DCHECK(attributes);

  auto maybe_value = ComputeAttribute(delegate_, attribute);
  if (maybe_value.has_value()) {
    std::string str_value = base::NumberToString(maybe_value.value());
    AddAttributeToList(name, str_value, attributes);
  }
}

void AXPlatformNodeBase::AddAttributeToList(const char* name,
                                            const std::string& value,
                                            PlatformAttributeList* attributes) {
  AddAttributeToList(name, value.c_str(), attributes);
}

AXLegacyHypertext::AXLegacyHypertext() = default;
AXLegacyHypertext::~AXLegacyHypertext() = default;
AXLegacyHypertext::AXLegacyHypertext(const AXLegacyHypertext& other) = default;
AXLegacyHypertext& AXLegacyHypertext::operator=(
    const AXLegacyHypertext& other) = default;
AXLegacyHypertext::AXLegacyHypertext(AXLegacyHypertext&& other) noexcept
    : needs_update(std::exchange(other.needs_update, true)),
      hyperlink_offset_to_index(std::move(other.hyperlink_offset_to_index)),
      hyperlinks(std::move(other.hyperlinks)),
      hypertext(std::move(other.hypertext)) {}
AXLegacyHypertext& AXLegacyHypertext::operator=(AXLegacyHypertext&& other) {
  needs_update = std::exchange(other.needs_update, true);
  hyperlink_offset_to_index = std::move(other.hyperlink_offset_to_index);
  hyperlinks = std::move(other.hyperlinks);
  hypertext = std::move(other.hypertext);
  return *this;
}

// TODO(nektar): To be able to use AXNode in Views, move this logic to AXNode.
void AXPlatformNodeBase::UpdateComputedHypertext() const {
  if (!delegate_)
    return;
  hypertext_ = AXLegacyHypertext();

  if (GetData().IsIgnored() || IsLeaf()) {
    hypertext_.hypertext = GetTextContentUTF16();
    hypertext_.needs_update = false;
    return;
  }

  // Construct the hypertext for this node, which contains the concatenation
  // of all of the static text and whitespace from this node's children, and an
  // embedded object character for all the other children. Build up a map from
  // the character index of each embedded object character to the id of the
  // child object it points to.
  std::u16string hypertext;
  for (AXPlatformNodeChildIterator child_iter = AXPlatformNodeChildrenBegin();
       child_iter != AXPlatformNodeChildrenEnd(); ++child_iter) {
    // Similar to Firefox, we don't expose text nodes in IAccessible2 and ATK
    // hypertext with the embedded object character. We copy all of their text
    // instead.
    if (child_iter->IsText()) {
      hypertext_.hypertext += child_iter->GetTextContentUTF16();
    } else {
      int32_t char_offset = static_cast<int32_t>(hypertext_.hypertext.size());
      int32_t child_unique_id = child_iter->GetUniqueId();
      int32_t index = static_cast<int32_t>(hypertext_.hyperlinks.size());
      hypertext_.hyperlink_offset_to_index[char_offset] = index;
      hypertext_.hyperlinks.push_back(child_unique_id);
      hypertext_.hypertext += kEmbeddedCharacter;
    }
  }

  hypertext_.needs_update = false;
}

void AXPlatformNodeBase::AddAttributeToList(const char* name,
                                            const char* value,
                                            PlatformAttributeList* attributes) {
}

std::optional<int> AXPlatformNodeBase::GetPosInSet() const {
  if (!delegate_)
    return std::nullopt;
  return delegate_->GetPosInSet();
}

std::optional<int> AXPlatformNodeBase::GetSetSize() const {
  if (!delegate_)
    return std::nullopt;
  return delegate_->GetSetSize();
}

bool AXPlatformNodeBase::ScrollToNode(ScrollType scroll_type) {
  // ax::mojom::Action::kScrollToMakeVisible wants a target rect in *local*
  // coords.
  gfx::Rect r = gfx::ToEnclosingRect(GetData().relative_bounds.bounds);
  r -= r.OffsetFromOrigin();
  switch (scroll_type) {
    case ScrollType::TopLeft:
      r = gfx::Rect(r.x(), r.y(), 0, 0);
      break;
    case ScrollType::BottomRight:
      r = gfx::Rect(r.right(), r.bottom(), 0, 0);
      break;
    case ScrollType::TopEdge:
      r = gfx::Rect(r.x(), r.y(), r.width(), 0);
      break;
    case ScrollType::BottomEdge:
      r = gfx::Rect(r.x(), r.bottom(), r.width(), 0);
      break;
    case ScrollType::LeftEdge:
      r = gfx::Rect(r.x(), r.y(), 0, r.height());
      break;
    case ScrollType::RightEdge:
      r = gfx::Rect(r.right(), r.y(), 0, r.height());
      break;
    case ScrollType::Anywhere:
      break;
  }

  AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;
  action_data.horizontal_scroll_alignment =
      ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
  action_data.vertical_scroll_alignment =
      ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
  action_data.scroll_behavior =
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible;
  action_data.target_rect = r;
  GetDelegate()->AccessibilityPerformAction(action_data);
  return true;
}

// static
void AXPlatformNodeBase::SanitizeStringAttribute(const std::string& input,
                                                 std::string* output) {
  DCHECK(output);
  // According to the IA2 spec and AT-SPI2, these characters need to be escaped
  // with a backslash: backslash, colon, comma, equals and semicolon.  Note
  // that backslash must be replaced first.
  base::ReplaceChars(input, "\\", "\\\\", output);
  base::ReplaceChars(*output, ":", "\\:", output);
  base::ReplaceChars(*output, ",", "\\,", output);
  base::ReplaceChars(*output, "=", "\\=", output);
  base::ReplaceChars(*output, ";", "\\;", output);
}

int32_t AXPlatformNodeBase::GetHyperlinkIndexFromChild(
    AXPlatformNodeBase* child) {
  if (hypertext_.hyperlinks.empty())
    return -1;

  auto iterator =
      base::ranges::find(hypertext_.hyperlinks, child->GetUniqueId());
  if (iterator == hypertext_.hyperlinks.end())
    return -1;

  return static_cast<int32_t>(iterator - hypertext_.hyperlinks.begin());
}

int32_t AXPlatformNodeBase::GetHypertextOffsetFromHyperlinkIndex(
    int32_t hyperlink_index) {
  for (auto& offset_index : hypertext_.hyperlink_offset_to_index) {
    if (offset_index.second == hyperlink_index)
      return offset_index.first;
  }
  return -1;
}

int32_t AXPlatformNodeBase::GetHypertextOffsetFromChild(
    AXPlatformNodeBase* child) {
  // TODO(dougt) DCHECK(child.owner()->PlatformGetParent() == owner());

  if (IsLeaf())
    return -1;

  // Handle the case when we are dealing with a text-only child.
  // Text-only children should not be present at tree roots and so no
  // cross-tree traversal is necessary.
  if (child->IsText()) {
    int32_t hypertext_offset = 0;
    for (auto child_iter = AXPlatformNodeChildrenBegin();
         child_iter != AXPlatformNodeChildrenEnd() && child_iter.get() != child;
         ++child_iter) {
      if (child_iter->IsText()) {
        hypertext_offset +=
            static_cast<int32_t>(child_iter->GetHypertext().size());
      } else {
        ++hypertext_offset;
      }
    }
    return hypertext_offset;
  }

  int32_t hyperlink_index = GetHyperlinkIndexFromChild(child);
  if (hyperlink_index < 0)
    return -1;

  return GetHypertextOffsetFromHyperlinkIndex(hyperlink_index);
}

int AXPlatformNodeBase::HypertextOffsetFromChildIndex(int child_index) const {
  DCHECK_GE(child_index, 0);
  DCHECK_LE(child_index, static_cast<int>(GetChildCount()));

  // Use both a child index and an iterator to avoid an O(n^2) complexity which
  // would be the case if we were to call GetChildAtIndex on each child.
  int hypertext_offset = 0;
  int endpoint_child_index = 0;
  for (AXPlatformNodeChildIterator child_iter = AXPlatformNodeChildrenBegin();
       child_iter != AXPlatformNodeChildrenEnd(); ++child_iter) {
    if (endpoint_child_index >= child_index) {
      break;
    }

    int child_text_len = 1;
    if (child_iter->IsText())
      child_text_len =
          base::checked_cast<int>(child_iter->GetHypertext().size());

    endpoint_child_index++;
    hypertext_offset += child_text_len;
  }
  return hypertext_offset;
}

int32_t AXPlatformNodeBase::GetHypertextOffsetFromDescendant(
    AXPlatformNodeBase* descendant) {
  auto* parent_object = static_cast<AXPlatformNodeBase*>(
      FromNativeViewAccessible(descendant->GetDelegate()->GetParent()));
  while (parent_object && parent_object != this) {
    descendant = parent_object;
    parent_object = static_cast<AXPlatformNodeBase*>(
        FromNativeViewAccessible(descendant->GetParent()));
  }
  if (!parent_object)
    return -1;

  return parent_object->GetHypertextOffsetFromChild(descendant);
}

int AXPlatformNodeBase::GetHypertextOffsetFromEndpoint(
    AXPlatformNodeBase* endpoint_object,
    int endpoint_offset) {
  DCHECK_GE(endpoint_offset, 0);

  // There are three cases:
  // 1. The selection endpoint is this object itself: endpoint_offset should be
  // returned, possibly adjusted from a child offset to a hypertext offset.
  // 2. The selection endpoint is an ancestor of this object. If endpoint_offset
  // points out after this object, then this object text length is returned,
  // otherwise 0.
  // 3. The selection endpoint is a descendant of this object. The offset of the
  // character in this object's hypertext corresponding to the subtree in which
  // the endpoint is located should be returned.
  // 4. The selection endpoint is in a completely different part of the tree.
  // Either 0 or hypertext length should be returned depending on the direction
  // that one needs to travel to find the endpoint.
  //
  // TODO(nektar): Replace all this logic with the use of AXNodePosition.

  // Case 1. Is the endpoint object equal to this object
  if (endpoint_object == this) {
    if (endpoint_object->IsLeaf())
      return endpoint_offset;
    return HypertextOffsetFromChildIndex(endpoint_offset);
  }

  // Case 2. Is the endpoint an ancestor of this object.
  if (IsDescendantOf(endpoint_object)) {
    DCHECK_LE(endpoint_offset,
              static_cast<int>(endpoint_object->GetChildCount()));

    AXPlatformNodeBase* closest_ancestor = this;
    while (closest_ancestor) {
      AXPlatformNodeBase* parent = static_cast<AXPlatformNodeBase*>(
          FromNativeViewAccessible(closest_ancestor->GetParent()));
      if (parent == endpoint_object)
        break;
      closest_ancestor = parent;
    }

    // If the endpoint is after this node, then return the node's
    // hypertext length, otherwise 0 as the endpoint points before the node.
    std::optional<size_t> index_in_parent =
        closest_ancestor->GetIndexInParent();
    DCHECK(index_in_parent)
        << "No index in parent for ancestor: " << *closest_ancestor;
    if (index_in_parent &&
        endpoint_offset > static_cast<int>(*index_in_parent)) {
      return static_cast<int>(GetHypertext().size());
    }
    return 0;
  }

  AXPlatformNodeBase* common_parent = this;
  std::optional<size_t> index_in_common_parent = GetIndexInParent();
  while (common_parent && !endpoint_object->IsDescendantOf(common_parent)) {
    index_in_common_parent = common_parent->GetIndexInParent();
    common_parent = static_cast<AXPlatformNodeBase*>(
        FromNativeViewAccessible(common_parent->GetParent()));
  }
  if (!common_parent)
    return -1;

  DCHECK(!(common_parent->IsText()));

  // Case 2. Is the selection endpoint inside a descendant of this object?
  //
  // We already checked in case 1 if our endpoint object is equal to this
  // object. We can safely assume that it is a descendant or in a completely
  // different part of the tree.
  if (common_parent == this) {
    int32_t hypertext_offset =
        GetHypertextOffsetFromDescendant(endpoint_object);
    auto* parent = static_cast<AXPlatformNodeBase*>(
        FromNativeViewAccessible(endpoint_object->GetParent()));
    if (parent == this && endpoint_object->IsText()) {
      // Due to a historical design decision, the hypertext of the immediate
      // parents of text objects includes all their text. We therefore need to
      // adjust the hypertext offset in the parent by adding any text offset.
      hypertext_offset += endpoint_offset;
    }

    return hypertext_offset;
  }

  // Case 3. Selection endpoint is in a completely different part of the tree:
  // - Return 0 if it's in an earlier part of the tree.
  // - Return GetHypertext.size() if it's in a later part of the tree.
  // We can safely assume that the endpoint is in another part of the tree or
  // at common parent, and that this object is a descendant of common parent.
  std::optional<size_t> endpoint_index_in_common_parent;
  for (auto child_iter = common_parent->AXPlatformNodeChildrenBegin();
       child_iter != common_parent->AXPlatformNodeChildrenEnd(); ++child_iter) {
    if (endpoint_object->IsDescendantOf(child_iter.get())) {
      endpoint_index_in_common_parent = child_iter->GetIndexInParent();
      break;
    }
  }

  if (endpoint_index_in_common_parent < index_in_common_parent) {
    // In earlier point in tree than endpoint_object.
    return 0;
  }
  if (endpoint_index_in_common_parent > index_in_common_parent) {
    // In later point in the tree than endpoint_object.
    return static_cast<int>(GetHypertext().size());
  }

  // TODO(crbug.com/40897578): Make sure this doesn't fire then turn the last
  // conditional into a CHECK_GT(endpoint_index_in_common_parent,
  // index_in_common_parent); and remove this code path.
  DUMP_WILL_BE_NOTREACHED()
      << "Was not in descendant, so the endpoint_index_in_common_parent should "
         "be < or > than the index_in_common_parent:\n"
      << "\n* This: " << this << "\n* Endpoint object: " << endpoint_object
      << "\n* Endpoint offset: " << endpoint_offset
      << "\n* Common parent: " << common_parent
      << "\n* Index in common parent: " << index_in_common_parent.value_or(-99)
      << "\n* Endpoint in common parent: "
      << endpoint_index_in_common_parent.value_or(-99);
  return -1;
}

AXPlatformNodeBase::AXPosition AXPlatformNodeBase::HypertextOffsetToEndpoint(
    int hypertext_offset) const {
  DCHECK_GE(hypertext_offset, 0);
  DCHECK_LT(hypertext_offset, static_cast<int>(GetHypertext().size()));

  if (IsLeaf()) {
    if (IsText())
      return GetDelegate()->CreateTextPositionAt(hypertext_offset);
    return GetDelegate()->CreatePositionAt(hypertext_offset);
  }

  int current_hypertext_offset = hypertext_offset;
  for (auto child_iter = AXPlatformNodeChildrenBegin();
       child_iter != AXPlatformNodeChildrenEnd() &&
       current_hypertext_offset >= 0;
       ++child_iter) {
    int child_text_len = 1;
    if (child_iter->IsText())
      child_text_len =
          base::checked_cast<int>(child_iter->GetHypertext().size());

    if (current_hypertext_offset < child_text_len) {
      int endpoint_offset = child_text_len - current_hypertext_offset;
      if (child_iter->IsText())
        return child_iter->GetDelegate()->CreateTextPositionAt(endpoint_offset);
      return child_iter->GetDelegate()->CreatePositionAt(endpoint_offset);
    }
    current_hypertext_offset -= child_text_len;
  }
  return AXNodePosition::CreateNullPosition();
}

int AXPlatformNodeBase::GetSelectionAnchor(const AXSelection* selection) {
  DCHECK(selection);
  AXNodeID anchor_id = selection->anchor_object_id;
  AXPlatformNodeBase* anchor_object =
      static_cast<AXPlatformNodeBase*>(delegate_->GetFromNodeID(anchor_id));
  if (!anchor_object)
    return -1;

  return GetHypertextOffsetFromEndpoint(anchor_object,
                                        selection->anchor_offset);
}

int AXPlatformNodeBase::GetSelectionFocus(const AXSelection* selection) {
  DCHECK(selection);
  AXNodeID focus_id = selection->focus_object_id;
  AXPlatformNodeBase* focus_object =
      static_cast<AXPlatformNodeBase*>(GetDelegate()->GetFromNodeID(focus_id));
  if (!focus_object)
    return -1;

  return GetHypertextOffsetFromEndpoint(focus_object, selection->focus_offset);
}

void AXPlatformNodeBase::GetSelectionOffsets(int* selection_start,
                                             int* selection_end) {
  GetSelectionOffsets(nullptr, selection_start, selection_end);
}

void AXPlatformNodeBase::GetSelectionOffsets(const AXSelection* selection,
                                             int* selection_start,
                                             int* selection_end) {
  DCHECK(selection_start && selection_end);

  if (IsAtomicTextField() &&
      GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart,
                      selection_start) &&
      GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, selection_end)) {
    return;
  }

  // If the unignored selection has not been computed yet, compute it now.
  AXSelection unignored_selection;
  if (!selection) {
    unignored_selection = delegate_->GetUnignoredSelection();
    selection = &unignored_selection;
  }
  DCHECK(selection);
  GetSelectionOffsetsFromTree(selection, selection_start, selection_end);
}

void AXPlatformNodeBase::GetSelectionOffsetsFromTree(
    const AXSelection* selection,
    int* selection_start,
    int* selection_end) {
  DCHECK(selection_start && selection_end);

  *selection_start = GetSelectionAnchor(selection);
  *selection_end = GetSelectionFocus(selection);
  if (*selection_start < 0 || *selection_end < 0)
    return;

  // There are three cases when a selection would start and end on the same
  // character:
  // 1. Anchor and focus are both in a subtree that is to the right of this
  // object.
  // 2. Anchor and focus are both in a subtree that is to the left of this
  // object.
  // 3. Anchor and focus are in a subtree represented by a single embedded
  // object character.
  // Only case 3 refers to a valid selection because cases 1 and 2 fall
  // outside this object in their entirety.
  // Selections that span more than one character are by definition inside
  // this object, so checking them is not necessary.
  if (*selection_start == *selection_end && !HasVisibleCaretOrSelection()) {
    *selection_start = -1;
    *selection_end = -1;
    return;
  }

  // The IA2 Spec says that if the largest of the two offsets falls on an
  // embedded object character and if there is a selection in that embedded
  // object, it should be incremented by one so that it points after the
  // embedded object character.
  // This is a signal to AT software that the embedded object is also part of
  // the selection.
  int* largest_offset =
      (*selection_start <= *selection_end) ? selection_end : selection_start;
  const std::map<int, int>& offset_to_child_index =
      delegate_->GetHypertextOffsetToHyperlinkChildIndex();
  auto index_iter = offset_to_child_index.find(*largest_offset);
  if (index_iter == offset_to_child_index.end())
    return;

  int child_index = index_iter->second;
  DCHECK_GE(child_index, 0);
  DCHECK_LT(static_cast<size_t>(child_index), GetChildCount());
  AXPlatformNodeBase* hyperlink = static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(ChildAtIndex(child_index)));
  if (!hyperlink)
    return;

  int hyperlink_selection_start, hyperlink_selection_end;
  hyperlink->GetSelectionOffsets(selection, &hyperlink_selection_start,
                                 &hyperlink_selection_end);
  if (hyperlink_selection_start >= 0 && hyperlink_selection_end >= 0 &&
      hyperlink_selection_start != hyperlink_selection_end) {
    ++(*largest_offset);
  }
}

bool AXPlatformNodeBase::IsSameHypertextCharacter(
    const AXLegacyHypertext& old_hypertext,
    size_t old_char_index,
    size_t new_char_index) {
  if (old_char_index >= old_hypertext.hypertext.size() ||
      new_char_index >= hypertext_.hypertext.size()) {
    return false;
  }

  // For anything other than the "embedded character", we just compare the
  // characters directly.
  char16_t old_ch = old_hypertext.hypertext[old_char_index];
  char16_t new_ch = hypertext_.hypertext[new_char_index];
  if (old_ch != new_ch)
    return false;
  if (new_ch != kEmbeddedCharacter)
    return true;

  // If it's an embedded character, they're only identical if the child id
  // the hyperlink points to is the same.
  const std::map<int32_t, int32_t>& old_offset_to_index =
      old_hypertext.hyperlink_offset_to_index;
  const std::vector<int32_t>& old_hyperlinks = old_hypertext.hyperlinks;
  int32_t old_hyperlinkscount = static_cast<int32_t>(old_hyperlinks.size());
  auto iter = old_offset_to_index.find(static_cast<int32_t>(old_char_index));
  int old_index = (iter != old_offset_to_index.end()) ? iter->second : -1;
  int old_child_id = (old_index >= 0 && old_index < old_hyperlinkscount)
                         ? old_hyperlinks[old_index]
                         : -1;

  const std::map<int32_t, int32_t>& new_offset_to_index =
      hypertext_.hyperlink_offset_to_index;
  const std::vector<int32_t>& new_hyperlinks = hypertext_.hyperlinks;
  int32_t new_hyperlinkscount = static_cast<int32_t>(new_hyperlinks.size());
  iter = new_offset_to_index.find(static_cast<int32_t>(new_char_index));
  int new_index = (iter != new_offset_to_index.end()) ? iter->second : -1;
  int new_child_id = (new_index >= 0 && new_index < new_hyperlinkscount)
                         ? new_hyperlinks[new_index]
                         : -1;

  return old_child_id == new_child_id;
}

// Return true if the index represents a text character.
bool AXPlatformNodeBase::IsText(const std::u16string& text,
                                size_t index,
                                bool is_indexed_from_end) {
  size_t text_len = text.size();
  if (index == text_len)
    return false;
  auto ch = text[is_indexed_from_end ? text_len - index - 1 : index];
  return ch != kEmbeddedCharacter;
}

bool AXPlatformNodeBase::IsPlatformCheckable() const {
  return delegate_ && GetData().HasCheckedState();
}

void AXPlatformNodeBase::ComputeHypertextRemovedAndInserted(
    const AXLegacyHypertext& old_hypertext,
    size_t* start,
    size_t* old_len,
    size_t* new_len) {
  *start = 0;
  *old_len = 0;
  *new_len = 0;

  // Do not compute for text objects, otherwise redundant text change
  // announcements will occur in live regions, as the parent hypertext also
  // changes.
  if (IsText())
    return;

  const std::u16string& old_text = old_hypertext.hypertext;
  const std::u16string& new_text = hypertext_.hypertext;

  // TODO(accessibility) Plumb through which part of text changed so we don't
  // have to guess what changed based on character differences. This can be
  // wrong in some cases as follows:
  // -- EDITABLE --
  // If editable: when part of the text node changes, assume only that part
  // changed, and not the entire thing. For example, if "car" changes to
  // "cat", assume only 1 letter changed. This code compares common characters
  // to guess what has changed.
  // -- NOT EDITABLE --
  // When part of the text changes, assume the entire node's text changed. For
  // example, if "car" changes to "cat" then assume all 3 letters changed.
  // Note, it is possible (though rare) that CharacterData methods are used to
  // remove, insert, replace or append a substring.
  bool allow_partial_text_node_changes = HasState(ax::mojom::State::kEditable);
  size_t prefix_index = 0;
  size_t common_prefix = 0;
  while (prefix_index < old_text.size() && prefix_index < new_text.size() &&
         IsSameHypertextCharacter(old_hypertext, prefix_index, prefix_index)) {
    ++prefix_index;
    if (allow_partial_text_node_changes ||
        (!IsText(old_text, prefix_index) && !IsText(new_text, prefix_index))) {
      common_prefix = prefix_index;
    }
  }

  size_t suffix_index = 0;
  size_t common_suffix = 0;
  while (common_prefix + suffix_index < old_text.size() &&
         common_prefix + suffix_index < new_text.size() &&
         IsSameHypertextCharacter(old_hypertext,
                                  old_text.size() - suffix_index - 1,
                                  new_text.size() - suffix_index - 1)) {
    ++suffix_index;
    if (allow_partial_text_node_changes ||
        (!IsText(old_text, suffix_index, true) &&
         !IsText(new_text, suffix_index, true))) {
      common_suffix = suffix_index;
    }
  }

  *start = common_prefix;
  *old_len = old_text.size() - common_prefix - common_suffix;
  *new_len = new_text.size() - common_prefix - common_suffix;
}

int AXPlatformNodeBase::FindTextBoundary(
    ax::mojom::TextBoundary boundary,
    int offset,
    ax::mojom::MoveDirection direction,
    ax::mojom::TextAffinity affinity) const {
  DCHECK_NE(boundary, ax::mojom::TextBoundary::kNone);
  if (!delegate_)
    return offset;  // Unable to compute text boundary.

  const AXPosition position = delegate_->CreateTextPositionAt(offset, affinity);
  // On Windows and Linux ATK, searching for a text boundary should always stop
  // at the boundary of the current object.
  AXMovementOptions options{AXBoundaryBehavior::kStopAtAnchorBoundary,
                            AXBoundaryDetection::kDontCheckInitialPosition};
  // On Windows and Linux ATK, it is standard text navigation behavior to stop
  // if we are searching in the backwards direction and the current position is
  // already at the required text boundary.
  if (direction == ax::mojom::MoveDirection::kBackward) {
    options.boundary_detection = AXBoundaryDetection::kCheckInitialPosition;
  }

  const AXPosition boundary_position =
      position->CreatePositionAtTextBoundary(boundary, direction, options);
  if (boundary_position->IsNullPosition())
    return -1;
  DCHECK_GE(boundary_position->text_offset(), 0);
  return boundary_position->text_offset();
}

AXPlatformNodeBase* AXPlatformNodeBase::NearestLeafToPoint(
    gfx::Point point) const {
  // First, scope the search to the node that contains point.
  AXPlatformNodeBase* nearest_node =
      static_cast<AXPlatformNodeBase*>(AXPlatformNode::FromNativeViewAccessible(
          GetDelegate()->HitTestSync(point.x(), point.y())));

  if (!nearest_node)
    return nullptr;

  AXPlatformNodeBase* parent = nearest_node;
  // GetFirstChild does not consider if the parent is a leaf.
  AXPlatformNodeBase* current_descendant =
      parent->GetChildCount() ? parent->GetFirstChild() : nullptr;
  AXPlatformNodeBase* nearest_descendant = nullptr;
  float shortest_distance;
  while (parent && current_descendant) {
    // Manhattan Distance is used to provide faster distance estimates.
    float current_distance = current_descendant->GetDelegate()
                                 ->GetClippedScreenBoundsRect()
                                 .ManhattanDistanceToPoint(point);

    if (!nearest_descendant || current_distance < shortest_distance) {
      shortest_distance = current_distance;
      nearest_descendant = current_descendant;
    }

    // Traverse
    AXPlatformNodeBase* next_sibling = current_descendant->GetNextSibling();
    if (next_sibling) {
      current_descendant = next_sibling;
    } else {
      // We have gone through all siblings, update nearest and descend if
      // possible.
      if (nearest_descendant) {
        nearest_node = nearest_descendant;
        // If the nearest node is a leaf that does not have a child tree, break.
        if (!nearest_node->GetChildCount())
          break;

        parent = nearest_node;
        current_descendant = parent->GetFirstChild();

        // Reset nearest_descendant to force the nearest node to be a descendant
        // of  "parent".
        nearest_descendant = nullptr;
      }
    }
  }
  return nearest_node;
}

int AXPlatformNodeBase::NearestTextIndexToPoint(gfx::Point point) {
  // For text objects, find the text position nearest to the point.The nearest
  // index of a non-text object is implicitly 0. Text fields such as textarea
  // have an embedded div inside them that holds all the text,
  // GetRangeBoundsRect will correctly handle these nodes
  int nearest_index = 0;
  const AXCoordinateSystem coordinate_system = AXCoordinateSystem::kScreenDIPs;
  const AXClippingBehavior clipping_behavior = AXClippingBehavior::kUnclipped;

  // Manhattan Distance  is used to provide faster distance estimates.
  // get the distance from the point to the bounds of each character.
  float shortest_distance = GetDelegate()
                                ->GetInnerTextRangeBoundsRect(
                                    0, 1, coordinate_system, clipping_behavior)
                                .ManhattanDistanceToPoint(point);
  for (int i = 1, text_length = GetTextContentUTF16().length(); i < text_length;
       ++i) {
    float current_distance =
        GetDelegate()
            ->GetInnerTextRangeBoundsRect(i, i + 1, coordinate_system,
                                          clipping_behavior)
            .ManhattanDistanceToPoint(point);
    if (current_distance < shortest_distance) {
      shortest_distance = current_distance;
      nearest_index = i;
    }
  }
  return nearest_index;
}

TextAttributeList AXPlatformNodeBase::ComputeTextAttributes() const {
  TextAttributeList attributes;

  // From the IA2 Spec:
  // Occasionally, word processors will automatically generate characters which
  // appear on a line along with editable text. The characters are not
  // themselves editable, but are part of the document. The most common examples
  // of automatically inserted characters are in bulleted and numbered lists.
  if (HasBoolAttribute(ax::mojom::BoolAttribute::kNotUserSelectableStyle)) {
    // From IA2 text attribute guide:
    // this attribute's value is “true” for list bullet/numbering prefix text or
    // layout-inserted text such as via the CSS pseudo styles :before or :after.
    attributes.emplace_back("auto-generated", "true");
  }

  int color;
  if ((color = delegate_->GetBackgroundColor())) {
    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    std::string color_value = "rgb(" + base::NumberToString(red) + ',' +
                              base::NumberToString(green) + ',' +
                              base::NumberToString(blue) + ')';
    SanitizeTextAttributeValue(color_value, &color_value);
    attributes.emplace_back(std::make_pair("background-color", color_value));
  }

  if ((color = delegate_->GetColor())) {
    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    std::string color_value = "rgb(" + base::NumberToString(red) + ',' +
                              base::NumberToString(green) + ',' +
                              base::NumberToString(blue) + ')';
    SanitizeTextAttributeValue(color_value, &color_value);
    attributes.emplace_back(std::make_pair("color", color_value));
  }

  // First try to get the inherited font family name from the delegate. If we
  // cannot find any name, fall back to looking the hierarchy of this node's
  // AXNodeData instead.
  std::string font_family(GetDelegate()->GetInheritedFontFamilyName());
  if (font_family.empty()) {
    font_family =
        GetInheritedStringAttribute(ax::mojom::StringAttribute::kFontFamily);
  }

  // Attribute has no default value.
  if (!font_family.empty()) {
    SanitizeTextAttributeValue(font_family, &font_family);
    attributes.emplace_back(std::make_pair("font-family", font_family));
  }

  std::optional<float> font_size_in_points = GetFontSizeInPoints();
  // Attribute has no default value.
  if (font_size_in_points) {
    attributes.emplace_back(std::make_pair(
        "font-size", base::NumberToString(*font_size_in_points) + "pt"));
  }

  // TODO(nektar): Add Blink support for the following attributes:
  // text-line-through-mode, text-line-through-width, text-outline:false,
  // text-position:baseline, text-shadow:none, text-underline-mode:continuous.

  int32_t text_style = GetIntAttribute(ax::mojom::IntAttribute::kTextStyle);
  if (text_style) {
    if (HasTextStyle(ax::mojom::TextStyle::kBold))
      attributes.emplace_back(std::make_pair("font-weight", "bold"));
    if (HasTextStyle(ax::mojom::TextStyle::kItalic))
      attributes.emplace_back(std::make_pair("font-style", "italic"));
    if (HasTextStyle(ax::mojom::TextStyle::kLineThrough)) {
      // TODO(nektar): Figure out a more specific value.
      attributes.emplace_back(
          std::make_pair("text-line-through-style", "solid"));
    }
    if (HasTextStyle(ax::mojom::TextStyle::kUnderline)) {
      // TODO(nektar): Figure out a more specific value.
      attributes.emplace_back(std::make_pair("text-underline-style", "solid"));
    }
  }

  std::string language = GetDelegate()->GetLanguage();
  if (!language.empty()) {
    SanitizeTextAttributeValue(language, &language);
    attributes.emplace_back(std::make_pair("language", language));
  }

  auto text_direction = static_cast<ax::mojom::WritingDirection>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextDirection));
  switch (text_direction) {
    case ax::mojom::WritingDirection::kNone:
      break;
    case ax::mojom::WritingDirection::kLtr:
      attributes.emplace_back(std::make_pair("writing-mode", "lr"));
      break;
    case ax::mojom::WritingDirection::kRtl:
      attributes.emplace_back(std::make_pair("writing-mode", "rl"));
      break;
    case ax::mojom::WritingDirection::kTtb:
      attributes.emplace_back(std::make_pair("writing-mode", "tb"));
      break;
    case ax::mojom::WritingDirection::kBtt:
      // Not listed in the IA2 Spec.
      attributes.emplace_back(std::make_pair("writing-mode", "bt"));
      break;
  }

  auto text_position = static_cast<ax::mojom::TextPosition>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextPosition));
  switch (text_position) {
    case ax::mojom::TextPosition::kNone:
      break;
    case ax::mojom::TextPosition::kSubscript:
      attributes.emplace_back(std::make_pair("text-position", "sub"));
      break;
    case ax::mojom::TextPosition::kSuperscript:
      attributes.emplace_back(std::make_pair("text-position", "super"));
      break;
  }

  return attributes;
}

int AXPlatformNodeBase::GetSelectionCount() const {
  int max_items = GetMaxSelectableItems();
  if (!max_items)
    return 0;
  return GetSelectedItems(max_items);
}

AXPlatformNodeBase* AXPlatformNodeBase::GetSelectedItem(
    int selected_index) const {
  DCHECK_GE(selected_index, 0);
  int max_items = GetMaxSelectableItems();
  if (max_items == 0)
    return nullptr;
  if (selected_index >= max_items)
    return nullptr;

  std::vector<AXPlatformNodeBase*> selected_children;
  int requested_count = selected_index + 1;
  int returned_count = GetSelectedItems(requested_count, &selected_children);

  if (returned_count <= selected_index)
    return nullptr;

  DCHECK(!selected_children.empty());
  DCHECK_LT(selected_index, static_cast<int>(selected_children.size()));
  return selected_children[selected_index];
}

int AXPlatformNodeBase::GetSelectedItems(
    int max_items,
    std::vector<AXPlatformNodeBase*>* out_selected_items) const {
  int selected_count = 0;
  for (auto child_iter = AXPlatformNodeChildrenBegin();
       child_iter != AXPlatformNodeChildrenEnd() && selected_count < max_items;
       ++child_iter) {
    if (!IsItemLike(child_iter->GetRole())) {
      selected_count += child_iter->GetSelectedItems(max_items - selected_count,
                                                     out_selected_items);
    } else if (child_iter->GetBoolAttribute(
                   ax::mojom::BoolAttribute::kSelected)) {
      selected_count++;
      if (out_selected_items)
        out_selected_items->emplace_back(child_iter.get());
    }
  }
  return selected_count;
}

void AXPlatformNodeBase::SanitizeTextAttributeValue(const std::string& input,
                                                    std::string* output) const {
  DCHECK(output);
}

bool AXPlatformNodeBase::IsDescribedByTooltip() const {
  const std::vector<int32_t>& description_ids =
      GetIntListAttribute(ax::mojom::IntListAttribute::kDescribedbyIds);

  std::string description_from;

  for (int id : description_ids) {
    AXPlatformNodeBase* description_object =
        static_cast<AXPlatformNodeBase*>(delegate_->GetFromNodeID(id));
    if (description_object &&
        description_object->GetRole() == ax::mojom::Role::kTooltip) {
      return true;
    }
  }

  return false;
}

std::string AXPlatformNodeBase::ComputeDetailsRoles() const {
  const std::vector<int32_t>& details_ids =
      GetIntListAttribute(ax::mojom::IntListAttribute::kDetailsIds);
  if (details_ids.empty())
    return std::string();

  std::set<std::string> details_roles_set;

  for (int id : details_ids) {
    AXPlatformNodeBase* detail_object =
        static_cast<AXPlatformNodeBase*>(delegate_->GetFromNodeID(id));
    if (!detail_object)
      continue;
    switch (detail_object->GetRole()) {
      case ax::mojom::Role::kComment:
        details_roles_set.insert("comment");
        break;
      case ax::mojom::Role::kDefinition:
        details_roles_set.insert("definition");
        break;
      case ax::mojom::Role::kDocEndnote:
        details_roles_set.insert("doc-endnote");
        break;
      case ax::mojom::Role::kDocFootnote:
        details_roles_set.insert("doc-footnote");
        break;
      case ax::mojom::Role::kGroup:
      case ax::mojom::Role::kRegion: {
        if (DescendantHasComment(detail_object)) {
          details_roles_set.insert("comment");
          break;
        }
        [[fallthrough]];
      }
      default:
        // If a popover of any kind, use "popover" -- technically this is not a
        // role, and therefore, details-roles is more of a hints field. Use * to
        // indicate some other role.
        if (detail_object->GetDelegate()->node()->HasIntAttribute(
                ax::mojom::IntAttribute::kIsPopup)) {
          details_roles_set.insert("popover");
        } else {
          details_roles_set.insert("*");
        }
        break;
    }
  }

  // Create space delimited list of types. The set will not be large, as there
  // are not very many possible types.
  std::vector<std::string> details_roles_vector(details_roles_set.begin(),
                                                details_roles_set.end());
  return base::JoinString(details_roles_vector, " ");
}

// static
bool AXPlatformNodeBase::DescendantHasComment(const AXPlatformNodeBase* node) {
  // These should still report comment if there are comments inside them.
  constexpr size_t kMaxChildrenToCheck = 8;
  constexpr size_t kMaxDepthToCheck = 4;
  if (FindDescendantRoleWithMaxDepth(node, ax::mojom::Role::kComment,
                                     kMaxDepthToCheck, kMaxChildrenToCheck)) {
    return true;
  }
  return false;
}

int AXPlatformNodeBase::GetMaxSelectableItems() const {
  if (IsLeaf())
    return 0;

  if (!IsContainerWithSelectableChildren(GetRole()))
    return 0;

  int max_items = 1;
  if (HasState(ax::mojom::State::kMultiselectable))
    max_items = std::numeric_limits<int>::max();
  return max_items;
}

}  // namespace ui
