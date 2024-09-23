// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @name Find all Functions in target.
 * @description Finds all Functions in the given target.
                (This is a trivial query intended to be used for test
                purposes only.)
 *              but not immediately followed by a return statement.
 * @kind problem
 * @problem.severity recommendation
 * @id cpp/hello-world
 */

import cpp

from Function f
select f
