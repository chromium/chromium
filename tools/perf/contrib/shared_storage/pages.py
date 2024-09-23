# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contrib.shared_storage.page_set import SharedStorageStory


class SharedStorageDocumentSetStory(SharedStorageStory):
  NAME = "SharedStorageDocumentSet"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  window.sharedStorage.set('a{{ index }}', 'b{{ index }}');
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSet',
      'params': {
          'key': 'a{{ index }}',
          'value': 'b{{ index }}'
      }
  }]


class SharedStorageDocumentAppendStory(SharedStorageStory):
  NAME = "SharedStorageDocumentAppend"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  window.sharedStorage.append('a{{ index }}', 'b{{ index }}');
  window.sharedStorage.append('a{{ index }}', 'c{{ index }}');
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentAppend',
      'params': {
          'key': 'a{{ index }}',
          'value': 'b{{ index }}'
      }
  }, {
      'type': 'documentAppend',
      'params': {
          'key': 'a{{ index }}',
          'value': 'c{{ index }}'
      }
  }]


class SharedStorageDocumentDeleteStory(SharedStorageStory):
  NAME = "SharedStorageDocumentDelete"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  window.sharedStorage.set('a{{ index }}', 'b{{ index }}');
  window.sharedStorage.delete('a{{ index }}');
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSet',
      'params': {
          'key': 'a{{ index }}',
          'value': 'b{{ index }}'
      }
  }, {
      'type': 'documentDelete',
      'params': {
          'key': 'a{{ index }}'
      }
  }]


class SharedStorageDocumentClearStory(SharedStorageStory):
  NAME = "SharedStorageDocumentClear"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  window.sharedStorage.set('a{{ index }}', 'b{{ index }}')
  window.sharedStorage.clear()
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSet',
      'params': {
          'key': 'a{{ index }}',
          'value': 'b{{ index }}'
      }
  }, {
      'type': 'documentClear'
  }]
  RENAVIGATE_AFTER_ACTION = True


class SharedStorageWorkletRunSetStory(SharedStorageStory):
  NAME = "SharedStorageWorkletRunSet"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.run('run-set-operation',
                    {data: {'key': 'a{{ index }}',
                            'value': 'b{{ index }}'},
                     keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentRun'
  }, {
      'type': 'workletSet',
      'params': {
          'key': 'a{{ index }}',
          'value': 'b{{ index }}'
      }
  }]


class SharedStorageWorkletSelectURLSetStory(SharedStorageStory):
  NAME = "SharedStorageWorkletSelectURLSet"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.selectURL('selecturl-set-operation',
                          [{url: 'with_worklet.html'}],
                          {data: {'key': 'a{{ index }}',
                                  'value': 'b{{ index }}'},
                           keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSelectURL'
  }, {
      'type': 'workletSet',
      'params': {
          'key': 'a{{ index }}',
          'value': 'b{{ index }}'
      }
  }]


class SharedStorageWorkletRunAppendStory(SharedStorageStory):
  NAME = "SharedStorageWorkletRunAppend"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.run('run-append-operation',
                    {data: {'key': 'a{{ index }}',
                            'value': 'b{{ index }}'},
                     keepAlive: true});
  sharedStorage.run('run-append-operation',
                    {data: {'key': 'a{{ index }}',
                            'value': 'c{{ index }}'},
                     keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentRun'
  }, {
      'type': 'documentRun'
  }, {
      'type': 'workletAppend',
      'params': {
          'key': 'a{{ index }}',
          'value': 'b{{ index }}'
      }
  }, {
      'type': 'workletAppend',
      'params': {
          'key': 'a{{ index }}',
          'value': 'c{{ index }}'
      }
  }]


