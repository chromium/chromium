# Things to note when doing metrics code review.

[TOC]

This covers how to review metrics code in the Chromium codebase.

## UMA Histograms

### What is covered under review?

During code review ensure the following -

#### XML metadata in histograms.xml is correct and descriptive.

*   Verify the histogram is created under the right histogram 'namespace'.
    (i.e if the histogram name is A.B.C, then A is the histogram namespace).
    If this is a new one, check if there a similar one that already exists?

*   Histogram owners match the
    [histogram owners guidelines](https://chromium.googlesource.com/chromium/src/+/HEAD/tools/metrics/histograms/README.md#owners).

*   If generating multiple histograms programmatically or defining common set of
    histogram, guide them to use
    [histogram-suffixes](https://chromium.googlesource.com/chromium/src/tools/+/refs/heads/master/metrics/histograms/README.md#Histogram-Suffixes).

*   Verify that expires_after is reasonable. CL author should be able to justify
    it. See guidance at on
    [histogram-expiry](https://chromium.googlesource.com/chromium/src/+/HEAD/tools/metrics/histograms/README.md#Histogram-Expiry).

*   Summary section should describe what is measured, when is it recorded and
    when not. The summary should generally describe a single emission of sample
    and not the statistics in aggregate.

*   If the histogram is recorded only for some platforms, then it should be
    included in the summary (unless part of its name).

*   Histogram and enum names don't include special characters besides dot,
    underscores or slashes.

#### Histogram is designed correctly.

*   Verify that the histogram follows the
    [UMA histogram design recommendation](https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md).
    If issues are found please link to docs in your review comments to
    increase their visibility.

#### Histogram is setup correctly.

*   If modifying an existing histogram, request that the histogram be
    renamed if its meaning has changed significantly. Common practices are to
    add suffix such as 2 to the name. When doing so, the existing entry for the
    histogram should also be kept but with `<obsolete>` tag.

*   Don't allow deleting histograms or enum buckets unless there is a *very*
    compelling reason to do so (e.g. never logged). Instead they should be
    marked as obsolete with `<obsolete>` tag.

*   Re-numbering enum bucket values is not allowed as these break backward
    compatibility with respect to the data stored.

*   When modifying enums it is better to add new values to the enum instead of
    re-purposing existing enum values.

*   Modifying enum labels / summary is safe and allowed without review. However,
    if reviewing these changes make sure the semantic meaning of the bucket
    remain unchanged. e.g re-labeling 'Has Error' to 'Has Warning' should not be
    allowed, while re-labeling 'Has Error' to 'Has Error (e.g this and that
    error)' is okay.

*   Verify that histogram buckets are not a privacy risk. Some of the types
    forbidden are if the buckets are encoding page contents, URL, domain name,
    or is including any other type of personally identifying or sensitive
    information. If during review you are unsure then do not hesitate to request
    that Chrome Privacy Team review the change.

*   Check that the histogram bucket space of all possible values for all clients
    will be limited to 50 generally. It should not exceed 100 unless the CL
    author has a justification for having extra buckets.

    *   If the lower bucket counts prove insufficient (e.g after reviewing
        the data for 50 buckets proves to be unhelpful, more buckets can be
        requested in a separate review).

    *   When logging hashes in a sparse histogram make sure the global space
        meet this criterion (not just a client). Another valid case is when
        logging API error codes which in theory could be potentially large,
        but on a particular day it is not the case.

*   Check that recorded samples will be within 0 and 2^31-1.

*   If you expect majority of values to be under a certain value, say X,
    then it's good practice to pick the next order of magnitude - e.g. 10X for
    the max. This ensures that even outliers will not overflow the distribution.

*   Verify that for an enum histogram, the enum described in enums.xml and the
    enum defined in the client code matches. Furthermore, the enum in the code
    should have a comment mentioning that the values must not be changes and
    also that additions to the enum should be synced to enums.xml.

    *   See the sample comment
        [here](https://cs.chromium.org/chromium/src/base/metrics/histogram_macros.h?rcl=2c99f35f64380ba63c928787834661fbc1fa4234&l=46).
        The comment should be identical or nearly so.

*   If the histogram is logged via a macro (rather than a call to the function),
    check that the names will be constant at runtime.

### What is not covered under review?

*   Metrics team members need not be versed with code where histogram is
    recorded and are not reviewing all the corner cases associated with
    collecting the histogram.

*   enums.xml changes don't need a review. However, it is still useful to verify
    that the changes match the guidelines mentioned above.

## User Actions

*   Verify that the user action logged is actually user triggered. If they
    are not then advise cl author to convert them to a histogram.

    *   However if ordering of actions is the interesting part of their analysis
        then maybe see if each order combination can become a histogram bucket
        instead of user action.

*   Don't allow logging of noisy user actions (like scroll events). Typical
    allowed frequency is to be less frequent than PageLoad or MobilePageLoaded
    event.

## UKMs

*   UKM metrics are to be reviewed by UKM
    [data privacy owners](https://cs.chromium.org/chromium/src/tools/metrics/ukm/PRIVACY_OWNERS).

*   The metrics must follow the
    [data collection guideline](/analysis/uma/g3doc/ukm/ukm.md#adding-ukms).

<!--TODO(ukm-team): Add other guidelines for reviewing UKM metrics changes. -->

## Other specialized metrics

The ChromeUserMetricsExtension proto includes a variety of other fields such as
records for Omnibox, Profiler, Stability, etc. These are specialized reviews and
should be routed to relevant owner. The guidelines here don't cover these cases
and typically require a server-side review first to change the proto.
