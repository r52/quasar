var websocket = null;

function poll() {
    var msg = {
        "method": "query",
        "params": {
            "target": "win_simple_perf",
            "params": "cpu,ram"
        }
    }

    websocket.send(JSON.stringify(msg));
}

function setData(source, value) {
    var $elm = null;

    switch (source) {
        case "cpu":
            $elm = $('#cpu');
            break;
        case "ram":
            value = Math.round((value["used"] / value["total"]) * 100);
            $elm = $('#ram');
            break;
    }

    if ($elm != null) {
        $elm.attr("aria-valuenow", value).text(value + "%").width(value + "%");
        $elm.removeClass("bg-success bg-info bg-warning bg-danger");

        if (value >= 80) {
            $elm.addClass("bg-danger");
        } else if (value >= 60) {
            $elm.addClass("bg-warning");
        } else {
            $elm.addClass("bg-success");
        }
    }
}

function processData(data) {
    var vals = data["data"]["win_simple_perf"];
    if ("cpu" in vals) {
        setData("cpu", vals["cpu"]);
    }

    if ("ram" in vals) {
        setData("ram", vals["ram"]);
    }
}

function parseMsg(msg) {
    var data = JSON.parse(msg);

    if ("data" in data && "win_simple_perf" in data["data"]) {
        processData(data);
    }
}

$(function () {
    try {
        if (websocket && websocket.readyState == 1)
            websocket.close();
        websocket = quasar_create_websocket();
        websocket.onopen = function (evt) {
            quasar_authenticate(websocket);
            setInterval(poll, 5000);
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
