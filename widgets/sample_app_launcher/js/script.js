var websocket = null;

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function launcher_cmd(cmd) {
    var msg = {
        method: "query",
        params: {
            target: "launcher",
            params: cmd
        }
    };

    websocket.send(JSON.stringify(msg));
}

async function initialize(socket) {
    quasar_authenticate(socket);
    sleep(10);
    launcher_cmd("get");
}

function create_launcher(data) {
    var entries = data["data"]["launcher"];

    if (entries.length > 0) {
        entries.forEach(function (e) {
            var template = `<div><a id="${e.command}" href="#${e.command}" style="background: url(${e.icon}); background-repeat: no-repeat; background-size: 50px 50px;"></a></div>`;
            $("body").prepend(template);
        });
    }

    $("a").click(function () {
        launcher_cmd(this.id);
    });
}

function parse_msg(msg) {
    var data = JSON.parse(msg);

    if ("data" in data && "launcher" in data["data"]) {
        create_launcher(data);
    }
}

$(function () {
    try {
        if (websocket && websocket.readyState == 1)
            websocket.close();
        websocket = quasar_create_websocket();
        websocket.onopen = function (evt) {
            initialize(websocket);
        };
        websocket.onmessage = function (evt) {
            parse_msg(evt.data);
        };
        websocket.onerror = function (evt) {
            console.log('ERROR: ' + evt.data);
        };
    } catch (exception) {
        console.log('Exception: ' + exception);
    }
});
