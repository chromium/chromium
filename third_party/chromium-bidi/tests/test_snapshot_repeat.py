# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pytest

# CQ checks new tests for flakiness by repeating them with pytest-repeat, which
# adds the repeat step (e.g. `1-2`) to the test name. Snapshots must be looked
# up by the test name without the step.


@pytest.mark.repeat(2)
def test_snapshot_ignores_repeat_step(snapshot):
    assert snapshot == "value"


@pytest.mark.repeat(2)
@pytest.mark.parametrize("value", ["first", "second"])
def test_snapshot_ignores_repeat_step_with_parameters(snapshot, value):
    assert snapshot == value
