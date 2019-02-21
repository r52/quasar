var seditor = null;
var reditor = null;
var websocket = null;
var tabSize = 2;

function parse_data(msg) {
    var data = JSON.parse(msg);

    reditor.setValue(JSON.stringify(data, null, tabSize));
}

$(function() {
    var sconfig = {
        value: "{\n  \"method\": \"query\",\n  \"params\": {\n    \"target\": \"simple_perf\",\n    \"params\": \"cpu\"\n  }\n}",
        mode: {name: "javascript", json: true },
        tabSize: tabSize
    };
    seditor = CodeMirror(document.getElementById('query-editor'), sconfig);

    var rconfig = {
        value: "{\n  \"data\": \"...\"\n}",
        mode: {name: "javascript", json: true },
        tabSize: tabSize,
        readOnly: true
    };
    reditor = CodeMirror(document.getElementById('result-window'), rconfig);

    try {
        if (websocket && websocket.readyState == 1)
            websocket.close();
        websocket = quasar_create_websocket();
        websocket.onopen = function(evt) {
            quasar_authenticate(websocket);
        };
        websocket.onmessage = function(evt) {
            parse_data(evt.data);
        };
        websocket.onerror = function(evt) {
            console.log('ERROR: ' + evt.data);
        };
    } catch (exception) {
        console.log('Exception: ' + exception);
    }

    // send button
    $('#send').click(function() {
        var res = "";
        var s = seditor.getValue().split("\n");
        s.forEach(function(e) {
            res += e.trim();
        });
        var json = JSON.parse(res);
        websocket.send(JSON.stringify(json));
    });

    // close button
    $('#close').click(function() {
        close();
    });
});
