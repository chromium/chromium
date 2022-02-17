# Measure Bug Activity

Developers do a lot of work to help with a bug that may not directly lead to
fixing the bug. This can include triaging a bug (e.g. set the correct component
on the bug, find the right owner for the bug, loop somebody else in to
investigate the bug, etc.), requesting more information from the OP,
investigating to help the bug owner resolve the bug, etc. We want to be able to
identify and measure such activities, so that these efforts can be acknowledged
and rewarded.

The `user-activity.js` script helps measure the bug-activity of a developer in
various monorail projects. Example usage:

```
      nodejs tools/crbug/user-activity.js           \
         -u sadrul@chromium.org,sadrul@google.com   \
         -p chromium,skia,v8,angleproject           \
         -s 2022-01-01
```

The following command line flags are supported:

- `-u`: The user-email(s) for the developers. If the user uses multiple
  accounts (e.g. @google.com, @chromium.org, etc.), then specify all of them
  comma-separated.

- `-p`: The project(s) the developer participated in. If the developer
  participated in the bug-tracker for multiple projects (e.g. chromium,
  angleproject, v8, etc.), then list all of them comma-separated. Default is
  just `chromium`.

- `-s`: The starting date from when the activities should be counted.


## Activity
Following are some notes on how activity is measured:

- Changing any of the following in a bug counts as an activity:
  - Label
  - Component
  - Owner, cc
  - Status
  - Priority
  - Blocked on / Blocking
- Leaving a comment on the bug counts as an activity.
- Starting a pinpoint job for a bug counts as a bug activity.
- Only one activity is counted for a bug for a contributor per day. To explain:
   - *Why per day?* Contributors often spend a significant amount of effort
     resolving a bug spanning across several days. We want to encourage and
     reward such efforts. So the bug-activity is counted for each day the
     contributor is active on the bug.
   - *Why count only one per day?* Sometimes when triaging a bug (or when
     filing a bug), multiple updates are made to the bug in quick succession
     (e.g.  Oops, forgot to include a component, or cc uberswe@ for real this
     time, etc.).  So to avoid overcounting such activities, only one activity
     is counted on a bug per day.
- To avoid overcounting bulk-updates, updates made too close to each other
  (within seconds) are merged into a single activity.

