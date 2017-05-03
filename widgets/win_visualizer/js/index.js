$(document).ready(function() {
    var websocket = null;

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

        var prop = {
            height: '100%',
            backgroundColor: '#ffffff'
        };

        bars.each(function(index, element) {
            var hp = (data["data"][index] * 100.0).toFixed(0);
            var php = Math.min((255 * hp / 100.0).toFixed(0), 255);
            var ihp = 255 - php;
            prop["height"] = hp + '%';
            prop["backgroundColor"] = '#' + php.toString(16) + ihp.toString(16) + '60';

            console.log(prop["backgroundColor"]);

            $(this).velocity("stop")
                .velocity(prop, {
                    duration: 30,
                    easing: [0.95, 0.05, 0.795, 0.035]
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
