# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Model objects for dwa.xml contents."""

import private_metrics_model_shared

DWA_XML_TYPE = private_metrics_model_shared.create_event_based_document_type(
    'dwa-configuration')


def PrettifyXML(original_xml):
  """Parses the original xml and return a pretty printed version.

  Args:
    original_xml: A string containing the original xml file contents.

  Returns:
    A pretty-printed xml string, or None if the config contains errors.
  """
  config = DWA_XML_TYPE.Parse(original_xml)
  return DWA_XML_TYPE.PrettyPrint(config)
