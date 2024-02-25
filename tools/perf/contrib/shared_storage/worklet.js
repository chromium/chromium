// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class RunSetOperation {
  async run(data) {
    if (data && data.hasOwnProperty('key') && data.hasOwnProperty('value')) {
      await sharedStorage.set(data['key'], data['value']);
    } else {
      console.error('received data: ' + JSON.stringify(data));
    }
  }
}

class SelectURLSetOperation {
  async run(urls, data) {
    if (data && data.hasOwnProperty('key') && data.hasOwnProperty('value')) {
      await sharedStorage.set(data['key'], data['value']);
      return 0;
    }
    return -1;
  }
}

class RunAppendOperation {
  async run(data) {
    if (data && data.hasOwnProperty('key') && data.hasOwnProperty('value')) {
      await sharedStorage.append(data['key'], data['value']);
    }
  }
}

class SelectURLAppendOperation {
  async run(urls, data) {
    if (data && data.hasOwnProperty('key') && data.hasOwnProperty('value')) {
      await sharedStorage.append(data['key'], data['value']);
      return 0;
    }
    return -1;
  }
}

class RunDeleteOperation {
  async run(data) {
    if (data && data.hasOwnProperty('key')) {
      await sharedStorage.delete(data['key']);
    }
  }
}

class SelectURLDeleteOperation {
  async run(urls, data) {
    if (data && data.hasOwnProperty('key')) {
      await sharedStorage.delete(data['key']);
      return 0;
    }
    return -1;
  }
}

class RunClearOperation {
  async run() {
    await sharedStorage.clear();
  }
}

class SelectURLClearOperation {
  async run(urls) {
    await sharedStorage.clear();
    return 0;
  }
}

class RunGetOperation {
  async run(data) {
    if (data && data.hasOwnProperty('key')) {
      console.log(await sharedStorage.get(data['key']));
    }
  }
}

class SelectURLGetOperation {
  async run(urls, data) {
    if (data && data.hasOwnProperty('key')) {
      console.log(await sharedStorage.get(data['key']));
      return 0;
    }
    return -1;
  }
}

class RunLengthOperation {
  async run() {
    const length = await sharedStorage.length();
    console.log(length);
  }
}

class SelectURLLengthOperation {
  async run(urls) {
    const length = await sharedStorage.length();
    console.log(length);
    return 0;
  }
}

class RunKeysOperation {
  async run() {
    for await (const key of sharedStorage.keys()) {
      console.log(key);
    }
  }
}

class SelectURLKeysOperation {
  async run(urls) {
    for await (const key of sharedStorage.keys()) {
      console.log(key);
    }
    return 0;
  }
}

class RunEntriesOperation {
  async run() {
    for await (const [key, value] of sharedStorage.entries()) {
      console.log(key + ';' + value);
    }
  }
}

class SelectURLEntriesOperation {
  async run(urls) {
    for await (const [key, value] of sharedStorage.entries()) {
      console.log(key + ';' + value);
    }
    return 0;
  }
}

class RunRemainingBudgetOperation {
  async run() {
    const remainingBudget = await sharedStorage.remainingBudget();
    console.log(remainingBudget);
  }
}

class SelectURLRemainingBudgetOperation {
  async run(urls) {
    const remainingBudget = await sharedStorage.remainingBudget();
    console.log(remainingBudget);
    return 0;
  }
}

register('run-set-operation', RunSetOperation);
register('selecturl-set-operation', SelectURLSetOperation);
register('run-append-operation', RunAppendOperation);
register('selecturl-append-operation', SelectURLAppendOperation);
register('run-delete-operation', RunDeleteOperation);
register('selecturl-delete-operation', SelectURLDeleteOperation);
register('run-clear-operation', RunClearOperation);
register('selecturl-clear-operation', SelectURLClearOperation);
register('run-get-operation', RunGetOperation);
register('selecturl-get-operation', SelectURLGetOperation);
register('run-length-operation', RunLengthOperation);
register('selecturl-length-operation', SelectURLLengthOperation);
register('run-keys-operation', RunKeysOperation);
register('selecturl-keys-operation', SelectURLKeysOperation);
register('run-entries-operation', RunEntriesOperation);
register('selecturl-entries-operation', SelectURLEntriesOperation);
register('run-remainingbudget-operation', RunRemainingBudgetOperation);
register(
    'selecturl-remainingbudget-operation', SelectURLRemainingBudgetOperation);