class SharedStorageWorkletSelectURLAppendStory(SharedStorageStory):
  NAME = "SharedStorageWorkletSelectURLAppend"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.selectURL('selecturl-append-operation',
                          [{url: 'with_worklet.html'}],
                          {data: {'key': 'a{{ index }}',
                                  'value': 'b{{ index }}'},
                           keepAlive: true});
  sharedStorage.selectURL('selecturl-append-operation',
                          [{url: 'with_worklet.html'}],
                          {data: {'key': 'a{{ index }}',
                                  'value': 'c{{ index }}'},
                           keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSelectURL'
  }, {
      'type': 'documentSelectURL'
  }, {
      'type': 'workletAppend',
      'params': {
          'key': 'a{{ index }}',
          'value': 'b{{ index }}'
      }
  }, {
      'type': 'workletAppend',
      'params': {
          'key': 'a{{ index }}',
          'value': 'c{{ index }}'
      }
  }]


class SharedStorageWorkletRunDeleteStory(SharedStorageStory):
  NAME = "SharedStorageWorkletRunDelete"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.set('a{{ index }}', 'any');
  sharedStorage.run('run-delete-operation',
                    {data: {'key': 'a{{ index }}'},
                     keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSet',
      'params': {
          'key': 'a{{ index }}',
          'value': 'any'
      }
  }, {
      'type': 'documentRun'
  }, {
      'type': 'workletDelete',
      'params': {
          'key': 'a{{ index }}'
      }
  }]


class SharedStorageWorkletSelectURLDeleteStory(SharedStorageStory):
  NAME = "SharedStorageWorkletSelectURLDelete"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.set('a{{ index }}', 'any');
  sharedStorage.selectURL('selecturl-delete-operation',
                          [{url: 'with_worklet.html'}],
                          {data: {'key': 'a{{ index }}'},
                           keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSet',
      'params': {
          'key': 'a{{ index }}',
          'value': 'any'
      }
  }, {
      'type': 'documentSelectURL'
  }, {
      'type': 'workletDelete',
      'params': {
          'key': 'a{{ index }}'
      }
  }]


class SharedStorageWorkletRunClearStory(SharedStorageStory):
  NAME = "SharedStorageWorkletRunClear"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.set('a{{ index }}', 'any');
  sharedStorage.run('run-clear-operation',
                    {keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSet',
      'params': {
          'key': 'a{{ index }}',
          'value': 'any'
      }
  }, {
      'type': 'documentRun'
  }, {
      'type': 'workletClear'
  }]
  RENAVIGATE_AFTER_ACTION = True


class SharedStorageWorkletSelectURLClearStory(SharedStorageStory):
  NAME = "SharedStorageWorkletSelectURLClear"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.set('a{{ index }}', 'any');
  sharedStorage.selectURL('selecturl-clear-operation',
                          [{url: 'with_worklet.html'}],
                          {keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSet',
      'params': {
          'key': 'a{{ index }}',
          'value': 'any'
      }
  }, {
      'type': 'documentSelectURL'
  }, {
      'type': 'workletClear'
  }]
  RENAVIGATE_AFTER_ACTION = True


class SharedStorageWorkletRunGetStory(SharedStorageStory):
  NAME = "SharedStorageWorkletRunGet"
  ABSTRACT_STORY = False
  SETUP_SCRIPT = """
  sharedStorage.set('k0', 'v0');
  """
  EXPECTED_SETUP_EVENTS = [{
      'type': 'documentSet',
      'params': {
          'key': 'k0',
          'value': 'v0'
      }
  }]
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.run('run-get-operation',
                    {data: {'key': 'k0'},
                     keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentRun'
  }, {
      'type': 'workletGet',
      'params': {
          'key': 'k0'
      }
  }]


class SharedStorageWorkletSelectURLGetStory(SharedStorageStory):
  NAME = "SharedStorageWorkletSelectURLGet"
  ABSTRACT_STORY = False
  SETUP_SCRIPT = """
  sharedStorage.set('k0', 'v0')
  """
  EXPECTED_SETUP_EVENTS = [{
      'type': 'documentSet',
      'params': {
          'key': 'k0',
          'value': 'v0'
      }
  }]
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.selectURL('selecturl-get-operation',
                          [{url: 'with_worklet.html'}],
                          {data: {'key': 'k0'},
                           keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSelectURL'
  }, {
      'type': 'workletGet',
      'params': {
          'key': 'k0'
      }
  }]


