Tests the getComputedStyle pseudo-element codepath with the
CSSOMGetComputedStylePseudoElementRequiresColon feature disabled,
which reflects the default user experience. This ensures the old
behavior (pseudo-element arguments without colons are resolved) is
preserved when the flag is not enabled.
