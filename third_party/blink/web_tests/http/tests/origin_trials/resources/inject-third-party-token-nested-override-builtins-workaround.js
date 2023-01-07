// Requires override-builtins.php to be included in the parent document to
// provide the overrideInsertionBuiltins() helper.
overrideInsertionBuiltins('all');

let script = document.createElement('script');
script.async = false;
script.src = 'http://localhost:8000/origin_trials/resources/inject-third-party-token-appendchild-workaround.js';
document.head.appendChild(script);
