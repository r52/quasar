var websocket = null;

function subscribe() {
  var msg = {
    method: "subscribe",
    params: {
      target: "win_simple_perf",
      params: ["sysinfo"],
    },
  };

  websocket.send(JSON.stringify(msg));
}

function setData(source, value) {
  var elm = null;

  switch (source) {
    case "cpu":
      elm = document.getElementById("cpu");
      break;
    case "ram":
      value = Math.round((value["used"] / value["total"]) * 100);
      elm = document.getElementById("ram");
      break;
  }

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

  if (
    "data" in data &&
    "win_simple_perf" in data["data"] &&
    "sysinfo" in data["data"]["win_simple_perf"]
  ) {
    var vals = data["data"]["win_simple_perf"]["sysinfo"];
    setData("cpu", vals["cpu"]);
    setData("ram", vals["ram"]);
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
