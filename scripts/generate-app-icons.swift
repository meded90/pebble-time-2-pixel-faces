#!/usr/bin/env swift

import CoreGraphics
import CoreText
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
  "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
  "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
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

    let cornerPixels = [
      (0, 0), (1, 0), (0, 1),
      (tileSize - 1, 0), (tileSize - 2, 0), (tileSize - 1, 1),
      (0, tileSize - 1), (1, tileSize - 1), (0, tileSize - 2),
      (tileSize - 1, tileSize - 1), (tileSize - 2, tileSize - 1),
      (tileSize - 1, tileSize - 2),
    ]
    for (cornerX, cornerY) in cornerPixels {
      canvas.set(x + cornerX, y + cornerY, black)
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

  return canvas
}

func robotoBoldFont(size: CGFloat) throws -> CTFont {
  let fontURL = FileManager.default.homeDirectoryForCurrentUser
    .appendingPathComponent("Library/Application Support/Pebble SDK/SDKs/4.17")
    .appendingPathComponent("toolchain/moddable/contributed/moddable_six/plug-schedule/fonts/Roboto-Bold.ttf")

  guard
    let provider = CGDataProvider(url: fontURL as CFURL),
    let graphicsFont = CGFont(provider)
  else {
    throw NSError(
      domain: "IconGenerator",
      code: 3,
      userInfo: [NSLocalizedDescriptionKey: "Roboto-Bold.ttf was not found in the Pebble SDK"]
    )
  }

  return CTFontCreateWithGraphicsFont(graphicsFont, size, nil, nil)
}

func overlayRobotoMosaicDigits(on canvas: inout PixelCanvas) throws {
  let size = canvas.width
  let scale = CGFloat(size) / 32
  let panelX: CGFloat = size == 25 ? 1 : scale
  let panelY: CGFloat = size == 25 ? 1 : scale
  let panelWidth: CGFloat = size == 25 ? 14 : 19 * scale
  let panelHeight: CGFloat = size == 25 ? 17 : 23 * scale
  let fontSize: CGFloat = size == 25 ? 7.4 : 10 * scale
  let font = try robotoBoldFont(size: fontSize)

  var mask = [UInt8](repeating: 0, count: size * size)
  guard let context = CGContext(
    data: &mask,
    width: size,
    height: size,
    bitsPerComponent: 8,
    bytesPerRow: size,
    space: CGColorSpaceCreateDeviceGray(),
    bitmapInfo: CGImageAlphaInfo.none.rawValue
  ) else {
    throw NSError(domain: "IconGenerator", code: 4)
  }

  context.setShouldAntialias(true)
  context.setShouldSmoothFonts(true)
  context.setAllowsFontSmoothing(true)
  context.setFillColor(gray: 1, alpha: 1)
  context.textMatrix = .identity

  let panelBottom = CGFloat(size) - panelY - panelHeight
  let halfHeight = panelHeight / 2
  let attributes: [NSAttributedString.Key: Any] = [
    NSAttributedString.Key(kCTFontAttributeName as String): font,
    NSAttributedString.Key(kCTForegroundColorAttributeName as String): CGColor(gray: 1, alpha: 1),
  ]

  for (index, text) in ["22", "00"].enumerated() {
    let line = CTLineCreateWithAttributedString(NSAttributedString(string: text, attributes: attributes))
    var ascent: CGFloat = 0
    var descent: CGFloat = 0
    var leading: CGFloat = 0
    let lineWidth = CGFloat(CTLineGetTypographicBounds(line, &ascent, &descent, &leading))
    let regionBottom = panelBottom + (index == 0 ? halfHeight : 0)
    let baseline = regionBottom + (halfHeight - ascent - descent) / 2 + descent
    context.textPosition = CGPoint(
      x: panelX + (panelWidth - lineWidth) / 2,
      y: baseline
    )
    CTLineDraw(line, context)
  }

  for y in 0..<size {
    for x in 0..<size {
      let coverage = mask[y * size + x]
      guard coverage > 0 else { continue }
      let original = canvas.pixels[y * size + x]
      let inverse = 255 - Int(coverage)
      canvas.set(
        x,
        y,
        RGB(
          red: UInt8(Int(original.red) * inverse / 255),
          green: UInt8(Int(original.green) * inverse / 255),
          blue: UInt8(Int(original.blue) * inverse / 255)
        )
      )
    }
  }
}

func drawCodexWeekly(size: Int) -> PixelCanvas {
  var canvas = PixelCanvas(width: size, height: size, fill: black)

  if size == 25 {
    let compactGlyphs: [Character: [String]] = [
      "C": ["011", "100", "100", "100", "011"],
      "O": ["010", "101", "101", "101", "010"],
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
      canvas.rect(x: 1 + column * 3, y: 7, width: 2, height: 3, color: color)
      if column == 6 {
        canvas.rect(x: 19, y: 7, width: 3, height: 3, color: lime)
        canvas.set(20, 8, black)
      }
    }

    let heatmap: [[RGB]] = [
      [cyan, vividBlue, navy, cyan],
      [vividBlue, cyan, vividBlue, navy],
      [navy, vividBlue, cyan, vividBlue],
      [vividBlue, navy, vividBlue, cyan],
      [cyan, vividBlue, cyan, vividBlue],
    ]
    for row in 0..<5 {
      for column in 0..<4 {
        canvas.rect(x: 3 + column * 5, y: 11 + row * 3, width: 4, height: 2, color: heatmap[row][column])
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
      [cyan, vividBlue, cyan, vividBlue],
    ]
    for row in 0..<5 {
      for column in 0..<4 {
        canvas.rect(x: 3 + column * 7, y: 16 + row * 3, width: 5, height: 2, color: heatmap[row][column])
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
    var icon = scaled(appMaster, to: size)
    if kind == .mosaicGrid {
      try overlayRobotoMosaicDigits(on: &icon)
    }
    try writePNG(icon, to: output)
    print(output.path)
  }

  let resourceDirectory = root
    .appendingPathComponent(kind.rawValue, isDirectory: true)
    .appendingPathComponent("resources/images", isDirectory: true)
  try FileManager.default.createDirectory(at: resourceDirectory, withIntermediateDirectories: true)
  let menuOutput = resourceDirectory.appendingPathComponent("\(kind.rawValue)-menu-icon.png")
  var menuIcon = draw(kind, size: 25)
  if kind == .mosaicGrid {
    try overlayRobotoMosaicDigits(on: &menuIcon)
  }
  try writePNG(menuIcon, to: menuOutput)
  print(menuOutput.path)
}
