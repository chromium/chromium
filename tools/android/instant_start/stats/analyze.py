# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Library for analyzing benchmark results for Instant start."""

import pandas
from scipy import stats


def print_report(runs, model, control='control', experiment='experiment'):
    """Print stats of A/B testing"""
    all_df = pandas.DataFrame(runs, dtype=float)
    report = pandas.DataFrame(
        columns=['Median', 'Diff with control', 'p-value'])
    for metric in sorted(set(all_df['metric_name'])):
        mdf = all_df[all_df['metric_name'] == metric]
        df = pandas.DataFrame()
        for variant in sorted(set(all_df['variant_name'])):
            df[variant] = mdf[mdf['variant_name'] == variant]\
                .value.reset_index(drop=True)

        diff_df = pandas.DataFrame()
        diff_df = df[experiment] - df[control]
        n = len(diff_df)

        row = {}
        row['Median'] = '%.1fms' % df[experiment].median()
        row['Diff with control'] = '%.1fms (%.2f%%)' % (
            diff_df.median(), diff_df.median() / df[experiment].median() * 100)
        row['p-value'] = '%f' % (stats.ttest_rel(df[experiment],
                                                 df[control])[1])
        report = report.append(pandas.Series(data=row, name=metric))
    print('%d samples on %s' % (n, model))
    print(report.sort_values(by='p-value'))
