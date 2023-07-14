function quasar_authenticate(socket) {
  var auth = {
    method: "auth",
    params: {
      code: "%2",
    },
  };

  socket.send(JSON.stringify(auth));
}

function quasar_create_websocket() {
  return new WebSocket("ws://localhost:%1");
}
