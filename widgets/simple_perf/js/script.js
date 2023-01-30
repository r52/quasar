let websocket = null;
let cpuElem, ramElem;

function subscribe() {
  const msg = {
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
  const data = JSON.parse(msg);

  if ("win_simple_perf/sysinfo" in data) {
    const vals = data["win_simple_perf/sysinfo"];
    setData(cpuElem, vals["cpu"]);
    setData(
      ramElem,
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
  cpuElem = document.getElementById("cpu");
  ramElem = document.getElementById("ram");

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
