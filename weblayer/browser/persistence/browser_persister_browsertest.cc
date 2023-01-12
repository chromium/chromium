// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/persistence/browser_persister.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/sessions/core/command_storage_backend.h"
#include "components/sessions/core/command_storage_manager_test_helper.h"
#include "components/sessions/core/session_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/persistence/browser_persister_file_utils.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/common/weblayer_paths.h"
#include "weblayer/public/browser_restore_observer.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/navigation_observer.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/interstitial_utils.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class BrowserPersisterTestHelper {
 public:
  static sessions::CommandStorageManager* GetCommandStorageManager(
      BrowserPersister* persister) {
    return persister->command_storage_manager_.get();
  }
};

namespace {
using testing::UnorderedElementsAre;

class BrowserNavigationObserverImpl : public BrowserRestoreObserver,
                                      public NavigationObserver {
 public:
  static void WaitForNewTabToCompleteNavigation(Browser* browser,
                                                const GURL& url,
                                                size_t tab_to_wait_for = 0) {
    BrowserNavigationObserverImpl observer(browser, url, tab_to_wait_for);
    observer.Wait();
  }

 private:
  BrowserNavigationObserverImpl(Browser* browser,
                                const GURL& url,
                                size_t tab_to_wait_for)
      : browser_(browser), url_(url), tab_to_wait_for_(tab_to_wait_for) {
    browser_->AddBrowserRestoreObserver(this);
  }
  ~BrowserNavigationObserverImpl() override {
    tab_->GetNavigationController()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // NavigationObserver;
  void NavigationCompleted(Navigation* navigation) override {
    if (navigation->GetURL() == *url_)
      run_loop_.Quit();
  }

  // BrowserRestoreObserver:
  void OnRestoreCompleted() override {
    browser_->RemoveBrowserRestoreObserver(this);
    ASSERT_LT(tab_to_wait_for_, browser_->GetTabs().size());
    ASSERT_EQ(nullptr, tab_.get());
    tab_ = browser_->GetTabs()[tab_to_wait_for_];
    tab_->GetNavigationController()->AddObserver(this);
  }

  raw_ptr<Browser> browser_;
  const raw_ref<const GURL> url_;
  raw_ptr<Tab> tab_ = nullptr;
  const size_t tab_to_wait_for_;
  std::unique_ptr<TestNavigationObserver> navigation_observer_;
  base::RunLoop run_loop_;
};

void ShutdownBrowserPersisterAndWait(BrowserImpl* browser) {
  auto task_runner = sessions::CommandStorageManagerTestHelper(
                         BrowserPersisterTestHelper::GetCommandStorageManager(
                             browser->browser_persister()))
                         .GetBackendTaskRunner();
  browser->PrepareForShutdown();
  base::RunLoop run_loop;
  task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                run_loop.QuitClosure());
  run_loop.Run();
}

std::unique_ptr<BrowserImpl> CreateBrowser(ProfileImpl* profile,
                                           const std::string& persistence_id) {
  Browser::PersistenceInfo info;
  info.id = persistence_id;
  auto browser = Browser::Create(profile, &info);
  return std::unique_ptr<BrowserImpl>(
      static_cast<BrowserImpl*>(browser.release()));
}

}  // namespace

using BrowserPersisterTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(BrowserPersisterTest, SingleTab) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<BrowserImpl> browser = CreateBrowser(GetProfile(), "x");
  Tab* tab = browser->CreateTab();
  EXPECT_TRUE(browser->IsRestoringPreviousState());
  const GURL url = embedded_test_server()->GetURL("/simple_page.html");
  NavigateAndWaitForCompletion(url, tab);
  ShutdownBrowserPersisterAndWait(browser.get());
  tab = nullptr;
  browser.reset();

  browser = CreateBrowser(GetProfile(), "x");
  // Should be no tabs while waiting for restore.
  EXPECT_TRUE(browser->GetTabs().empty());
  EXPECT_TRUE(browser->IsRestoringPreviousState());
  // Wait for the restore and navigation to complete.
  BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
      browser.get(), url);

  ASSERT_EQ(1u, browser->GetTabs().size());
  EXPECT_EQ(browser->GetTabs()[0], browser->GetActiveTab());
  EXPECT_EQ(1, browser->GetTabs()[0]
                   ->GetNavigationController()
                   ->GetNavigationListSize());
  EXPECT_FALSE(browser->IsRestoringPreviousState());
}

