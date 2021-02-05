import './es_module.php?url=empty.js?defer';
import('./es_module.php?url=empty.js?defer_dynamic').then(module => {
  console.log("Loaded a module");
});

