# win_visualizer_webgl

This is a WebGL audio visualizer, adapted from the [three.js webaudio visualizer demo](https://threejs.org/examples/webaudio_visualizer.html), to demonstrate WebGL usage in Quasar widgets.

## Technical Notes

While more performant than the pure CSS/JS animated win_visualizer_css, this WebGL version looks worse than the CSS counterpart due to its data updates being tied to the rate that we get them from the win_audio_viz extension, instead of having smooth increments calculated within the rendering loop like in usual 3D applications, or inherently having smooth interpolated high frame rate animation like in the CSS version.
