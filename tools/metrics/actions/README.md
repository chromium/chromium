# User Action Guidelines

This document gives the best practices on how to use user actions in code and
how to document them for the dashboard.  User actions come with only a name and
a timestamp.  They are best used when you care about a sequence--which actions
happen in what order.  If you don't care about the order, you should be using
histograms (likely enumerated histograms).

Often, you want both user actions and histogram logging in your code.  They
enable different analyses.  They're complementary.

[TOC]

## Coding (Emitting to User Actions)

Generally you'll want to call `base::RecordAction()`, which is defined in
[user_metrics.h](https://cs.chromium.org/chromium/src/base/metrics/user_metrics.h).


### Emit at a High-Level, not Deep in the Implementation

Prefer to emit at the highest level reasonable, closest to the code that handles
the UI interaction.  Emitting deep in implementation code can cause problems
because that code may get reused (and thus called more times in more places) or
may get called fewer times (due to caching for example).  In cases like this,
the logged user action will not longer correspond with a meaningful action by
the user.

### Don't Use Same String in Multiple Places

Generally a logged user action should correspond with a single, uh, action by
the user. :-) As such, they should probably only appear in a single place in the
code.  If the same user action needs to be logged in multiple places, consider
whether you should be using different user action names for these separate call
paths.

That said, if you truly need to record the same user action in multiple places,
that's okay.  Use a compile-time constant of appropriate scope that can be
referenced everywhere.  Using inline strings in multiple places can lead to
errors if you ever need to revise the name and you update one one location and
forget another.

### Efficiency

Due to the practices about when and how often to emit a user action, actions
should not be emitted often enough to cause efficiency issues.  (If actions are
emitted often enough to cause a problem, they're not being emitted at
appropriate times.  See advice below.)

## Emitting Strategies

### Emit Once Per Action

A user action should be tied to an actual action taken by a user.  Each
meaningful unit of action should cause one emit.

### Do Not Emit Redundantly

Generally a meaningful user action should cause only one emit.  For example, if
the browser already has a "Back" user action, it's poor practice to add a
"BackViaKeyboardShortcut" user action.  This is mostly redundant.  (If you're
trying to determine the breakdown of keyboard-shortcut backs versus all backs,
use a histogram.)

### Do Not Emit Excessively

Again, choose an appropriately-sized meaningful unit.  For example, emit
"DragScrolled" for a whole scroll action.  Don't emit this action every time the
user pauses scrolling if the user remains in the process of scrolling (mouse
button still down).

As another example, you may want to emit "FocusOmnibox" (upon focus),
"OmniboxEditInProgress" (upon first keystroke), and "OmniboxUse" (upon going
somwhere) but forswear "OmniboxKeystroke".  That's probably more detailed than
you need.

### Generally, Do Not Emit Impressions

It's okay to emit user actions such as "ShowTranslateInfobar" or
"DisplayedImageLinkContextMenu".  However, more detailed impression information,
especially those not caused by the user (as in the second example) and not as
attention-grabbing (as in the first example), is often not useful for analyzing
sequences of user actions.  For example, don't emit
"ShowedSecureIconNextToOmnibox".

## Testing

Test your user actions using *chrome://user-actions*.  Make sure they're being
emitted when you expect and not emitted at other times.

If this is a general UI surface, please try to check every platform.  In
particular, check Windows (Views-based platforms), Mac (non-Views), Android
phone (yet other UI wrapper code), Android tablet (often triggers lookalike but
different menus), and iOS (yet more different UI wrapper code).

Also, check that your new user action is not mostly redundant with other user
actions (see [advice above](#Do-Not-Emit-Redundantly)) and not emitted
excessively (see [advice above](#Do-Not-Emit-Excessively)).

In addition to testing interactively, you can have unit tests check the number
of times a user action was emitted.  See [user_action_tester.h](https://cs.chromium.org/chromium/src/base/test/metrics/user_action_tester.h)
for details.

## Interpreting the Resulting Data

The top of [go/uma-guide](http://go/uma-guide) has good advice on how to go
about analyzing and interpreting the results of UMA data uploaded by users.  If
you're reading this page, you've probably just finished adding a user action to
the Chromium source code and you're waiting for users to update their version of
Chrome to a version that includes your code.  In this case, the best advice is
to remind you that users who update frequently / quickly are biased.  Best take
the initial statistics with a grain of salt; they're probably *mostly* right but
not entirely so.

## Revising User Actions

When changing the semantics of a user action (when it's emitted), make it into
a new user action with a new name.  Otherwise the dashboard will be mixing two
different interpretations of the data and make no sense.

## Documenting User Actions

Document user actions in [actions.xml](./actions.xml).  There is also a
[google-internal version of the file](http://go/chrome-user-actions-internal)
for the rare case when the user action is confidential (added only to Chrome
code, not Chromium code; or, an accurate description about how to interpret the
user action would reveal information about Google's plans).

### Add User Actions and Documentation in the Same Changelist

If possible, please add the actions.xml description in the same changelist in
which you add the user-action-emitting code.  This has several benefits.  One,
it sometimes happens that the actions.xml reviewer has questions or concerns
about the user action description that reveal problems with interpretation of
the data and call for a different recording strategy.  Two, it allows the user
action reviewer to easily review the emission code to see if it comports with
these best practices, and to look for other errors.
 
### Understandable to Everyone

User actions descriptions should be understandable to someone not familiar with
your feature.  Please add a sentence or two of background if necessary.

It is good practice to note caveats associated with your user actions in this
section, such as which platforms are supported (if the set of supported
platforms is surprising).  E.g., a desktop feature that happens not to be logged
on Mac.

### State When It Is Emitted

User action descriptions should clearly state when the action is emitted.

### Owners

User actions need to be owned by a person or set of people.  These indicate who
the current experts on it are.  Being the owner means you are responsible for
answering questions about it, handling the maintenance if there are functional
changes.  The owners should be added in the original user action description.
If you are using a user action heavily and understand it intimately, feel free
to add yourself as an owner. @chromium.org email addresses are preferred.

### Beware `not_user_triggered="true"`

actions.xml allows you to annotate an action as `not_user_triggered="true"`.  This
feature should be used rarely.  If you think you want to annotate your action
thusly, please re-review the best practices above.

## Deleting User Action Entries

Do not delete actions from actions.xml.  Instead, mark unused user actions as
obsolete, annotating them with the associated date or milestone in the obsolete
tag entry.  If your user action is being replaced by a new version, we suggest
noting that in the previous user action's description.

A changelist that marks a user action as obsolete should be reviewed by all
current owners.

Deleting user action entries would be bad if someone accidentally reused your
old user action name and which therefore corrupts new data with whatever old
data is still coming in.  It's also useful to keep obsolete user action
descriptions in actions.xml--that way, if someone is searching for a user action
to answer a particular question, they can learn if there was a user action at
some point that did so even if it isn't active now.
