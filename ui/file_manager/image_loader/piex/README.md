Install emscripten https://emscripten.org/docs/getting_started/index.html

```shell
  git clone https://github.com/emscripten-core/emsdk.git
  cd emsdk
  ./emsdk install 1.38.33
  ./emsdk activate 1.38.33
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
  git commit -a -m "Release piexwasm at emscripten <version> ..."
  git cl upload
```

Release Builder: emscripten 1.38.33
