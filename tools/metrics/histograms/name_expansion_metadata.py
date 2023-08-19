# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Data structures to expand patterned/suffixed histogram names.

See here for specifications:
https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/histograms/histogram_configuration_model.py
"""

from typing import TypedDict


class Variant(TypedDict, total=False):
  """Representation of a histograms.xml <variant> tag for a patterned histogram.

  See _VARIANT_TYPE in histogram_configuration_model.py for specifications.

  For example:
    <variant
        name="TOSAccepted"
        summary="Terms of service were accepted.">
      <obsolete>Removed in M90.</obsolete>
      <owner>abc@google.com</owner>
      <owner>xyz@chromium.org</owner>
    </variant>

  This would be represented as:
    Variant(
      name='TOSAccepted',
      summary='Terms of service were accepted.',
      obsolete='Removed in M90.',
      owners=['abc@google.com', 'xyz@chromium.org'],
    )

  Attributes:
    name: Name attribute of the variant.
    summary: Summary attribute of the variant.
    obsolete: Optional <obsolete> text content.
    owners: List of <owner> text content.
  """
  name: str
  summary: str
  obsolete: str | None
  owners: list[str]


class Token(TypedDict, total=False):
  """Representation of a histograms.xml <token> tag for a patterned histogram.

  See _TOKEN_TYPE in histogram_configuration_model.py for specifications.

  For example:
    <token key="UserChoice" variants="TOS">
      <variant name="TOSIgnored">
    <token/>

  Note that the variants attribute refers to a Variants instance. The totality
  of variants is taken from that <variants> block as well as child <variant>
  nodes.

  That example would be represented as:
    Token(
        key='UserChoice',
        variants=[
            Variant(name='TOSAccepted'),
            Variant(name='TOSDeclined'),
            Variant(name='TOSIgnored'),
        ],
    )

  Attributes:
    key: The key attribute that is used in histogram names/summaries as '{key}'.
    variants: The variants whose names are substituted for '{key}'.
  """
  key: str
  variants: list[Variant]


# Assignment of a variant for each token in a patterned histogram.
TokenAssignment = dict[str, Variant]