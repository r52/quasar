var websocket = null;

function subscribe() {
  var msg = {
    method: "subscribe",
    params: {
      topics: ["win_simple_perf/sysinfo"],
    },
  };

  websocket.send(JSON.stringify(msg));
}

function setData(elm, value) {
  if (elm != null) {
    elm.setAttribute("aria-valuenow", value);
    elm.textContent = value + "%";
    elm.style.width = value + "%";
    elm.classList.remove("bg-success", "bg-info", "bg-warning", "bg-danger");

    if (value >= 80) {
      elm.classList.add("bg-danger");
    } else if (value >= 60) {
      elm.classList.add("bg-warning");
    } else {
      elm.classList.add("bg-success");
    }
  }
}

function parseMsg(msg) {
  var data = JSON.parse(msg);

  if ("win_simple_perf/sysinfo" in data) {
    var vals = data["win_simple_perf/sysinfo"];
    setData(document.getElementById("cpu"), vals["cpu"]);
    setData(
      document.getElementById("ram"),
      Math.round((vals["ram"]["used"] / vals["ram"]["total"]) * 100),
    );
  }
}

function ready(fn) {
  if (document.readyState !== "loading") {
    fn();
  } else {
    document.addEventListener("DOMContentLoaded", fn);
  }
}

ready(function () {
  try {
    if (websocket && websocket.readyState == 1) websocket.close();
    websocket = quasar_create_websocket();
    websocket.onopen = function (evt) {
      quasar_authenticate(websocket);
      subscribe();
    };
    websocket.onmessage = function (evt) {
      parseMsg(evt.data);
    };
    websocket.onerror = function (evt) {
      console.log("ERROR: " + evt.data);
    };
  } catch (exception) {
    console.log("Exception: " + exception);
  }
});
