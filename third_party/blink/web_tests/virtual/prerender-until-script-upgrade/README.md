This directory is for testing the prerender-until-script to prerender
upgrade (https://crbug.com/477974765), where a prerender-until-script
host that has paused its inline JavaScript is "upgraded" to a full
prerender host when a matching prerender speculation rule is added
for the same URL, causing the paused script to run during prerendering.

The virtual suite enables the content-side `PrerenderUntilScriptUpgrade`
feature, which is disabled by default.
