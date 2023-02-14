let websocket = null;

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function launcher_cmd(cmd, arg) {
  let msg = {
    method: "query",
    params: {
      topics: [`applauncher/${cmd}`],
    },
  };

  if (arg) {
    msg.params["args"] = arg;
  }

  websocket.send(JSON.stringify(msg));
}

async function initialize(socket) {
  quasar_authenticate(socket);
  sleep(10);
  launcher_cmd("list");
}

function create_launcher(data) {
  var entries = data["applauncher/list"];

  if (entries.length > 0) {
    const container = document.getElementById("container");

    entries.forEach(function (e) {
      const d = document.createElement("div");

      const a = document.createElement("a");
      a.id = e.command;
      a.href = `#${e.command}`;
      a.style.cssText = `background: url(${e.icon}); background-repeat: no-repeat; background-size: 60px 60px;`;

      a.addEventListener("click", () => {
        launcher_cmd("launch", e.command);
      });

      d.appendChild(a);

      container.appendChild(d);
    });
  }
}

function parse_msg(msg) {
  var data = JSON.parse(msg);

  if ("applauncher/list" in data) {
    create_launcher(data);
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
      initialize(websocket);
    };
    websocket.onmessage = function (evt) {
      parse_msg(evt.data);
    };
    websocket.onerror = function (evt) {
      console.log("ERROR: " + evt.data);
    };
  } catch (exception) {
    console.log("Exception: " + exception);
  }
});
