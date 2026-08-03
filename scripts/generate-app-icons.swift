#!/usr/bin/env swift

import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

struct RGB {
  let red: UInt8
  let green: UInt8
  let blue: UInt8
}

enum IconKind: String, CaseIterable {
  case flipBoard = "flip-board"
  case mosaicGrid = "mosaic-grid"
  case codexWeekly = "codex-weekly"
}

let black = RGB(red: 0, green: 0, blue: 0)
let darkGray = RGB(red: 85, green: 85, blue: 85)
let midGray = RGB(red: 170, green: 170, blue: 170)
let white = RGB(red: 255, green: 255, blue: 255)
let coral = RGB(red: 255, green: 85, blue: 85)
let paleYellow = RGB(red: 255, green: 255, blue: 170)
let blue = RGB(red: 0, green: 85, blue: 170)
let vividBlue = RGB(red: 0, green: 85, blue: 255)
let navy = RGB(red: 0, green: 0, blue: 85)
let cyan = RGB(red: 0, green: 255, blue: 255)
let lime = RGB(red: 170, green: 255, blue: 0)

let digits: [Character: [String]] = [
  "0": ["111", "101", "101", "101", "111"],
  "2": ["111", "001", "111", "100", "111"],
  "4": ["101", "101", "111", "001", "001"],
  "8": ["111", "101", "111", "101", "111"],
]

let codexGlyphs: [Character: [String]] = [
  "C": ["11111", "10000", "10000", "10000", "10000", "10000", "11111"],
  "O": ["11111", "10001", "10001", "10001", "10001", "10001", "11111"],
  "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
  "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
  "X": ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
]

struct PixelCanvas {
  let width: Int
  let height: Int
  var pixels: [RGB]

  init(width: Int, height: Int, fill: RGB) {
    self.width = width
    self.height = height
    self.pixels = Array(repeating: fill, count: width * height)
  }

  mutating func set(_ x: Int, _ y: Int, _ color: RGB) {
    guard x >= 0, x < width, y >= 0, y < height else { return }
    pixels[y * width + x] = color
  }

  mutating func rect(x: Int, y: Int, width: Int, height: Int, color: RGB) {
    guard width > 0, height > 0 else { return }
    for row in y..<(y + height) {
      for column in x..<(x + width) {
        set(column, row, color)
      }
    }
  }

  mutating func glyph(
    _ rows: [String],
    x: Int,
    y: Int,
    color: RGB,
    scaleX: Int = 1,
    scaleY: Int = 1
  ) {
    for (rowIndex, row) in rows.enumerated() {
      for (columnIndex, value) in row.enumerated() where value == "1" {
        rect(
          x: x + columnIndex * scaleX,
          y: y + rowIndex * scaleY,
          width: scaleX,
          height: scaleY,
          color: color
        )
      }
    }
  }
}

func drawFlipBoard(size: Int) -> PixelCanvas {
  var canvas = PixelCanvas(width: size, height: size, fill: black)
  let tileSize = size == 25 ? 11 : 14
  let positions = size == 25 ? [1, 13] : [1, 17]
  let digitScale = size == 25 ? 1 : 2
  let values: [Character] = ["2", "4", "0", "8"]

  for index in 0..<4 {
    let x = positions[index % 2]
    let y = positions[index / 2]
    canvas.rect(x: x, y: y, width: tileSize, height: tileSize, color: darkGray)
    canvas.rect(x: x + 1, y: y, width: tileSize - 2, height: 1, color: midGray)
    canvas.rect(x: x, y: y + tileSize - 1, width: tileSize, height: 1, color: black)
    canvas.rect(x: x, y: y + tileSize / 2, width: tileSize, height: 1, color: black)
    canvas.set(x, y + tileSize / 2, midGray)
    canvas.set(x + tileSize - 1, y + tileSize / 2, midGray)

    if let rows = digits[values[index]] {
      let glyphWidth = 3 * digitScale
      let glyphHeight = 5 * digitScale
      canvas.glyph(
        rows,
        x: x + (tileSize - glyphWidth) / 2,
        y: y + (tileSize - glyphHeight) / 2,
        color: white,
        scaleX: digitScale,
        scaleY: digitScale
      )
    }
  }

  return canvas
}

func drawMosaicGrid(size: Int) -> PixelCanvas {
  var canvas = PixelCanvas(width: size, height: size, fill: black)
  let leftWidth = size == 25 ? 15 : 20
  let rightX = leftWidth + 1
  let rightWidth = size - rightX - 1
  let lowerY = size == 25 ? 19 : 25

  canvas.rect(x: 1, y: 1, width: leftWidth - 1, height: lowerY - 2, color: white)
  canvas.rect(x: 1, y: lowerY, width: leftWidth - 1, height: size - lowerY - 1, color: blue)
  canvas.rect(x: rightX, y: 1, width: rightWidth, height: size == 25 ? 7 : 9, color: coral)
  canvas.rect(x: rightX, y: size == 25 ? 9 : 11, width: rightWidth, height: size == 25 ? 8 : 11, color: white)
  canvas.rect(x: rightX, y: size == 25 ? 19 : 24, width: rightWidth, height: size - (size == 25 ? 20 : 25), color: paleYellow)

  let scale = size == 25 ? 1 : 2
  let digitWidth = 3 * scale
  let pairWidth = digitWidth * 2 + scale
  let digitX = 1 + (leftWidth - 1 - pairWidth) / 2
  let topY = size == 25 ? 3 : 3
  let bottomY = size == 25 ? 10 : 14

  for (index, value) in [Character("2"), Character("2")].enumerated() {
    canvas.glyph(digits[value]!, x: digitX + index * (digitWidth + scale), y: topY, color: black, scaleX: scale, scaleY: scale)
  }
  for (index, value) in [Character("0"), Character("0")].enumerated() {
    canvas.glyph(digits[value]!, x: digitX + index * (digitWidth + scale), y: bottomY, color: black, scaleX: scale, scaleY: scale)
  }

  return canvas
}

