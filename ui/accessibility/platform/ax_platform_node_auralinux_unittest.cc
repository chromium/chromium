// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium cannot upgrade to ATK 2.12 API as it still needs to run
// valid builds for Ubuntu Trusty.
#define ATK_DISABLE_DEPRECATION_WARNINGS

#include <atk/atk.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_platform_node_unittest.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"

namespace ui {

class AXPlatformNodeAuraLinuxTest : public AXPlatformNodeTest {
 public:
  AXPlatformNodeAuraLinuxTest() {}
  ~AXPlatformNodeAuraLinuxTest() override {}

  void SetUp() override {}

 protected:
  AtkObject* AtkObjectFromNode(AXNode* node) {
    TestAXNodeWrapper* wrapper =
        TestAXNodeWrapper::GetOrCreate(tree_.get(), node);
    if (!wrapper)
      return nullptr;
    AXPlatformNode* ax_platform_node = wrapper->ax_platform_node();
    AtkObject* atk_object = ax_platform_node->GetNativeViewAccessible();
    return atk_object;
  }

  AtkObject* GetRootAtkObject() { return AtkObjectFromNode(GetRootNode()); }
};

static void EnsureAtkObjectHasAttributeWithValue(
    AtkObject* atk_object,
    const gchar* attribute_name,
    const gchar* attribute_value) {
  AtkAttributeSet* attributes = atk_object_get_attributes(atk_object);
  bool saw_attribute = false;

  AtkAttributeSet* current = attributes;
  while (current) {
    AtkAttribute* attribute = static_cast<AtkAttribute*>(current->data);

    if (0 == strcmp(attribute_name, attribute->name)) {
      // Ensure that we only see this attribute once.
      ASSERT_FALSE(saw_attribute);

      EXPECT_STREQ(attribute_value, attribute->value);
      saw_attribute = true;
    }

    current = current->next;
  }

  ASSERT_TRUE(saw_attribute);
  atk_attribute_set_free(attributes);
}

static void EnsureAtkObjectDoesNotHaveAttribute(
    AtkObject* atk_object,
    const gchar* attribute_name) {
  AtkAttributeSet* attributes = atk_object_get_attributes(atk_object);
  AtkAttributeSet* current = attributes;
  while (current) {
    AtkAttribute* attribute = static_cast<AtkAttribute*>(current->data);
    ASSERT_NE(0, strcmp(attribute_name, attribute->name));
    current = current->next;
  }
  atk_attribute_set_free(attributes);
}

static void SetStringAttributeOnNode(
    AXNode* ax_node,
    ax::mojom::StringAttribute attribute,
    const char* attribute_value,
    base::Optional<ax::mojom::Role> role = base::nullopt) {
  AXNodeData new_data = AXNodeData();
  new_data.role = role.value_or(ax::mojom::Role::kApplication);
  new_data.id = ax_node->data().id;
  new_data.AddStringAttribute(attribute, attribute_value);
  ax_node->SetData(new_data);
}

static void TestAtkObjectIntAttribute(
    AXNode* ax_node,
    AtkObject* atk_object,
    ax::mojom::IntAttribute mojom_attribute,
    const gchar* attribute_name,
    base::Optional<ax::mojom::Role> role = base::nullopt) {
  AXNodeData new_data = AXNodeData();
  new_data.role = role.value_or(ax::mojom::Role::kApplication);
  ax_node->SetData(new_data);
  EnsureAtkObjectDoesNotHaveAttribute(atk_object, attribute_name);

  std::pair<int, const char*> tests[] = {
      std::make_pair(0, "0"),       std::make_pair(1, "1"),
      std::make_pair(2, "2"),       std::make_pair(-100, "-100"),
      std::make_pair(1000, "1000"),
  };

  for (unsigned i = 0; i < G_N_ELEMENTS(tests); i++) {
    AXNodeData new_data = AXNodeData();
    new_data.role = role.value_or(ax::mojom::Role::kApplication);
    new_data.id = ax_node->data().id;
    new_data.AddIntAttribute(mojom_attribute, tests[i].first);
    ax_node->SetData(new_data);
    EnsureAtkObjectHasAttributeWithValue(atk_object, attribute_name,
                                         tests[i].second);
  }
}

static void TestAtkObjectStringAttribute(
    AXNode* ax_node,
    AtkObject* atk_object,
    ax::mojom::StringAttribute mojom_attribute,
    const gchar* attribute_name,
    base::Optional<ax::mojom::Role> role = base::nullopt) {
  AXNodeData new_data = AXNodeData();
  new_data.role = role.value_or(ax::mojom::Role::kApplication);
  ax_node->SetData(new_data);
  EnsureAtkObjectDoesNotHaveAttribute(atk_object, attribute_name);

  const char* tests[] = {
      "",
      "a string with spaces"
      "a string with , a comma",
      "\xE2\x98\xBA",  // The smiley emoji.
  };

  for (unsigned i = 0; i < G_N_ELEMENTS(tests); i++) {
    SetStringAttributeOnNode(ax_node, mojom_attribute, tests[i], role);
    EnsureAtkObjectHasAttributeWithValue(atk_object, attribute_name, tests[i]);
  }
}

static void TestAtkObjectBoolAttribute(
    AXNode* ax_node,
    AtkObject* atk_object,
    ax::mojom::BoolAttribute mojom_attribute,
    const gchar* attribute_name,
    base::Optional<ax::mojom::Role> role = base::nullopt) {
  AXNodeData new_data = AXNodeData();
  new_data.role = role.value_or(ax::mojom::Role::kApplication);
  ax_node->SetData(new_data);
  EnsureAtkObjectDoesNotHaveAttribute(atk_object, attribute_name);

  new_data = AXNodeData();
  new_data.role = role.value_or(ax::mojom::Role::kApplication);
  new_data.id = ax_node->data().id;
  new_data.AddBoolAttribute(mojom_attribute, true);
  ax_node->SetData(new_data);
  EnsureAtkObjectHasAttributeWithValue(atk_object, attribute_name, "true");

  new_data = AXNodeData();
  new_data.role = role.value_or(ax::mojom::Role::kApplication);
  new_data.id = ax_node->data().id;
  new_data.AddBoolAttribute(mojom_attribute, false);
  ax_node->SetData(new_data);
  EnsureAtkObjectHasAttributeWithValue(atk_object, attribute_name, "false");
}

//
// AtkObject tests
//
#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 16, 0)
#define ATK_216
#endif

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkObjectDetachedObject) {
  AXNodeData root;
  root.id = 1;
  root.SetName("Name");
  Init(root);

  AtkObject* root_obj(GetRootAtkObject());
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  g_object_ref(root_obj);

  const gchar* name = atk_object_get_name(root_obj);
  EXPECT_STREQ("Name", name);

  AtkStateSet* state_set = atk_object_ref_state_set(root_obj);
  ASSERT_TRUE(ATK_IS_STATE_SET(state_set));
  EXPECT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_DEFUNCT));
  g_object_unref(state_set);

  tree_.reset(new AXTree());
  EXPECT_EQ(nullptr, atk_object_get_name(root_obj));

  state_set = atk_object_ref_state_set(root_obj);
  ASSERT_TRUE(ATK_IS_STATE_SET(state_set));
  EXPECT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_DEFUNCT));
  g_object_unref(state_set);

  g_object_unref(root_obj);
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkObjectName) {
  AXNodeData root;
  root.id = 1;
  root.SetName("Name");
  Init(root);

  AtkObject* root_obj(GetRootAtkObject());
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  g_object_ref(root_obj);

  const gchar* name = atk_object_get_name(root_obj);
  EXPECT_STREQ("Name", name);

  g_object_unref(root_obj);
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkObjectDescription) {
  AXNodeData root;
  root.id = 1;
  root.AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                          "Description");
  Init(root);

  AtkObject* root_obj(GetRootAtkObject());
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  g_object_ref(root_obj);

  const gchar* description = atk_object_get_description(root_obj);
  EXPECT_STREQ("Description", description);

  g_object_unref(root_obj);
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkObjectRole) {
  AXNodeData root;
  root.id = 1;
  root.child_ids.push_back(2);
  root.role = ax::mojom::Role::kApplication;

  AXNodeData child;
  child.id = 2;

  Init(root, child);
  AXNode* child_node = GetRootNode()->children()[0];

  AtkObject* root_obj(AtkObjectFromNode(GetRootNode()));
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  g_object_ref(root_obj);
  EXPECT_EQ(ATK_ROLE_APPLICATION, atk_object_get_role(root_obj));
  g_object_unref(root_obj);

  child.role = ax::mojom::Role::kAlert;
  child_node->SetData(child);
  AtkObject* child_obj(AtkObjectFromNode(child_node));
  ASSERT_TRUE(ATK_IS_OBJECT(child_obj));
  g_object_ref(child_obj);
  EXPECT_EQ(ATK_ROLE_ALERT, atk_object_get_role(child_obj));
  g_object_unref(child_obj);

  child.role = ax::mojom::Role::kButton;
  child_node->SetData(child);
  child_obj = AtkObjectFromNode(child_node);
  ASSERT_TRUE(ATK_IS_OBJECT(child_obj));
  g_object_ref(child_obj);
  EXPECT_EQ(ATK_ROLE_PUSH_BUTTON, atk_object_get_role(child_obj));
  g_object_unref(child_obj);

  child.role = ax::mojom::Role::kCanvas;
  child_node->SetData(child);
  child_obj = AtkObjectFromNode(child_node);
  ASSERT_TRUE(ATK_IS_OBJECT(child_obj));
  g_object_ref(child_obj);
  EXPECT_EQ(ATK_ROLE_CANVAS, atk_object_get_role(child_obj));
  g_object_unref(child_obj);

  child.role = ax::mojom::Role::kApplication;
  child_node->SetData(child);
  child_obj = AtkObjectFromNode(child_node);
  ASSERT_TRUE(ATK_IS_OBJECT(child_obj));
  g_object_ref(child_obj);
  EXPECT_EQ(ATK_ROLE_EMBEDDED, atk_object_get_role(child_obj));
  g_object_unref(child_obj);

  child.role = ax::mojom::Role::kWindow;
  child_node->SetData(child);
  child_obj = AtkObjectFromNode(child_node);
  ASSERT_TRUE(ATK_IS_OBJECT(child_obj));
  g_object_ref(child_obj);
  EXPECT_EQ(ATK_ROLE_FRAME, atk_object_get_role(child_obj));
  g_object_unref(child_obj);
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkObjectState) {
  AXNodeData root;
  root.id = 1;
  Init(root);

  AtkObject* root_obj(GetRootAtkObject());
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  g_object_ref(root_obj);

  AtkStateSet* state_set = atk_object_ref_state_set(root_obj);
  ASSERT_TRUE(ATK_IS_STATE_SET(state_set));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_ENABLED));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_SENSITIVE));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_SHOWING));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_VISIBLE));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_BUSY));
