// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libxml/xmlversion.h>

// Basic test that libxml features that are unused in Chromium remain disabled.

#ifdef LIBXML_AUTOMATA_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_C14N_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_CATALOG_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_DEBUG_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_DOCB_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_EXPR_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_LEGACY_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_MODULES_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_REGEXP_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_SCHEMAS_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_SCHEMATRON_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_VALID_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_XINCLUDE_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_XPTR_ENABLED
static_assert(false);
#endif

#ifdef LIBXML_ZLIB_ENABLED
static_assert(false);
#endif
