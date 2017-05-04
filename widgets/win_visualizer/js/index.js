$(document).ready(function() {
    var websocket = null;

    var prop = {
        height: '100%'
    };

    var bars = $('.wav> div> i');

    function subscribe() {
        var reg = {};

        reg["widget"] = qWidgetName;
        reg["type"] = "subscribe";
        reg["plugin"] = "win_audio_viz";
        reg["source"] = "viz";

        websocket.send(JSON.stringify(reg));
    }

    function parseMsg(msg) {
        var data = JSON.parse(msg);

        bars.each(function(index, element) {
            var hp = (data["data"][index] * 100.0).toFixed(0);
            prop["height"] = hp + '%';

            $(this).velocity("stop")
                .velocity(prop, {
                    duration: 50,
                    easing: "linear"
                });
        });
    }

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
