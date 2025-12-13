# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Model objects for dkm.xml contents."""

import private_metrics_model_shared

DKM_XML_TYPE = private_metrics_model_shared.create_event_based_document_type(
    'dkm-configuration')


def prettify_xml(original_xml):
  """Parses the original xml and return a pretty printed version.

  Args:
    original_xml: A string containing the original xml file contents.

  Returns:
    A pretty-printed xml string, or None if the config contains errors.
  """
  config = DKM_XML_TYPE.Parse(original_xml)
  return DKM_XML_TYPE.PrettyPrint(config)
