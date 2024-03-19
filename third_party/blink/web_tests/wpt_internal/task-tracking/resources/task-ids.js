let nextId = 10000;

// Helper function to set a an ID for the current task, which will be propagated
// to descendant tasks and microtasks.
function initializeTaskId() {
  const id = nextId++;
  scheduler.taskId = id;
  return id;
}
