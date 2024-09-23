class TestOperation {
  async run(data) {
    await sharedStorage.set('key0-set-from-worklet', 'value0');
    await sharedStorage.set(
        'key1-set-from-worklet', 'value1', {ignoreIfPresent: false});
    await sharedStorage.set(
        'key2-set-from-worklet', 'value2', {ignoreIfPresent: true});
    await sharedStorage.append('key1-set-from-worklet', 'value1');
    await sharedStorage.delete('key2-set-from-worklet', 'value2');
    console.log(await sharedStorage.length());
    console.log(await sharedStorage.remainingBudget());
    for await (const key of sharedStorage.keys()) {
      console.log(key);
    }
    for await (const [key, value] of sharedStorage.entries()) {
      console.log(key + ';' + value);
    }
  }
}

class TestURLSelectionOperation {
  async run(urls, data) {
    if (await sharedStorage.get('key0-set-from-worklet') === 'value0' &&
        await sharedStorage.get('key0-set-from-document') === 'value0') {
      return 1;
    }

    return -1;
  }
}

class ClearOperation {
  async run(data) {
    sharedStorage.clear();
  }
}

register('test-operation', TestOperation);
register('test-url-selection-operation', TestURLSelectionOperation);
register('clear-operation', ClearOperation);
