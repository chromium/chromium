#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import pandas as pd
import numpy

from scipy import stats as scipy_stats


def get_diamond_string(diamond_count: int):
  if diamond_count == 0:
    return "~"
  elif diamond_count == 1:
    return "◆"
  elif diamond_count == 2:
    return "◆◆"
  elif diamond_count == 3:
    return "◆◆◆"
  elif diamond_count == 4:
    return "◆◆◆◆"


def get_diamonds_count(significance: pd.DataFrame):
  """
  This function emulates the "diamond" significance representation
  that is familiar to UMA users.
  """

  assert (not (significance > 1).any().any())
  assert (not (significance < 0).any().any())

  # Avoid log10(0) which is undefined.
  significance = numpy.clip(significance, 0, 0.999999)

  # scipy_stats.norm.cdf(1.96) = 0.975 and we're interested in 2 tail
  # test. 1.96 gives a 0.05 p-value. Multiply by 2 here to correct.
  p_value = (1 - significance) * 2

  # floor() to avoid exaggerating results and to round.
  # absolute() to make the result positive.
  log_p_value = numpy.floor(numpy.absolute(numpy.log10(p_value)))

  # Clip because 4 diamond is the max no matter the p-value.
  return numpy.clip(log_p_value, 0, 4)


def compute_mean_and_stderr(summary_path: str):
  df = pd.read_csv(summary_path)

  # skipna because no line has as all measurements. This is because of the
  # different sampling rates of the data sources in power_sampler
  # and power_metrics.
  means = df.mean(skipna=True)

  # Calculate the standard error of each column.
  stderrs = df.std(skipna=True) / numpy.sqrt(df.count())
  stats = means.to_frame().join(stderrs.to_frame(),
                                lsuffix='mean',
                                rsuffix='stderr')
  stats = stats.rename(columns={"0mean": "mean", "0stderr": "stderr"})

  return stats


def percent_difference(first_value: pd.DataFrame, second_value: pd.DataFrame):
  """
  Returns the comparative percentage difference between two
  values/columns.

  The result is to be read as :
    |second_value| is X% smaller/larger than |first_value|.

  Ex: percent_difference(20, 10) --> -50
  Ex: percent_difference(10, 50) --> 500
  """

  return ((second_value - first_value) / first_value) * 100


def compare(data_dir: str, baseline_summary: str, alternative_summary: str):
  """Open two summary files and compare their values. Saves the results
  in data_dir.

  Args:
    data_dir: The directory to save the comparison csv in.
    baseline_summary: summary.csv for the baseline.
    alternative_summary: summary.csv for the comparison.
  """

  # Get names of the browsers being compared from the paths.
  baseline_name = os.path.basename(
      os.path.dirname(baseline_summary)).split("_")[0]
  alternative_name = os.path.basename(
      os.path.dirname(alternative_summary)).split("_")[0]

  all_stats = []

  # Extract mean and std values for each column of |summary| into a new
  # dataframe.
  baseline_stats = compute_mean_and_stderr(baseline_summary)
  alternative_stats = compute_mean_and_stderr(alternative_summary)

  # Join the calculated values for both browsers into a single dataframe.
  comparison_summary = baseline_stats.join(alternative_stats,
                                           lsuffix=f"_{baseline_name}",
                                           rsuffix=f"_{alternative_name}")

  # Calculate the difference in percent between the baseline and comparison.
  comparison_summary["difference"] = percent_difference(
      baseline_stats["mean"], alternative_stats["mean"])

  # See https://www.cliffsnotes.com/study-guides/statistics/univariate-inferential-tests/two-sample-z-test-for-comparing-two-means
  comparison_summary["z_score"] = (baseline_stats["mean"] -
                                   alternative_stats["mean"]) / numpy.sqrt(
                                       pow(baseline_stats["stderr"], 2) +
                                       pow(alternative_stats["stderr"], 2))

  # See  https://machinelearningmastery.com/critical-values-for-statistical-hypothesis-testing/
  comparison_summary["significance_level"] = scipy_stats.norm.cdf(
      abs(comparison_summary["z_score"]))

  diamond_count = get_diamonds_count(comparison_summary["significance_level"])
  comparison_summary["diamonds"] = diamond_count.apply(get_diamond_string)

  # Drop results for which comparing the mean makes no sense.
  comparison_summary = comparison_summary.drop([
      'battery_max_capacity', 'battery_current_capacity', 'sample_time',
      'elapsed_ns'
  ])

  # Display and save results.
  logging.info(comparison_summary)
  comparison_summary.to_csv(f"{data_dir}/comparison_summary.csv")


def main():
  parser = argparse.ArgumentParser(
      description='Compares two summary files for analysis.')
  parser.add_argument("--output_dir",
                      help="Directory where to write the comparison file.",
                      required=True)
  parser.add_argument("--baseline_dir",
                      help="Directory containing the baseline benchmark data.",
                      required=True)
  parser.add_argument(
      "--alternative_dir",
      help="Directory containing the alternative benchmark data.",
      required=True)
  parser.add_argument('--verbose',
                      action='store_true',
                      help='Print verbose output.')
  args = parser.parse_args()

  if args.verbose:
    log_level = logging.DEBUG
  else:
    log_level = logging.INFO
  logging.basicConfig(format='%(levelname)s: %(message)s', level=log_level)

  baseline_summary_path = os.path.join(args.baseline_dir, "summary.csv")
  alternative_summary_path = os.path.join(args.alternative_dir, "summary.csv")
  summaries = [baseline_summary_path, alternative_summary_path]

  for summary in summaries:
    if not os.path.isfile(summary):
      logging.error(f"summary.csv missing in {summary}.")
      sys.exit(-1)

  compare(args.output_dir, summaries[0], summaries[1])


if __name__ == "__main__":

  # Avoid scientific notation when printing numbers.
  pd.options.display.float_format = '{:.6f}'.format

  main()