IN_PROC_BROWSER_TEST_F(BrowserPersisterTest, RestoresGuid) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<BrowserImpl> browser = CreateBrowser(GetProfile(), "x");
  Tab* tab = browser->CreateTab();
  const std::string original_guid = tab->GetGuid();
  EXPECT_FALSE(original_guid.empty());
  EXPECT_TRUE(base::IsValidGUID(original_guid));
  const GURL url = embedded_test_server()->GetURL("/simple_page.html");
  NavigateAndWaitForCompletion(url, tab);
  ShutdownBrowserPersisterAndWait(browser.get());
  tab = nullptr;
  browser.reset();

  browser = CreateBrowser(GetProfile(), "x");
  // Should be no tabs while waiting for restore.
  EXPECT_TRUE(browser->GetTabs().empty());
  // Wait for the restore and navigation to complete.
  BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
      browser.get(), url);

  ASSERT_EQ(1u, browser->GetTabs().size());
  EXPECT_EQ(browser->GetTabs()[0], browser->GetActiveTab());
  EXPECT_EQ(original_guid, browser->GetTabs()[0]->GetGuid());
}

IN_PROC_BROWSER_TEST_F(BrowserPersisterTest, RestoresData) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<BrowserImpl> browser = CreateBrowser(GetProfile(), "x");
  Tab* tab = browser->CreateTab();
  tab->SetData({{"abc", "efg"}});
  const GURL url = embedded_test_server()->GetURL("/simple_page.html");
  NavigateAndWaitForCompletion(url, tab);
  ShutdownBrowserPersisterAndWait(browser.get());
  tab = nullptr;
  browser.reset();

  browser = CreateBrowser(GetProfile(), "x");
  // Should be no tabs while waiting for restore.
  EXPECT_TRUE(browser->GetTabs().empty());
  // Wait for the restore and navigation to complete.
  BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
      browser.get(), url);

  ASSERT_EQ(1u, browser->GetTabs().size());
  EXPECT_EQ(browser->GetTabs()[0], browser->GetActiveTab());
  EXPECT_THAT(browser->GetTabs()[0]->GetData(),
              UnorderedElementsAre(std::make_pair("abc", "efg")));
}

IN_PROC_BROWSER_TEST_F(BrowserPersisterTest, RestoresMostRecentData) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<BrowserImpl> browser = CreateBrowser(GetProfile(), "x");
  Tab* tab = browser->CreateTab();
  tab->SetData({{"xxx", "xxx"}});
  const GURL url = embedded_test_server()->GetURL("/simple_page.html");
  NavigateAndWaitForCompletion(url, tab);

  // Make sure the data has been saved, then set different data on the tab.
  BrowserPersisterTestHelper::GetCommandStorageManager(
      browser->browser_persister())
      ->Save();
  tab->SetData({{"abc", "efg"}});

  ShutdownBrowserPersisterAndWait(browser.get());
  tab = nullptr;
  browser.reset();

  browser = CreateBrowser(GetProfile(), "x");
  // Should be no tabs while waiting for restore.
  EXPECT_TRUE(browser->GetTabs().empty());
  // Wait for the restore and navigation to complete.
  BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
      browser.get(), url);

  ASSERT_EQ(1u, browser->GetTabs().size());
  EXPECT_EQ(browser->GetTabs()[0], browser->GetActiveTab());
  EXPECT_THAT(browser->GetTabs()[0]->GetData(),
              UnorderedElementsAre(std::make_pair("abc", "efg")));
}

