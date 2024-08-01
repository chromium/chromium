# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import models


# Model definitions for ukm.xml content
_OBSOLETE_TYPE = models.TextNodeType('obsolete')
_OWNER_TYPE = models.TextNodeType('owner', single_line=True)
_SUMMARY_TYPE = models.TextNodeType('summary')

# A key for sorting XML nodes by the value of |attribute|.
_LOWERCASE_FN = lambda attribute: (lambda node: node.get(attribute).lower())
# A constant function as the sorting key for nodes whose orderings should be
# kept as given in the XML file within their parent node.
_KEEP_ORDER = lambda node: 1
