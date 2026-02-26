Tests with the CSSLineClampLineBreakingEllipsis feature disabled.

This feature makes the (-webkit-)line-clamp ellipsis be taken into account when
line breaking, as well as making it be taken into account for bidi reordering
and text alignment/justification. With it disabled, the ellipsis behaves
similarly to `text-overflow: ellipsis`.
