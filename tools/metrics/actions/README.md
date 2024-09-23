# User Action Guidelines

[TOC]

This document covers the best practices on using user actions in code and
documenting them for the dashboard. User actions come with only a name and
a timestamp. They are best used when you care about a sequence—which actions
happen in what order. If you don't care about the order, you should be using
[histograms](https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md)
(likely [enumerated histograms](https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#Enum-Histograms)).

Often, you want both user actions and histogram logging in your code. They
enable different analyses. They're complementary.

## Coding (emitting to user actions)

Generally you should call `base::RecordAction()`, which is defined in
[user_metrics.h](https://cs.chromium.org/chromium/src/base/metrics/user_metrics.h).


### Emit at a high level, not deep in the implementation

Prefer to emit at the highest level reasonable, closest to the code that handles
the UI interaction. Emitting deep in implementation code can cause problems
because that code may get reused (and thus called more times in more places) or
may get called fewer times (due to caching for example). In cases like this,
the logged user action would no longer correspond to a meaningful action
performed by the user.

### Don't use the same string in multiple places

Generally a logged user action should correspond to a single event. Thus, the
logging should probably only appear in a single place in the code. If the same
user action needs to be logged in multiple places, consider whether you should
be using different user action names for these separate call paths.

In rare cases, the same user action can be recorded in multiple places as long
as only one of the places can be reached. This may be necessary if the user
action is logged in platform-specific code or if one implementation is being
replaced with another. When recording an action in multiple places, use a
compile-time constant of appropriate scope that can be referenced everywhere.
Using inline strings in multiple places can lead to errors if you ever need to
revise the name and you update one location but forget another.

### Efficiency

Due to the practices about when and how often to emit a user action, actions
should not be emitted often enough to cause efficiency issues. (If actions are
emitted often enough to cause a problem, they're not being emitted at
appropriate times. See advice below.)

## Emitting strategies

### Emit once per action

A user action should be tied to a single event, such as a user doing something
or a user seeing something new. Each meaningful unit should cause one emit.
For example, showing the history page is a meaningful unit; querying the
history database—which might need to be queried multiple times to fill
the page—is probably not a meaningful unit for most use cases.

### Try to avoid redundant emits

A meaningful user action should usually cause only one emit. This advice is
mainly because user action sequences are easier to analyze without redudancy.

For example, if the browser already has a "Back" user action, avoid adding a
"BackViaKeyboardShortcut" user action—it's mostly redundant—unless it's
necessary because you care about how the different types of Back button work in
sequences of user actions. If you don't care about how BackViaKeyboardShortcut
works in a sequence and only want to count them or determine the breakdown of
keyboard-shortcut backs versus all backs, use a histogram.

### Do not emit excessively

Again, choose an appropriately-sized meaningful unit. For example, emit
"DragScrolled" for a whole scroll action. Don't emit this action every time the
user pauses scrolling if the user remains in the process of scrolling (mouse
button still down). However, if you need to understand the sequence of partial
scrolls, emitting this for each scroll pause is acceptable.

As another example, you may want to emit "FocusOmnibox" (upon focus),
"OmniboxEditInProgress" (upon first keystroke), and "OmniboxUse" (upon going
somewhere) but skip "OmniboxKeystroke". That's probably more detailed than
you need.

### Emitting impressions is okay

It's okay to emit user actions such as "ShowTranslateInfobar" or
"DisplayedImageLinkContextMenu". Remember to mark them as
[not_user_triggered](#not-user-triggered) and please try to make sure they're
not excessive. For example, don't emit "ShowedSecureIconNextToOmnibox" because
that's likely to appear on most pages. That said, if you need
ShowedSecureIconNextToOmnibox logged in order to analyze a sequence of user
actions that include it, go ahead.

## Testing

Test your user actions using `chrome://user-actions`. Make sure they're being
emitted when you expect and not emitted otherwise.

If this is a general UI surface, please try to check every platform. In
particular, check Windows (Views-based platforms), Android phone (yet other UI
wrapper code), Android tablet (often triggers look-alike but different menus),
and iOS (yet more different UI wrapper code).

Also, check that your new user action is not mostly redundant in light of
existing user actions (see [advice above](#Try-to-avoid-redundant-emits)) and
not emitted excessively (see [advice above](#Do-not-emit-excessively)).

In addition to testing interactively, unit tests can check the number of times a
user action was emitted. See [user_action_tester.h](https://cs.chromium.org/chromium/src/base/test/metrics/user_action_tester.h)
for details.

See also `chrome://metrics-internals` ([docs](https://chromium.googlesource.com/chromium/src/+/master/components/metrics/debug/README.md))
for more thorough manual testing if needed.

### Verify Action Suffixes

If you have <action-suffix> entries that need to be updated to match code,
you can use
[ActionSuffixReader](https://cs.chromium.org/chromium/src/base/test/metrics/action_suffix_reader.h)
to read and verify the expected values in a unit test. This prevents a mismatch
between code and action data from slipping through CQ.

For an example, see
[BrowserUserEducationServiceTest.CheckFeaturePromoActions](https://cs.chromium.org/chromium/src/chrome/browser/ui/views/user_education/browser_user_education_service_unittest.cc).

## Interpreting the resulting data

The top of [go/uma-guide](http://go/uma-guide) has good advice on how to go
about analyzing and interpreting the results of UMA data uploaded by users. If
you're reading this page, you've probably just finished adding a user action to
the Chromium source code and you're waiting for users to update their version of
Chrome to a version that includes your code. In this case, the best advice is
to remind you that users who update frequently or quickly are biased. Best take
the initial statistics with a grain of salt; they're probably *mostly* right but
not entirely so.

## Revising user actions

When changing the semantics of a user action (when it's emitted), make it into
a new user action with a new name. Otherwise the dashboard will mix two
different interpretations of the data and make no sense.

## Documenting user actions

Document user actions in [actions.xml](./actions.xml). There is also a
[Google-internal version of the file](http://go/chrome-user-actions-internal)
for user actions that exist in Google-internal codebases. Confidential actions
are added only to Chrome code, not Chromium code.

### Add user actions and documentation in the same changelist

If possible, please add the actions.xml description in the same changelist in
which you add the user-action-emitting code. This has several benefits. One,
it sometimes happens that the actions.xml reviewer has questions or concerns
about the user action description that reveal problems with interpretation of
the data and call for a different recording strategy. Two, it allows the user
action reviewer to easily review the emission code to see if it comports with
these best practices, and to look for other errors.

### Understandable to everyone

User action descriptions should be understandable to someone not familiar with
your feature. Please add a sentence or two of background if necessary.

It's a good practice to note caveats associated with your user actions in this
section, such as which platforms are supported (if the set of supported
platforms is surprising, such as a desktop feature that happens to not be logged
on Mac).

### State when it is emitted

User action descriptions should clearly state when the action is emitted.

### Owners

Each user action needs owners, who are the current expert on the metric. Owners
are responsible for answering questions about the metric, handling any
maintenance tasks, and deprecating the metric if it has outlived its usefulness.
If you are using a metric heavily and understand it intimately, feel free to add
yourself as an owner. @chromium.org email addresses are preferred.

The primary owner must be an individual, who is ultimately responsible for the
metric. It's a best practice to list multiple owners, which makes it less likely
that maintenance tasks will slip through the cracks. This is important because
the metrics team may file bugs related to user actions, and such bugs need to be
triaged by someone familiar with the metric. If an appropriate mailing list or
team email is available, it's a good idea to list it as a secondary owner.

### Set `not_user_triggered="true"` when appropriate {#not-user-triggered}

actions.xml allows you to annotate an action as `not_user_triggered="true"`.
Use it when appropriate. For example, showing a notification is not user
triggered. However, please remember: Before adding something marked as
`not_user_triggered="true"`, consider whether you need to analyze sequences of
actions. If not, please use a histogram to count these events instead.

## Cleaning up user action entries

Do not delete actions from actions.xml. Instead, mark unused user actions as
obsolete, annotating them with the associated date or milestone in the obsolete
tag entry.

If the user action is being replaced by a new version:

* Note in the `<obsolete>` message the name of the replacement action.

* Make sure the descriptions of the original and replacement user actions
  are different. It's never appropriate for them to be identical. Either
  the old description was wrong, and it should be revised to explain what
  it actually measured, or the old user action was measuring something not
  as useful as the replacement, in which case the new user action is
  measuring something different and needs to have a new description.

A changelist that marks a user action as obsolete should be reviewed by all
current owners.

Deleting user action entries would be bad if someone accidentally reused your
old user action name. If this happened, new data would be corrupted by whatever
old data was still coming in. It's also useful to keep obsolete user action
descriptions in actions.xml. That way, someone searching for a user action to
answer a particular question can learn if there was a user action at some point
that did so—even if it isn't active now.