#if defined(ATK_216)
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_CHECKABLE));
#endif
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_CHECKED));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_DEFAULT));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_EDITABLE));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_EXPANDABLE));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_EXPANDED));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_FOCUSABLE));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_FOCUSED));
#if defined(ATK_216)
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_HAS_POPUP));
#endif
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_HORIZONTAL));
  ASSERT_FALSE(
      atk_state_set_contains_state(state_set, ATK_STATE_INVALID_ENTRY));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_MODAL));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_MULTI_LINE));
  ASSERT_FALSE(
      atk_state_set_contains_state(state_set, ATK_STATE_MULTISELECTABLE));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_REQUIRED));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_SELECTABLE));
  ASSERT_FALSE(
      atk_state_set_contains_state(state_set, ATK_STATE_SELECTABLE_TEXT));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_SELECTED));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_SINGLE_LINE));
  ASSERT_FALSE(atk_state_set_contains_state(state_set,
                                            ATK_STATE_SUPPORTS_AUTOCOMPLETION));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_VERTICAL));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_VISITED));
  g_object_unref(state_set);

  root = AXNodeData();
  root.AddState(ax::mojom::State::kDefault);
  root.AddState(ax::mojom::State::kEditable);
  root.AddState(ax::mojom::State::kExpanded);
  root.AddState(ax::mojom::State::kFocusable);
  root.AddState(ax::mojom::State::kMultiselectable);
  root.AddState(ax::mojom::State::kRequired);
  root.AddState(ax::mojom::State::kVertical);
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, true);
  root.SetInvalidState(ax::mojom::InvalidState::kTrue);
  root.AddStringAttribute(ax::mojom::StringAttribute::kAutoComplete, "foo");
  GetRootNode()->SetData(root);

  state_set = atk_object_ref_state_set(root_obj);
  ASSERT_TRUE(ATK_IS_STATE_SET(state_set));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_BUSY));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_DEFAULT));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_EDITABLE));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_EXPANDABLE));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_EXPANDED));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_FOCUSABLE));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_INVALID_ENTRY));
  ASSERT_TRUE(
      atk_state_set_contains_state(state_set, ATK_STATE_MULTISELECTABLE));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_REQUIRED));
  ASSERT_TRUE(atk_state_set_contains_state(state_set,
                                           ATK_STATE_SUPPORTS_AUTOCOMPLETION));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_VERTICAL));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_FOCUSED));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_HORIZONTAL));
  g_object_unref(state_set);

  root = AXNodeData();
  root.AddState(ax::mojom::State::kCollapsed);
  root.AddState(ax::mojom::State::kHorizontal);
  root.AddState(ax::mojom::State::kVisited);
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  root.SetHasPopup(ax::mojom::HasPopup::kTrue);
  GetRootNode()->SetData(root);

  state_set = atk_object_ref_state_set(root_obj);
  ASSERT_TRUE(ATK_IS_STATE_SET(state_set));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_EXPANDABLE));
