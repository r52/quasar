var websocket = null;

function subscribe() {
    var reg = {};

    reg["widget"] = qWidgetName;
    reg["type"] = "subscribe";
    reg["plugin"] = "win_simple_perf";
    reg["source"] = "cpu,ram";

    websocket.send(JSON.stringify(reg));
}

function processData(data) {
    if (data["plugin"] == "win_simple_perf") {
        var val = data["data"];

        switch (data["source"]) {
            case "cpu":
                $('span#cpu_val').text(val);
                break;
            case "ram":
                var pct = Math.round((val["used"] / val["total"]) * 100);
                $('span#ram_val').text(pct);
                break;
            default:
                console.log("Unknown source type " + data["source"]);
                break;
        }
    }
}

function parseMsg(msg) {
    var data = JSON.parse(msg);

    switch (data["type"]) {
        case "data":
            processData(data);
            break;
        default:
            console.log("Unsupported message type " + data["type"]);
            break;
    }
}

$(document).ready(function() {
    try {
        if (typeof MozWebSocket == 'function')
            WebSocket = MozWebSocket;
        if (websocket && websocket.readyState == 1)
            websocket.close();
        websocket = new WebSocket(qWsServerUrl);
        websocket.onopen = function(evt) {
            subscribe();
        };
        websocket.onclose = function(evt) {
            console.log("Disconnected");
        };
        websocket.onmessage = function(evt) {
            parseMsg(evt.data);
        };
        websocket.onerror = function(evt) {
            console.log('ERROR: ' + evt.data);
        };
    } catch (exception) {
        console.log('Exception: ' + exception);
    }
});
