# Color Pipeline

This directory implements the cross-platform
[Color Pipeline](http://go/chrome-color-pipeline) machinery, including all core
classes and the //ui-level mixers/recipes. Embedders may add more mixers/recipes
to define additional colors or modify how these appear (for an example in
Chrome, see the [chrome/browser/ui/color/](/chrome/browser/ui/color/)
directory).

To the greatest degree possible, colors in Chromium should be identified using
[`ColorId`s](color_id.h) and their physical values obtained from an appropriate
[`ColorProvider`](color_provider.h); direct use of `SkColor` outside
[`ColorRecipe`s](color_recipe.h) should be limited to colors which are
necessarily transient, e.g. colors based on a current animation state or colors
sampled dynamically from playing media.
