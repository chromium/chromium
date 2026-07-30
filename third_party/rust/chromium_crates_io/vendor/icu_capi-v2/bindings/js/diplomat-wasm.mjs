import cfg from '../diplomat.config.mjs';
import {readString8} from './diplomat-runtime.mjs'

let wasm;

const imports = {
env: {
    diplomat_console_debug_js(ptr, len) {
        console.debug(readString8(wasm, ptr, len));
    },
    diplomat_console_error_js(ptr, len) {
        console.error(readString8(wasm, ptr, len));
    },
    diplomat_console_info_js(ptr, len) {
        console.info(readString8(wasm, ptr, len));
    },
    diplomat_console_log_js(ptr, len) {
        console.log(readString8(wasm, ptr, len));
    },
    diplomat_console_warn_js(ptr, len) {
        console.warn(readString8(wasm, ptr, len));
    },
    diplomat_throw_error_js(ptr, len) {
        throw new Error(readString8(wasm, ptr, len));
    }
}
}

if (globalThis.process?.getBuiltinModule) {
    // Node (>=22)
    const fs = globalThis.process.getBuiltinModule('fs');
    const wasmFile = new Uint8Array(fs.readFileSync(cfg['wasm_path']));
    const loadedWasm = await WebAssembly.instantiate(wasmFile, imports);
    wasm = loadedWasm.instance.exports;
} else if (globalThis.process) {
    // Node (<22)
    const fs = await import('fs');
    const wasmFile = new Uint8Array(fs.readFileSync(cfg['wasm_path']));
    const loadedWasm = await WebAssembly.instantiate(wasmFile, imports);
    wasm = loadedWasm.instance.exports;
} else {
    // Browser
    const loadedWasm = await WebAssembly.instantiateStreaming(fetch(cfg['wasm_path']), imports);
    wasm = loadedWasm.instance.exports;
}

wasm.diplomat_init();
if (cfg['init'] !== undefined) {
    cfg['init'](wasm);
}

export default wasm;
