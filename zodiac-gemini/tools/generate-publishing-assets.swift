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

  mutating func roundedRect(
    x: Int,
    y: Int,
    width: Int,
    height: Int,
    radius: Int,
    color: RGB
  ) {
    guard width > 0, height > 0 else { return }
    let clampedRadius = max(0, min(radius, min(width, height) / 2))
    let leftCenter = Double(x + clampedRadius)
    let rightCenter = Double(x + width - clampedRadius - 1)
    let topCenter = Double(y + clampedRadius)
    let bottomCenter = Double(y + height - clampedRadius - 1)
    let radiusSquared = Double(clampedRadius * clampedRadius)

    for row in y..<(y + height) {
      for column in x..<(x + width) {
        let centerX = Double(column)
        let centerY = Double(row)
        let nearestX = min(max(centerX, leftCenter), rightCenter)
        let nearestY = min(max(centerY, topCenter), bottomCenter)
        let deltaX = centerX - nearestX
        let deltaY = centerY - nearestY
        if deltaX * deltaX + deltaY * deltaY <= radiusSquared {
          set(column, row, color)
        }
      }
    }
  }

  mutating func framedRoundedRect(
    x: Int,
    y: Int,
    width: Int,
    height: Int,
    radius: Int,
    border: Int,
    fill: RGB,
    stroke: RGB
  ) {
    roundedRect(x: x, y: y, width: width, height: height, radius: radius, color: stroke)
    roundedRect(
      x: x + border,
      y: y + border,
      width: width - border * 2,
      height: height - border * 2,
      radius: max(0, radius - border),
      color: fill
    )
  }

  mutating func glyph(_ rows: [String], x: Int, y: Int, scale: Int, color: RGB) {
    for (rowIndex, row) in rows.enumerated() {
      for (columnIndex, value) in row.enumerated() where value == "1" {
        rect(
          x: x + columnIndex * scale,
          y: y + rowIndex * scale,
          width: scale,
          height: scale,
          color: color
        )
      }
    }
  }
}

let black = RGB(red: 0, green: 0, blue: 0)
let darkRed = RGB(red: 170, green: 0, blue: 0)
let red = RGB(red: 255, green: 0, blue: 0)
let lightGray = RGB(red: 170, green: 170, blue: 170)
let white = RGB(red: 255, green: 255, blue: 255)
let yellow = RGB(red: 255, green: 255, blue: 0)

let glyphs: [Character: [String]] = [
  "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
  "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
  "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
  "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
  "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
  "G": ["01111", "10000", "10000", "10111", "10001", "10001", "01111"],
  "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
  "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
  "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
  "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
  "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
  "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
  "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
  "Z": ["11111", "00001", "00010", "00100", "01000", "10000", "11111"],
  "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
]

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
    throw NSError(domain: "ZodiacGeminiAssets", code: 1)
  }

  CGImageDestinationAddImage(destination, image, nil)
  guard CGImageDestinationFinalize(destination) else {
    throw NSError(domain: "ZodiacGeminiAssets", code: 2)
  }
  print(url.path)
}

func loadImage(_ url: URL) throws -> PixelCanvas {
  guard
    let source = CGImageSourceCreateWithURL(url as CFURL, nil),
    let image = CGImageSourceCreateImageAtIndex(source, 0, nil)
  else {
    throw NSError(domain: "ZodiacGeminiAssets", code: 3)
  }

  let width = image.width
  let height = image.height
  var bytes = [UInt8](repeating: 0, count: width * height * 4)
  guard let context = CGContext(
    data: &bytes,
    width: width,
    height: height,
    bitsPerComponent: 8,
    bytesPerRow: width * 4,
    space: CGColorSpaceCreateDeviceRGB(),
    bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
  ) else {
    throw NSError(domain: "ZodiacGeminiAssets", code: 4)
  }

  context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))

  var canvas = PixelCanvas(width: width, height: height, fill: white)
  for y in 0..<height {
    for x in 0..<width {
      let index = (y * width + x) * 4
      canvas.set(x, y, RGB(red: bytes[index], green: bytes[index + 1], blue: bytes[index + 2]))
    }
  }
  return canvas
}

