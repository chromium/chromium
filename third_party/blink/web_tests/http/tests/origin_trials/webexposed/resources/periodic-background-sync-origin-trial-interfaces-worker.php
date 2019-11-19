<?php
// Generate token with the command:
// generate_token.py http://127.0.0.1:8000 PeriodicBackgroundSync --expire-timestamp=2000000000
header("Origin-Trial: ArBC4XZnvKrTOwjYpe4wXy6t+PrIdFoyqLkTNBVamdBgdFTxwMc9xfEJonBUlsX1LLLTzzLKhFZujvwwZrbPog8AAABeeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiUGVyaW9kaWNCYWNrZ3JvdW5kU3luYyIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==");
header('Content-Type: application/javascript');
?>

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