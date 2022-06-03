# Clustering Benchmark Stories

The code in this directory provides support for clustering and choosing
representatives for benchmarks.

Input needed for the clustering methods are:
1. Benchmark name
2. List of metrics to use.
    Clustering will be done once for each metric.
3. List of platforms to gather data from.
4. List of test-cases/story-names in the benchmark which should be clustered.
    If some stories are recognized as outliers, they can be removed from this
    list. The testcases can be provided as a story per line text file.
5. Maximum number of clusters.
    The actual number of clusters may be less than this number, as clusters
    with only one member are not presented as a cluster.
6. How many days of data history to be used for clustering

Examples of creating clusters:
```shell
python ./tools/perf/experimental/story_clustering/gather_historical_records_and_cluster_stories.py \
rendering.desktop \
--metrics frame_times thread_total_all_cpu_time_per_frame \
--platforms ChromiumPerf:mac-10_13_laptop_high_end-perf ChromiumPerf:mac-10_12_laptop_low_end-perf \
--testcases-path //tmp/story_clustering/rendering.desktop/test_cases.txt \
--days=100 \
--normalize \
--processes 20
```

```shell
python ./tools/perf/experimental/story_clustering/gather_historical_records_and_cluster_stories.py \
rendering.desktop \
--metrics frame_times thread_total_all_cpu_time_per_frame \
--platforms 'ChromiumPerf:Win 7 Nvidia GPU Perf' 'ChromiumPerf:Win 7 Perf' ChromiumPerf:win-10-perf \
--testcases-path //tmp/story_clustering/rendering.desktop/test_cases.txt \
--days=100 \
--normalize
```

```shell
python ./tools/perf/experimental/story_clustering/gather_historical_records_and_cluster_stories.py \
rendering.mobile \
--metrics frame_times thread_total_all_cpu_time_per_frame \
--platforms 'ChromiumPerf:Android Nexus5 Perf' 'ChromiumPerf:Android Nexus5X WebView Perf' \
'ChromiumPerf:Android Nexus6 WebView Perf' \
--testcases-path //tmp/story_clustering/rendering.mobile/test_cases.txt \
--days=100 \
--normalize
```

Results of the clustering will be written in `clusters.json` file, located in the output directory given to the script

If the script fails due to a "HTTP Error 429: Rate exceeded" error, try a smaller number for the `--processes` argument (defaulted to 20).

[Method explanation](https://goto.google.com/chrome-benchmark-clustering)