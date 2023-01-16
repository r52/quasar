let websocket = null;
const source = "win_audio_viz/" + src;

const vertexShader = `
varying vec2 vUv;

void main() {
    vUv = uv;
	  gl_Position = vec4( position, 1.0 );
}
`;

const fragmentShader = `
uniform sampler2D tAudioData;
varying vec2 vUv;

float HueToRGB(float f1, float f2, float hue) {
    if (hue < 0.0)
        hue += 1.0;
    else if (hue > 1.0)
        hue -= 1.0;
        
    float res;

    if ((6.0 * hue) < 1.0)
        res = f1 + (f2 - f1) * 6.0 * hue;
    else if ((2.0 * hue) < 1.0)
        res = f2;
    else if ((3.0 * hue) < 2.0)
        res = f1 + (f2 - f1) * ((2.0 / 3.0) - hue) * 6.0;
    else
        res = f1;
        
    return res;
}


vec3 HSLToRGB(vec3 hsl) {
    vec3 rgb;
    
    if (hsl.y == 0.0) {
        rgb = vec3(hsl.z); // Luminance
    } else {
        float f2;
        
        if (hsl.z < 0.5) {
            f2 = hsl.z * (1.0 + hsl.y);
        } else {
            f2 = (hsl.z + hsl.y) - (hsl.y * hsl.z);
        }
            
        float f1 = 2.0 * hsl.z - f2;
        
        rgb.r = HueToRGB(f1, f2, hsl.x + (1.0/3.0));
        rgb.g = HueToRGB(f1, f2, hsl.x);
        rgb.b = HueToRGB(f1, f2, hsl.x - (1.0/3.0));
    }
    
    return rgb;
}


void main() {
    vec3 backgroundColor = vec3( ${backgroundColor} );

    float f = texture2D( tAudioData, vec2( vUv.x, 0.0 ) ).r;

    float i = ${barStyle};

    vec3 color = ${color};

    gl_FragColor = vec4( mix( backgroundColor, color, i ), mix( 0.0, ${barTransparency}, i ) );
}
`;

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
    vertexShader: vertexShader,
    fragmentShader: fragmentShader,
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

  // render();
}

function render() {
  uniforms.tAudioData.value.needsUpdate = true;
  renderer.render(scene, camera);
}

function parseMsg(msg) {
  const data = JSON.parse(msg);

  if (source in data) {
    sound_data.set(Uint8Array.from(data[source], (x) => Math.floor(x * 255)));
    render();
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
