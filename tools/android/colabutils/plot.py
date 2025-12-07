# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import seaborn as sns
import matplotlib.pyplot as plt
import matplotlib.lines as mlines
import matplotlib.colors as mcolors


def stripplot(ax,
              data,
              x_key,
              y_key,
              x_label=None,
              y_label=None,
              title=None,
              units='',
              color_palette='Set2'):
    """Generates a strip plot with overlaid mean and confidence intervals.

    This function creates a seaborn stripplot to visualize the distribution of
    data points, and overlays a pointplot to show the mean and 95% confidence
    interval for each group. It also annotates the mean value for each group
    on the plot.

    Args:
        ax: The matplotlib axes object to draw the plot on.
        data: A pandas DataFrame containing the data to plot.
        x_key: The name of the column in `data` to group by for the x-axis.
        y_key: The name of the column in `data` for the y-axis values.
        x_label: Optional label for the x-axis. If None, `x_key` is used.
        y_label: Optional label for the y-axis. If None, `y_key` is used.
        title: Optional title for the plot.
        units: Optional string to append to the mean value labels (e.g., 'ms').
        color_palette: The seaborn color palette to use for the plot.

    Usage::

        import pandas as pd
        import matplotlib.pyplot as plt
        import colabutils

        # Sample data
        df = pd.DataFrame({
            'version': ['A', 'A', 'A', 'B', 'B', 'B'],
            'latency': [100, 105, 102, 110, 112, 115]
        })

        # Create plot
        sns.set_theme(style='darkgrid')
        _, axes = plt.subplots(1, 1, figsize=(20, 15))
        colabutils.plot.stripplot(ax=axes,
                        data=df,
                        x_key='version',
                        y_key='latency',
                        x_label='Version',
                        y_label='Latency (ms)',
                        title='Latency Comparison',
                        units='ms',
                        color_palette='Set1')
        plt.show()
    """
    sns.set_palette(color_palette)

    # Use seaborn.stripplot to show individual data points for each group.
    # `hue` is used to give each group a distinct color.
    sns.stripplot(x=x_key,
                  y=y_key,
                  data=data,
                  hue=x_key,
                  size=8,
                  alpha=0.6,
                  ax=ax,
                  legend=False)

    # Overlay a pointplot to display the mean and 95% confidence interval.
    # `join=False` prevents drawing lines between points of different groups.
    point_plot = sns.pointplot(x=x_key,
                               y=y_key,
                               data=data,
                               hue=x_key,
                               ax=ax,
                               join=False,
                               markers='d',
                               errorbar=('ci', 95),
                               capsize=0.05)

    # Get the unique groups to ensure a consistent order for colors and labels.
    unique_groups = data[x_key].unique()

    # Get the colors used by seaborn for each group to ensure consistent
    # coloring for the mean value labels.
    colors = sns.color_palette(color_palette, n_colors=len(unique_groups))

    # Manually create legend handles with circular markers in the palette color.
    legend_handles = [
        mlines.Line2D([], [],
                      color=colors[i],
                      marker='o',
                      linestyle='None',
                      markersize=8,
                      label=group) for i, group in enumerate(unique_groups)
    ]
    ax.legend(handles=legend_handles, title=x_label)

    # Add data labels for the mean values with corresponding colors
    for i, group in enumerate(unique_groups):
        mean_value = data[data[x_key] == group][y_key].mean()
        # Darken the color for the text annotation to improve contrast and
        # readability against the plot background.
        r, g, b, a = mcolors.to_rgba(colors[i], alpha=1.0)
        darker_color = (r * 0.6, g * 0.6, b * 0.6, a)
        # Add a text annotation for the mean. A small horizontal offset is added
        # to prevent the label from overlapping with the point marker.
        ax.text(i + 0.1,
                mean_value,
                f'{mean_value:.0f}{units}',
                ha='left',
                va='center',
                color=darker_color,
                fontweight='bold')

    ax.set_ylabel(y_label or y_key)
    ax.set_xlabel(x_label or x_key)
    if title:
        ax.set_title(title)
