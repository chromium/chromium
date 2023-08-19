// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_CA_TRANSACTION_OBSERVER_H_
#define UI_ACCELERATED_WIDGET_MAC_CA_TRANSACTION_OBSERVER_H_

#include <set>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/time/time.h"

#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"

namespace ui {

// CATransactionCoordinator is an interface to undocumented macOS APIs which
// run callbacks at different stages of committing a CATransaction to the
// window server. There is no guarantee that it will call registered observers
// at all.
//
// - Pre-commit: After all outstanding CATransactions have committed and after
//   layout, but before the new layer tree has been sent to the window server.
//   Safe to block here waiting for drawing/layout in other processes (but
//   you're on the main thread, so be reasonable).
//
// - Post-commit: After the new layer tree has been sent to the server but
//   before the transaction has been finalized. In post-commit, the screen area
//   occupied by the window and its shadow are frozen, so it's important to
//   block as briefly as possible (well under a frame) or else artifacts will
//   be visible around affected windows if screen content is changing behind
//   them (think resizing a browser window while a video plays in a second
//   window behind it). This is a great place to call -[CATransaction commit]
//   (or otherwise flush pending changes to the screen) in other processes,
//   because their updates will appear atomically.
//
// It has been observed that committing a CATransaction in the GPU process
// which changes which IOSurfaces are assigned to layers' contents is *faster*
// if done during the browser's post-commit phase vs. its pre-commit phase.

class ACCELERATED_WIDGET_MAC_EXPORT CATransactionCoordinator {
 public:
  class PreCommitObserver {
   public:
    virtual bool ShouldWaitInPreCommit() = 0;
    virtual base::TimeDelta PreCommitTimeout() = 0;
  };

  // PostCommitObserver sub-classes must communicate with the IO thread. The
  // CATransactionCoordinator will retain registered observers to ensure that
  // they are not deleted while registered.
  class PostCommitObserver
      : public base::RefCountedThreadSafe<PostCommitObserver> {
   public:
    virtual void OnActivateForTransaction() = 0;
    virtual void OnEnterPostCommit() = 0;
    virtual bool ShouldWaitInPostCommit() = 0;

   protected:
    virtual ~PostCommitObserver() = default;

   private:
    friend class base::RefCountedThreadSafe<PostCommitObserver>;
  };

  static CATransactionCoordinator& Get();

  CATransactionCoordinator(const CATransactionCoordinator&) = delete;
  CATransactionCoordinator& operator=(const CATransactionCoordinator&) = delete;

  void Synchronize();
  void DisableForTesting() { disabled_for_testing_ = true; }

  void AddPreCommitObserver(PreCommitObserver*);
  void RemovePreCommitObserver(PreCommitObserver*);

  void AddPostCommitObserver(scoped_refptr<PostCommitObserver>);
  void RemovePostCommitObserver(scoped_refptr<PostCommitObserver>);

 private:
  friend class base::NoDestructor<CATransactionCoordinator>;
  CATransactionCoordinator();
  ~CATransactionCoordinator();

  void SynchronizeImpl();
  void PreCommitHandler();
  void PostCommitHandler();

  bool active_ = false;
  bool disabled_for_testing_ = false;
  base::ObserverList<PreCommitObserver>::Unchecked pre_commit_observers_;
  std::set<scoped_refptr<PostCommitObserver>> post_commit_observers_;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_CA_TRANSACTION_OBSERVER_H_
