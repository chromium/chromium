Install emscripten http://lmgtfy.com/?q=install+the+emscripten+sdk

```shell
  git clone https://github.com/juj/emsdk.git
  cd emsdk
  ./emsdk install latest
  ./emsdk activate latest
  source ./emsdk_env.sh
```

Install piexwasm project components

```shell
  cd chrome/src/ui/file_manager/image_loader/piex
  npm install
```

Build piexwasm code: piex.js.wasm piex.out.wasm

```shell
  npm run build
```

Run tests: they must PASS

```shell
  npm run test
```

Release: submit piex.js.wasm piex.out.wasm to the Chromium repository

```shell
  git commit -a -m "Release piexwasm ..."
  git cl upload
```
