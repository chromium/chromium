This suite runs the prefetch and speculation-tags web tests with the
SpeculationRulesRendererSideHeuristics feature enabled, exercising the
renderer-driven enactment path where the pointerdown/hover/viewport heuristics
select and enact non-immediate candidates via SpeculationHost.EnactCandidate.

Scoped to the directories that use pointer-triggered, non-immediate speculation;
the prerender restriction/window.open tests don't exercise this feature and are
timing-sensitive, so they're intentionally excluded. See
https://crbug.com/532860179.
