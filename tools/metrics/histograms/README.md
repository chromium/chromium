# Histogram Guidelines

This document gives the best practices on how to use histograms in code and how
to document the histograms for the dashboards. There are three general types
of histograms: [enumerated histograms](#Enum-Histograms),
[count histograms](#Count-Histograms) (for arbitrary numbers), and
[sparse histograms](#When-To-Use-Sparse-Histograms) (for anything when the
precision is important over a wide range and/or the range is not possible to
specify a priori).

[TOC]

## Defining Useful Metrics

### Directly Measure What You Want

Measure exactly what you want, whether that's the time used for a function call,
the number of bytes transmitted to fetch a page, the number of items in a list,
etc. Do not assume you can calculate what you want from other histograms, as
most ways of doing this are incorrect.

For example, suppose you want to measure the runtime of a function that just
calls two subfunctions, each of which is instrumented with histogram logging.
You might assume that you can simply sum the histograms for those two functions
to get the total time, but that results in misleading data. If we knew which
emissions came from which calls, we could pair them up and derive the total time
for the function. However, histograms are pre-aggregated client-side, which
means that there's no way to recover which emissions should be paired up. If you
simply add up the two histograms to get a total duration histogram, you're
implicitly assuming the two histograms' values are independent, which may not be
the case.

Directly measure what you care about; don't try to derive it from other data.

### Provide Context

When defining a new metric, think ahead about how you will analyze the
data. Often, this will require providing context in order for the data to be
interpretable.

For enumerated histograms in particular, that often means including a bucket
that can be used as a baseline for understanding the data recorded to other
buckets: see the [enumerated histogram section](#Enum-Histograms).

### Naming Your Histogram

Histograms are taxonomized into categories, using dot (`.`) characters as
separators. Thus, histogram names should be in the form Category.Name or
Category.Subcategory.Name, etc., where each category organizes related
histograms.

It should be quite rare to introduce new top-level categories into the existing
taxonomy. If you're tempted to do so, please look through the existing
categories to see whether any matches the metric(s) that you are adding. To
create a new category, the CL must be reviewed by
chromium-metrics-reviews@google.com.

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
parameters (defining the number of buckets for example). Use a helper function
if possible. This leads to shorter, more readable code that's also more
resilient to problems that could be introduced when making changes. (One could,
for example, erroneously change the bucketing of the histogram in one call but
not the other.)

### Use Fixed Strings When Using Histogram Macros

When using histogram macros (calls such as `UMA_HISTOGRAM_ENUMERATION`), you're
not allowed to construct your string dynamically so that it can vary at a
callsite. At a given callsite (preferably you have only one), the string
should be the same every time the macro is called. If you need to use dynamic
names, use the functions in histogram_functions.h instead of the macros.

### Don't Use Same Inline String in Multiple Places

If you must use the histogram name in multiple places, use a compile-time
constant of appropriate scope that can be referenced everywhere. Using inline
strings in multiple places can lead to errors if you ever need to revise the
name and you update one location and forget another.

### Efficiency

Generally, don't be concerned about the processing cost of emitting to a
histogram (unless you're using [sparse
histograms](#When-To-Use-Sparse-Histograms)). The normal histogram code is
highly optimized. If you are recording to a histogram in particularly
performance-sensitive or "hot" code, make sure you're using the histogram
macros; see [reasons above](#Coding-Emitting-to-Histograms).

## Picking Your Histogram Type

### Enum Histograms

Enumerated histogram are most appropriate when you have a list of connected /
related states that should be analyzed jointly. For example, the set of actions
that can be done on the New Tab Page (use the omnibox, click a most visited
tile, click a bookmark, etc.) would make a good enumerated histogram.
If the total count of your histogram (i.e. the sum across all buckets) is
something meaningful—as it is in this example—that is generally a good sign.
However, the total count does not have to be meaningful for an enum histogram
to still be the right choice.

Enumerated histograms are also appropriate for counting events. Use a simple
boolean histogram. It's usually best if you have a comparison point in the same
histogram. For example, if you want to count pages opened from the history page,
it might be a useful comparison to have the same histogram record the number of
times the history page was opened.

In rarer cases, it's okay if you only log to one bucket (say, `true`). However,
think about whether this will provide enough [context](#Provide-Context). For
example, suppose we want to understand how often users interact with a button.
Just knowing that users clicked this particular button 1 million times in a day
is not very informative on its own: The size of Chrome's user base is constantly
changing, only a subset of users have consented to metrics reporting, different
platforms have different sampling rates for metrics reporting, and so on. The
data would be much easier to make sense of if it included a baseline: how often
is the button shown?

There is another problem with using another histogram as a comparison point.
Google systems for processing UMA data attempt to exclude data that is
deemed unreliable or somehow anomalous. It's possible that it may exclude data
from a client for one histogram and not exclude data from that client for the
other.

If only a few buckets are emitted to, consider using a [sparse
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
- have enumerators with explicit values (`= 0`, `= 1`, `= 2`) to make it clear
  that the actual values are important. This also makes it easy to match the
  values between the C++/Java definition and [histograms.xml](./histograms.xml).
- not renumber or reuse enumerator values. When adding a new enumerator, append
  the new enumerator to the end. When removing an unused enumerator, comment it
  out, making it clear the value was previously used.

If your enum histogram has a catch-all / miscellaneous bucket, put that bucket
first (`= 0`). This makes the bucket easy to find on the dashboard if additional
buckets are added later.

#### Usage

*In C++*, define an `enum class` with a `kMaxValue` enumerator:

```c++
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(NewTabPageAction)
enum class NewTabPageAction {
  kUseOmnibox = 0,
  kClickTitle = 1,
  // kUseSearchbox = 2,  // no longer used, combined into omnibox
  kOpenBookmark = 3,
  kMaxValue = kOpenBookmark,
};
// LINT.ThenChange(//path/to/enums.xml:NewTabPageActionEnum)
```

The `LINT``.IfChange` / `LINT``.ThenChange` comments point between the code and XML
definitions of the enum, to encourage them to be kept in sync. See
[guide](https://www.chromium.org/chromium-os/developer-library/guides/development/keep-files-in-sync/)
and [more details](http://go/gerrit-ifthisthenthat).

`kMaxValue` is a special enumerator that must share the highest enumerator
value, typically done by aliasing it with the enumerator with the highest
value: clang automatically checks that `kMaxValue` is correctly set for `enum
class`.

*In Mojo*, define an `enum` without a `kMaxValue` enumerator as `kMaxValue` is
autogenerated for Mojo C++ bindings:

```c++
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PreloadType)
enum PrerenderType {
  kPrefetch = 0,
  // kPrerender = 1,  // deprecated, revamped as kPrerender2
  kNoStatePrefetch = 2,
  kPrerender2 = 3,
};
// LINT.ThenChange(//path/to/enums.xml:PreloadType)
```

*In C++*, the histogram helpers use the `kMaxValue` convention, and the enum may
be logged with:

```c++
UMA_HISTOGRAM_ENUMERATION("NewTabPageAction", action);
```

or:

```c++
UmaHistogramEnumeration("NewTabPageAction", action);
```

where `action` is an enumerator of the enumeration type `NewTabPageAction`.

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

Finally, regardless of the programming language you are using, add the
definition of the enumerator to [enums.xml](./enums.xml), and add linter checks
to keep the C++/Java and XML values in sync:

```xml
<!-- LINT.IfChange(NewTabPageActionEnum) -->
<enum name="NewTabPageActionEnum">
  ...
</enum>
<!-- LINT.ThenChange(//path/to/cpp_definition.h:NewTabPageAction) -->
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
corresponding entry to [enums.xml](./enums.xml). This is automatically verified
by the `AboutFlagsHistogramTest` unit test.

To add a new entry:

1. After adding flags
   to [about_flags.cc](../../../chrome/browser/about_flags.cc),
   run `generate_flag_enums.py --feature <your awesome feature>` or
   simply `generate_flag_enums.py` (slower).

You can alternatively follow these steps:

1. Edit [enums.xml](./enums.xml), adding the feature to the `LoginCustomFlags`
   enum section, with any unique value (just make one up, although whatever it
   is needs to appear in sorted order; `pretty_print.py` can do this for you).
2. Build `unit_tests`, then run `unit_tests
   --gtest_filter=AboutFlagsHistogramTest.*` to compute the correct value.
3. Update the entry in [enums.xml](./enums.xml) with the correct value, and move
   it so the list is sorted by value (`pretty_print.py` can do this for you).
4. Re-run the test to ensure the value and ordering are correct.

You can also use `tools/metrics/histograms/validate_format.py` to check the
ordering (but not that the value is correct).

Don't remove or modify entries when removing a flag; they are still used to
decode data from previous Chrome versions.

### Count Histograms

[histogram_macros.h](https://cs.chromium.org/chromium/src/base/metrics/histogram_macros.h)
provides macros for some common count types, such as memory or elapsed time, in
addition to general count macros. These have reasonable default values; you
seldom need to choose the number of buckets or histogram min. However, you still
need to choose the histogram max (use the advice below).

If none of the default macros work well for you, please thoughtfully choose a
min, max, and bucket count for your histogram using the advice below.

#### Count Histograms: Choosing Min and Max

For the max, choose a value such that very few histogram samples exceed the max.
If a sample is greater than or equal to the max value, it is put in an
"overflow" bucket. If this bucket is too large, it can be difficult to compute
statistics. One rule of thumb is that at most 1% of samples should be in the
overflow bucket (and ideally, less). This allows analysis of the 99th
percentile. Err on the side of too large a range versus too short a range.
Remember that if you choose poorly, you'll have to wait for another release
cycle to fix it.

For the min, use 1 if you care about all possible values (zero and above). All
histograms have an underflow bucket for emitted zeros, so a min of 1 is
appropriate. Otherwise, choose the min appropriate for your particular
situation.

#### Count Histograms: Choosing Number of Buckets

Choose the smallest number of buckets that give you the granularity you need. By
default, count histogram bucket sizes increase exponentially with respect to the
value (i.e., exponential binning), so you can get fine granularity when the
values are small yet still reasonable resolution when the values are larger. The
macros default to 50 buckets (or 100 buckets for histograms with wide ranges),
which is appropriate for most purposes. Because histograms pre-allocate all the
buckets, the number of buckets selected directly dictates how much memory is
used. Do not exceed 100 buckets without good reason (and consider whether
[sparse histograms](#When-To-Use-Sparse-Histograms) might work better for you in
that case—they do not pre-allocate their buckets).

### Timing Histograms

You can easily emit a time duration (time delta) using base::UmaHistogramTimes,
base::UmaHistogramMediumTimes, base::UmaHistogramLongTimes, and their friends.
For the critical path, UMA_HISTOGRAM_TIMES, UMA_HISTOGRAM_MEDIUM_TIMES,
UMA_HISTOGRAM_LONG_TIMES macros, and their friends, as well as helpers like
SCOPED_UMA_HISTOGRAM_TIMER are also available. Many timing
histograms are used for performance monitoring; if this is the case for you,
please read [this document about how to structure timing histograms to make
them more useful and
actionable](https://chromium.googlesource.com/chromium/src/+/lkgr/docs/speed/diagnostic_metrics.md).

### Percentage or Ratio Histograms

You can easily emit a percentage histogram using the UMA_HISTOGRAM_PERCENTAGE
macro provided in
[histogram_macros.h](https://cs.chromium.org/chromium/src/base/metrics/histogram_macros.h).
You can also easily emit any ratio as a linear histogram (for equally sized
buckets).

For such histograms, you want each value recorded to cover approximately the
same span of time. This typically means emitting values periodically at a set
time interval, such as every 5 minutes. We do not recommend recording a ratio at
the end of a video playback, as video lengths vary greatly.

It is okay to emit at the end of an animation sequence when what's being
animated is fixed / known. In this case, each value represents roughly the same
span of time.

Why? You typically cannot make decisions based on histograms whose values are
recorded in response to an event that varies in length because such metrics can
conflate heavy usage with light usage. It's easier to reason about metrics that
avoid this source of bias.

Many developers have been bitten by this. For example, it was previously common
to emit an actions-per-minute ratio whenever Chrome was backgrounded. Precisely,
these metrics computed the number of uses of a particular action during a Chrome
session, divided by length of time Chrome had been open. Sometimes, the recorded
rate was based on a short interaction with Chrome–a few seconds or a minute.
Other times, the recorded rate was based on a long interaction, tens of minutes
or hours. These two situations are  indistinguishable in the UMA logs–the
recorded values can be identical.

The inability to distinguish these two qualitatively different settings make
such histograms effectively uninterpretable and not actionable. Emitting at a
regular interval avoids the issue. Each value represents the same amount of time
(e.g., one minute of video playback).

### Local Histograms

Histograms can be added via [Local macros](https://codesearch.chromium.org/chromium/src/base/metrics/histogram_macros_local.h).
These still record locally, but are not uploaded to UMA and are therefore not
available for analysis. This can be useful for metrics only needed for local
debugging. We don't recommend using local histograms outside of that scenario.

### Multidimensional Histograms

It is common to be interested in logging multidimensional data–where multiple
pieces of information need to be logged together. For example, a developer may
be interested in the counts of features X and Y based on whether a user is in
state A or B. In this case, they want to know the count of X under state A,
as well as the other three permutations.

There is no general purpose solution for this type of analysis. We suggest
using the workaround of using an enum of length MxN, where you log each unique
pair {state, feature} as a separate entry in the same enum. If this causes a
large explosion in data (i.e. >100 enum entries), a [sparse histogram](#When-To-Use-Sparse-Histograms)
may be appropriate. If you are unsure of the best way to proceed, please contact
someone from the OWNERS file.

## Histogram Expiry

Histogram expiry is specified by the `expires_after` attribute in histogram
descriptions in histograms.xml. It is a required attribute. The attribute can
be specified as date in **YYYY-MM-DD** format or as Chrome milestone in
**M**\*(e.g. M105) format. In the latter case, the actual expiry date is about
12 weeks after that branch is cut, or basically when it is replaced on the
"stable" channel by the following release.

After a histogram expires, it ceases to be displayed on the dashboard.
Follow [these directions](#extending) to extend it.

Once a histogram has expired, the code that records it becomes dead code and
should be removed from the codebase. You should also [clean up](#obsolete) the
corresponding entry in histograms.xml. In _rare_ cases, a histogram may be
expired intentionally while keeping the code around; such cases must be
[annotated appropriately](#Intentionally-expired-histograms) in histograms.xml.

In **rare** cases, the expiry can be set to "never". This is used to denote
metrics of critical importance that are, typically, used for other reports. For
example, all metrics of the
"[heartbeat](https://uma.googleplex.com/p/chrome/variations)" are set to never
expire. All metrics that never expire must have an XML comment describing why so
that it can be audited in the future. Setting an expiry to "never" must be
reviewed by chromium-metrics-reviews@google.com.

```
<!-- expires-never: "heartbeat" metric (internal: go/uma-heartbeats) -->
```

It is never appropriate to set the expiry to "never" on a new histogram. Most
new histograms don't turn out to have the properties the implementer wants,
whether due to bugs in the implementation or simply an evolving understanding
of what should be measured.

#### Guidelines on expiry

Here are some guidelines for common scenarios:

*   If the listed owner moved to a different project, find a new owner.
*   If neither the owner nor the team uses the histogram, remove it.
*   If the histogram is not in use now, but might be useful in the far future,
    remove it.
*   If the histogram is not in use now, but might be useful in the near
    future, pick ~3 months (also ~3 milestones) ahead.
*   Otherwise, pick an expiry that is reasonable for how long the metric should
    be used, up to a year.

We also have a tool that automatically extends expiry dates. The most frequently
accessed histograms, currently 99%, have their expirations automatically
extended every Tuesday to 6 months from the date of the run. Googlers can view
the [design
doc](https://docs.google.com/document/d/1IEAeBF9UnYQMDfyh2gdvE7WlUKsfIXIZUw7qNoU89A4)
of the program that does this.  The bottom line is: If the histogram is being
checked, it should be extended without developer interaction.

#### How to choose expiry for new histograms

In general, set an expiry that is reasonable for how long the metric should
be used, up to a year.

Some common cases:

*   When adding a histogram to evaluate a feature launch, set an expiry date
    consistent with the expected feature launch date.
*   If you expect the histogram to be useful for an indefinite time, set an
    expiry date up to 1 year out. This gives a chance to re-evaluate whether
    the histogram indeed proved to be useful.
*   Otherwise, 3-6 months (3-6 milestones) is typically a good choice.

#### How to extend an expired histogram {#extending}

You can revive an expired histogram by setting the expiration date to a
date in the future.

There's some leeway here. A client may continue to send data for that
histogram for some time after the official expiry date so simply bumping
the 'expires_after' date at HEAD may be sufficient to resurrect it without
any data discontinuity.

If a histogram expired more than a month ago (for histograms with an
expiration date) or more than one milestone ago (for histograms with
expiration milestones; this means top-of-tree is two or more milestones away
from expired milestone), then you may be outside the safety window. In this
case, when extending the histogram add to the histogram description a
message: "Warning: this histogram was expired from DATE to DATE; data may be
missing." (For milestones, write something similar.)

When reviving a histogram outside the safety window, realize the change to
histograms.xml to revive it rolls out with the binary release. It takes
some time to get to the stable channel.

It you need to revive it faster, the histogram can be re-enabled via adding to
the [expired histogram allowlist](#Expired-histogram-allowlist).

### Expired histogram notifier

The expired histogram notifier notifies histogram owners before their histograms
expire by creating crbugs, which are assigned to owners. This allows owners to
extend the lifetime of their histograms, if needed, or deprecate them. The
notifier regularly checks all histograms across the histograms.xml files and
identifies expired or soon-to-be expired histograms. It then creates or updates
crbugs accordingly.

### Expired histogram allowlist

If a histogram expires but turns out to be useful, you can add the histogram's
name to the allowlist to re-enable logging for it, until the updated expiration
date reaches the Stable channel. When doing so, update the histogram's summary
to document the period during which the histogram's data is incomplete. To add a
histogram to the allowlist, see the internal documentation:
[Histogram Expiry](https://goto.google.com/histogram-expiry-gdoc).

### Intentionally expired histograms

In **rare** cases, a histogram may be expired intentionally while keeping the
code around. For example, this can be useful for diagnostic metrics that are
occasionally needed to investigate specific bugs, but do not need to be reported
otherwise.

To avoid such histograms to be flagged for code clean up, they must be annotated
in the histograms.xml with the `expired_intentionally` tag as follows:

```xml
<histogram name="Tab.Open" enum="TabType" expires_after="M100">
 <expired_intentionally>Kept as a diagnostic metric.</expired_intentionally>
 <owner>histogramowner@chromium.org</owner>
 <summary>Histogram summary.</summary>
</histogram>
```

## Testing

Test your histograms using `chrome://histograms`. Make sure they're being
emitted to when you expect and not emitted to at other times. Also check that
the values emitted to are correct. Finally, for count histograms, make sure
that buckets capture enough precision for your needs over the range.

Pro tip: You can filter the set of histograms shown on `chrome://histograms` by
appending to the URL. For example, `chrome://histograms/UserActions` shows
only histograms whose names contain "UserActions", such as
"UMA.UserActionsCount".

In addition to testing interactively, you can have unit tests examine the
values emitted to histograms. See [histogram_tester.h](https://cs.chromium.org/chromium/src/base/test/metrics/histogram_tester.h)
for details.

See also `chrome://metrics-internals` ([docs](https://chromium.googlesource.com/chromium/src/+/master/components/metrics/debug/README.md))
for more thorough manual testing if needed.

By default, histograms in unit or browser tests will not be actually uploaded.
In general, you can rely on the UMA infrastructure to upload the metrics correctly.

### Don't Use Histograms to Prove Main Logic Correctness

Do not rely upon using histograms in tests as a way to prove correctness of
your main program logic. If a unit or browser test uses a histogram count as a
way to validate logic then that test coverage would be lost if the histogram is
deleted after it has expired. That situation would prevent cleanup of the
histogram. Construct your tests using other means to validate your general
logic, and only use
[`HistogramTester`](https://cs.chromium.org/chromium/src/base/test/metrics/histogram_tester.h)
to verify that the histogram values are being generated as you would expect.

### Verify Enum and Variant Values

If you have <enum> or <variant> entries that need to be updated to match code,
you can use
[HistogramEnumReader](https://cs.chromium.org/chromium/src/base/test/metrics/histogram_enum_reader.h)
or
[HistogramVariantsReader](https://cs.chromium.org/chromium/src/base/test/metrics/histogram_enum_reader.h)
to read and verify the expected values in a unit test. This prevents a mismatch
between code and histogram data from slipping through CQ.

For an example, see
[BrowserUserEducationServiceTest.CheckFeaturePromoHistograms](https://cs.chromium.org/chromium/src/chrome/browser/ui/views/user_education/browser_user_education_service_unittest.cc).

## Interpreting the Resulting Data

The top of [go/uma-guide](http://go/uma-guide) has good advice on how to go
about analyzing and interpreting the results of UMA data uploaded by users. If
you're reading this page, you've probably just finished adding a histogram to
the Chromium source code and you're waiting for users to update their version of
Chrome to a version that includes your code. In this case, the best advice is
to remind you that users who update frequently / quickly are biased. Best take
the initial statistics with a grain of salt; they're probably *mostly* right but
not entirely so.

## Revising Histograms

When changing the semantics of a histogram (when it's emitted, what the buckets
represent, the bucket range or number of buckets, etc.), create a new histogram
with a new name. Otherwise analysis that mixes the data pre- and post- change
may be misleading. If the histogram name is still the best name choice, the
recommendation is to simply append a '2' to the name. See [Cleaning Up Histogram
Entries](#obsolete) for details on how to handle the XML changes.

## Deleting Histograms

Please delete code that emits to histograms that are no longer needed.
Histograms take up memory. Cleaning up histograms that you no longer care
about is good! But see the note below on
[Cleaning Up Histogram Entries](#obsolete).

## Documenting Histograms

Document histograms in an appropriate [metadata/foo/histograms.xml](https://source.chromium.org/search?q=f:metadata%2F.*%2Fhistograms.xml&ss=chromium%2Fchromium%2Fsrc)
file.

There is also a [google-internal version of the file](https://goto.google.com/chrome-histograms-internal)
for two cases:

* The histogram is confidential (an accurate description about how to interpret
  the histogram would reveal information about Google's plans). In this case,
  you must only document the histogram in the internal version.
* The corresponding code that emits the histogram is internal (added only to
  Chrome code, not to Chromium code). In this case, you may document the
  histogram in either the internal or external version.

### Add Histogram and Documentation in the Same Changelist

If possible, please add the [histograms.xml](./histograms.xml) description in
the same changelist in which you add the histogram-emitting code. This has
several benefits. One, it sometimes happens that the
[histograms.xml](./histograms.xml) reviewer has questions or concerns about the
histogram description that reveal problems with interpretation of the data and
call for a different recording strategy. Two, it allows the histogram reviewer
to easily review the emission code to see if it comports with these best
practices and to look for other errors.

### Understandable to Everyone

Histogram descriptions should be roughly understandable to someone not familiar
with your feature. Please add a sentence or two of background if necessary.

Note any caveats associated with your histogram in the summary. For example, if
the set of supported platforms is surprising, such as if a desktop feature is
not available on Mac, the summary should explain where it is recorded. It is
also common to have caveats along the lines of "this histogram is only recorded
if X" (e.g., upon a successful connection to a service, a feature is enabled by
the user).


### State When It Is Recorded

Histogram descriptions should clearly state when the histogram is emitted
(profile open? network request received? etc.).

Some histograms record error conditions. These should be clear about whether
all errors are recorded or only the first. If only the first, the histogram
description should have text like:
```
In the case of multiple errors, only the first reason encountered is recorded. Refer
to Class::FunctionImplementingLogic() for details.
```

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
owner is a Googler with an @google.com or @chromium.org email address, e.g.
<owner>lucy@chromium.org</owner>, who is ultimately responsible for maintaining
the metric. Secondary owners may be other individuals, team mailing lists, e.g.
<owner>my-team@google.com</owner>, or paths to OWNERS files, e.g.
<owner>src/directory/OWNERS</owner>.

It's a best practice to list multiple owners, so that there's no single point
of failure for histogram-related questions and maintenance tasks. If you are
using a metric heavily and understand it intimately, feel free to add yourself
as an owner.

Notably, owners are asked to determine whether histograms have outlived their
usefulness. When a histogram is nearing expiry, a robot files a reminder bug in
Monorail. It's important that somebody familiar with the histogram notices and
triages such bugs!

Tip: When removing someone from the owner list for a histogram, it's a nice
courtesy to ask them for approval.

### Components

Histograms may be associated with a component, which can help make sure that
histogram expiry bugs don't fall through the cracks.

A histogram is associated with the `buganizer_public` component listed in the
DIR_METADATA file adjacent to the histograms.xml file if present.

There are two other ways in which components may be associated with a
histogram. The first way is to add a tag containing the component ID to a
histogram or histogram suffix, e.g. <component>1456399</component>. The second
way is to specify an OWNERS file as a secondary owner for a histogram. If the
OWNERS file has an adjacent DIR_METADATA file that contains a
`buganizer_public` component, then that component is associated with the
histogram. If there isn't a parallel DIR_METADATA file with such a component,
but an ancestor directory has one, then the ancestor directory's component is
used.

If more than one component is associated with a histogram, <component> tag is
favored over adjacent DIR_METADATA file and over OWNERS file.

**Note:** For non-Chromium Issue Tracker (ChromeOS Public Tracker or internal)
components, make sure uma-tools@prod.google.com has access to create and
update issues.


### Improvement Direction
For some histograms, an increase or a decrease in the reported values can be
associated with either an improvement or a deterioration. For example, if you
are tracking page load speed, then seeing your metrics tracking page load time
in milliseconds getting gradually larger values, perhaps as the result of a
Finch study, may signify worse performance; on the contrary, seeing a reduction
in the page load speed may indicate an improvement. You can provide this
information on the movement direction by adding a tag
 `<improvement direction="LOWER_IS_BETTER"/>` within your `<histogram>`. The
opposite is `<improvement direction="HIGHER_IS_BETTER"/>`.

For other histograms where there may not be a movement direction that's clearly
better, you can set `<improvement direction="NEITHER_IS_BETTER"/>`.

This `<improvement>` tag is optional. You can also add/delete this tag or make a
correction to its `direction` attribute any time.

### Cleaning Up Histogram Entries {#obsolete}

When the code to log a histogram is deleted, its corresponding histograms.xml
entry should also be removed. Past histogram data will still be available for
viewing on Google's internal UMA dashboard.

The CL to remove one or more histograms can also specify an obsoletion message
through special syntax in the CL description. This also applies to variants of a
[patterned histogram](#Patterned-Histograms) and to suffix entries for a
suffixed histogram.

The changelist that obsoletes a histogram entry should be reviewed by all
current owners.

#### Remove the Entry

Delete the entry in the histograms.xml file.

* In some cases there may be artifacts that remain, with some examples being:
  * Empty `<token>` blocks, or individual `<variant>`s.
  * `<enum>` blocks from enums.xml that are no longer used.
  * Suffix entries in `histogram_suffixes_list.xml`.
* Please remove these artifacts if you find them.
  * **Exception**: please update the label of `<int value=... label=... />` with
    the `(Obsolete) ` prefix, e.g.
    `<int value="1" label="(Obsolete) Navigation failed. Removed in 2023/01."/>`
    rather than deleting them, if the surrounding `<enum>` block is not being
    deleted.

#### Add an Obsoletion Message

An obsoletion message is displayed on the dashboard and provides developers
context for why the histogram was removed and, if applicable, which histogram
it was replaced by.

**Note:** You can skip this step if the histogram is expired. This is because
tooling automatically records the date and milestone of a histogram's
removal.

You can provide a custom obsoletion message for a removed histogram via tags
on the CL description:

* Add the obsoletion message in the CL description in the format
  `OBSOLETE_HISTOGRAM[histogram name]=message`, e.g.:
  `OBSOLETE_HISTOGRAM[Tab.Count]=Replaced by Tab.Count2`
* To add the same obsoletion message to all the histograms removed in the CL,
  you can use `OBSOLETE_HISTOGRAMS=message`, e.g.:
  `OBSOLETE_HISTOGRAMS=Patterned histogram Hist.{Token} is replaced by Hist.{Token}.2`
* **Notes:**
  * **The full tag should be put on a single line, even if it is longer than the
    maximum CL description width.**
  * You can add multiple obsoletion message tags in one CL.
  * `OBSOLETE_HISTOGRAMS` messages will be overwritten by histogram-specific
    ones, if present.
* You could also include information about why the histogram was removed. For
  example, you might indicate how the histogram's summary did not accurately
  describe the collected data.
* If the histogram is being replaced, include the name of the replacement and
  make sure that the new description is different from the original to reflect
  the change between versions.

### Patterned Histograms

It is sometimes useful to record several closely related metrics, which measure
the same type of data, with some minor variations. You can declare the metadata
for these concisely using patterned histograms. For example:

```xml
<histogram name="Pokemon.{Character}.EfficacyAgainst{OpponentType}"
    units="multiplier" expires_after="M95">
  <owner>individual@chromium.org</owner>
  <owner>team@chromium.org</owner>
  <summary>
    The efficacy multiplier for {Character} against an opponent of
    {OpponentType} type.
  </summary>
  <token key="Character">
    <variant name="Bulbasaur"/>
    <variant name="Charizard"/>
    <variant name="Mewtwo"/>
  </token>
  <token key="OpponentType">
    <variant name="Dragon" summary="dragon"/>
    <variant name="Flying" summary="flappity-flap"/>
    <variant name="Psychic" summary="psychic"/>
    <variant name="Water" summary="water"/>
  </token>
</histogram>
```

This example defines metadata for 12 (= 3 x 4) concrete histograms, such as

```xml
<histogram name="Pokemon.Charizard.EfficacyAgainstWater"
    units="multiplier" expires_after="M95">
  <owner>individual@chromium.org</owner>
  <owner>team@chromium.org</owner>
  <summary>
    The efficacy multiplier for Charizard against an opponent of water type.
  </summary>
</histogram>
```

Each token `<variant>` defines what text should be substituted for it,
both in the histogram name and in the summary text. The name part gets
substituted into the histogram name; the summary part gets substituted in
the summary field (the histogram description). As shorthand, a
`<variant>` that omits the `summary` attribute substitutes the value of
the `name` attribute in the histogram's `<summary>` text as well.

*** promo
Tip: You can declare an optional token by listing an empty name: `<variant
name="" summary="aggregated across all breakdowns"/>`. This can be useful when
recording a "parent" histogram that aggregates across a set of breakdowns.
***

You can use the `<variants>` tag to define a set of `<variant>`s out-of-line.
This is useful for token substitutions that are shared among multiple families
of histograms within the same file. See
[histograms.xml](https://source.chromium.org/search?q=file:histograms.xml%20%3Cvariants)
for examples.

*** promo
Warning: The `name` attribute of the `<variants>` tag is globally scoped, so
use detailed names to avoid collisions. The `<variants>` defined should only
be used within the file.
***

By default, a `<variant>` inherits the owners declared for the patterned
histogram. Each variant can optionally override the inherited list with custom
owners:
```xml
<variant name="SubteamBreakdown" ...>
  <owner>subteam-lead@chromium.org</owner>
  <owner>subteam@chromium.org</owner>
</variant>
```

*** promo
Tip: You can run `print_expanded_histograms.py --pattern=` to show all generated
histograms by patterned histograms or histogram suffixes including their
summaries and owners. For example, this can be run (from the repo root) as:
```
./tools/metrics/histograms/print_expanded_histograms.py --pattern=^UMA.A.B
```
***

*** promo
Tip: You can run `print_histogram_names.py --diff` to enumerate all the
histogram names that are generated by a particular CL. For example, this can be
run (from the repo root) as:
```
./tools/metrics/histograms/print_histogram_names.py --diff origin/main
```
***

For documentation about the `<histogram_suffixes>` syntax, which is deprecated,
see
https://chromium.googlesource.com/chromium/src/+/refs/tags/87.0.4270.1/tools/metrics/histograms/one-pager.md#histogram-suffixes-deprecated-in-favor-of-pattern-histograms

## When To Use Sparse Histograms

Sparse histograms are well-suited for recording counts of exact sample values
that are sparsely distributed over a large range. They can be used with enums
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


# Becoming a Metrics Reviewer

Any Chromium committer who is also a Google employee is eligible to become a
metrics reviewer. Please follow the instructions at [go/reviewing-metrics](https://goto.google.com/reviewing-metrics).
This consists of reviewing our training materials and passing an informational
quiz. Since metrics have a direct impact on internal systems and have privacy
considerations, we're currently only adding Googlers into this program.


# Reviewing Metrics CLs

If you are a metric OWNER, you have the serious responsibility of ensuring
Chrome's data collection is following best practices. If there's any concern
about an incoming metrics changelist, please escalate by assigning to
chromium-metrics-reviews@google.com.

When reviewing metrics CLs, look at the following, listed in approximate order
of importance:

## Privacy

Does anything tickle your privacy senses? (Googlers, see
[go/uma-privacy](https://goto.google.com/uma-privacy) for guidelines.)

**Please escalate if there's any doubt!**

## Clarity

Is the metadata clear enough for [all Chromies](#Understandable-to-Everyone) to
understand what the metric is recording? Consider the histogram name,
description, units, enum labels, etc.

It's really common for developers to forget to list [when the metric is
recorded](#State-When-It-Is-Recorded). This is particularly important context,
so please remind developers to clearly document it.

Note: Clarity is a bit less important for very niche metrics used only by a
couple of engineers. However, it's hard to assess the metric design and
correctness if the metadata is especially unclear.

## Metric design

* Does the metric definition make sense?
* Will the resulting data be interpretable at analysis time?

## Correctness

Is the histogram being recorded correctly?

* Does the bucket layout look reasonable?

  * The metrics APIs like base::UmaHistogram* have some sharp edges,
    especially for the APIs that require specifying the number of
    buckets. Check for off-by-one errors and unused buckets.

  * Is the bucket layout efficient? Typically, push back if there are >50
    buckets -- this can be ok in some cases, but make sure that the CL author
    has consciously considered the tradeoffs here and is making a reasonable
    choice.

  * For timing metrics, do the min and max bounds make sense for the duration
    that is being measured?

* The base::UmaHistogram* functions are
  [generally preferred](#Coding-Emitting-to-Histograms) over the
  UMA_HISTOGRAM_* macros. If using the macros, remember that names must be
  runtime constants!

Also, related to [clarity](#Clarity): Does the client logic correctly implement
the metric described in the XML metadata? Some common errors to watch out for:

* The metric is only emitted within an if-stmt (e.g., only if some data is
  available) and this restriction isn't mentioned in the metadata description.

* The metric description states that it's recorded when X happens, but it's
  actually recorded when X is scheduled to occur, or only emitted when X
  succeeds (but omitted on failure), etc.

When the metadata and the client logic do not match, the appropriate solution
might be to update the metadata, or it might be to update the client
logic. Guide this decision by considering what data will be more easily
interpretable and what data will have hidden surprises/gotchas.

## Sustainability

* Is the CL adding a reasonable number of metrics/buckets?
  * When reviewing a CL that is trying to add many metrics at once, guide the CL
    author toward an appropriate solution for their needs. For example,
    multidimensional metrics can be recorded via UKM, and we are currently
    building support for structured metrics in UMA.
  * There's no hard rule, but anything above 20 separate histograms should be
    escalated by being assigned to chromium-metrics-reviews@google.com.
  * Similarly, any histogram with more than 100 possible buckets should be
    escalated by being assigned to chromium-metrics-reviews@google.com.

* Are expiry dates being set
  [appropriately](#How-to-choose-expiry-for-new-histograms)?

## Everything Else!

This document describes many other nuances that are important for defining and
recording useful metrics. Check CLs for these other types of issues as well.

And, as you would with a language style guide, periodically re-review the doc to
stay up to date on the details.


# Team Documentation


## Processing histograms.xml

When working with histograms.xml, verify whether you require fully expanded
OWNERS files. Many scripts in this directory process histograms.xml, and
sometimes OWNERS file paths are expanded and other times they are not. OWNERS
paths are expanded when scripts make use of merge_xml's function MergeFiles;
otherwise, they are not.