#if defined(ATK_216)
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_HAS_POPUP));
#endif
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_HORIZONTAL));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_SELECTABLE));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_SELECTED));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_VISITED));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_EXPANDED));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_VERTICAL));
  g_object_unref(state_set);

  root = AXNodeData();
  root.AddState(ax::mojom::State::kInvisible);
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
  GetRootNode()->SetData(root);

  state_set = atk_object_ref_state_set(root_obj);
  ASSERT_TRUE(ATK_IS_STATE_SET(state_set));
  ASSERT_TRUE(atk_state_set_contains_state(state_set, ATK_STATE_MODAL));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_SHOWING));
  ASSERT_FALSE(atk_state_set_contains_state(state_set, ATK_STATE_VISIBLE));
  g_object_unref(state_set);

  g_object_unref(root_obj);
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkObjectChildAndParent) {
  AXNodeData root;
  root.id = 1;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  AXNodeData button;
  button.role = ax::mojom::Role::kButton;
  button.id = 2;

  AXNodeData checkbox;
  checkbox.role = ax::mojom::Role::kCheckBox;
  checkbox.id = 3;

  Init(root, button, checkbox);
  AXNode* button_node = GetRootNode()->children()[0];
  AXNode* checkbox_node = GetRootNode()->children()[1];
  AtkObject* root_obj = GetRootAtkObject();
  AtkObject* button_obj = AtkObjectFromNode(button_node);
  AtkObject* checkbox_obj = AtkObjectFromNode(checkbox_node);

  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  EXPECT_EQ(2, atk_object_get_n_accessible_children(root_obj));
  ASSERT_TRUE(ATK_IS_OBJECT(button_obj));
  EXPECT_EQ(0, atk_object_get_n_accessible_children(button_obj));
  ASSERT_TRUE(ATK_IS_OBJECT(checkbox_obj));
  EXPECT_EQ(0, atk_object_get_n_accessible_children(checkbox_obj));

  {
    AtkObject* result = atk_object_ref_accessible_child(root_obj, 0);
    EXPECT_TRUE(ATK_IS_OBJECT(root_obj));
    EXPECT_EQ(result, button_obj);
    g_object_unref(result);
  }
  {
    AtkObject* result = atk_object_ref_accessible_child(root_obj, 1);
    EXPECT_TRUE(ATK_IS_OBJECT(root_obj));
    EXPECT_EQ(result, checkbox_obj);
    g_object_unref(result);
  }

  // Now check parents.
  {
    AtkObject* result = atk_object_get_parent(button_obj);
    EXPECT_TRUE(ATK_IS_OBJECT(result));
    EXPECT_EQ(result, root_obj);
  }
  {
    AtkObject* result = atk_object_get_parent(checkbox_obj);
    EXPECT_TRUE(ATK_IS_OBJECT(result));
    EXPECT_EQ(result, root_obj);
  }
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkObjectIndexInParent) {
  AXNodeData root;
  root.id = 1;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  AXNodeData left;
  left.id = 2;

  AXNodeData right;
  right.id = 3;

  Init(root, left, right);

  AtkObject* root_obj(GetRootAtkObject());
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  g_object_ref(root_obj);

  AtkObject* left_obj = atk_object_ref_accessible_child(root_obj, 0);
  ASSERT_TRUE(ATK_IS_OBJECT(left_obj));
  AtkObject* right_obj = atk_object_ref_accessible_child(root_obj, 1);
  ASSERT_TRUE(ATK_IS_OBJECT(right_obj));

  EXPECT_EQ(0, atk_object_get_index_in_parent(left_obj));
  EXPECT_EQ(1, atk_object_get_index_in_parent(right_obj));

  g_object_unref(left_obj);
  g_object_unref(right_obj);
  g_object_unref(root_obj);
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkObjectStringAttributes) {
  AXNodeData root_data;
  root_data.id = 1;

  Init(root_data);

  AXNode* root_node = GetRootNode();
  AtkObject* root_atk_object(AtkObjectFromNode(root_node));
  ASSERT_TRUE(ATK_IS_OBJECT(root_atk_object));
  g_object_ref(root_atk_object);

  std::pair<ax::mojom::StringAttribute, const char*> tests[] = {
      std::make_pair(ax::mojom::StringAttribute::kDisplay, "display"),
      std::make_pair(ax::mojom::StringAttribute::kHtmlTag, "tag"),
      std::make_pair(ax::mojom::StringAttribute::kRole, "xml-roles"),
      std::make_pair(ax::mojom::StringAttribute::kPlaceholder, "placeholder"),
      std::make_pair(ax::mojom::StringAttribute::kRoleDescription,
                     "roledescription"),
      std::make_pair(ax::mojom::StringAttribute::kKeyShortcuts, "keyshortcuts"),
      std::make_pair(ax::mojom::StringAttribute::kLiveStatus, "live"),
      std::make_pair(ax::mojom::StringAttribute::kLiveRelevant, "relevant"),
      std::make_pair(ax::mojom::StringAttribute::kContainerLiveStatus,
                     "container-live"),
      std::make_pair(ax::mojom::StringAttribute::kContainerLiveRelevant,
                     "container-relevant"),
  };

  for (unsigned i = 0; i < G_N_ELEMENTS(tests); i++) {
    TestAtkObjectStringAttribute(root_node, root_atk_object, tests[i].first,
                                 tests[i].second);
  }

  g_object_unref(root_atk_object);
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkObjectBoolAttributes) {
  AXNodeData root_data;
  root_data.id = 1;

  Init(root_data);

  AXNode* root_node = GetRootNode();
  AtkObject* root_atk_object(AtkObjectFromNode(root_node));
  ASSERT_TRUE(ATK_IS_OBJECT(root_atk_object));
  g_object_ref(root_atk_object);

  std::pair<ax::mojom::BoolAttribute, const char*> tests[] = {
      std::make_pair(ax::mojom::BoolAttribute::kLiveAtomic, "atomic"),
      std::make_pair(ax::mojom::BoolAttribute::kBusy, "busy"),
      std::make_pair(ax::mojom::BoolAttribute::kContainerLiveAtomic,
                     "container-atomic"),
      std::make_pair(ax::mojom::BoolAttribute::kContainerLiveBusy,
                     "container-busy"),
  };

  for (unsigned i = 0; i < G_N_ELEMENTS(tests); i++) {
    TestAtkObjectBoolAttribute(root_node, root_atk_object, tests[i].first,
                               tests[i].second);
  }

  g_object_unref(root_atk_object);
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkObjectIntAttributes) {
  AXNodeData root_data;
  root_data.id = 1;

  Init(root_data);

  AXNode* root_node = GetRootNode();
  AtkObject* root_atk_object(AtkObjectFromNode(root_node));
  ASSERT_TRUE(ATK_IS_OBJECT(root_atk_object));
  g_object_ref(root_atk_object);

  TestAtkObjectIntAttribute(root_node, root_atk_object,
                            ax::mojom::IntAttribute::kHierarchicalLevel,
                            "level");
  TestAtkObjectIntAttribute(root_node, root_atk_object,
                            ax::mojom::IntAttribute::kSetSize, "setsize");
  TestAtkObjectIntAttribute(root_node, root_atk_object,
                            ax::mojom::IntAttribute::kPosInSet, "posinset");

  TestAtkObjectIntAttribute(root_node, root_atk_object,
                            ax::mojom::IntAttribute::kAriaColumnCount,
                            "colcount", ax::mojom::Role::kTable);
  TestAtkObjectIntAttribute(root_node, root_atk_object,
                            ax::mojom::IntAttribute::kAriaColumnCount,
                            "colcount", ax::mojom::Role::kGrid);
  TestAtkObjectIntAttribute(root_node, root_atk_object,
                            ax::mojom::IntAttribute::kAriaColumnCount,
                            "colcount", ax::mojom::Role::kTreeGrid);

  TestAtkObjectIntAttribute(root_node, root_atk_object,
                            ax::mojom::IntAttribute::kAriaRowCount, "rowcount",
                            ax::mojom::Role::kTable);
  TestAtkObjectIntAttribute(root_node, root_atk_object,
                            ax::mojom::IntAttribute::kAriaRowCount, "rowcount",
                            ax::mojom::Role::kGrid);
  TestAtkObjectIntAttribute(root_node, root_atk_object,
                            ax::mojom::IntAttribute::kAriaRowCount, "rowcount",
                            ax::mojom::Role::kTreeGrid);

  TestAtkObjectIntAttribute(root_node, root_atk_object,
                            ax::mojom::IntAttribute::kAriaCellColumnIndex,
                            "colindex", ax::mojom::Role::kCell);
  TestAtkObjectIntAttribute(root_node, root_atk_object,
                            ax::mojom::IntAttribute::kAriaCellRowIndex,
                            "rowindex", ax::mojom::Role::kCell);

  g_object_unref(root_atk_object);
}

