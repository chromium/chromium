# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Histogram metadata format specification.

This should be kept in sync with the protobuf specs in
histogram_metadata.proto (Google internal). This format
can be converted to protobuf using json_format.ParseDict:
https://googleapis.dev/python/protobuf/latest/google/protobuf/json_format.html

For example:
{
    "histogramName": "Test.Histogram",
    "nameHash": 3306893902404473075,
    "descriptions": [{"source": "SUMMARY", "content": "A histogram!"}],
    "enumDetails": {
        "name": "WhatHappened",
        "buckets": [
            {"key": 0, "label": "This", "summary": "THIS"},
            {"key": 1, "label": "That", "summary": "THAT"}
        ]
    },
    "owners": [{"email": "a@b.c"}],
    "units": "stuff",
    "obsoletionMessage": "No longer useful.",
    "expiresAfter": "never",
    "estimatedExpiryDate": "never",
    "components": ['foo', 'bar'],
    "improvement": "NEITHER_IS_BETTER",
}
"""

from typing import Literal, TypedDict


class Description(TypedDict):
  """Description of a histogram.

  Attributes:
    source: The source type. This is taken from an enum, but all values
      except for "SUMMARY" (with value 1) are deprecated.
    content: The content of the description.
  """
  source: Literal['SUMMARY']
  content: str


class BucketMetadata(TypedDict, total=False):
  """Metadata for an enum bucket.

  Attributes:
    key: The value to identify this bucket. For enumerated histograms that
      are not sparse, this is read from enums.xml.
    label: The label to describe this bucket.
    summary: An optional longer description of the bucket.
  """
  key: int
  label: str
  summary: str


class EnumDetails(TypedDict, total=False):
  """Metadata for a sequence of enum buckets.

  Attributes:
    buckets: The enum buckets.
    name: Name for this enum.
    summary: Description of this enum.
  """
  buckets: list[BucketMetadata]
  name: str
  summary: str


class Owner(TypedDict):
  """Metadata about a histogram owner.

  Attributes:
    email: The email of the owner.
  """
  email: str


# Which direction of movement is better for the histogram values.
ImprovementDirection = Literal['UNKNOWN', 'HIGHER_IS_BETTER', 'LOWER_IS_BETTER',
                               'NEITHER_IS_BETTER']


class HistogramMetadataJSON(TypedDict, total=False):
  """Histogram metadata as a nested JSON dictionary.

  Attributes:
    histogramName: Name of the histogram.
    nameHash: Hash of the histogram name.
    descriptions: Contains the histogram summary.
    enumDetails: Details about the enum if applicable.
    owners: The histogram owners.
    units: The units for the histogram values.
    obsoletionMessage: Obsoletion message if applicable.
    expiresAfter: Scheduled expiry.
    estimatedExpiryDate: Estimated expiry date.
    components: The components that this histogram applies to.
    improvement: Spec for the desired movement in values.
  """
  histogramName: str
  nameHash: int
  descriptions: list[Description]
  enumDetails: EnumDetails
  owners: list[Owner]
  units: str
  obsoletionMessage: str
  expiresAfter: str
  estimatedExpiryDate: str
  components: list[str]
  improvement: ImprovementDirection
