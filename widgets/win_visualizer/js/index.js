var websocket = null;

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
            }
        });
    }
}

function bounce(dat) {
    anime({
        targets: '.wav> div> i',
        height: function (el, i) {
            return Math.round(dat[i] * 100.0) + '%';
        },
        background: function (el, i) {
            return 'hsl(' + (120 - Math.min(Math.round(dat[i] * 135.0), 120)) + ', 100%, 66%)'
        },
        duration: 90,
        easing: 'linear'
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