IN_PROC_BROWSER_TEST_F(BrowserPersisterTest, TwoTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<BrowserImpl> browser = CreateBrowser(GetProfile(), "x");
  Tab* tab1 = browser->CreateTab();
  const GURL url1 = embedded_test_server()->GetURL("/simple_page.html");
  NavigateAndWaitForCompletion(url1, tab1);

  Tab* tab2 = browser->CreateTab();
  const GURL url2 = embedded_test_server()->GetURL("/simple_page2.html");
  NavigateAndWaitForCompletion(url2, tab2);
  browser->SetActiveTab(tab2);

  // Shut down the service.
  ShutdownBrowserPersisterAndWait(browser.get());
  tab1 = tab2 = nullptr;
  browser.reset();

  // Recreate the browser and run the assertions twice to ensure we handle
  // correctly storing state of tabs that need to be reloaded.
  for (int i = 0; i < 2; ++i) {
    browser = CreateBrowser(GetProfile(), "x");
    // Should be no tabs while waiting for restore.
    EXPECT_TRUE(browser->GetTabs().empty()) << "iteration " << i;
    // Wait for the restore and navigation to complete. This waits for the
    // second tab as that was the active one.
    BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
        browser.get(), url2, 1);

    ASSERT_EQ(2u, browser->GetTabs().size()) << "iteration " << i;
    // The first tab shouldn't have loaded yet, as it's not active.
    EXPECT_TRUE(static_cast<TabImpl*>(browser->GetTabs()[0])
                    ->web_contents()
                    ->GetController()
                    .NeedsReload())
        << "iteration " << i;
    EXPECT_EQ(browser->GetTabs()[1], browser->GetActiveTab())
        << "iteration " << i;
    EXPECT_EQ(1, browser->GetTabs()[1]
                     ->GetNavigationController()
                     ->GetNavigationListSize())
        << "iteration " << i;

    ShutdownBrowserPersisterAndWait(browser.get());
  }
}

IN_PROC_BROWSER_TEST_F(BrowserPersisterTest, MoveBetweenBrowsers) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a browser with two tabs.
  std::unique_ptr<BrowserImpl> browser1 = CreateBrowser(GetProfile(), "x");
  Tab* tab1 = browser1->CreateTab();
  const GURL url1 = embedded_test_server()->GetURL("/simple_page.html");
  NavigateAndWaitForCompletion(url1, tab1);

  Tab* tab2 = browser1->CreateTab();
  const GURL url2 = embedded_test_server()->GetURL("/simple_page2.html");
  NavigateAndWaitForCompletion(url2, tab2);
  browser1->SetActiveTab(tab2);

  // Create another browser with a single tab.
  std::unique_ptr<BrowserImpl> browser2 = CreateBrowser(GetProfile(), "y");
  Tab* tab3 = browser2->CreateTab();
  const GURL url3 = embedded_test_server()->GetURL("/simple_page3.html");
  NavigateAndWaitForCompletion(url3, tab3);

  // Move |tab2| to |browser2|.
  browser2->AddTab(tab2);
  browser2->SetActiveTab(tab2);

  ShutdownBrowserPersisterAndWait(browser1.get());
  ShutdownBrowserPersisterAndWait(browser2.get());
  tab1 = nullptr;
  browser1.reset();

  tab2 = tab3 = nullptr;
  browser2.reset();

  // Restore the browsers.
  browser1 = CreateBrowser(GetProfile(), "x");
  BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
      browser1.get(), url1);
  ASSERT_EQ(1u, browser1->GetTabs().size());
  EXPECT_EQ(1, browser1->GetTabs()[0]
                   ->GetNavigationController()
                   ->GetNavigationListSize());

  browser2 = CreateBrowser(GetProfile(), "y");
  BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
      browser2.get(), url2, 1);
  ASSERT_EQ(2u, browser2->GetTabs().size());
  EXPECT_EQ(1, browser2->GetTabs()[1]
                   ->GetNavigationController()
                   ->GetNavigationListSize());

  // As |tab3| isn't active it needs to be loaded. Force that now.
  TabImpl* restored_tab_3 = static_cast<TabImpl*>(browser2->GetTabs()[0]);
  EXPECT_TRUE(restored_tab_3->web_contents()->GetController().NeedsReload());
  restored_tab_3->web_contents()->GetController().LoadIfNecessary();
  EXPECT_TRUE(content::WaitForLoadStop(restored_tab_3->web_contents()));
}

