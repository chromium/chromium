import './es_module.php?url=empty.js?async';
import('./es_module.php?url=empty.js?async_dynamic').then(module => {
  console.log("Loaded a module");
});