func mix(_ a: UInt8, _ b: UInt8, amount: Double) -> UInt8 {
  UInt8(max(0, min(255, Int((Double(a) * (1 - amount) + Double(b) * amount).rounded()))))
}

func bilinearSample(_ source: PixelCanvas, x: Double, y: Double) -> RGB {
  let x0 = max(0, min(source.width - 1, Int(floor(x))))
  let y0 = max(0, min(source.height - 1, Int(floor(y))))
  let x1 = min(source.width - 1, x0 + 1)
  let y1 = min(source.height - 1, y0 + 1)
  let amountX = x - floor(x)
  let amountY = y - floor(y)
  let topLeft = source.pixels[y0 * source.width + x0]
  let topRight = source.pixels[y0 * source.width + x1]
  let bottomLeft = source.pixels[y1 * source.width + x0]
  let bottomRight = source.pixels[y1 * source.width + x1]

  let top = RGB(
    red: mix(topLeft.red, topRight.red, amount: amountX),
    green: mix(topLeft.green, topRight.green, amount: amountX),
    blue: mix(topLeft.blue, topRight.blue, amount: amountX)
  )
  let bottom = RGB(
    red: mix(bottomLeft.red, bottomRight.red, amount: amountX),
    green: mix(bottomLeft.green, bottomRight.green, amount: amountX),
    blue: mix(bottomLeft.blue, bottomRight.blue, amount: amountX)
  )
  return RGB(
    red: mix(top.red, bottom.red, amount: amountY),
    green: mix(top.green, bottom.green, amount: amountY),
    blue: mix(top.blue, bottom.blue, amount: amountY)
  )
}

func drawScaled(
  source: PixelCanvas,
  cropX: Double,
  cropY: Double,
  cropWidth: Double,
  cropHeight: Double,
  canvas: inout PixelCanvas,
  x: Int,
  y: Int,
  width: Int,
  height: Int,
  roundedRadius: Int? = nil
) {
  for outputY in 0..<height {
    for outputX in 0..<width {
      if let radius = roundedRadius {
        let nearestX = min(max(outputX, radius), width - radius - 1)
        let nearestY = min(max(outputY, radius), height - radius - 1)
        let dx = outputX - nearestX
        let dy = outputY - nearestY
        if dx * dx + dy * dy > radius * radius { continue }
      }
      let sourceX = cropX + (Double(outputX) + 0.5) * cropWidth / Double(width) - 0.5
      let sourceY = cropY + (Double(outputY) + 0.5) * cropHeight / Double(height) - 0.5
      canvas.set(x + outputX, y + outputY, bilinearSample(source, x: sourceX, y: sourceY))
    }
  }
}

func drawGeminiMark(canvas: inout PixelCanvas, x: Int, y: Int, size: Int, framed: Bool) {
  if framed {
    let border = max(1, size / 22)
    canvas.framedRoundedRect(
      x: x,
      y: y,
      width: size,
      height: size,
      radius: max(2, size / 6),
      border: border,
      fill: lightGray,
      stroke: black
    )
  }

  let inset = max(4, size * 5 / 23)
  let barWidth = max(2, size * 3 / 23)
  let crossHeight = max(2, size * 3 / 23)
  let crossWidth = size - inset * 2
  let topY = y + inset
  let bottomY = y + size - inset - crossHeight
  let verticalY = topY + crossHeight / 2
  let verticalHeight = bottomY - verticalY + crossHeight / 2

  canvas.roundedRect(
    x: x + inset,
    y: topY,
    width: crossWidth,
    height: crossHeight,
    radius: crossHeight / 2,
    color: black
  )
  canvas.roundedRect(
    x: x + inset,
    y: bottomY,
    width: crossWidth,
    height: crossHeight,
    radius: crossHeight / 2,
    color: black
  )
  canvas.roundedRect(
    x: x + inset + barWidth / 2,
    y: verticalY,
    width: barWidth,
    height: verticalHeight,
    radius: barWidth / 2,
    color: darkRed
  )
  canvas.roundedRect(
    x: x + size - inset - barWidth - barWidth / 2,
    y: verticalY,
    width: barWidth,
    height: verticalHeight,
    radius: barWidth / 2,
    color: darkRed
  )
  let accent = max(1, size / 24)
  canvas.rect(
    x: x + size - inset - accent,
    y: y + inset,
    width: accent,
    height: accent,
    color: yellow
  )
}

