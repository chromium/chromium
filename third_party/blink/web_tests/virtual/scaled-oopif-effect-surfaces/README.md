Runs the scaled cross-site iframe backdrop-filter repaint regression in
`http/tests/html` with threaded compositing and
`SizeOopifEffectSurfacesAtExternalScale` enabled, and `TreesInViz` disabled.
This covers renderer-side effect surface sizing using the host's external scale
before commit, followed by repainting at the same scale.
