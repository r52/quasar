// "band" or "fft"
const src = "band";

// Bar Style
// Bars
const barStyle = `step( vUv.y, f )`;

// Caps only
// const barStyle = `step( vUv.y, f ) * step( f - 0.0125, vUv.y )`;

// Colors

// RGB
const backgroundColor = `0.0, 0.0, 0.0`;

// Green to Red
// const color = `HSLToRGB(vec3((0.33 - min(f * 0.4, 0.33)), 1.0, 0.66))`;

// Red to white
const color = `HSLToRGB(vec3(0.0, 1.0, max(0.33, f * 1.2)))`;

// Bar Transparency
const barTransparency = `1.0`;
