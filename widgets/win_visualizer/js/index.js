var websocket = null;
var bars = null;

var prop = {
    height: '100%'
};

function subscribe() {
    var msg = {
        "method": "subscribe",
        "params": {
            "target": "win_audio_viz",
            "params": "band"
        }
    }

    websocket.send(JSON.stringify(msg));
}

function initialize(dat) {
    var element = `<div><i></i></div>`;

    if ("win_audio_viz" in dat["data"]["settings"] && "settings" in dat["data"]["settings"]["win_audio_viz"]) {
        var settings = dat["data"]["settings"]["win_audio_viz"]["settings"];

        settings.forEach(function (e) {
            if (e.name === "Bands") {
                var main = $(".wav");
                main.empty();

                for (var i = 0; i < e.val; i++) {
                    main.append(element);
                }

                bars = $('.wav> div> i');
            }
        });
    }
}

function bounce(dat) {
    bars.each(function (index, element) {
        var hp = (dat[index] * 100.0).toFixed(0);
        prop["height"] = hp + '%';

        $(this).velocity("stop")
            .velocity(prop, {
                duration: 100
            });
    });
}

function parseMsg(msg) {
    var data = JSON.parse(msg);

    if ("data" in data && "win_audio_viz" in data["data"] && "band" in data["data"]["win_audio_viz"]) {
        bounce(data["data"]["win_audio_viz"]["band"]);
        return;
    }

    if ("data" in data && "settings" in data["data"]) {
        initialize(data);
        return;
    }
}

$(document).ready(function () {
    try {
        if (websocket && websocket.readyState == 1)
            websocket.close();
        websocket = quasar_create_websocket();;
        websocket.onopen = function (evt) {
            quasar_authenticate(websocket);
            subscribe();
        };
        websocket.onmessage = function (evt) {
            parseMsg(evt.data);
        };
        websocket.onerror = function (evt) {
            console.log('ERROR: ' + evt.data);
        };
    } catch (exception) {
        console.log('Exception: ' + exception);
    }
});