class BrowserPersisterTestWithTwoPersistedIds : public WebLayerBrowserTest {
 public:
  // WebLayerBrowserTest:
  void SetUpOnMainThread() override {
    WebLayerBrowserTest::SetUpOnMainThread();
    // Configure two browsers with ids 'x' and 'y'.
    ASSERT_TRUE(embedded_test_server()->Start());
    std::unique_ptr<BrowserImpl> browser1 = CreateBrowser(GetProfile(), "x");
    const GURL url1 = embedded_test_server()->GetURL("/simple_page.html");
    NavigateAndWaitForCompletion(url1, browser1->CreateTab());

    std::unique_ptr<BrowserImpl> browser2 = CreateBrowser(GetProfile(), "y");
    const GURL url2 = embedded_test_server()->GetURL("/simple_page3.html");
    NavigateAndWaitForCompletion(url2, browser2->CreateTab());

    // Shut down the browsers.
    ShutdownBrowserPersisterAndWait(browser1.get());
    browser1.reset();
    ShutdownBrowserPersisterAndWait(browser2.get());
    browser2.reset();
  }
};

IN_PROC_BROWSER_TEST_F(BrowserPersisterTestWithTwoPersistedIds,
                       GetBrowserPersistenceIds) {
  {
    // Create a file that has the name of a valid persistence file, but has
    // invalid contents.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::WriteFile(BuildBasePathForBrowserPersister(
                        GetProfile()->GetBrowserPersisterDataBaseDir(), "z"),
                    "a bogus persistence file");
  }

  base::RunLoop run_loop;
  base::flat_set<std::string> persistence_ids;
  GetProfile()->GetBrowserPersistenceIds(
      base::BindLambdaForTesting([&](base::flat_set<std::string> ids) {
        persistence_ids = std::move(ids);
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_EQ(2u, persistence_ids.size());
  EXPECT_TRUE(persistence_ids.contains("x"));
  EXPECT_TRUE(persistence_ids.contains("y"));
}

bool HasSessionFileStartingWith(const base::FilePath& path) {
  auto paths = sessions::CommandStorageBackend::GetSessionFilePaths(
      path, sessions::CommandStorageManager::kOther);
  return paths.size() == 1;
}

IN_PROC_BROWSER_TEST_F(BrowserPersisterTestWithTwoPersistedIds,
                       RemoveBrowserPersistenceStorage) {
  base::FilePath file_path1 = BuildBasePathForBrowserPersister(
      GetProfile()->GetBrowserPersisterDataBaseDir(), "x");
  base::FilePath file_path2 = BuildBasePathForBrowserPersister(
      GetProfile()->GetBrowserPersisterDataBaseDir(), "y");

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(HasSessionFileStartingWith(file_path1));
    ASSERT_TRUE(HasSessionFileStartingWith(file_path2));
  }
  base::RunLoop run_loop;
  base::flat_set<std::string> persistence_ids;
  persistence_ids.insert("x");
  persistence_ids.insert("y");
  GetProfile()->RemoveBrowserPersistenceStorage(
      base::BindLambdaForTesting([&](bool result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }),
      std::move(persistence_ids));
  run_loop.Run();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(file_path1));
    EXPECT_FALSE(base::PathExists(file_path2));
  }
}

IN_PROC_BROWSER_TEST_F(BrowserPersisterTest, OnErrorWritingSessionCommands) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<BrowserImpl> browser = CreateBrowser(GetProfile(), "x");
  Tab* tab = browser->CreateTab();
  EXPECT_TRUE(browser->IsRestoringPreviousState());
  const GURL url = embedded_test_server()->GetURL("/simple_page.html");
  NavigateAndWaitForCompletion(url, tab);
  static_cast<sessions::CommandStorageManagerDelegate*>(
      browser->browser_persister())
      ->OnErrorWritingSessionCommands();
  ShutdownBrowserPersisterAndWait(browser.get());
  tab = nullptr;
  browser.reset();

  browser = CreateBrowser(GetProfile(), "x");
  // Should be no tabs while waiting for restore.
  EXPECT_TRUE(browser->GetTabs().empty());
  EXPECT_TRUE(browser->IsRestoringPreviousState());
  // Wait for the restore and navigation to complete.
  BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
      browser.get(), url);

  ASSERT_EQ(1u, browser->GetTabs().size());
  EXPECT_EQ(browser->GetTabs()[0], browser->GetActiveTab());
  EXPECT_EQ(1, browser->GetTabs()[0]
                   ->GetNavigationController()
                   ->GetNavigationListSize());
  EXPECT_FALSE(browser->IsRestoringPreviousState());
}

}  // namespace weblayer
