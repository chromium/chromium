// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/remote_view/remote_view_host.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/unguessable_token.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

namespace {

// Embeds using |token| and waits for it. Returns true if embed succeeds.
bool Embed(RemoteViewHost* host, const base::UnguessableToken& token) {
  base::RunLoop run_loop;
  bool embed_result = false;
  host->EmbedUsingToken(
      token, 0u /* no flags */,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool* result, bool success) {
            *result = success;
            run_loop->Quit();
          },
          &run_loop, &embed_result));
  run_loop.Run();
  return embed_result;
}

}  // namespace

class RemoteViewHostTest : public aura::test::AuraTestBase {
 public:
  RemoteViewHostTest() = default;
  ~RemoteViewHostTest() override = default;

  // aura::test::AuraTestBase
  void SetUp() override {
    EnableMusWithTestWindowTree();
    AuraTestBase::SetUp();
  }

  // Creates a widget to host |contents|.
  std::unique_ptr<views::Widget> CreateTestWidget(views::View* contents) {
    std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 100, 100);
    params.context = root_window();
    widget->Init(params);
    widget->SetContentsView(contents);

    return widget;
  }

  // Helper callback invoked during embed to simulate adding to widget during
  // embed operation.
  void CreateTestWidgetWhileEmbeddingHelper(
      base::RunLoop* run_loop,
      views::View* contents,
      std::unique_ptr<views::Widget>* widget,
      bool success) {
    ASSERT_TRUE(success);
    *widget = CreateTestWidget(contents);
    run_loop->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteViewHostTest);
};

// Tests that the embed operation fails with an unknown token.
TEST_F(RemoteViewHostTest, BadEmbed) {
  const base::UnguessableToken unknown_token = base::UnguessableToken::Create();

  // Ownership will be passed to |widget| later.
  RemoteViewHost* host = new RemoteViewHost();
  std::unique_ptr<views::Widget> widget = CreateTestWidget(host);
  EXPECT_TRUE(host->native_view());

  // Embed fails with unknown token.
  EXPECT_FALSE(Embed(host, unknown_token));

  // |host| is still attached despite the Embed failure.
  EXPECT_TRUE(host->native_view());
}

// Tests when RemoveViewHost is added to a widget before embedding.
TEST_F(RemoteViewHostTest, AddToWidgetBeforeEmbed) {
  const base::UnguessableToken token = base::UnguessableToken::Create();
  window_tree()->AddScheduledEmbedToken(token);

  // Ownership will be passed to |widget| later.
  RemoteViewHost* host = new RemoteViewHost();

  // |host| is not attached until the widget is created.
  EXPECT_FALSE(host->native_view());
  std::unique_ptr<views::Widget> widget = CreateTestWidget(host);
  EXPECT_TRUE(host->native_view());

  // Embed succeeds.
  EXPECT_TRUE(Embed(host, token));

  // |host| is still attached to the embedding window.
  EXPECT_TRUE(host->native_view());
}

// Tests when RemoveViewHost is added to a widget after embedding.
TEST_F(RemoteViewHostTest, AddToWidgetAfterEmbed) {
  const base::UnguessableToken token = base::UnguessableToken::Create();
  window_tree()->AddScheduledEmbedToken(token);

  // Ownership will be passed to |widget| later.
  RemoteViewHost* host = new RemoteViewHost();

  // Request embedding but it will be deferred until added to a widget.
  base::RunLoop run_loop;
  bool embed_result = false;
  host->EmbedUsingToken(
      token, 0u /* no flags */,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool* result, bool success) {
            *result = success;
            run_loop->Quit();
          },
          &run_loop, &embed_result));

  // |host| is not attached before adding to a widget.
  EXPECT_FALSE(host->native_view());

  // Add to a widget and wait for embed to finish.
  std::unique_ptr<views::Widget> widget = CreateTestWidget(host);
  run_loop.Run();

  // |host| is attached after added to a widget.
  EXPECT_TRUE(host->native_view());
}

}  // namespace views
