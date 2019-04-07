function quasar_authenticate(socket) {
    var auth = {
        method: "auth",
        params: {
            code: "%3"
        }
    };

    socket.send(JSON.stringify(auth));
}

function quasar_create_websocket() {
    return new WebSocket("%1://127.0.0.1:%2");
}
