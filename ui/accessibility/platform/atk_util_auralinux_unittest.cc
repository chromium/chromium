// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atk/atk.h>

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/atk_util_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_unittest.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"

namespace ui {

class AtkUtilAuraLinuxTest : public AXPlatformNodeTest {
 public:
  AtkUtilAuraLinuxTest() {
    // We need to create a platform node in order to install it as the root
    // ATK node. The ATK bridge will complain if we try to use it without a
    // root node installed.
    AXNodeData root;
    root.id = 1;
    Init(root);

    TestAXNodeWrapper* wrapper =
        TestAXNodeWrapper::GetOrCreate(tree_.get(), GetRootNode());
    if (!wrapper)
      NOTREACHED();
    AXPlatformNodeAuraLinux::SetApplication(wrapper->ax_platform_node());

    AtkUtilAuraLinux::GetInstance()->InitializeForTesting();
  }

  ~AtkUtilAuraLinuxTest() override {
    TestAXNodeWrapper* wrapper =
        TestAXNodeWrapper::GetOrCreate(tree_.get(), GetRootNode());
    if (!wrapper)
      NOTREACHED();
    g_object_unref(wrapper->ax_platform_node()->GetNativeViewAccessible());
  }
};

TEST_F(AtkUtilAuraLinuxTest, KeySnooping) {
  AtkKeySnoopFunc key_snoop_func = reinterpret_cast<AtkKeySnoopFunc>(
      +[](AtkKeyEventStruct* key_event, int* keyval_seen) {
        *keyval_seen = key_event->keyval;
      });

  int keyval_seen = 0;
  guint listener_id = atk_add_key_event_listener(key_snoop_func, &keyval_seen);

  AtkKeyEventStruct atk_key_event;
  atk_key_event.type = ATK_KEY_EVENT_PRESS;
  atk_key_event.state = 0;
  atk_key_event.keyval = 55;
  atk_key_event.keycode = 10;
  atk_key_event.timestamp = 10;
  atk_key_event.string = nullptr;
  atk_key_event.length = 0;

  AtkUtilAuraLinux* atk_util = AtkUtilAuraLinux::GetInstance();
  atk_util->HandleAtkKeyEvent(&atk_key_event);
  EXPECT_EQ(keyval_seen, 55);

  atk_remove_key_event_listener(listener_id);

  keyval_seen = 0;
  atk_util->HandleAtkKeyEvent(&atk_key_event);

  EXPECT_EQ(keyval_seen, 0);
}

}  // namespace ui
