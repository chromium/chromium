# Render Tests

Render tests are the way of performing pixel diff/image comparison tests in
Chromium's Android instrumentation tests. They are backed by the Skia team's
Gold image diffing service, which means that baselines (golden images) are
stored outside of the repo. Image triage (approval/rejection) is handled via the
Gold web UI, located [here](https://chrome-gold.skia.org/). The UI can also be
used to look at what images are currently being produced for tests.

## Fixing a failing Render Test

### Failing on trybots

Anytime a patchset produces new golden images, Gold should automatically
comment on your CL with a link to the triage page. If it fails to do so (e.g.
your test is failing because it is producing an image that was explicitly
marked as negative/invalid by someone previously), you can do the following to
access the triage page manually:

1. On the failed trybot run, locate and follow the `results_details` link under
the `chrome_public_test_apk` step to go to the **Suites Summary** page.
2. On the **Suites Summary** page, follow the link to the test suite that is
failing.
3. On the **Test Results of Suite** page, follow the links in the **log** column
corresponding to the renders mentioned in the failure stack trace. The links
will be named "[Public|Internal] Skia Gold triage link for entire CL".

In most cases, the public and internal links are equivalent. The difference is:
1. The internal link will not show any results unless logged in with an
@google.com account.
2. The internal link will show internal-only results, e.g. from an internal
repo.

So, unless you're working on an internal repo or otherwise expect results to
be marked as internal-only, the public link should be fine.

Once on the triage page, make sure you are logged in at the top-right.
Currently, only @google.com and @chromium.org accounts work, but other domains
such as @opera.com can be allowed if requested. Any domain that can log into
using the Google login flow (e.g. what's used to log into crbug.com) should be
able to be allowed. @microsoft.com accounts are supposed to work, but currently
don't due to some issues. You should then be able to triage any newly produced
images.

If the newly generated golden images are "breaking", i.e. it would be a
regression if Chrome continued to produce the old golden images (such as due
to a major UI change), you should **NOT** approve them as-is.

Instead:
1. Increment the revision using SkiaGoldBuilders's `setRevision`.
1. If you would like to add a description of what the revision represents that
will be visible on the Gold triage page, add or modify a call to
SkiaGoldBuilder's `setDescription`
1. Upload the new patchset, re-run the tryjobs, and approve the new baselines.

The revision increment is so that Gold separates any new golden images from any
that were produced before. It will affect **any** images produced using
RenderTestRule that had its revision incremented (i.e. any images produced in
that test class), so you may have to re-triage additional images. If there
are many images that need to be triaged, you can use the "Bulk Triage" option
in Gold under the "ACTIONS" menu item.

Also see the [Skia Gold Triage FAQ](https://docs.google.com/document/d/1BnwcxzhT8FFvY3YF-6BT4Mqgrb9U40t0HMfEVSSEpNs/edit#heading=h.7up1pxqhb2se)
for help with failing tests. If necessary you can also file a bug against Skai
Gold via [go/gold-bug](go/gold-bug).

### Failing on CI bots

If a test is failing on the CI bots, i.e. after a CL has already been merged,
you can perform the same steps as in the above section with the following
differences:

1. You must manually find the triage links, as Gold has nowhere to post a
comment to. Alternatively, you can check for untriaged images directly in the
[public gold instance](https://chrome-public-gold.skia.org) or
[internal gold instance](https://chrome-gold.skia.org).
2. Triage links are for specific images instead of for an entire CL, and are
thus named after the render name.

### Failing locally

Skia Gold does not allow you to update golden images from local runs. You will
still have access to the generated image, the closest golden image, and the diff
between them in the test results, but this is purely for local debugging. New
golden images must come from either trybots or CI bots.

## Writing a new Render Test

### Writing the test

To write a new test, start with the example in the javadoc for
[RenderTestRule](https://cs.chromium.org/chromium/src/ui/android/javatests/src/org/chromium/ui/test/util/RenderTestRule.java)
or [ChromeRenderTestRule](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/util/ChromeRenderTestRule.java).

You will need to decide whether you want your test results to be public
(viewable by anyone) or internal (only viewable by Googlers) and call
`setCorpus()` accordingly. Public results should use the
`Corpus.ANDROID_RENDER_TESTS_PUBLIC` corpus, while internal results should use
the `Corpus.ANDROID_RENDER_TESTS_INTERNAL` corpus. Alternatively, you can use
`Builder.withPublicCorpus()` as shorthand for creating a builder with the
default public corpus.

**Note:** Each instance/corpus/description combination results in needing to
create a new Gold session under the hood, which adds ~250 ms due to extra
initialization the first time that combination is used in a particular test
suite run. Instance and corpus are pretty constant, so the main culprit is the
description. This overhead can be kept low by using identical descriptions in
multiple test classes, including multiple test cases in a test class that has a
description, or avoiding descriptions altogether.

### Running the tests locally

When running instrumentation tests locally, pass the `--local-output` option to
the test runner to generate a folder in your output directory (in the example
`out/Debug`) looking like `TEST_RESULTS_2017_11_09T13_50_49` containing the
failed renders, eg:

```
./out/Debug/bin/run_chrome_public_test_apk -A Feature=RenderTest --local-output
```

## Implementation Details

### Supported devices

How a View is rendered depends on both the device model and the version of
Android it is running. We only want to maintain golden files for model/SDK pairs
that occur on the trybots, otherwise the golden files will get out of date as
changes occur and render tests will either fail on the Testers with no warning,
or be useless.

Currently, the render tests are only run on the CQ on Nexus 5Xs running
Android Marshmallow, so that is the only model/sdk combination for which we
expect golden images to be maintained. The tests run on other devices and OS
versions, but the results are made available mostly as an FYI, and a comparison
failure on these other configurations will not result in a test failure.

### Sanitizing Views

Certain features lead to flaky tests, for example any sort of animation we don't
take into account while writing the tests. To help deal with this, you can use
`ChromeRenderTestRule.sanitize` to modify the View hierarchy and remove some of the
more troublesome attributes (for example, it disables the blinking cursor in
`EditText`s).
