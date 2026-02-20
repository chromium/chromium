This dir contains a schedule file per available benchmark.
Each line contains a bot_platform name and the number of repeats
(use 1 by default). Additional columns map to non-default kwargs
on the configs (telemetry, executable, crossbench)

# Example:
Example contents for `speedometer2.csv`:
```
bot,repeats,shards
linux-perf-pgo,1,1
```

For more complex sharing / repeat configurations see
`tools/perf/cross_device_test_config.py`.