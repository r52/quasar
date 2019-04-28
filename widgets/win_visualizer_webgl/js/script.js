var websocket = null;

var scene, camera, renderer, uniforms;
var sound_data;

// acceptable sources
var sources = {
    "band": {
        "size": "Bands",
        "calc": function (e) {
            return e.val;
        }
    },
    "fft": {
        "size": "FFTSize",
        "calc": function (e) {
            return ((e.val / 2) + 1);
        }
    }
};

function subscribe() {
    var msg = {
        "method": "subscribe",
        "params": {
            "target": "win_audio_viz",
            "params": src
        }
    }

    websocket.send(JSON.stringify(msg));
}

function init(dat) {
    var bSize = 0;

    if ("win_audio_viz" in dat["data"]["settings"]) {
        if ("settings" in dat["data"]["settings"]["win_audio_viz"]) {
            var settings = dat["data"]["settings"]["win_audio_viz"]["settings"];

            settings.forEach(function (e) {
                if (e.name === sources[src].size) {
                    bSize = sources[src].calc(e);
                }
            });
        }
    }

    sound_data = new Uint8Array(bSize);

    var container = document.getElementById('container');

    while(container.firstChild) {
        container.removeChild(container.firstChild);
    }

    renderer = new THREE.WebGLRenderer({
        antialias: true,
        alpha: true
    });
    renderer.setSize(window.innerWidth, window.innerHeight);
    renderer.setClearColor(0x000000, 0.0);
    renderer.setPixelRatio(window.devicePixelRatio);
    container.appendChild(renderer.domElement);

    scene = new THREE.Scene();

    camera = new THREE.Camera();

    uniforms = {
        tAudioData: {
            value: new THREE.DataTexture(sound_data, bSize, 1, THREE.LuminanceFormat)
        }
    };

    var material = new THREE.ShaderMaterial({
        uniforms: uniforms,
        vertexShader: document.getElementById('vertexShader').textContent,
        fragmentShader: document.getElementById('fragmentShader').textContent
    });

    var geometry = new THREE.PlaneBufferGeometry(2, 2);

    var mesh = new THREE.Mesh(geometry, material);
    scene.add(mesh);

    window.addEventListener('resize', onResize, false);

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
    var data = JSON.parse(msg);

    if ("data" in data && "win_audio_viz" in data["data"] && src in data["data"]["win_audio_viz"]) {
        sound_data.set(Uint8Array.from(data["data"]["win_audio_viz"][src], x => Math.floor(x * 255)));
        return;
    }

    if ("data" in data && "settings" in data["data"]) {
        init(data);
        return;
    }
}

try {
    if (websocket && websocket.readyState == 1)
        websocket.close();
    websocket = quasar_create_websocket();
    websocket.onopen = function (evt) {
        quasar_authenticate(websocket);
        subscribe();
    };
    websocket.onmessage = function (evt) {
        parseMsg(evt.data);
    };
    websocket.onerror = function (evt) {
        console.warn('ERROR: ' + evt.data);
    };
} catch (exception) {
    console.warn('Exception: ' + exception);
}
