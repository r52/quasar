var websocket = null;

// acceptable sources
var sources = {
    "band": {
        "size": "Bands",
        "calc": function (e) {
            return e.val;
        }
    },
    "fft": {
        "size": "FFTSize",
        "calc": function (e) {
            return ((e.val / 2) + 1);
        }
    }
};

var rate = 90; // milliseconds

function subscribe() {
    var msg = {
        "method": "subscribe",
        "params": {
            "target": "win_audio_viz",
            "params": src
        }
    }

    websocket.send(JSON.stringify(msg));
}

function initialize(dat) {
    var element = `<div><i></i></div>`;

    if ("win_audio_viz" in dat["data"]["settings"]) {
        if ("settings" in dat["data"]["settings"]["win_audio_viz"]) {
            var settings = dat["data"]["settings"]["win_audio_viz"]["settings"];

            settings.forEach(function (e) {
                if (e.name === sources[src].size) {
                    var main = $(".wav");
                    main.empty();

                    var size = sources[src].calc(e);
                    for (var i = 0; i < size; i++) {
                        main.append(element);
                    }
                }
            });
        }

        if ("rates" in dat["data"]["settings"]["win_audio_viz"]) {
            var rates = dat["data"]["settings"]["win_audio_viz"]["rates"];

            rates.forEach(function (e) {
                if (e.name === src) {
                    rate = Math.round(e.rate * 0.9);
                }
            });
        }
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
        duration: rate,
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
            console.warn('ERROR: ' + evt.data);
        };
    } catch (exception) {
        console.warn('Exception: ' + exception);
    }
});
