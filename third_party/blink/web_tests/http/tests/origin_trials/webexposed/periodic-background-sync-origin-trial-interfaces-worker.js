importScripts('/resources/testharness.js',
              '/resources/origin-trials-helper.js');

test(t => {

  OriginTrialsHelper.check_properties_exist(this, {
       'ServiceWorkerRegistration': ['periodicSync'],
       'PeriodicSyncManager': ['register', 'getTags', 'unregister'],
       'PeriodicSyncEvent': ['tag'],
       });
}, 'Periodic Background Sync API interfaces and properties in Origin-Trial enabled worker.');

done();