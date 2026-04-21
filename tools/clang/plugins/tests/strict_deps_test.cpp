// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "strict_deps_test.h"

#include <atomic>  // Simulate header in system include missing from modulemap
#include <not_in_modulemap>  // Header in sysroot missing from modulemap
#include <private>  // Simulate header in sysroot private in the modulemap
#include <string>   // OK (from system include)
#include <valid>    // OK (from sysroot)

#include "direct.h"    // OK (in deps)
#include "impl_dep.h"  // OK (in implementation deps)
#include "indirect.h"  // Simulate transitive dep on module defining indirect.h
#include "missing_dep.h"        // Missing dep on module defining missing_dep.h.
#include "undeclared_header.h"  // Simulate header file missing from BUILD.gn.
