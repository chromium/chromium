// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Liblouis wrapper.
 */

/**
 * A utility class that acts as a memory pool and wraps the wasm module.
 */
function WasmMemPool(module) {
  this.module = module;

  this.ptrs_ = [];
}

WasmMemPool.prototype = {
  malloc: function(bytes) {
    const ptr = this.module._malloc(bytes);
    this.ptrs_.push(ptr);
    return ptr;
  },

  allocate: function(array, type) {
    const ptr = this.module.allocate(array, this.module.ALLOC_NORMAL);
    this.ptrs_.push(ptr);
    return ptr;
  },

  freeAll: function() {
    this.ptrs_.forEach((ptr) => {
      this.module._free(ptr);
    });
    this.ptrs_ = [];
  }
};

function LiblouisWrapper() {
  self.addEventListener('message', (message) => {
    const command = JSON.parse(message.data);
    switch (command.command) {
      case 'CheckTable':
        this.checkTable(command);
        break;
      case 'Translate':
        this.translate(command);
        break;
      case 'BackTranslate':
        this.backTranslate(command);
        break;
      case 'load':
        this.initWasm(command);
        break;
    }
  });

  importScripts('liblouis_wasm.js');
}

LiblouisWrapper.prototype = {
  initWasm: function(message) {
    if (!self.liblouisBuild) {
      setTimeout(this.initWasm.bind(this, message), 20);
      return;
    }
    liblouisBuild().then((module) => {
      this.module = module;
      this.pool_ = new WasmMemPool(this.module);

      const reply = {
        loaded: true,
        success: true,
        in_reply_to: message['message_id']
      };
      self.postMessage(JSON.stringify(reply));
    });
  },


  checkTable: function(command) {
    // Free any loaded tables.
    this.module._lou_free();

    const tableNames = command['table_names'];
    const tableNamesPtr =
        this.pool_.allocate(this.module.intArrayFromString(tableNames));
    const tableCount = this.module._lou_checkTable(tableNamesPtr);
    this.pool_.freeAll();
    const msg = {in_reply_to: command['message_id'], success: tableCount > 0};
    self.postMessage(JSON.stringify(msg));
  },

  translate: function(command) {
    this.translateOrBackTranslate_(
        command['table_names'], command['text'], command['message_id'],
        command['form_type_map']);
  },

  backTranslate: function(command) {
    this.translateOrBackTranslate_(
        command['table_names'], command['cells'], command['message_id'], null,
        true);
  },

  translateOrBackTranslate_: function(
      tableNames, contents, messageId, formTypeMap, backTranslate) {
    const tableNamesPtr =
        this.pool_.allocate(this.module.intArrayFromString(tableNames));

    let formTypeMapPtr = 0;
    if (formTypeMap) {
      formTypeMapPtr = this.pool_.malloc(formTypeMap.length * 4);
      for (let i = 0; i < formTypeMap.length; i++) {
        this.module.setValue(formTypeMapPtr + i * 4, formTypeMap[i], 'i32');
      }
    }

    // |tableNamesPtr| is a char* natively.

    // The backtranslated string is encoded as 2-hex characters, which equal one
    // byte. The forward translated string is an ordinary js string. Both
    // require a null terminator.
    const inLen =
        backTranslate ? (contents.length / 2 + 1) : (contents.length + 1);

    // |inBufPtr| and |outBufPtr| are both widechar*. (i.e. 2-byte characters).
    const inBufPtr = this.pool_.malloc(inLen * 2);

    if (backTranslate) {
      // |contents| is a hex encoded string. Two characters encodes a byte.
      if (contents.length % 2 != 0) {
        throw 'Expected contents to be of even length.';
      }

      for (let i = 0; i < contents.length; i = i + 2) {
        // Always set the high order bit to ensure empty cells are not ignored.
        let twoBytes = 0x8000;
        twoBytes |= parseInt(contents[i], 16) << 4;
        twoBytes |= parseInt(contents[i + 1], 16);
        this.module.setValue(inBufPtr + i, twoBytes, 'i16');
      }
    } else {
      // This method takes its length in bytes.
      this.module.stringToUTF16(contents, inBufPtr, inLen * 2);
    }

    // Liblouis expects a null terminator.
    this.module.setValue(inBufPtr + (inLen - 1) * 2, 0, 'i16');

    // LibLouis writes how many characters of |inBuf| are consumed into this int
    // pointer.
    const inLenPtr = this.pool_.malloc(4);

    // We need to gradually increase |outLen| since we can't precompute the
    // length given by liblouis.
    let outLen = inLen;
    const maxAlloc = (inLen + 1) * 8;
    let msg;
    while (outLen < maxAlloc) {
      // This is required as consecutive tries to [back]Translate requires
      // resetting the value of this int pointer.
      this.module.setValue(inLenPtr, inLen, 'i32');

      // A widechar*.
      const outBufPtr = this.pool_.malloc(outLen * 2);
      const outLenPtr = this.pool_.malloc(4);
      this.module.setValue(outLenPtr, outLen, 'i32');
      let brailleToTextPtr;
      let textToBraillePtr;
      if (backTranslate) {
        this.module._lou_backTranslateString(
            tableNamesPtr, inBufPtr, inLenPtr, outBufPtr, outLenPtr, 0, 0,
            4 /* dots */);
      } else {
        // These two refer to an array of integers.
        brailleToTextPtr = this.pool_.malloc(outLen * 4);
        textToBraillePtr = this.pool_.malloc(outLen * 4);

        this.module._lou_translate(
            tableNamesPtr, inBufPtr, inLenPtr, outBufPtr, outLenPtr,
            formTypeMapPtr, 0, textToBraillePtr, brailleToTextPtr, 0,
            4 /* dots */);
      }

      // If the entire inBuf was not consumed, it means outBuf was not large
      // enough, so we need to try again. LibLouis is loose with its |inLenPtr|
      // values. It sometimes consumes the null terminator, it sometimes
      // doesn't.
      const actualInLen = this.module.getValue(inLenPtr, 'i32');
      const actualOutLen = this.module.getValue(outLenPtr, 'i32');
      if ((inLen - 1) <= actualInLen && actualOutLen > 0) {
        msg = {in_reply_to: messageId, success: true};
        if (backTranslate) {
          let outBuf = '';
          for (let i = 0; i < actualOutLen; i++) {
            outBuf += String.fromCharCode(
                this.module.getValue(outBufPtr + i * 2, 'i16'));
          }
          msg['text'] = outBuf;
        } else {
          msg['cells'] = this.getHexEncoding_(outBufPtr, actualOutLen);
          msg['text_to_braille'] =
              this.getIntArray(textToBraillePtr, actualInLen);
          msg['braille_to_text'] =
              this.getIntArray(brailleToTextPtr, actualOutLen);
        }

        // TODO(accessibility): this check controls a workaround for a
        // regression in LibLouis 3.21. It used to work in 3.19. The issue is
        // that sometimes, LibLouis sets an empty translation result which
        // appears to be valid, but requires us to increase our output buffer
        // size to get the non-empty braille translation. Try removing on the
        // next uprev to LibLouis.
        if (backTranslate || actualInLen !== 1 || actualOutLen !== 1 ||
            msg['cells'] !== '00') {
          break;
        }
      }

      outLen = outLen * 2;
    }

    if (msg) {
      self.postMessage(JSON.stringify(msg));
    }

    this.pool_.freeAll();
  },

  getHexEncoding_: function(bufPtr, len) {
    let ret = '';
    for (let i = 0; i < len; i++) {
      // Note that pointer arithmetic here is in bytes. Each cell is encoded in
      // 16-bits.
      let byte = this.module.getValue(bufPtr + i * 2);

      // Ignore the high order bits.
      byte &= 0x00ff;
      ret += LiblouisWrapper.BYTE_TO_HEX[byte >> 4];
      ret += LiblouisWrapper.BYTE_TO_HEX[byte & 0x0f];
    }

    return ret;
  },

  getIntArray: function(ptr, len) {
    const ret = [];
    for (let i = 0; i < len; i++) {
      ret.push(this.module.getValue(ptr + i * 4, 'i32'));
    }
    return ret;
  }
};

LiblouisWrapper.BYTE_TO_HEX = '0123456789abcdef';

new LiblouisWrapper();
