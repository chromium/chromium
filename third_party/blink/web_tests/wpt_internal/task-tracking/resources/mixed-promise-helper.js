let parent_task;
let sibling_task;
let image_loaded;
let fetched_response;
let image_can_load;
let image_can_load_promise;

const prepare_and_run_test = test => {
  // The task before the test's promise starts running, independent from the
  // image load task. Initialize this here so the propagated data is different
  // from the sibling task.
  parent_task = initializeTaskId();

  // Run the test in its own task.
  setTimeout(test, 5);
};

const load_image = async () => {
  sibling_task = scheduler.taskId;
  await image_can_load_promise;
  const img = new Image();
  img.src = "/images/blue.png?pipe=trickle(d0.05)";
  img.addEventListener("load", () => {
    image_loaded(fetched_response);
  });
  document.body.appendChild(img);
};

const run_test = test => {
  image_can_load_promise = new Promise(r => { image_can_load = r; });
  // Run the test prep in one task.
  setTimeout("prepare_and_run_test('" + test + "')", 5);
  // Load the image in a separate task.
  setTimeout("load_image()", 5);
};
