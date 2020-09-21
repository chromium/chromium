# Histogram Guidelines

This document gives the best practices on how to use histograms in code and how
to document the histograms for the dashboards.  There are three general types
of histograms: [enumerated histograms](#Enum-Histograms),
[count histograms](#Count-Histograms) (for arbitrary numbers), and
[sparse histograms](#When-To-Use-Sparse-Histograms) (for anything when the
precision is important over a wide range and/or the range is not possible to
specify a priori).

[TOC]

## Naming Your Histogram

Histogram names should be in the form Group.Name or Group.Subgroup.Name,
etc., where each group organizes related histograms.

## Coding (Emitting to Histograms)

Prefer the helper functions defined in
[histogram_functions.h](https://cs.chromium.org/chromium/src/base/metrics/histogram_functions.h).
These functions take a lock and perform a map lookup, but the overhead is
generally insignificant. However, when recording metrics on the critical path
(e.g. called in a loop or logged multiple times per second), use the macros in
[histogram_macros.h](https://cs.chromium.org/chromium/src/base/metrics/histogram_macros.h)
instead. These macros cache a pointer to the histogram object for efficiency,
though this comes at the cost of increased binary size: 130 bytes/macro usage
sounds small but quickly adds up.

### Don't Use the Same Histogram Logging Call in Multiple Places

These logging macros and functions have long names and sometimes include extra
parameters (defining the number of buckets for example).  Use a helper function
if possible.  This leads to shorter, more readable code that's also more
resilient to problems that could be introduced when making changes.  (One could,
for example, erroneously change the bucketing of the histogram in one call but
not the other.)

### Use Fixed Strings When Using Histogram Macros

When using histogram macros (calls such as `UMA_HISTOGRAM_ENUMERATION`), you're
not allowed to construct your string dynamically so that it can vary at a
callsite.  At a given callsite (preferably you have only one), the string
should be the same every time the macro is called.  If you need to use dynamic
names, use the functions in histogram_functions.h instead of the macros.

### Don't Use Same String in Multiple Places

If you must use the histogram name in multiple places, use a compile-time
constant of appropriate scope that can be referenced everywhere. Using inline
strings in multiple places can lead to errors if you ever need to revise the
name and you update one one location and forget another.

### Efficiency

Generally, don't be concerned about the processing cost of emitting to a
histogram (unless you're using [sparse
histograms](#When-To-Use-Sparse-Histograms)). The normal histogram code is
highly optimized. If you are recording to a histogram in particularly
performance-sensitive or "hot" code, make sure you're using the histogram
macros; see [reasons above](#Coding-Emitting-to-Histograms).

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

Enumerated histograms are also appropriate for counting events.  Use a simple
boolean histogram.  It's okay if you only log to one bucket (say, `true`).
It's usually best (though not necessary), if you have a comparison point in
the same histogram.  For example, if you want to count pages opened from the
history page, it might be a useful comparison to have the same histogram
record the number of times the history page was opened.

If only a few buckets will be emitted to, consider using a [sparse
histogram](#When-To-Use-Sparse-Histograms).

#### Requirements

Enums logged in histograms must:

- be prefixed with the comment:
  ```c++
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  ```
- be numbered starting from `0`. Note this bullet point does *not* apply for
  enums logged with sparse histograms.
- have enumerators with explicit values (`= 0`, `= 1`, `= 2`), to make it clear
  that the actual values are important. This also makes it easy to match the
  values between the C++/Java definition and [histograms.xml](./histograms.xml).
- not renumber or reuse enumerator values. When adding a new enumerator, append
  the new enumerator to the end. When removing an unused enumerator, comment it
  out, making it clear the value was previously used.

If your enum histogram has a catch-all / miscellaneous bucket, put that bucket
first (`= 0`). This will make the bucket easy to find on the dashboard if
additional buckets are added later.

#### Usage

*In C++*, define an `enum class` with a `kMaxValue` enumerator:

```c++
enum class NewTabPageAction {
  kUseOmnibox = 0,
  kClickTitle = 1,
  // kUseSearchbox = 2,  // no longer used, combined into omnibox
  kOpenBookmark = 3,
  kMaxValue = kOpenBookmark,
};
```

`kMaxValue` is a special enumerator that must share the highest enumerator
value, typically done by aliasing it with the enumerator with the highest
value: clang automatically checks that `kMaxValue` is correctly set for `enum
class`.

The histogram helpers use the `kMaxValue` convention, and the enum may be
logged with:

```c++
UMA_HISTOGRAM_ENUMERATION("NewTabPageAction", action);
```

or:

```c++
UmaHistogramEnumeration("NewTabPageAction", action);
```

Logging histograms from Java should look similar:

```java
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
@IntDef({NewTabPageAction.USE_OMNIBOX, NewTabPageAction.CLICK_TITLE,
        NewTabPageAction.OPEN_BOOKMARK})
private @interface NewTabPageAction {
    int USE_OMNIBOX = 0;
    int CLICK_TITLE = 1;
    // int USE_SEARCHBOX = 2;  // no longer used, combined into omnibox
    int OPEN_BOOKMARK = 3;
    int COUNT = 4;
}

// Using a helper function is optional, but avoids some boilerplate.
private static void logNewTabPageAction(@NewTabPageAction int action) {
    RecordHistogram.recordEnumeratedHistogram(
            "NewTabPageAction", action, NewTabPageAction.COUNT);
}
```

#### Legacy Enums

**Note: this method of defining histogram enums is deprecated. Do not use this
for new enums *in C++*.**

Many legacy enums define a `kCount` sentinel, relying on the compiler to
automatically update it when new entries are added:

```c++
enum class NewTabPageAction {
  kUseOmnibox = 0,
  kClickTitle = 1,
  // kUseSearchbox = 2,  // no longer used, combined into omnibox
  kOpenBookmark = 3,
  kCount,
};
```

These enums must be recorded using the legacy helpers:

```c++
UMA_HISTOGRAM_ENUMERATION("NewTabPageAction", action, NewTabPageAction::kCount);
```

or:

```c++
UmaHistogramEnumeration("NewTabPageAction", action, NewTabPageAction::kCount);
```

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
good reason (and consider whether [sparse
histograms](#When-To-Use-Sparse-Histograms) might work better for you in that
case--they do not pre-allocate their buckets).

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

For such histograms, you want each value recorded to cover approximately
the same span of time.  This typically means emitting values periodically
at a set time interval, such as every 5 minutes.  We do not recommend
recording a ratio at the end of a video playback, as lengths of videos
vary greatly.

It is okay to emit at the end of an animation sequence when what's being
animated is fixed / known.  In this case, each value will represent
roughly the same span of time.

Why?  You typically cannot make decisions based on histograms whose
values are recorded in response to an event that varies in length,
because such metrics can conflate heavy usage with light usage.  It's
easier to reason about metrics that route around this source of bias.

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

Histogram expiry/expiration is specified by the `expires_after` attribute in
histogram descriptions in histograms.xml. The attribute can be specified as date
in **YYYY-MM-DD** format or as Chrome milestone in **M**\*(e.g. M68) format. In
the latter case, the actual expiry date is about 12 weeks after that branch is
cut, or basically when it is replaced on the "stable" channel by the following
release.

After a histogram expires, it will cease to be displayed on the dashboard.
However, the client may continue to send data for that histogram for some time
after the official expiry date so simply bumping the 'expires_after' date in
HEAD may be sufficient to resurrect it without any discontinuity. If too much
time has passed and the client is no longer sending data, it can be re-enabled
via Finch: see [Expired Histogram Whitelist](#Expired-histogram-whitelist).

Once a histogram has expired, the code to record it becomes dead code and should
be removed from the codebase along with marking the histogram definition as
obsolete.

In **rare** cases, the expiry can be set to "never". This is used to denote
metrics of critical importance that are, typically, used for other reports.
For example, all metrics of the "[heartbeat](https://uma.googleplex.com/p/chrome/variations)"
are set to never expire.  All metrics that never expire must have an XML
comment describing why so that it can be audited in the future.

```
<!-- expires-never: "heartbeat" metric (internal: go/uma-heartbeats) -->
```

For all the new histograms the use of expiry attribute will be strongly
encouraged and enforced by Chrome metrics team through reviews.

#### How to choose expiry for histograms

If you are adding a histogram that will be used to evaluate a feature launch,
set an expiry date consistent with the expected feature launch date. Otherwise,
we recommend choosing 3-6 months.

Here are some guidelines for common scenarios:

*   If the listed owner moved to different project, find a new owner.
*   If neither the owner nor the team uses the histogram, remove it.
*   If the histogram is not in use now, but might be useful in the far future,
    remove it.
*   If the histogram is not in use now, but might be useful in the near
    future, pick ~3 months or ~2 milestones ahead.
*   If the histogram is actively in use now and useful for a short term, pick
    3-6 month or 2-4 milestones ahead.
*   If the histogram is actively in use and seems useful for an indefinite time,
    pick 1 year.

We also have a tool that automatically extends expiry dates. The 80% more
frequently accessed histograms are pushed out every Tuesday, to 6 months from
the date of the run. Googlers can view the [design
doc](https://docs.google.com/document/d/1IEAeBF9UnYQMDfyh2gdvE7WlUKsfIXIZUw7qNoU89A4).

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
about is good!  But see the note below on
[Cleaning Up Histogram Entries](#Cleaning-Up-Histogram-Entries).

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

### Provide Clear Units or Enum Labels

For enumerated histograms, including boolean and sparse histograms, provide an
`enum=` attribute mapping enum values to semantically contentful labels. Define
the `<enum>` in enums.xml if none of the existing enums are a good fit. Use
labels whenever they would be clearer than raw numeric values.

For non-enumerated histograms, include a `units=` attribute. Be specific:
e.g. distinguish "MB" vs. "MiB", refine generic labels like "counts" to more
precise labels like "pages", etc.

### Owners

Histograms need owners, who are the experts on the metric and the points of
contact for any questions or maintenance tasks, such as extending a histogram's
expiry or deprecating the metric.

Histograms must have a primary owner and may have secondary owners. A primary
owner is an individual, e.g. <owner>lucy@chromium.org</owner>, who is
ultimately responsible for maintaining the metric. Secondary owners may be
other individuals, team mailing lists, e.g. <owner>my-team@google.com</owner>,
or paths to OWNERS files, e.g. <owner>src/directory/OWNERS</owner>.

It's a best practice to list multiple owners, so that there's no single point
of failure for histogram-related questions and maintenance tasks. If you are
using a metric heavily and understand it intimately, feel free to add yourself
as an owner. For individuals, @chromium.org email addresses are preferred.

Notably, owners are asked to determine whether histograms have outlived their
usefulness. When a histogram is nearing expiry, a robot will file a reminder
bug in Monorail. It's important that somebody familiar with the histogram
notices and triages such bugs!

### Cleaning Up Histogram Entries

Do not delete histograms from histograms.xml. Instead, mark unused
histograms as obsolete and annotate them with the date or milestone in
the `<obsolete>` tag entry.

If the histogram used [histogram suffixes](#Histogram-Suffixes), mark
the suffix entry for the histogram as obsolete as well.

If the histogram is being replaced by a new version:

* Note in the `<obsolete>` message the name of the replacement histogram.

* Make sure the descriptions of the original and replacement histogram
  are different.  It's never appropriate for them to be identical.  Either
  the old description was wrong, and it should be revised to explain what
  it actually measured, or the old histogram was measuring something not
  as useful as the replacement, in which case the new histogram is
  measuring something different and needs to have a new description.

A changelist that marks a histogram as obsolete should be reviewed by all
current owners.

Deleting histogram entries would be bad if someone to accidentally reused your
old histogram name and thereby corrupts new data with whatever old data is still
coming in.  It's also useful to keep obsolete histogram descriptions in
[histograms.xml](./histograms.xml) -- that way, if someone is searching for a
histogram to answer a particular question, they can learn if there was a
histogram at some point that did so even if it isn't active now.

*Exception:* It is ok to delete the metadata for any histogram that has never
been recorded to. For example, it's fine to correct a typo where the histogram
name in the metadata does not match the name in the Chromium source code.

### Pattern histogram

It is sometimes useful to record several closely related metrics, which measure
the same type of data, with some minor variations. It is often useful to use one
or more <variants> elements to save on redundant verbosity in
[histograms.xml](./histograms.xml). You can put the placeholder string in
anywhere of the histogram name. Each placeholder string should associate with
a single token based on the token's key. Each token can specify the `variants`
attribute to associate with an out-of-line `<variants>` or declare a list of
`<variant>`s as its children. These <variant>s will then replace the
corresponding placeholder string in the histogram name to create a family of
histograms.

You can't declare ownership of `<variants>` but can declare ownership of
`<variant>`. If there's no owner specified, the generated histograms will
inherit owners from the parents.

As [with histogram entries](#Cleaning-Up-Histogram-Entries), never delete
variants. If the variant expansion is no longer used, mark it as
obsolete.

If you feel unclear about which histograms will be generated from the pattern
histogram. Consider using the `print_histogram_names.py --diff` tool to
enumerate all the histogram names that are generated by a particular CL. e.g.
(from the repo root):
```
./tools/metrics/histograms/print_histogram_names.py --diff origin/master
```

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

Please talk with the metrics team if there are more than a thousand possible
different values that you could emit.

For more information, see [sparse_histograms.h](https://cs.chromium.org/chromium/src/base/metrics/sparse_histogram.h).

# Team Documentation

This section contains useful information for folks on Chrome Metrics.

## Processing histograms.xml

When working with histograms.xml, verify whether you require fully expanded
OWNERS files. Many scripts in this directory process histograms.xml, and
sometimes OWNERS file paths are expanded and other times they are not. OWNERS
paths are expanded when scripts make use of merge_xml's function MergeFiles;
otherwise, they are not.