func drawText(
  _ text: String,
  canvas: inout PixelCanvas,
  x: Int,
  y: Int,
  scale: Int,
  color: RGB
) {
  var cursor = x
  for character in text {
    if character == " " {
      cursor += 4 * scale
      continue
    }
    if let rows = glyphs[character] {
      canvas.glyph(rows, x: cursor, y: y, scale: scale, color: color)
    }
    cursor += 6 * scale
  }
}

func appIcon(source: PixelCanvas, size: Int) -> PixelCanvas {
  var canvas = PixelCanvas(width: size, height: size, fill: lightGray)
  let margin = max(1, Int((Double(size) * 0.035).rounded()))
  let artSize = size - margin * 2
  let radius = max(2, size * 14 / 100)
  drawScaled(
    source: source,
    cropX: 58,
    cropY: 0,
    cropWidth: 136,
    cropHeight: 136,
    canvas: &canvas,
    x: margin,
    y: margin,
    width: artSize,
    height: artSize,
    roundedRadius: radius
  )

  let border = max(1, size * 25 / 1000)
  for step in 0..<border {
    let inset = margin + step
    let width = size - inset * 2
    for column in inset..<(inset + width) {
      canvas.set(column, inset, black)
      canvas.set(column, inset + width - 1, black)
    }
    for row in inset..<(inset + width) {
      canvas.set(inset, row, black)
      canvas.set(inset + width - 1, row, black)
    }
  }

  let badgeSize = max(12, size * 34 / 100)
  drawGeminiMark(
    canvas: &canvas,
    x: max(1, size * 55 / 1000),
    y: size - badgeSize - max(1, size * 55 / 1000),
    size: badgeSize,
    framed: true
  )
  return canvas
}

func banner(source: PixelCanvas) -> PixelCanvas {
  var canvas = PixelCanvas(width: 720, height: 320, fill: lightGray)
  canvas.rect(x: 424, y: 0, width: 296, height: 320, color: white)
  drawScaled(
    source: source,
    cropX: 0,
    cropY: 0,
    cropWidth: 200,
    cropHeight: 228,
    canvas: &canvas,
    x: 439,
    y: 0,
    width: 281,
    height: 320
  )
  drawGeminiMark(canvas: &canvas, x: 42, y: 42, size: 92, framed: true)
  drawText("ZODIAC", canvas: &canvas, x: 42, y: 145, scale: 8, color: black)
  drawText("GEMINI", canvas: &canvas, x: 38, y: 209, scale: 10, color: darkRed)
  drawText("PEBBLE TIME 2", canvas: &canvas, x: 45, y: 290, scale: 3, color: black)
  canvas.rect(x: 412, y: 0, width: 12, height: 320, color: black)
  canvas.rect(x: 412, y: 0, width: 12, height: 76, color: red)
  canvas.rect(x: 412, y: 76, width: 12, height: 18, color: yellow)
  return canvas
}

let root = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
let source = try loadImage(root.appendingPathComponent("zodiac-gemini/resources/images/background.png"))
let iconDirectory = root.appendingPathComponent("assets/icons", isDirectory: true)
let bannerDirectory = root.appendingPathComponent("assets/banners", isDirectory: true)
try FileManager.default.createDirectory(at: iconDirectory, withIntermediateDirectories: true)
try FileManager.default.createDirectory(at: bannerDirectory, withIntermediateDirectories: true)

for size in [32, 80, 144, 512] {
  try writePNG(
    appIcon(source: source, size: size),
    to: iconDirectory.appendingPathComponent("zodiac-gemini-icon-\(size).png")
  )
}

var menuIcon = PixelCanvas(width: 25, height: 25, fill: lightGray)
drawGeminiMark(canvas: &menuIcon, x: 1, y: 1, size: 23, framed: true)
try writePNG(
  menuIcon,
  to: root.appendingPathComponent("zodiac-gemini/resources/images/zodiac-gemini-menu-icon.png")
)
try writePNG(
  banner(source: source),
  to: bannerDirectory.appendingPathComponent("zodiac-gemini-banner-720x320.png")
)
