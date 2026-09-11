# Original-ribbon flow study 03

Open `index.html` directly; all image assets are embedded. Rebuild with
`node build-preview.cjs` from any working directory.

This version reuses the original 200×228 background, without generating new
artwork. It transports the actual source raster along curvilinear coordinates,
with lane-dependent travel distance, phase, transverse deformation and restrained
blue/cyan color variation. Two staggered weighted advection phases hide texture
resets. This is a visual approximation, not a fluid simulation or independently
segmented artwork layers. Some texture softening can occur where phases blend.

Moon disk (radius 20 around 166,33) and the city region (y >= 194) are restored
unchanged. Original digit assets use the existing positions and dimensions.
96 frames play at 12 fps by default, with an 8-second periodic cycle. Every
rendered background channel is quantized to 0,85,170,255.

Browser verification:
- 96 frames, 200×228, union of 31 colors from the Pebble palette.
- Fixed-region channel differences against source across all frames: 0.
- Render at full-cycle phase versus phase zero: 0 differing bytes.
- Last-to-first summed absolute RGB delta: 202045; all step deltas range from
  189125 to 272510 (the calculation includes the wrap step).
- Inspected the browser layout and image after reducing excessive deformation.

The watchface implementation, package version and PBW are unchanged. Device
performance, battery impact and the user's visual acceptance remain untested.
