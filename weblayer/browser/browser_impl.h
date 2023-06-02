// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_IMPL_H_
#define WEBLAYER_BROWSER_BROWSER_IMPL_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "weblayer/public/browser.h"

#include "base/android/scoped_java_ref.h"

namespace base {
class FilePath;
}

namespace blink {
namespace web_pref {
struct WebPreferences;
}
}  // namespace blink

namespace content {
class WebContents;
}

namespace weblayer {

class BrowserPersister;
class ProfileImpl;
class TabImpl;

class BrowserImpl : public Browser {
 public:
  // Prefix used for storing persistence state.
  static constexpr char kPersistenceFilePrefix[] = "State";

  BrowserImpl(const BrowserImpl&) = delete;
  BrowserImpl& operator=(const BrowserImpl&) = delete;
  ~BrowserImpl() override;

  BrowserPersister* browser_persister() { return browser_persister_.get(); }

  ProfileImpl* profile() { return profile_; }

  // Creates and adds a Tab from session restore. The returned tab is owned by
  // this Browser.
  TabImpl* CreateTabForSessionRestore(
      std::unique_ptr<content::WebContents> web_contents,
      const std::string& guid);
  TabImpl* CreateTab(std::unique_ptr<content::WebContents> web_contents);

  // Called from BrowserPersister when restore has completed.
  void OnRestoreCompleted();

  const std::string& GetPackageName() { return package_name_; }

  bool CompositorHasSurface();

  base::android::ScopedJavaGlobalRef<jobject> java_browser() {
    return java_impl_;
  }

  void AddTab(JNIEnv* env, long native_tab);
  base::android::ScopedJavaLocalRef<jobjectArray> GetTabs(JNIEnv* env);
  void SetActiveTab(JNIEnv* env, long native_tab);
  base::android::ScopedJavaLocalRef<jobject> GetActiveTab(JNIEnv* env);
  void PrepareForShutdown(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> GetPersistenceId(JNIEnv* env);
  void SaveBrowserPersisterIfNecessary(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jbyteArray> GetMinimalPersistenceState(
      JNIEnv* env,
      int max_navigations_per_tab);
  void RestoreStateIfNecessary(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_persistence_id);
  void WebPreferencesChanged(JNIEnv* env);

  bool IsRestoringPreviousState(JNIEnv* env) {
    return IsRestoringPreviousState();
  }

  // Used in tests to specify a non-default max (0 means use the default).
  std::vector<uint8_t> GetMinimalPersistenceState(int max_navigations_per_tab,
                                                  int max_size_in_bytes);

  // Used by tests to specify a callback to listen to changes to visible
  // security state.
  void set_visible_security_state_callback_for_tests(
      base::OnceClosure closure) {
    visible_security_state_changed_callback_for_tests_ = std::move(closure);
  }

  bool GetPasswordEchoEnabled();
  void SetWebPreferences(blink::web_pref::WebPreferences* prefs);

  // On Android the Java Tab class owns the C++ Tab. DestroyTab() calls to the
  // Java Tab class to initiate deletion. This function is called from the Java
  // side to remove the tab from the browser and shortly followed by deleting
  // the tab.
  void RemoveTabBeforeDestroyingFromJava(Tab* tab);

  // Browser:
  void AddTab(Tab* tab) override;
  void DestroyTab(Tab* tab) override;
  void SetActiveTab(Tab* tab) override;
  Tab* GetActiveTab() override;
  std::vector<Tab*> GetTabs() override;
  Tab* CreateTab() override;
  void PrepareForShutdown() override;
  std::string GetPersistenceId() override;
  bool IsRestoringPreviousState() override;
  void AddObserver(BrowserObserver* observer) override;
  void RemoveObserver(BrowserObserver* observer) override;
  void AddBrowserRestoreObserver(BrowserRestoreObserver* observer) override;
  void RemoveBrowserRestoreObserver(BrowserRestoreObserver* observer) override;
  void VisibleSecurityStateOfActiveTabChanged() override;

 private:
  // For creation.
  friend class Browser;

  friend BrowserImpl* CreateBrowserForAndroid(
      ProfileImpl*,
      const std::string&,
      const base::android::JavaParamRef<jobject>&);

  explicit BrowserImpl(ProfileImpl* profile);

  void RestoreStateIfNecessary(const PersistenceInfo& persistence_info);

  TabImpl* AddTab(std::unique_ptr<Tab> tab);
  std::unique_ptr<Tab> RemoveTab(Tab* tab);

  // Returns the path used by |browser_persister_|.
  base::FilePath GetBrowserPersisterDataPath();

  void OnWebPreferenceChanged(const std::string& pref_name);

  base::android::ScopedJavaGlobalRef<jobject> java_impl_;
  base::ObserverList<BrowserObserver> browser_observers_;
  base::ObserverList<BrowserRestoreObserver> browser_restore_observers_;
  const raw_ptr<ProfileImpl> profile_;
  std::vector<std::unique_ptr<Tab>> tabs_;
  raw_ptr<TabImpl> active_tab_ = nullptr;
  std::string persistence_id_;
  std::unique_ptr<BrowserPersister> browser_persister_;
  base::OnceClosure visible_security_state_changed_callback_for_tests_;
  PrefChangeRegistrar profile_pref_change_registrar_;
  std::string package_name_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_IMPL_H_
