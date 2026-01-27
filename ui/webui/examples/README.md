The `webui_examples` is a standalone example app that hosts
chrome://webui-gallery without needing to build all of Chrome. This provides
parity with `views_examples` in `//ui/views/examples` for WebUI components.

It can also be run with `--webshell`, which is a //content embedder that paints
the browser UI in a WebUI.

*To run the WebUI Examples Standalone App:*

`<output_dir>/webui_examples`

*To run the WebUI Browser (WebShell):*

`<output_dir>/webui_examples --webshell`