//
// AtkComponent tests
//

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkComponentRefAtPoint) {
  AXNodeData root;
  root.id = 0;
  root.child_ids.push_back(1);
  root.child_ids.push_back(2);
  root.location = gfx::RectF(0, 0, 30, 30);

  AXNodeData node1;
  node1.id = 1;
  node1.location = gfx::RectF(0, 0, 10, 10);
  node1.SetName("Name1");

  AXNodeData node2;
  node2.id = 2;
  node2.location = gfx::RectF(20, 20, 10, 10);
  node2.SetName("Name2");

  Init(root, node1, node2);

  AtkObject* root_obj(GetRootAtkObject());
  EXPECT_TRUE(ATK_IS_OBJECT(root_obj));
  EXPECT_TRUE(ATK_IS_COMPONENT(root_obj));
  g_object_ref(root_obj);

  AtkObject* child_obj = atk_component_ref_accessible_at_point(
      ATK_COMPONENT(root_obj), 50, 50, ATK_XY_SCREEN);
  EXPECT_EQ(nullptr, child_obj);

  // this is directly on node 1.
  child_obj = atk_component_ref_accessible_at_point(ATK_COMPONENT(root_obj), 5,
                                                    5, ATK_XY_SCREEN);
  ASSERT_NE(nullptr, child_obj);
  EXPECT_TRUE(ATK_IS_OBJECT(child_obj));

  const gchar* name = atk_object_get_name(child_obj);
  EXPECT_STREQ("Name1", name);

  g_object_unref(child_obj);
  g_object_unref(root_obj);
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkComponentsGetExtentsPositionSize) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kWindow;
  root.location = gfx::RectF(10, 40, 800, 600);
  root.child_ids.push_back(2);

  AXNodeData child;
  child.id = 2;
  child.location = gfx::RectF(100, 150, 200, 200);
  Init(root, child);

  TestAXNodeWrapper::SetGlobalCoordinateOffset(gfx::Vector2d(100, 200));

  AtkObject* root_obj = GetRootAtkObject();
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  ASSERT_TRUE(ATK_IS_COMPONENT(root_obj));
  g_object_ref(root_obj);

  gint x_left, y_top, width, height;
  atk_component_get_extents(ATK_COMPONENT(root_obj), &x_left, &y_top, &width,
                            &height, ATK_XY_SCREEN);
  EXPECT_EQ(110, x_left);
  EXPECT_EQ(240, y_top);
  EXPECT_EQ(800, width);
  EXPECT_EQ(600, height);

  atk_component_get_position(ATK_COMPONENT(root_obj), &x_left, &y_top,
                             ATK_XY_SCREEN);
  EXPECT_EQ(110, x_left);
  EXPECT_EQ(240, y_top);

  atk_component_get_extents(ATK_COMPONENT(root_obj), &x_left, &y_top, &width,
                            &height, ATK_XY_WINDOW);
  EXPECT_EQ(110, x_left);
  EXPECT_EQ(240, y_top);
  EXPECT_EQ(800, width);
  EXPECT_EQ(600, height);

  atk_component_get_position(ATK_COMPONENT(root_obj), &x_left, &y_top,
                             ATK_XY_WINDOW);
  EXPECT_EQ(110, x_left);
  EXPECT_EQ(240, y_top);

  atk_component_get_size(ATK_COMPONENT(root_obj), &width, &height);
  EXPECT_EQ(800, width);
  EXPECT_EQ(600, height);

  AXNode* child_node = GetRootNode()->children()[0];
  AtkObject* child_obj = AtkObjectFromNode(child_node);
  ASSERT_TRUE(ATK_IS_OBJECT(child_obj));
  ASSERT_TRUE(ATK_IS_COMPONENT(child_obj));
  g_object_ref(child_obj);

  atk_component_get_extents(ATK_COMPONENT(child_obj), &x_left, &y_top, &width,
                            &height, ATK_XY_SCREEN);
  EXPECT_EQ(200, x_left);
  EXPECT_EQ(350, y_top);
  EXPECT_EQ(200, width);
  EXPECT_EQ(200, height);

  atk_component_get_extents(ATK_COMPONENT(child_obj), &x_left, &y_top, &width,
                            &height, ATK_XY_WINDOW);
  EXPECT_EQ(90, x_left);
  EXPECT_EQ(110, y_top);
  EXPECT_EQ(200, width);
  EXPECT_EQ(200, height);

  atk_component_get_extents(ATK_COMPONENT(child_obj), nullptr, &y_top, &width,
                            &height, ATK_XY_SCREEN);
  EXPECT_EQ(200, height);
  atk_component_get_extents(ATK_COMPONENT(child_obj), &x_left, nullptr, &width,
                            &height, ATK_XY_SCREEN);
  EXPECT_EQ(200, x_left);
  atk_component_get_extents(ATK_COMPONENT(child_obj), &x_left, &y_top, nullptr,
                            &height, ATK_XY_SCREEN);
  EXPECT_EQ(350, y_top);
  atk_component_get_extents(ATK_COMPONENT(child_obj), &x_left, &y_top, &width,
                            nullptr, ATK_XY_SCREEN);
  EXPECT_EQ(200, width);

  g_object_unref(child_obj);
  g_object_unref(root_obj);
}

