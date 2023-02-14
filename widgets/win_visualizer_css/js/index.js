let websocket = null;
const source = "win_audio_viz/" + src;
let rate = 90; // milliseconds

// acceptable sources
const sources = {
  band: {
    size: "win_audio_viz/Bands",
    calc: function (e) {
      return e.val;
    },
  },
  fft: {
    size: "win_audio_viz/FFTSize",
    calc: function (e) {
      return e.val / 2 + 1;
    },
  },
};

function subscribe() {
  const msg = {
    method: "subscribe",
    params: {
      topics: [source],
    },
  };

  websocket.send(JSON.stringify(msg));
}

function initialize(dat) {
  const element = `<div><i></i></div>`;

  if ("win_audio_viz/settings" in dat) {
    let settings = dat["win_audio_viz/settings"];

    settings.forEach(function (e) {
      if (e.name === sources[src].size) {
        let main = document.getElementsByClassName("wav")[0];
        main.replaceChildren();

        let size = sources[src].calc(e);

        console.log("Generating", size, "bands");

        for (let i = 0; i < size; i++) {
          main.innerHTML += element;
        }
      }
    });
  }

  if ("rates" in dat["win_audio_viz/metadata"]) {
    let rates = dat["win_audio_viz/metadata"]["rates"];

    rates.forEach(function (e) {
      if (e.name === source) {
        rate = Math.round(e.rate * 0.9);
      }
    });
  }
}

function bounce(dat) {
  anime({
    targets: ".wav> div> i",
    height: function (el, i) {
      return Math.round(dat[i] * 100.0) + "%";
    },
    background: function (el, i) {
      return (
        "hsl(" +
        (120 - Math.min(Math.round(dat[i] * 135.0), 120)) +
        ", 100%, 66%)"
      );
    },
    duration: rate,
    easing: "linear",
  });
}

function parseMsg(msg) {
  const data = JSON.parse(msg);

  if (source in data) {
    bounce(data[source]);
    return;
  }

  if ("win_audio_viz/settings" in data) {
    initialize(data);
    return;
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
      console.warn("ERROR: " + evt.data);
    };
  } catch (exception) {
    console.warn("Exception: " + exception);
  }
});
