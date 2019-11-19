# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from core.external_modules import pandas

HIGHEST_VALID_NAN_RATIO = 0.5


def CalculateDistances(
  input_dataframe,
  metric,
  normalize=False,
  output_path=None):
  """Calculates the distances of stories.

  If normalize flag is set the values are first normalized using min-max
  normalization. Then the similarity measure between every two stories is
  calculated using pearson correlation.

  Args:
    input_dataframe: A dataframe containing a list of records
    having (test_case, commit_pos, bot, value).
    metric: String containing name of the metric.
    normalize: A flag to determine if normalization is needed.
    output_path: Path to write the calculated distances.

  Returns:
    A dataframe containing the distance matrix of the stories.
  """
  input_by_story = input_dataframe.groupby('test_case')['value']
  total_values_per_story = input_by_story.size()
  nan_values_per_story = input_by_story.apply(lambda s: s.isna().sum())
  should_keep = nan_values_per_story < (
    total_values_per_story * HIGHEST_VALID_NAN_RATIO)
  valid_stories = total_values_per_story[should_keep].index

  filtered_dataframe = input_dataframe[
    input_dataframe['test_case'].isin(valid_stories)]

  temp_df = filtered_dataframe.copy()

  if normalize:
    # Min Max normalization
    grouped = temp_df.groupby(['bot', 'test_case'])['value']
    min_value = grouped.transform('min')
    max_value = grouped.transform('max')
    temp_df['value'] = temp_df['value'] / (1 + max_value - min_value)

  distances = pandas.DataFrame()
  grouped_temp = temp_df.groupby(temp_df['bot'])
  for _, group in grouped_temp:
    sample_df = group.pivot(index='commit_pos', columns='test_case',
      values='value')

    if distances.empty:
      distances = 1 - sample_df.corr(method='pearson')
    else:
      distances = distances.add(1 - sample_df.corr(method='pearson'),
        fill_value=0)

  if output_path is not None:
    if not os.path.isdir(output_path):
      os.makedirs(output_path)
    distances.to_csv(
      os.path.join(output_path, metric + '_distances.csv')
    )

  return distances
