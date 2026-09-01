# Art sources

**English** · [Русский](README.ru.md)

These files are retained as art sources and are not included directly in the
Pebble resource package.

- `background-concept-source.png` and `sprite-atlas-source.png` preserve the
  original visual concept.
- `user-reference-*.png` are unchanged user-provided references.
- `*-generated.png` files are generated backgrounds and sprite atlases.
- `reported-defect-*.png` preserve enlarged defect references used during
  cleanup.

`tools/build-assets.swift` extracts the transparent atlas cells, removes
semi-transparent fringes, scales with nearest-neighbour sampling, and maps every
pixel to a themed subset of Pebble's official 64-color palette. The build audit
requires RGB channels from `00/55/AA/FF` and alpha from `0/255` only.
