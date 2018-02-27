$(document).ready(function() {
    var websocket = null;

    var prop = {
        height: '100%'
    };

    var bars = $('.wav> div> i');

    function subscribe() {
        var reg = {
            widget: qWidgetName,
            type: "subscribe",
            plugin: "win_audio_viz",
            source: "viz"
        };

        websocket.send(JSON.stringify(reg));
    }

    function bounce(dat) {
        bars.each(function(index, element) {
            var hp = (dat[index] * 100.0).toFixed(0);
            prop["height"] = hp + '%';

            $(this).velocity("stop")
                .velocity(prop, {
                    duration: 50,
                    easing: "linear"
                });
        });
    }

    function parseMsg(msg) {
        var data = JSON.parse(msg);

        switch (data["type"]) {
            case "data":
                bounce(data["data"]);
                break;
            case "settings":
                break;
            default:
                break;
        }
    }

    try {
        if (websocket && websocket.readyState == 1)
            websocket.close();
        websocket = new WebSocket(qWsServerUrl);
        websocket.onopen = function(evt) {
            subscribe();
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
