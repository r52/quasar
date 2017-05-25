$(document).ready(function() {
    var websocket = null;

    var msg = {};
    msg["widget"] = qWidgetName;
    msg["type"] = "launcher";

    function launch(app) {
        msg["app"] = app;
        websocket.send(JSON.stringify(msg));
    }

    try {
        if (typeof MozWebSocket == 'function')
            WebSocket = MozWebSocket;
        if (websocket && websocket.readyState == 1)
            websocket.close();
        websocket = new WebSocket(qWsServerUrl);
        websocket.onopen = function(evt) {
            console.log("Connected");
        };
        websocket.onclose = function(evt) {
            console.log("Disconnected");
        };
        websocket.onmessage = function(evt) {};
        websocket.onerror = function(evt) {
            console.log('ERROR: ' + evt.data);
        };
    } catch (exception) {
        console.log('Exception: ' + exception);
    }

    $("a").click(function() {
        launch(this.id);
    });
});
