let websocket = null;
const source = "win_audio_viz/" + src;

let scene, camera, renderer, uniforms, stats;
let sound_data;

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

function init(dat) {
  let bSize = 0;

  if ("win_audio_viz/settings" in dat) {
    let settings = dat["win_audio_viz/settings"];

    settings.forEach(function (e) {
      if (e.name === sources[src].size) {
        bSize = sources[src].calc(e);
      }
    });
  }

  sound_data = new Uint8Array(bSize);

  const container = document.getElementById("container");

  while (container.firstChild) {
    container.removeChild(container.firstChild);
  }

  renderer = new THREE.WebGLRenderer({
    antialias: false,
    alpha: true,
  });
  renderer.setSize(window.innerWidth, window.innerHeight);
  renderer.setClearColor(0x000000, 0.0);
  renderer.setPixelRatio(window.devicePixelRatio);
  container.appendChild(renderer.domElement);

  scene = new THREE.Scene();

  camera = new THREE.Camera();

  const format = renderer.capabilities.isWebGL2
    ? THREE.RedFormat
    : THREE.LuminanceFormat;

  uniforms = {
    tAudioData: {
      value: new THREE.DataTexture(sound_data, bSize, 1, format),
    },
  };

  const material = new THREE.ShaderMaterial({
    uniforms: uniforms,
    vertexShader: document.getElementById("vertexShader").textContent,
    fragmentShader: document.getElementById("fragmentShader").textContent,
  });

  const geometry = new THREE.PlaneGeometry(2, 2);

  const mesh = new THREE.Mesh(geometry, material);
  scene.add(mesh);

  window.addEventListener("resize", onResize, false);

  animate();
}

function onResize() {
  renderer.setSize(window.innerWidth, window.innerHeight);
}

function animate() {
  requestAnimationFrame(animate);

  render();
}

function render() {
  uniforms.tAudioData.value.needsUpdate = true;
  renderer.render(scene, camera);
}

function parseMsg(msg) {
  const data = JSON.parse(msg);

  if (source in data) {
    sound_data.set(Uint8Array.from(data[source], (x) => Math.floor(x * 255)));
    return;
  }

  if ("win_audio_viz/settings" in data) {
    init(data);
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