//
// AtkValue tests
//

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkValueGetCurrentValue) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kSlider;
  root.AddFloatAttribute(ax::mojom::FloatAttribute::kValueForRange, 5.0);
  Init(root);

  AtkObject* root_obj(GetRootAtkObject());
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  ASSERT_TRUE(ATK_IS_VALUE(root_obj));
  g_object_ref(root_obj);

  GValue current_value = G_VALUE_INIT;
  atk_value_get_current_value(ATK_VALUE(root_obj), &current_value);

  EXPECT_EQ(G_TYPE_FLOAT, G_VALUE_TYPE(&current_value));
  EXPECT_EQ(5.0, g_value_get_float(&current_value));

  g_value_unset(&current_value);
  g_object_unref(root_obj);
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkValueGetMaximumValue) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kSlider;
  root.AddFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange, 5.0);
  Init(root);

  AtkObject* root_obj(GetRootAtkObject());
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  ASSERT_TRUE(ATK_IS_VALUE(root_obj));
  g_object_ref(root_obj);

  GValue max_value = G_VALUE_INIT;
  atk_value_get_maximum_value(ATK_VALUE(root_obj), &max_value);

  EXPECT_EQ(G_TYPE_FLOAT, G_VALUE_TYPE(&max_value));
  EXPECT_EQ(5.0, g_value_get_float(&max_value));

  g_value_unset(&max_value);
  g_object_unref(root_obj);
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkValueGetMinimumValue) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kSlider;
  root.AddFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange, 5.0);
  Init(root);

  AtkObject* root_obj(GetRootAtkObject());
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  ASSERT_TRUE(ATK_IS_VALUE(root_obj));
  g_object_ref(root_obj);

  GValue min_value = G_VALUE_INIT;
  atk_value_get_minimum_value(ATK_VALUE(root_obj), &min_value);

  EXPECT_EQ(G_TYPE_FLOAT, G_VALUE_TYPE(&min_value));
  EXPECT_EQ(5.0, g_value_get_float(&min_value));

  g_value_unset(&min_value);
  g_object_unref(root_obj);
}

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkValueGetMinimumIncrement) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kSlider;
  root.AddFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange, 5.0);
  Init(root);

  AtkObject* root_obj(GetRootAtkObject());
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  ASSERT_TRUE(ATK_IS_VALUE(root_obj));
  g_object_ref(root_obj);

  GValue increment = G_VALUE_INIT;
  atk_value_get_minimum_increment(ATK_VALUE(root_obj), &increment);

  EXPECT_EQ(G_TYPE_FLOAT, G_VALUE_TYPE(&increment));
  EXPECT_EQ(5.0, g_value_get_float(&increment));

  g_value_unset(&increment);
  g_object_unref(root_obj);
}

