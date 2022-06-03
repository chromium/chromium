# TBMv3 Metric Validators

The validate_tbmv3_metric tool works with two types of validators: A simple
config validator, or a script validator.

## Simple config validator

In `simple_configs.pyl`, you can define a validator. This is the syntax:

```text
{
  '<validator_name>': {
    'v2_metric': '<tbmv2_metric_name>',
    'v3_metric': '<tbmv3_metric_name>',
    'float_precision': <optional>
    'histogram_mappings': {
      '<v2_histogram_1>': '<v3_histogram_1>',
      '<v2_histogram_2>': ('<v3_histogram_2>', <precision>),
      ...
    },
  },
}
```

For each histogram mapping defined, the validator will check all the major
statistics (mean, sum, max, min, count), and all sample values match up. The
default precision for float comparison is 1e3. You can override the precision
for all histograms by providing 'float_precision', or for a specific histogram
by using the `'<v2_histogram_2>': ('<v3_histogram_2>', <precision>)` syntax.

## Script validator

If you want to do more complex checks (e.g. you don't want to check all the
samples values are equal because some samples were dropped in the tbmv2 metric
etc), you can opt for a script validator.

To write a script validator, write a python file in this directory with a
CompareHistogram method. The method takes in one argument: test_ctx.

test_ctx is an instance of unittest.TestCase, so it has all the familiar assert
methods. Additionally, it also has `RunTBMv2`, and `RunTBMv3` methods to run the
metrics you want to run on the current trace. They return histogram set obecjts;
see `third_party/catapult/tracing/tracing/value/histogram_set.py` for the API.
You can whatever histogram you want from the histogram_set objects and write
your custom checks.

The functions in the `utils` module assert common patterns.
