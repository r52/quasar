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
    var val = data["data"];
    var $elm = null;

    switch (data["source"]) {
        case "cpu":
            $elm = $('#cpu');
            break;
        case "ram":
            val = Math.round((val["used"] / val["total"]) * 100);
            $elm = $('#ram');
            break;
        default:
            console.log("Unknown source type " + data["source"]);
            break;
    }

    if ($elm != null) {
        $elm.attr("aria-valuenow", val).text(val + "%").width(val + "%");
        $elm.removeClass("bg-success bg-info bg-warning bg-danger");

        if (val >= 80) {
            $elm.addClass("bg-danger");
        } else if (val >= 60) {
            $elm.addClass("bg-warning");
        } else {
            $elm.addClass("bg-success");
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
