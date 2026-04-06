// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/tracked_element/interaction_test_util_web_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/webui/resources/js/tracked_element/tracked_element.mojom.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace ui {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementIdentifier);

class MockTrackedElementManager
    : public tracked_element::mojom::TrackedElementManager {
 public:
  explicit MockTrackedElementManager(
      mojo::PendingReceiver<tracked_element::mojom::TrackedElementManager>
          pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  void OnElementHighlightChanged(const std::string& native_identifier,
                                 bool highlighted) override {}

  void ClickElement(const std::string& native_identifier,
                    ClickElementCallback callback) override {
    interaction_events_.push_back("Click:" + native_identifier);
    std::move(callback).Run(true);
  }

  void FocusElement(const std::string& native_identifier,
                    FocusElementCallback callback) override {
    interaction_events_.push_back("Focus:" + native_identifier);
    std::move(callback).Run(true);
  }

  void SelectTab(const std::string& native_identifier,
                 uint32_t index,
                 SelectTabCallback callback) override {
    interaction_events_.push_back(base::StringPrintf(
        "SelectTab:%s:%u", native_identifier.c_str(), index));
    std::move(callback).Run(true);
  }

  void SelectDropdownItem(const std::string& native_identifier,
                          uint32_t index,
                          SelectDropdownItemCallback callback) override {
    interaction_events_.push_back(base::StringPrintf(
        "SelectDropdownItem:%s:%u", native_identifier.c_str(), index));
    std::move(callback).Run(true);
  }

  void EnterText(const std::string& native_identifier,
                 const std::u16string& text,
                 tracked_element::mojom::TextEntryMode mode,
                 EnterTextCallback callback) override {
    interaction_events_.push_back(base::StringPrintf(
        "EnterText:%s:%s:%d", native_identifier.c_str(),
        base::UTF16ToUTF8(text).c_str(), static_cast<int>(mode)));
    std::move(callback).Run(true);
  }

  void Confirm(const std::string& native_identifier,
               ConfirmCallback callback) override {
    interaction_events_.push_back("Confirm:" + native_identifier);
    std::move(callback).Run(true);
  }

  const std::vector<std::string>& interaction_events() const {
    return interaction_events_;
  }

 private:
  mojo::Receiver<tracked_element::mojom::TrackedElementManager> receiver_{this};
  std::vector<std::string> interaction_events_;
};

}  // namespace

class InteractionTestUtilSimulatorWebUITest : public views::test::WidgetTest {
 public:
  InteractionTestUtilSimulatorWebUITest()
      : views::test::WidgetTest(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>())) {}
  ~InteractionTestUtilSimulatorWebUITest() override = default;

  void SetUp() override {
    content::SetBrowserClientForTesting(&test_browser_client_);
    rvh_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();
    views::test::WidgetTest::SetUp();

    browser_context_ = std::make_unique<content::TestBrowserContext>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        browser_context_.get(), nullptr);

    mojo::PendingRemote<tracked_element::mojom::TrackedElementHandler> remote;
    handler_ = std::make_unique<TrackedElementHandler>(
        web_contents_.get(), remote.InitWithNewPipeAndPassReceiver(),
        ui::ElementContext::CreateFakeContextForTesting(web_contents_.get()),
        std::vector<ui::ElementIdentifier>{kTestElementIdentifier});
    tracked_element_handler_remote_.Bind(std::move(remote));

    mojo::Remote<tracked_element::mojom::TrackedElementManager> manager_remote;
    manager_ = std::make_unique<MockTrackedElementManager>(
        manager_remote.BindNewPipeAndPassReceiver());
    tracked_element_handler_remote_->SetManager(manager_remote.Unbind());

    // Make the element visible.
    tracked_element_handler_remote_->TrackedElementVisibilityChanged(
        kTestElementIdentifier.GetName(), true, gfx::RectF(0, 0, 10, 10));
    tracked_element_handler_remote_.FlushForTesting();

    simulator_ = std::make_unique<InteractionTestUtilSimulatorWebUI>();
  }

  void TearDown() override {
    simulator_.reset();
    manager_.reset();
    tracked_element_handler_remote_.reset();
    handler_.reset();
    web_contents_.reset();
    browser_context_.reset();
    rvh_enabler_.reset();
    views::test::WidgetTest::TearDown();
  }

 protected:
  TrackedElementWebUI* GetElement() {
    auto* const element =
        ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
            kTestElementIdentifier);
    return element ? element->AsA<TrackedElementWebUI>() : nullptr;
  }

  content::ContentBrowserClient test_browser_client_;
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_enabler_;
  std::unique_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TrackedElementHandler> handler_;
  mojo::Remote<tracked_element::mojom::TrackedElementHandler>
      tracked_element_handler_remote_;
  std::unique_ptr<MockTrackedElementManager> manager_;
  std::unique_ptr<InteractionTestUtilSimulatorWebUI> simulator_;
};

