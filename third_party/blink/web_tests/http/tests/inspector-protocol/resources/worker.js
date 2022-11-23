let message_id = 0;
onmessage = (event) => {
  postMessage(message_id++);
};