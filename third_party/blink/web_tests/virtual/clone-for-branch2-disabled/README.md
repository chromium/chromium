# virtual/clone-for-branch2-disabled

This directory is for tests that need the cloneForBranch2 feature flag disabled
to test the the old behavior for fetch where teeing does not clone for the
second branch.
Tests under `virtual/clone-for-branch2-disabled` are run with
`--disable-features=ReadableStreamTeeCloneForBranch2`.