func drawCodexWeekly(size: Int) -> PixelCanvas {
  var canvas = PixelCanvas(width: size, height: size, fill: black)

  if size == 25 {
    let compactGlyphs: [Character: [String]] = [
      "C": ["111", "100", "100", "100", "111"],
      "O": ["111", "101", "101", "101", "111"],
      "D": ["110", "101", "101", "101", "110"],
      "E": ["111", "100", "110", "100", "111"],
      "X": ["101", "101", "010", "101", "101"],
    ]
    var x = 3
    for character in "CODEX" {
      canvas.glyph(compactGlyphs[character]!, x: x, y: 1, color: white)
      x += 4
    }

    for column in 0..<7 {
      let color = column == 6 ? black : lime
      canvas.rect(x: 1 + column * 3, y: 8, width: 2, height: 3, color: color)
      if column == 6 {
        canvas.rect(x: 19, y: 8, width: 3, height: 3, color: lime)
        canvas.set(20, 9, black)
      }
    }

    let heatmap: [[RGB]] = [
      [cyan, vividBlue, navy, cyan],
      [vividBlue, cyan, vividBlue, navy],
      [navy, vividBlue, cyan, vividBlue],
      [vividBlue, navy, vividBlue, cyan],
    ]
    for row in 0..<4 {
      for column in 0..<4 {
        canvas.rect(x: 1 + column * 6, y: 13 + row * 3, width: 5, height: 2, color: heatmap[row][column])
      }
    }
  } else {
    var x = 1
    for character in "CODEX" {
      canvas.glyph(codexGlyphs[character]!, x: x, y: 2, color: white)
      x += 6
    }

    for column in 0..<7 {
      let color = column == 6 ? black : lime
      canvas.rect(x: 1 + column * 4, y: 11, width: 3, height: 4, color: color)
      if column == 6 {
        canvas.rect(x: 25, y: 11, width: 4, height: 4, color: lime)
        canvas.rect(x: 26, y: 12, width: 2, height: 2, color: black)
      }
    }

    let heatmap: [[RGB]] = [
      [cyan, vividBlue, navy, cyan],
      [vividBlue, cyan, vividBlue, navy],
      [navy, vividBlue, cyan, vividBlue],
      [vividBlue, navy, vividBlue, cyan],
    ]
    for row in 0..<4 {
      for column in 0..<4 {
        canvas.rect(x: 4 + column * 7, y: 17 + row * 4, width: 6, height: 3, color: heatmap[row][column])
      }
    }
  }

  return canvas
}

func draw(_ kind: IconKind, size: Int) -> PixelCanvas {
  switch kind {
  case .flipBoard:
    return drawFlipBoard(size: size)
  case .mosaicGrid:
    return drawMosaicGrid(size: size)
  case .codexWeekly:
    return drawCodexWeekly(size: size)
  }
}

func scaled(_ source: PixelCanvas, to outputSize: Int) -> PixelCanvas {
  guard source.width != outputSize || source.height != outputSize else { return source }
  var output = PixelCanvas(width: outputSize, height: outputSize, fill: black)
  for y in 0..<outputSize {
    for x in 0..<outputSize {
      let sourceX = x * source.width / outputSize
      let sourceY = y * source.height / outputSize
      output.set(x, y, source.pixels[sourceY * source.width + sourceX])
    }
  }
  return output
}

func writePNG(_ canvas: PixelCanvas, to url: URL) throws {
  var bytes = [UInt8]()
  bytes.reserveCapacity(canvas.width * canvas.height * 3)
  for pixel in canvas.pixels {
    bytes.append(pixel.red)
    bytes.append(pixel.green)
    bytes.append(pixel.blue)
  }

  guard
    let provider = CGDataProvider(data: Data(bytes) as CFData),
    let image = CGImage(
      width: canvas.width,
      height: canvas.height,
      bitsPerComponent: 8,
      bitsPerPixel: 24,
      bytesPerRow: canvas.width * 3,
      space: CGColorSpaceCreateDeviceRGB(),
      bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.none.rawValue),
      provider: provider,
      decode: nil,
      shouldInterpolate: false,
      intent: .defaultIntent
    ),
    let destination = CGImageDestinationCreateWithURL(
      url as CFURL,
      UTType.png.identifier as CFString,
      1,
      nil
    )
  else {
    throw NSError(domain: "IconGenerator", code: 1)
  }

  CGImageDestinationAddImage(destination, image, nil)
  guard CGImageDestinationFinalize(destination) else {
    throw NSError(domain: "IconGenerator", code: 2)
  }
}

let root = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
let iconDirectory = root.appendingPathComponent("assets/icons", isDirectory: true)
try FileManager.default.createDirectory(at: iconDirectory, withIntermediateDirectories: true)

for kind in IconKind.allCases {
  let appMaster = draw(kind, size: 32)
  for size in [32, 80, 144, 512] {
    let output = iconDirectory.appendingPathComponent("\(kind.rawValue)-icon-\(size).png")
    try writePNG(scaled(appMaster, to: size), to: output)
    print(output.path)
  }

  let resourceDirectory = root
    .appendingPathComponent(kind.rawValue, isDirectory: true)
    .appendingPathComponent("resources/images", isDirectory: true)
  try FileManager.default.createDirectory(at: resourceDirectory, withIntermediateDirectories: true)
  let menuOutput = resourceDirectory.appendingPathComponent("\(kind.rawValue)-menu-icon.png")
  try writePNG(draw(kind, size: 25), to: menuOutput)
  print(menuOutput.path)
}