class SharedStorageWorkletRunLengthStory(SharedStorageStory):
  NAME = "SharedStorageWorkletRunLength"
  ABSTRACT_STORY = False
  SETUP_SCRIPT = """
  sharedStorage.set('k0', 'v0')
  sharedStorage.set('k1', 'v1')
  sharedStorage.set('k2', 'v2')
  """
  EXPECTED_SETUP_EVENTS = [{
      'type': 'documentSet',
      'params': {
          'key': 'k0',
          'value': 'v0'
      }
  }, {
      'type': 'documentSet',
      'params': {
          'key': 'k1',
          'value': 'v1'
      }
  }, {
      'type': 'documentSet',
      'params': {
          'key': 'k2',
          'value': 'v2'
      }
  }]
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.run('run-length-operation',
                    {keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentRun'
  }, {
      'type': 'workletLength'
  }]


class SharedStorageWorkletSelectURLLengthStory(SharedStorageStory):
  NAME = "SharedStorageWorkletSelectURLLength"
  ABSTRACT_STORY = False
  SETUP_SCRIPT = """
  sharedStorage.set('k0', 'v0');
  sharedStorage.set('k1', 'v1');
  sharedStorage.set('k2', 'v2');
  """
  EXPECTED_SETUP_EVENTS = [{
      'type': 'documentSet',
      'params': {
          'key': 'k0',
          'value': 'v0'
      }
  }, {
      'type': 'documentSet',
      'params': {
          'key': 'k1',
          'value': 'v1'
      }
  }, {
      'type': 'documentSet',
      'params': {
          'key': 'k2',
          'value': 'v2'
      }
  }]
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.selectURL('selecturl-length-operation',
                          [{url: 'with_worklet.html'}],
                          {keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSelectURL'
  }, {
      'type': 'workletLength'
  }]


class SharedStorageWorkletRunKeysStory(SharedStorageStory):
  NAME = "SharedStorageWorkletRunKeys"
  ABSTRACT_STORY = False
  SETUP_SCRIPT = """
  sharedStorage.set('k0', 'v0')
  sharedStorage.set('k1', 'v1')
  sharedStorage.set('k2', 'v2')
  """
  EXPECTED_SETUP_EVENTS = [{
      'type': 'documentSet',
      'params': {
          'key': 'k0',
          'value': 'v0'
      }
  }, {
      'type': 'documentSet',
      'params': {
          'key': 'k1',
          'value': 'v1'
      }
  }, {
      'type': 'documentSet',
      'params': {
          'key': 'k2',
          'value': 'v2'
      }
  }]
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.run('run-keys-operation',
                    {keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentRun'
  }, {
      'type': 'workletKeys'
  }]
  # Expect to iterate over `3 + self.SIZE` keys, each with a call to
  # `GetNextIterationResult()`, then make one last call to
  # `GetNextIterationResult()` to terminate the iteration with
  # `MakeEndOfIteration()`.
  EXPECTED_ITERATOR_HISTOGRAM_COUNT = "4 + self.SIZE"


class SharedStorageWorkletSelectURLKeysStory(SharedStorageStory):
  NAME = "SharedStorageWorkletSelectURLKeys"
  ABSTRACT_STORY = False
  SETUP_SCRIPT = """
  sharedStorage.set('k0', 'v0');
  sharedStorage.set('k1', 'v1');
  sharedStorage.set('k2', 'v2');
  """
  EXPECTED_SETUP_EVENTS = [{
      'type': 'documentSet',
      'params': {
          'key': 'k0',
          'value': 'v0'
      }
  }, {
      'type': 'documentSet',
      'params': {
          'key': 'k1',
          'value': 'v1'
      }
  }, {
      'type': 'documentSet',
      'params': {
          'key': 'k2',
          'value': 'v2'
      }
  }]
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.selectURL('selecturl-keys-operation',
                          [{url: 'with_worklet.html'}],
                          {keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSelectURL'
  }, {
      'type': 'workletKeys'
  }]
  # Expect to iterate over `3 + self.SIZE` keys, each with a call to
  # `GetNextIterationResult()`, then make one last call to
  # `GetNextIterationResult()` to terminate the iteration with
  # `MakeEndOfIteration()`.
  EXPECTED_ITERATOR_HISTOGRAM_COUNT = "4 + self.SIZE"