//
// AtkHyperlinkImpl interface
//

TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkHyperlink) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kLink;
  root.AddStringAttribute(ax::mojom::StringAttribute::kUrl, "http://foo.com");
  Init(root);

  AtkObject* root_obj(GetRootAtkObject());
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  ASSERT_TRUE(ATK_IS_HYPERLINK_IMPL(root_obj));
  g_object_ref(root_obj);

  AtkHyperlink* hyperlink(
      atk_hyperlink_impl_get_hyperlink(ATK_HYPERLINK_IMPL(root_obj)));
  ASSERT_TRUE(ATK_IS_HYPERLINK(hyperlink));

  EXPECT_EQ(1, atk_hyperlink_get_n_anchors(hyperlink));
  gchar* uri = atk_hyperlink_get_uri(hyperlink, 0);
  EXPECT_STREQ("http://foo.com", uri);
  g_free(uri);

  g_object_unref(hyperlink);
  g_object_unref(root_obj);
}

//
// AtkText interface
//
//
TEST_F(AXPlatformNodeAuraLinuxTest, TestAtkTextCharacterGranularity) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kTextField;
  root.AddStringAttribute(ax::mojom::StringAttribute::kValue,
                          "A decently long string \xE2\x98\xBA with an emoji.");
  Init(root);

  AtkObject* root_obj(GetRootAtkObject());
  ASSERT_TRUE(ATK_IS_OBJECT(root_obj));
  g_object_ref(root_obj);

  ASSERT_TRUE(ATK_IS_TEXT(root_obj));
  AtkText* atk_text = ATK_TEXT(root_obj);

  EXPECT_EQ(static_cast<gunichar>('d'),
            atk_text_get_character_at_offset(atk_text, 2));
  EXPECT_EQ(static_cast<gunichar>('A'),
            atk_text_get_character_at_offset(atk_text, -1));
  EXPECT_EQ(0u, atk_text_get_character_at_offset(atk_text, 42342));
  EXPECT_EQ(0x263Au, atk_text_get_character_at_offset(atk_text, 23));
  EXPECT_EQ(static_cast<gunichar>(' '),
            atk_text_get_character_at_offset(atk_text, 24));

  auto verify_text = [&](const char* expected_text, char* text,
                         int expected_start, int expected_end, int start,
                         int end) {
    EXPECT_STREQ(expected_text, text);
    EXPECT_EQ(start, expected_start);
    EXPECT_EQ(end, expected_end);
    g_free(text);
  };

  auto verify_text_at_offset = [&](const char* expected_text, int offset,
                                   int expected_start, int expected_end) {
    int start = 0, end = 0;
    char* text = atk_text_get_text_at_offset(
        atk_text, offset, ATK_TEXT_BOUNDARY_CHAR, &start, &end);
    verify_text(expected_text, text, expected_start, expected_end, start, end);
  };

  verify_text_at_offset("d", 2, 2, 3);
  verify_text_at_offset("A", -1, 0, 1);
  verify_text_at_offset("", 42342, 39, 39);
  verify_text_at_offset("\xE2\x98\xBA", 23, 23, 24);
  verify_text_at_offset(" ", 24, 24, 25);

  auto verify_text_after_offset = [&](const char* expected_text, int offset,
                                      int expected_start, int expected_end) {
    int start = 0, end = 0;
    char* text = atk_text_get_text_after_offset(
        atk_text, offset, ATK_TEXT_BOUNDARY_CHAR, &start, &end);
    verify_text(expected_text, text, expected_start, expected_end, start, end);
  };

  verify_text_after_offset("d", 1, 2, 3);
  verify_text_after_offset("", 42342, 39, 39);
  verify_text_after_offset("\xE2\x98\xBA", 22, 23, 24);
  verify_text_after_offset(" ", 23, 24, 25);

  // This boundary condition is enforced by ATK for some reason.
  verify_text_after_offset(nullptr, -1, 0, 0);

  auto verify_text_before_offset = [&](const char* expected_text, int offset,
                                       int expected_start, int expected_end) {
    int start = 0, end = 0;
    char* text = atk_text_get_text_before_offset(
        atk_text, offset, ATK_TEXT_BOUNDARY_CHAR, &start, &end);
    verify_text(expected_text, text, expected_start, expected_end, start, end);
  };

  verify_text_before_offset("d", 3, 2, 3);
  verify_text_before_offset("", 42342, 39, 39);
  verify_text_before_offset("\xE2\x98\xBA", 24, 23, 24);
  verify_text_before_offset(" ", 25, 24, 25);

  // This boundary condition is enforced by ATK for some reason.
  verify_text_after_offset(nullptr, -1, 0, 0);

  g_object_unref(root_obj);
}

}  // namespace ui
