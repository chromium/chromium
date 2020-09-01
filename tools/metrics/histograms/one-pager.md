# Histogram one-pager

For best practices on writing histogram descriptions, see
[README.md](./README.md)
For details on how to modify histograms.xml to add your description, keep reading.

In [histograms.xml](./histograms.xml)
file you will find two sections:

* The histograms section describes base histograms, giving their name,
  their units or enum type, expiration, a short one-line summary, and a more
  detailed description.
* The histogram_suffixes section provides a compact way of defining histograms
  by applying affixes to existing histograms. This can be done recursively
  to a max recursion depth of 5. See the example below for details on how it
  works.

Enum labels of histograms are defined and described in [enums.xml](./enums.xml).

## Histogram expiration

Histogram expiration can be specified as a date in YYYY-MM-DD format or as a
Chrome milestone. For example:
  `<histogram name="HistogramName" expires_after="2018-01-01">`
or
  `<histogram name="HistogramName" expires_after="M68">`

After a histogram expires, it will not be recorded (nor uploaded to the UMA
servers). The code to record it becomes dead code, and should be removed from
the codebase; and the histogram definition should be marked as obsolete. However,
if the histogram would remain useful, the expiration should be extended
accordingly. We recommend to always specify an expiry for new histograms.

## Pattern histogram

Pattern histograms are a convenient way of describing a set of similar
histograms. Placeholders are used in the <histogram> name and summary, which
have substitutions described by <token> tags.  Each possible values is described
by a <variant> tag, which may either be inline in the <token>, or described in a
<variants> tag that can be referenced by <token>s.

A `<variant>` may be marked obsolete, or specify explicit `<owner>`s that
override the pattern's owners.

If we define the following:

```
<variants name="OmniboxProviderVersion">
  <variant name=".Provider" summary="old version">
    <obsolete>
  	Deprecated. Replaced by Provider2.
    </obsolete>
  </variant>
  <variant name=".Provider2" summary="second version"/>
</variants>

<histogram name="Omnibox{version}{content}.Time" units="ms"
  expires_after="2020-12-25">
  <owner>me@chromium.org</owner>
  <summary>
    The length of time taken by the {version} of {content} provider's
    synchronous pass.
  </summary>
  <token key="version" variants="OmniboxProviderVersion"/>
  <token key="content">
    <variant name=".All" summary="All"/>
    <variant name=".ExtensionApp" summary="the ExtensionApp">
      <owner>you@chromium.org</owner>
    </variant>
    <variant name=".HistoryContents" summary=" the HistoryContents"/>
  </token>
</histogram>
```

The complete list of histograms and their summary will be:

```
Omnibox.Provider.All.Time: The length of time taken by old versions of
    All provider's synchronous pass. (obsolete)
Omnibox.Provider.ExtensionApp.Time: The length of time taken by the old version
    of the ExtensionApp provider's synchronous pass. (obsolete)
Omnibox.Provider.HistoryContents.Time: The length of time taken by the old
    version of the HistoryContents provider's synchronous pass. (obsolete)
Omnibox.Provider2.All.Time: The length of time taken by the second version of
    All provider's synchronous pass.
Omnibox.Provider2.ExtensionApp.Time: The length of time taken by the second
    version of the ExtensionApp provider's synchronous pass.
Omnibox.Provider2.HistoryContents.Time: The length of time taken by the second
    version of the HistoryContents provider's synchronous pass.
```

Among the above histograms, `Omnibox.Provider.ExtensionApp.Time` and
`Omnibox.Provider2.ExtensionApp.Time` histograms belong to `you@chromium.org`
whereas other histograms belong to `me@chromium.org`.

The out-of-line variants can be shared between multiple histograms. Example:

```
<variants name="MyVariants">
  <variant name=".V1" summary="V1"/>
  <variant name=".V2" summary="V2"/>
</variants>

<histogram name="MyHist1{MyVaraints}">
  <token key="MyVariants" varaints="MyVariants"/>
</histogram>

<histogram name="MyHist2{MyVaraints}">
  <token key="MyVariants" varaints="MyVariants"/>
</histogram>
```