class SharedStorageWorkletRunEntriesStory(SharedStorageStory):
  NAME = "SharedStorageWorkletRunEntries"
  ABSTRACT_STORY = False
  SETUP_SCRIPT = """
  sharedStorage.set('k0', 'v0')
  sharedStorage.set('k1', 'v1')
  sharedStorage.set('k2', 'v2')
  """
  EXPECTED_SETUP_EVENTS = [{
      'type': 'documentSet',
      'params': {
          'key': 'k0',
          'value': 'v0'
      }
  }, {
      'type': 'documentSet',
      'params': {
          'key': 'k1',
          'value': 'v1'
      }
  }, {
      'type': 'documentSet',
      'params': {
          'key': 'k2',
          'value': 'v2'
      }
  }]
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.run('run-entries-operation',
                    {keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentRun'
  }, {
      'type': 'workletEntries'
  }]
  # Expect to iterate over `3 + self.SIZE` entries, each with a call to
  # `GetNextIterationResult()`, then make one last call to
  # `GetNextIterationResult()` to terminate the iteration with
  # `MakeEndOfIteration()`.
  EXPECTED_ITERATOR_HISTOGRAM_COUNT = "4 + self.SIZE"


class SharedStorageWorkletSelectURLEntriesStory(SharedStorageStory):
  NAME = "SharedStorageWorkletSelectURLEntries"
  ABSTRACT_STORY = False
  SETUP_SCRIPT = """
  sharedStorage.set('k0', 'v0');
  sharedStorage.set('k1', 'v1');
  sharedStorage.set('k2', 'v2');
  """
  EXPECTED_SETUP_EVENTS = [{
      'type': 'documentSet',
      'params': {
          'key': 'k0',
          'value': 'v0'
      }
  }, {
      'type': 'documentSet',
      'params': {
          'key': 'k1',
          'value': 'v1'
      }
  }, {
      'type': 'documentSet',
      'params': {
          'key': 'k2',
          'value': 'v2'
      }
  }]
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.selectURL('selecturl-entries-operation',
                          [{url: 'with_worklet.html'}],
                          {keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSelectURL'
  }, {
      'type': 'workletEntries'
  }]
  # Expect to iterate over `3 + self.SIZE` entries, each with a call to
  # `GetNextIterationResult()`, then make one last call to
  # `GetNextIterationResult()` to terminate the iteration with
  # `MakeEndOfIteration()`.
  EXPECTED_ITERATOR_HISTOGRAM_COUNT = "4 + self.SIZE"


class SharedStorageWorkletRunRemainingBudgetStory(SharedStorageStory):
  NAME = "SharedStorageWorkletRunRemainingBudget"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.run('run-remainingbudget-operation',
                    {keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentRun'
  }, {
      'type': 'workletRemainingBudget'
  }]


class SharedStorageWorkletSelectURLRemainingBudgetStory(SharedStorageStory):
  NAME = "SharedStorageWorkletSelectURLRemainingBudget"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """
  sharedStorage.selectURL('selecturl-remainingbudget-operation',
                          [{url: 'with_worklet.html'}],
                          {keepAlive: true});
  """
  EXPECTED_ACTION_EVENTS_TEMPLATE = [{
      'type': 'documentSelectURL'
  }, {
      'type': 'workletRemainingBudget'
  }]


class SharedStorageDocumentAddModuleStory(SharedStorageStory):
  NAME = "SharedStorageDocumentAddModule"
  ABSTRACT_STORY = False
  ACTION_SCRIPT_TEMPLATE = """"""
  EXPECTED_ACTION_EVENTS_TEMPLATE = []
  RENAVIGATE_AFTER_ACTION = True
