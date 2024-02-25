load("@prelude//:build_mode.bzl", "BuildModeInfo")

def _build_mode_impl(ctx: AnalysisContext) -> list[Provider]:
    return [
        DefaultInfo(),
        BuildModeInfo(cell = ctx.attrs.cell),
    ]

build_mode = rule(
    impl = _build_mode_impl,
    attrs = {
        "cell": attrs.string(),
    },
)
