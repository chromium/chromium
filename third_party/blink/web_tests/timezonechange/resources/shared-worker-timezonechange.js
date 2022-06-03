onconnect = connectEvent => {
  const port = connectEvent.ports[0];
  ontimezonechange = () => port.postMessage("SUCCESS");
  port.postMessage("READY");  // (the html will change the timezone)
}
