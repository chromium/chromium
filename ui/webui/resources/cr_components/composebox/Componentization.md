# Componentization of Composebox

## Instruction for CL Authors

If you are reading this, you likely have reached here because your CL updates
`composebox.ts` and there is an IFTTT warning about this file.

If your change does not introduce any new logic (e.g., formatting the file or a
cosmetic change), please add `NO_IFTTT=no logic added to composebox` tag and
continue the review.

Otherwise, please follow the steps below to resolve the warning. For more
information about why we are doing this, please read the
[Context section](#context).

## Steps to Follow

There are two cases:

1. Your methods are used by multiple (>= 2) surfaces.
2. Your methods are used only by a specific surface (Cobrowse, NTP, or Omnibox).

Once one of the sets of steps below is taken, please add
`NO_IFTTT=added a componentization reviewer` tag and continue the review.

If you are unsure about what to do, please add an owner from the OWNERS file to
the reviewers list with a comment containing your question.

### Case 1: Shared Logic/Methods

1. Move the logic to `ComposeboxEmbedderMixin` in
   `ui/webui/resources/cr_components/composebox/composebox_mixin.ts` and remove
   the logic from `composebox.ts`.

2. Add any of the owners to the reviewer list.

### Case 2: Surface-specific Logic/Methods

Please do the following depending on which surface uses your methods and then
add an owner from the OWNERS file to the reviewers list.

#### Cobrowse/Contextual Tasks

Please add a comment
`"// TODO: crbug.com/486707842 - Move to the Contextual Tasks embedder"` to the
methods added, and then add `jamesleung@` to the cc list.

#### NTP

Please add a comment `"// TODO: crbug.com/486707841 - Move to the NTP embedder"`
to the methods added and then add `jonnalad@` to the cc list.

#### Omnibox

Please update `OmniboxComposeboxElement` in
`chrome/browser/resources/omnibox_popup/omnibox_composebox.ts` instead of
`composebox.ts`.

______________________________________________________________________

## Context

The `cr-composebox` component was originally designed as a monolithic component.
As more surfaces (such as NTP Realbox, Omnibox, and Cobrowse) began to use and
customize it, the file `composebox.ts` accumulated surface-specific logic. This
led to a complex, hard-to-maintain codebase where changes for one surface could
inadvertently affect others, and the file became a bottleneck for development.

## Decision

We decided to componentize the `cr-composebox` architecture to separate shared
logic from surface-specific implementations.

1. **Shared Logic**: Core behaviors and common features are extracted into
   `ComposeboxEmbedderMixin`.
2. **Surface-Specific Logic**: Features unique to a specific surface should be
   implemented in that surface's specific embedder.
3. **Ownership**: Members listed in OWNERS file should ensure that changes are
   properly reviewed. It is these owners' responsibility to enforce the
   separation of logic above.
4. **Enforcement (Temporary)**: We added an IFTTT (If This Then That) check on
   `composebox.ts` to ensure that new methods are properly evaluated and placed
   either in the mixin or marked for moving to an embedder. This IFTTT should be
   removed when `composebox.ts` is removed at the end of the componentization
   project.

## Consequences

### Positive

- **Modularity**: Clear separation of concerns between shared logic and
  surface-specific UI.
- **Maintainability**: Reduced risk of regressions when updating specific
  surfaces.
- **Scalability**: Easier to add new surfaces or features without cluttering the
  shared codebase.

### Negative/Friction

- **Developer Friction**: Authors modifying `composebox.ts` must follow extra
  steps, add specific TODOs, and seek review from the componentization team.
- **Complexity**: Moving to a mixin-based architecture introduces some
  indirection compared to a single monolithic file.