The complete list of histograms will be:

```
MyHist1.V1
MyHist1.V2
MyHist2.V1
MyHist2.V2
```

The variant name is allowed to be an empty string, to make it easy to define
patterns that include legacy histogram names. When defining new histograms,
prefer an **explicit variant name** instead. Example:

```
<histogram name="MyHist1.A{MyVariants}">
  <token key="MyVariants">
    <variant name="" summary="All"/>
    <variant name=".B" summary="B"/>
  </token>
</histogram>
```

The complete list of histograms will be:

```
MyHist1.A
MyHist1.A.B
```

## Histogram suffixes (_deprecated in favor of pattern histograms_)

The histogram suffixes syntax will be replaced by the above pattern histograms.
The new pattern histogram syntax provides a much clearer picture of what
histograms will be generated and it’s easier to use.
*   It allows users to put format strings in the middle of histogram name and
    specify variants that will be used to replace these format strings both
    inline and out-of-line.
*   It gives users better control over each generated histogram's description.
    The placeholder string in the histogram's summary will be replaced by the
    attribute of the corresponding variant.

Each histogram_suffixes tag lists the histograms that it affects. The complete
list of histograms is computed by appending (or prepending - see below) the
suffix names to each of the affected histograms. For example, if we define the
following:

  ```
  <histogram name="FileLoadLatency"/>

  <histogram_suffixes name="SuperHttpExperiment" separator=".">
    <suffix name="SuperHttpEnabled"/>
    <suffix name="SuperHttpDisabled"/>
    <affected-histogram name="FileLoadLatency"/>
  </histogram_suffixes>
  ```

The complete list of histograms will be:

  ```
  FileLoadLatency
  FileLoadLatency.SuperHttpEnabled
  FileLoadLatency.SuperHttpDisabled
  ```

histogram_suffixes can also be used to insert affix in the middle. Example:

  ```
  <histogram name="Prerender.Events"/>

  <histogram_suffixes name="HoverStatsTypes" separator="_" ordering="prefix">
    <suffix name="HoverStats50"/>
    <affected-histogram name="Prerender.Events"/>
  </histogram_suffixes>
  ```

The complete list of histograms will be:

  ```
  Prerender.Events
  Prerender.HoverStats50_Events
  ```

When 'ordering="prefix"' is present in the histogram_suffixes tag, the suffix
will be inserted after the first dot separator of the affected-histogram name.
Optionally, ordering can be specified as "prefix,N" where N indicates after
how many dots the suffix should be inserted (default=1). The affected-histogram
name has to have at least N dots in it.

If a `<histogram>` or a `<suffix>` element is only provided so that its name can
be extended by `<histogram_suffixes>`, the element should be annotated with the
attribute `base="true"`. This instructs tools not to treat the base name as a
distinct histogram.

If two histogram_suffixes affect the same histogram, generated lists will be
combined as if only one suffix is defined. Histogram suffix may affect histogram
generated by another suffix (maximum depth of this recursion is 5). Example:

```
<histogram name="MyHist">

<histogram_suffixes name="Suffix1">
  <suffix name="A"/>
  <suffix name="B"/>
  <affected-histogram name="MyHist"/>
</histogram_suffixes>

<histogram_suffixes name="Suffix2">
  <suffix name="C"/>
  <suffix name="D"/>
  <affected-histogram name="MyHist"/>
  <affected-histogram name="MyHist.A"/>
</histogram_suffixes>
```

The complete list of histograms will be:

  ```
  MyHist.A
  MyHist.B
  MyHist.C
  MyHist.D
  MyHist.A.C
  MyHist.A.D
  ```

## Private histograms

Googlers: There are also a small number of private internal histograms found at
http://cs/file:chrome/histograms.xml - but prefer this file for new entries.