TEST_F(InteractionTestUtilSimulatorWebUITest, PressButton) {
  auto* const element = GetElement();
  ASSERT_TRUE(element);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            simulator_->PressButton(
                element, ui::test::InteractionTestUtil::InputType::kDontCare));
  EXPECT_THAT(
      manager_->interaction_events(),
      testing::ElementsAre("Click:" + kTestElementIdentifier.GetName()));
}

TEST_F(InteractionTestUtilSimulatorWebUITest, SelectMenuItem) {
  auto* const element = GetElement();
  ASSERT_TRUE(element);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            simulator_->SelectMenuItem(
                element, ui::test::InteractionTestUtil::InputType::kDontCare));
  EXPECT_THAT(
      manager_->interaction_events(),
      testing::ElementsAre("Click:" + kTestElementIdentifier.GetName()));
}

TEST_F(InteractionTestUtilSimulatorWebUITest, DoDefaultAction) {
  auto* const element = GetElement();
  ASSERT_TRUE(element);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            simulator_->DoDefaultAction(
                element, ui::test::InteractionTestUtil::InputType::kDontCare));
  EXPECT_THAT(
      manager_->interaction_events(),
      testing::ElementsAre("Click:" + kTestElementIdentifier.GetName()));
}

TEST_F(InteractionTestUtilSimulatorWebUITest, SelectTab) {
  auto* const element = GetElement();
  ASSERT_TRUE(element);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            simulator_->SelectTab(
                element, 2, ui::test::InteractionTestUtil::InputType::kDontCare,
                std::nullopt));
  EXPECT_THAT(manager_->interaction_events(),
              testing::ElementsAre(
                  "SelectTab:" + kTestElementIdentifier.GetName() + ":2"));
}

TEST_F(InteractionTestUtilSimulatorWebUITest, SelectDropdownItem) {
  auto* const element = GetElement();
  ASSERT_TRUE(element);
  EXPECT_EQ(
      ui::test::ActionResult::kSucceeded,
      simulator_->SelectDropdownItem(
          element, 1, ui::test::InteractionTestUtil::InputType::kDontCare));
  EXPECT_THAT(manager_->interaction_events(),
              testing::ElementsAre("SelectDropdownItem:" +
                                   kTestElementIdentifier.GetName() + ":1"));
}

TEST_F(InteractionTestUtilSimulatorWebUITest, EnterText) {
  auto* const element = GetElement();
  ASSERT_TRUE(element);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            simulator_->EnterText(
                element, u"hello",
                ui::test::InteractionTestUtil::TextEntryMode::kAppend));
  EXPECT_THAT(
      manager_->interaction_events(),
      testing::ElementsAre("EnterText:" + kTestElementIdentifier.GetName() +
                           ":hello:2"));
}

TEST_F(InteractionTestUtilSimulatorWebUITest, FocusElement) {
  auto* const element = GetElement();
  ASSERT_TRUE(element);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            simulator_->FocusElement(element));
  EXPECT_THAT(
      manager_->interaction_events(),
      testing::ElementsAre("Focus:" + kTestElementIdentifier.GetName()));
}

TEST_F(InteractionTestUtilSimulatorWebUITest, Confirm) {
  auto* const element = GetElement();
  ASSERT_TRUE(element);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded, simulator_->Confirm(element));
  EXPECT_THAT(
      manager_->interaction_events(),
      testing::ElementsAre("Confirm:" + kTestElementIdentifier.GetName()));
}

}  // namespace ui
