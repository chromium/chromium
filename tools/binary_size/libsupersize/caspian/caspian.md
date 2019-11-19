# Caspian

## What is it?

Caspian is the name for the WebAssembly portion of the SuperSize Tiger Viewer.

## How do I build it?

1. Apply the patch file at `tools/binary_size/libsupersize/caspian/wasmbuild.patch`

2. From `src/`, run
`gn gen out/caspian &&
third_party/depot_tools/autoninja -C out/caspian tools/binary_size:caspian_web -v &&
cp out/caspian/wasm/caspian_web.* tools/binary_size/libsupersize/static/`

  * If doing a release build, you should change your gn args: set is\_debug=false

3. You can then launch a local instance using `tools/binary_size/supersize start_server out.size`

