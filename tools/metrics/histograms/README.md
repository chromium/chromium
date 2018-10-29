# Histogram Guidelines

This document gives the best practices on how to use histograms in code and how
to document the histograms for the dashboards.  There are three general types
of histograms: enumerated histograms, count histograms (for arbitrary numbers),
and sparse histograms (for anything when the precision is important over a wide
range and/or the range is not possible to specify a priori).

[TOC]

## Naming Your Histogram

Histogram names should be in the form Group.Name or Group.Subgroup.Name,
etc., where each group organizes related histograms.

## Coding (Emitting to Histograms)

Generally you'll be best served by using one of the macros in
[histogram_macros.h](https://cs.chromium.org/chromium/src/base/metrics/histogram_macros.h)
if possible.

### Don't Use the Same Histogram Logging Call in Multiple Places

These logging macros and functions have long names and sometimes include extra
parameters (defining the number of buckets for example).  Use a helper function
if possible.  This leads to shorter, more readable code that's also more
resilient to problems that could be introduced when making changes.  (One could,
for example, erroneously change the bucketing of the histogram in one call but
not the other.)

### Use Fixed Strings When Using Histogram Macros

When using histogram macros (calls such as `UMA_HISTOGRAM_ENUMERATION`), you're
not allow to construct your string dynamically so that it can vary at a
callsite.  At a given callsite (preferably you have only one), the string should
be the same every time the macro is called.  If you need to use dynamic names,
use the functions in histogram_functions.h instead of the macros.

### Don't Use Same String in Multiple Places

If you must use the histogram name in multiple places, use a compile-time
constant of appropriate scope that can be referenced everywhere. Using inline
strings in multiple places can lead to errors if you ever need to revise the
name and you update one one location and forget another.

### Efficiency

Don't worry about it.  In general, the histogram code is highly optimized.  Do
not be concerned about the processing cost of emitting to a histogram (unless
you're using [sparse histograms](#When-To-Use-Sparse-Histograms)).

## Picking Your Histogram Type

### Directly Measure What You Want

Measure exactly what you want, whether that's time used for a function call,
number of bytes transmitted to fetch a page, number of items in a list, etc.
Do not assume you can calculate what you want from other histograms.  Most of
the ways to do this are incorrect.  For example, if you want to know the time
taken by a function that all it does is call two other functions, both of which
are have histogram logging, you might think you can simply add up those
the histograms for those functions to get the total time.  This is wrong.
If we knew which emissions came from which calls, we could pair them up and
derive the total time for the function.  However, histograms entries do not
come with timestamps--we pair them up appropriately.  If you simply add up the
two histograms to get the total histogram, you're implicitly assuming those
values are independent, which may not be the case.  Directly measure what you
care about; don't try to derive it from other data.

### Enum Histograms

Enumerated histogram are most appropriate when you have a list of connected /
related states that should be analyzed jointly.  For example, the set of
actions that can be done on the New Tab Page (use the omnibox, click a most
visited tile, click a bookmark, etc.) would make a good enumerated histogram.
If the total count of your histogram (i.e. the sum across all buckets) is
something meaningful--as it is in this example--that is generally a good sign.
However, the total count does not have to be meaningful for an enum histogram
to still be the right choice.

If few buckets will be emitted to, consider using a [sparse
histogram](#When-To-Use-Sparse-Histograms).

You may append to your enum if the possible states/actions grows.  However, you
should not reorder, renumber, or otherwise reuse existing values. Definitions
for enums recorded in histograms should be prefixed by the following warning:
```c++
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
```

The enums themselves should have explicit enumerator values set (`= 0`, `= 1`,
`= 2`), to make it clear that the actual values are important and to make it
easy to match the values between the C++ definition and
[histograms.xml](./histograms.xml).

For new enums used in histograms, prefer using an enum class with a kMaxValue
element, like this:
```c++ {.good}
enum class NewTabPageAction {
  kUseOmnibox = 0,
  kClickTitle = 1,
  kOpenBookmark = 2,
  kMaxValue = kOpenBookmark,
};
```
`kMaxValue` is a special enumerator value that shares the value of the highest
enumerator: this should be done by assigning it the name of the enumerator with
the highest explicit integral value. There is a presubmit check which will
enforce this semantic. Enums defined this way have better type checking support
from the compiler, allow inferring kMaxValue from the type, and allow `switch`
statements over them will not need to handle an otherwise unused sentinel value.

Enumerators defined in this way should be recorded using the two argument
version of `UMA_HISTOGRAM_ENUMERATION`:
```
UMA_HISTOGRAM_ENUMERATION("NewTabPageAction", action);
```
which automatically deduces the range of the enum from `kMaxValue`.

If you need to record a histogram based on an enum without kMaxValue, you can
use the three argument version, which takes the number of buckets as the argument, e.g:
```c++
UMA_HISTOGRAM_ENUMERATION("NewTabPageAction", action,
                          NewTabPageAction_MaxValue + 1);
```

This is often seen with enums defined with a sentinal enumerator value at the
end, relying on the compiler to keep the value up to date:
```c++
enum class NewTabPageAction {
  kUseOmnibox = 0,
  kClickTitle = 1,
  kOpenBookmark = 2,
  kCount,
};

UMA_HISTOGRAM_ENUMERATION("NewTabPageAction", action, NewTabPageAction::kCount);
```

Finally, if your enum histogram has a catch-all / miscellaneous bucket, put that
bucket first (`= 0`).  This will make the bucket easy to find on the dashboard
if later you add additional buckets to your histogram.

### Flag Histograms

When adding a new flag in
[about_flags.cc](../../../chrome/browser/about_flags.cc), you need to add a
corresponding entry to [enums.xml](./enums.xml). This will be automatically
verified by the `AboutFlagsHistogramTest` unit test.

To add a new entry:

1. Edit [enums.xml](./enums.xml), adding the feature to the `LoginCustomFlags`
   enum section, with any unique value (just make one up, although whatever it
   is needs to appear in sorted order; `pretty_print.py` will do this for you).
2. Build `unit_tests`, then run `unit_tests
   --gtest_filter='AboutFlagsHistogramTest.*'` to compute the correct value.
3. Update the entry in [enums.xml](./enums.xml) with the correct value, and move
   it so the list is sorted by value (`pretty_print.py` will do this for you).
4. Re-run the test to ensure the value and ordering are correct.

You can also use `tools/metrics/histograms/validate_format.py` to check the
ordering (but not that the value is correct).

Don't remove entries when removing a flag; they are still used to decode data
from previous Chrome versions.

### Count Histograms

[histogram_macros.h](https://cs.chromium.org/chromium/src/base/metrics/histogram_macros.h)
provides macros for some common count types such as memory or elapsed time, in
addition to general count macros.  These have reasonable default values; you
will not often need to choose number of buckets or histogram min.  You still
will need to choose the histogram max (use the advice below).

If none of the default macros work well for you, please thoughtfully choose
a min, max, and bucket count for your histogram using the advice below.

#### Count Histograms: Choosing Min and Max

For histogram max, choose a value so that very few emission to the histogram
will exceed the max.  If many emissions hit the max, it can be difficult to
compute statistics such as average.  One rule of thumb is at most 1% of samples
should be in the overflow bucket.  This allows analysis of the 99th percentile.
Err on the side of too large a range versus too short a range.  (Remember that
if you choose poorly, you'll have to wait for another release cycle to fix it.)

For histogram min, if you care about all possible values (zero and above),
choose a min of 1.  (All histograms have an underflow bucket; emitted zeros
will go there.  That's why a min of 1 is appropriate.)  Otherwise, choose the
min appropriate for your particular situation.

#### Count Histograms: Choosing Number of Buckets

Choose the smallest number of buckets that will get you the granularity you
need.  By default count histograms bucket sizes scale exponentially so you can
get fine granularity when the numbers are small yet still reasonable resolution
for larger numbers.  The macros default to 50 buckets (or 100 buckets for
histograms with wide ranges) which is appropriate for most purposes.  Because
histograms pre-allocate all the buckets, the number of buckets selected
directly dictate how much memory is used.  Do not exceed 100 buckets without
good reason (and consider whether [sparse histograms](#When-To-Use-Sparse-
Histograms) might work better for you in that case--they do not pre- allocate
their buckets).

### Timing Histograms

You can easily emit a time duration (time delta) using UMA_HISTOGRAM_TIMES,
UMA_HISTOGRAM_MEDIUM_TIMES, and UMA_HISTOGRAM_LONG_TIMES macros, and their
friends, as well as helpers such as SCOPED_UMA_HISTOGRAM_TIMER. Many timing
histograms are used for performance monitoring; if this is the case for you,
please read [this document about how to structure timing histograms to make
them more useful and
actionable](https://chromium.googlesource.com/chromium/src/+/lkgr/docs/speed/diagnostic_metrics.md).

### Percentage or Ratio Histograms

You can easily emit a percentage histogram using the
UMA_HISTOGRAM_PERCENTAGE macro provided in
[histogram_macros.h](https://cs.chromium.org/chromium/src/base/metrics/histogram_macros.h).
You can also easily emit any ratio as a linear histogram (for equally
sized buckets).

For such histograms, you should think carefully about _when_ the values are
emitted.  Normally, you should emit values periodically at a set time interval,
such as every 5 minutes.  Conversely, we strongly discourage emitting values
based on event triggers.  For example, we do not recommend recording a ratio
at the end of a video playback.

Why?  You typically cannot make decisions based on histograms whose values are
recorded in response to an event, because such metrics can conflate heavy usage
with light usage.  It's easier to reason about metrics that route around this
source of bias.

Many developers have been bitten by this.  For example, it was previously common
to emit an actions-per-minute ratio whenever Chrome was backgrounded.
Precisely, these metrics computed the number of uses of a particular action
during a Chrome session, divided by length of time Chrome had been open.
Sometimes, the recorded rate was based on a short interaction with Chrome – a
few seconds or a minute.  Other times, the recorded rate was based on a long
interaction, tens of minutes or hours.  These two situations are
indistinguishable in the UMA logs – the recorded values can be identical.

This inability to distinguish these two qualitatively different settings make
such histograms effectively uninterpretable and not actionable.  Emitting at a
regular interval avoids the issue.  Each value will represent the same amount of
time (e.g., one minute of video playback).

### Local Histograms

Histograms can be added via [Local macros](https://codesearch.chromium.org/chromium/src/base/metrics/histogram_macros_local.h).
These will still record locally, but will not be uploaded to UMA and will
therefore not be available for analysis. This can be useful for metrics only
needed for local debugging. We don't recommend using local histograms outside
of that scenario.

### Multidimensional Histograms

It is common to be interested in logging multidimensional data - where multiple
pieces of information need to be logged together. For example, a developer may
be interested in the counts of features X and Y based on whether a user is in
state A or B. In this case, they want to know the count of X under state A,
as well as the other three permutations.

There is no general purpose solution for this type of analysis. We suggest
using the workaround of using an enum of length MxN, where you log each unique
pair {state, feature} as a separate entry in the same enum. If this causes a
large explosion in data (i.e. >100 enum entries), a [sparse histogram](#When-To-Use-Sparse-Histograms)
may be appropriate. If you are unsure of the best way to proceed, please
contact someone from the OWNERS file.

## Histogram Expiry

Histogram expiry is specified by **'expires_after'** attribute in histogram
descriptions in histograms.xml. The attribute can be specified as date in
**YYYY-MM-DD** format or as Chrome milestone in **M**\*(e.g. M68) format. After
a histogram expires, it will not be recorded (nor uploaded to the UMA servers).
The code to record it becomes dead code, and should be removed from the
codebase along with marking the histogram definition as obsolete. However, if
histogram would remain useful, the expiration should be extended accordingly
before it becomes expired. If histogram you care about already expired, see
[Expired Histogram Whitelist](#Expired-histogram-whitelist).

For all the new histograms the use of expiry attribute will be strongly
encouraged and enforced by Chrome metrics team through reviews.

#### How to choose expiry for histograms

For new histograms if it is used for launching a project for which the timeline
is known then pick an expiry based on your project timeline. Otherwise, we
recommend choosing 3-6 months.

For already existing histograms here are different scenarios:

*   Owner moved to different project - find new owner
*   Owner doesn’t use it, team doesn’t use it - remove
*   Not in use now, but maybe useful in the far future - remove
*   Not in use now, but maybe useful in the near future - pick 3 months or 2
    milestone ahead
*   Actively in use now, useful for short term - pick 3-6 month or appropriate
    number of milestones ahead
*   Actively in use, seems useful for indefinite time - pick 1 year or more

### Expired histogram notifier

Expired histogram notifier will notify owners in advance by creating crbugs so
that the owners can extend the lifetime of the histogram if needed or deprecate
it. It will regularly check all the histograms in histograms.xml and will
determine expired histograms or histograms expiring soon. Based on that it will
create or update crbugs that will be assigned to histogram owners.

### Expired histogram whitelist

If a histogram expires but turns out to be useful, you can add histogram name
to the whitelist until the updated expiration date reaches to the stable
channel. For adding histogram to the whitelist, see internal documentation
[Histogram Expiry](https://goto.google.com/histogram-expiry-gdoc)

## Testing

Test your histograms using `chrome://histograms`.  Make sure they're being
emitted to when you expect and not emitted to at other times. Also check that
the values emitted to are correct.  Finally, for count histograms, make sure
that buckets capture enough precision for your needs over the range.

Pro tip: You can filter the set of histograms shown on `chrome://histograms` by
specifying a prefix. For example, `chrome://histograms/Extensions.Load` will
show only histograms whose names match the pattern "Extensions.Load*".

In addition to testing interactively, you can have unit tests examine the
values emitted to histograms.  See [histogram_tester.h](https://cs.chromium.org/chromium/src/base/test/metrics/histogram_tester.h)
for details.

## Interpreting the Resulting Data

The top of [go/uma-guide](http://go/uma-guide) has good advice on how to go
about analyzing and interpreting the results of UMA data uploaded by users.  If
you're reading this page, you've probably just finished adding a histogram to
the Chromium source code and you're waiting for users to update their version of
Chrome to a version that includes your code.  In this case, the best advice is
to remind you that users who update frequently / quickly are biased.  Best take
the initial statistics with a grain of salt; they're probably *mostly* right but
not entirely so.

## Revising Histograms

When changing the semantics of a histogram (when it's emitted, what buckets
mean, etc.), make it into a new histogram with a new name.  Otherwise the
"Everything" view on the dashboard will be mixing two different
interpretations of the data and make no sense.

## Deleting Histograms

Please delete the code that emits to histograms that are no longer needed.
Histograms take up memory.  Cleaning up histograms that you no longer care
about is good!  But see the note below on [Deleting Histogram Entries](#Deleting-Histogram-Entries).

## Documenting Histograms

Document histograms in [histograms.xml](./histograms.xml).  There is also a
[google-internal version of the file](http://go/chrome-histograms-internal) for
the rare case when the histogram is confidential (added only to Chrome code,
not Chromium code; or, an accurate description about how to interpret the
histogram would reveal information about Google's plans).

### Add Histogram and Documentation in the Same Changelist

If possible, please add the [histograms.xml](./histograms.xml) description in
the same changelist in which you add the histogram-emitting code.  This has
several benefits.  One, it sometimes happens that the
[histograms.xml](./histograms.xml) reviewer has questions or concerns about the
histogram description that reveal problems with interpretation of the data and
call for a different recording strategy.  Two, it allows the histogram reviewer
to easily review the emission code to see if it comports with these best
practices, and to look for other errors.

### Understandable to Everyone

Histogram descriptions should be roughly understandable to someone not familiar
with your feature.  Please add a sentence or two of background if necessary.

It is good practice to note caveats associated with your histogram in this
section, such as which platforms are supported (if the set of supported
platforms is surprising).  E.g., a desktop feature that happens not to be
logged on Mac.

### State When It Is Recorded

Histogram descriptions should clearly state when the histogram is emitted
(profile open? network request received? etc.).

### Owners

Histograms need to be owned by a person or set of people. These indicate who
the current experts on this metric are. Being the owner means you are
responsible for answering questions about the metric, handling the maintenance
if there are functional changes, and deprecating the metric if it outlives its
usefulness. The owners should be added in the original histogram description.
If you are using a metric heavily and understand it intimately, feel free to
add yourself as an owner. @chromium.org email addresses are preferred.


### Deleting Histogram Entries

Do not delete histograms from [histograms.xml](./histograms.xml).  Instead, mark
unused histograms as obsolete, annotating them with the associated date or
milestone in the obsolete tag entry.  If your histogram is being replaced by a
new version, we suggest noting that in the previous histogram's description.

A changelist that marks a histogram as obsolete should be reviewed by all
current owners.

Deleting histogram entries would be bad if someone to accidentally reused your
old histogram name and thereby corrupts new data with whatever old data is still
coming in.  It's also useful to keep obsolete histogram descriptions in
[histograms.xml](./histograms.xml) -- that way, if someone is searching for a
histogram to answer a particular question, they can learn if there was a
histogram at some point that did so even if it isn't active now.

### Histogram Suffixes

It is sometimes useful to record several closely related metrics, which measure
the same type of data, with some minor variations. It is often useful to use one
or more <histogram_suffixes> elements to save on redundant verbosity
in [histograms.xml](./histograms.xml). If a root `<histogram>` or a `<suffix>`
element is used only to construct a partial name, to be completed by further
suffixes, annotate the element with the attribute `base="true"`. This instructs
tools not to treat the partial base name as a distinct histogram. Note that
suffixes can be applied recursively.

You can also declare ownership of `<histogram_suffixes>`. If there's no owner
specified, the generated histograms will inherit owners from the parents.

### Enum labels

_All_ histograms, including boolean and sparse histograms, may have enum labels
provided via [enums.xml](./enums.xml). Using labels is encouraged whenever
labels would be clearer than raw numeric values.

## When To Use Sparse Histograms

Sparse histograms are well suited for recording counts of exact sample values
that are sparsely distributed over a large range.  They can be used with enums
as well as regular integer values. It is often valuable to provide labels in
[enums.xml](./enums.xml).

The implementation uses a lock and a map, whereas other histogram types use a
vector and no lock. It is thus more costly to add values to, and each value
stored has more overhead, compared to the other histogram types. However it
may be more efficient in memory if the total number of sample values is small
compared to the range of their values.

For more information, see [sparse_histograms.h](https://cs.chromium.org/chromium/src/base/metrics/sparse_histogram.h).
