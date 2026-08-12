#!/usr/bin/env python3

from collections import deque
from pathlib import Path
import sys

from PIL import Image


SOURCE_COLOR = (255, 170, 170)
BACKGROUND_COLOR = (255, 255, 170)
SEED = (199, 0)
EXPECTED_COMPONENT_SIZE = 7650


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: recolor-background.py INPUT.png OUTPUT.png", file=sys.stderr)
        return 2

    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])
    image = Image.open(input_path).convert("RGB")
    pixels = image.load()
    width, height = image.size

    if image.size != (200, 228):
        raise ValueError(f"Expected 200x228 input, got {image.size}")
    if pixels[SEED] != SOURCE_COLOR:
        raise ValueError(f"Unexpected seed color: {pixels[SEED]}")

    queue = deque([SEED])
    component = {SEED}

    while queue:
        x, y = queue.popleft()
        for neighbor in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            nx, ny = neighbor
            if (
                0 <= nx < width
                and 0 <= ny < height
                and neighbor not in component
                and pixels[nx, ny] == SOURCE_COLOR
            ):
                component.add(neighbor)
                queue.append(neighbor)

    if len(component) != EXPECTED_COMPONENT_SIZE:
        raise ValueError(
            f"Expected {EXPECTED_COMPONENT_SIZE} background pixels, "
            f"found {len(component)}"
        )

    for point in component:
        pixels[point] = BACKGROUND_COLOR

    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path)
    print(
        f"Recolored {len(component)} connected background pixels "
        f"from {SOURCE_COLOR} to {BACKGROUND_COLOR}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
