This suite runs the prefetch and speculation-tags web tests with the
SpeculationRulesRendererSideHeuristics feature disabled, exercising the legacy
browser-driven enactment path where PreloadingDecider (rather than the renderer)
selects and enacts non-immediate candidates on pointerdown/hover/viewport.

That feature is enabled by default, so this suite covers the configuration the
kill switch falls back to. Both this suite and the feature flag should be
removed once the browser-driven path is deleted.

Scoped to the directories that use pointer-triggered, non-immediate speculation;
the prerender restriction/window.open tests don't exercise this feature and are
timing-sensitive, so they're intentionally excluded. Note that the
speculation-measurement tests require the feature to be enabled and so are
deliberately not covered here. See https://crbug.com/532860179.
