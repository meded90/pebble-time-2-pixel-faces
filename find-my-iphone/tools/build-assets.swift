#!/usr/bin/env swift

import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

// Reproducible Pebble Time 2 asset builder. The v4 sofa and foreground were
// redrawn from the user's original prototype with ImageGen. Outputs are
// cleaned and constrained to Pebble's 2-bit-per-channel (64-colour) palette.

let scriptURL = URL(fileURLWithPath: #filePath)
let appRoot = scriptURL.deletingLastPathComponent().deletingLastPathComponent()
let sourceDir = appRoot.appendingPathComponent("resources/source")
let imageDir = appRoot.appendingPathComponent("resources/images")
let assetDir = appRoot.appendingPathComponent("assets")

try FileManager.default.createDirectory(at: imageDir, withIntermediateDirectories: true)
try FileManager.default.createDirectory(at: assetDir, withIntermediateDirectories: true)

struct Canvas {
  let width: Int
  let height: Int
  let context: CGContext

  init(width: Int, height: Int, opaque: Bool = false) {
    self.width = width
    self.height = height
    guard let context = CGContext(
      data: nil, width: width, height: height, bitsPerComponent: 8,
      bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
      bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue | CGBitmapInfo.byteOrder32Big.rawValue
    ) else { fatalError("Unable to create canvas") }
    self.context = context
    context.setShouldAntialias(false)
    context.interpolationQuality = .none
    context.setFillColor(CGColor(red: 0, green: 0, blue: 0, alpha: opaque ? 1 : 0))
    context.fill(CGRect(x: 0, y: 0, width: width, height: height))
  }

  func image() -> CGImage {
    guard let image = context.makeImage() else { fatalError("Unable to finalize canvas") }
    return image
  }
}

func load(_ name: String) -> CGImage {
  let url = sourceDir.appendingPathComponent(name)
  guard let source = CGImageSourceCreateWithURL(url as CFURL, nil),
        let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
    fatalError("Unable to load \(url.path)")
  }
  return image
}

func save(_ image: CGImage, name: String) {
  let url = imageDir.appendingPathComponent(name)
  guard let destination = CGImageDestinationCreateWithURL(
    url as CFURL, UTType.png.identifier as CFString, 1, nil
  ) else { fatalError("Unable to create \(url.path)") }
  CGImageDestinationAddImage(destination, image, nil)
  guard CGImageDestinationFinalize(destination) else { fatalError("Unable to write \(url.path)") }
  print("wrote resources/images/\(name) [\(image.width)x\(image.height)]")
}

func render(_ image: CGImage, width: Int, height: Int, opaque: Bool = false) -> CGImage {
  let canvas = Canvas(width: width, height: height, opaque: opaque)
  canvas.context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
  return canvas.image()
}

func renderAveraged(_ image: CGImage, width: Int, height: Int) -> CGImage {
  let canvas = Canvas(width: width, height: height, opaque: true)
  canvas.context.interpolationQuality = .high
  canvas.context.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
  return canvas.image()
}

func alphaBounds(_ image: CGImage) -> CGRect {
  let canvas = Canvas(width: image.width, height: image.height)
  canvas.context.draw(image, in: CGRect(x: 0, y: 0, width: image.width, height: image.height))
  guard let bytes = canvas.context.data?.assumingMemoryBound(to: UInt8.self) else {
    return CGRect(x: 0, y: 0, width: image.width, height: image.height)
  }
  var minX = image.width
  var minY = image.height
  var maxX = -1
  var maxY = -1
  for y in 0..<image.height {
    for x in 0..<image.width where bytes[(y * image.width + x) * 4 + 3] > 24 {
      minX = min(minX, x)
      minY = min(minY, y)
      maxX = max(maxX, x)
      maxY = max(maxY, y)
    }
  }
  guard maxX >= minX, maxY >= minY else { return .zero }
  return CGRect(x: minX, y: minY, width: maxX - minX + 1, height: maxY - minY + 1)
}

func fit(_ image: CGImage, width: Int, height: Int) -> CGImage {
  let bounds = alphaBounds(image)
  guard !bounds.isEmpty, let cropped = image.cropping(to: bounds) else {
    return Canvas(width: width, height: height).image()
  }
  let scale = min(CGFloat(width) / CGFloat(cropped.width), CGFloat(height) / CGFloat(cropped.height))
  let drawWidth = max(1, Int((CGFloat(cropped.width) * scale).rounded()))
  let drawHeight = max(1, Int((CGFloat(cropped.height) * scale).rounded()))
  let canvas = Canvas(width: width, height: height)
  canvas.context.draw(cropped, in: CGRect(
    x: (width - drawWidth) / 2, y: (height - drawHeight) / 2,
    width: drawWidth, height: drawHeight
  ))
  return canvas.image()
}

func fitWithPadding(_ image: CGImage, width: Int, height: Int, padding: Int) -> CGImage {
  let innerWidth = width - padding * 2
  let innerHeight = height - padding * 2
  let fitted = fit(image, width: innerWidth, height: innerHeight)
  let canvas = Canvas(width: width, height: height)
  canvas.context.draw(fitted, in: CGRect(x: padding, y: padding, width: innerWidth, height: innerHeight))
  return canvas.image()
}

typealias PixelColor = (UInt8, UInt8, UInt8, UInt8)

struct PixelCanvas {
  let width: Int
  let height: Int
  var pixels: [UInt8]

  init(width: Int, height: Int) {
    self.width = width
    self.height = height
    self.pixels = Array(repeating: 0, count: width * height * 4)
  }

  mutating func set(_ x: Int, _ y: Int, _ color: PixelColor) {
    guard x >= 0, x < width, y >= 0, y < height else { return }
    let offset = (y * width + x) * 4
    pixels[offset] = color.0
    pixels[offset + 1] = color.1
    pixels[offset + 2] = color.2
    pixels[offset + 3] = color.3
  }

  mutating func fillRect(x: Int, y: Int, width: Int, height: Int, color: PixelColor) {
    for py in y..<(y + height) {
      for px in x..<(x + width) { set(px, py, color) }
    }
  }

  mutating func fillPolygon(_ points: [(Int, Int)], color: PixelColor) {
    guard points.count >= 3 else { return }
    let minX = points.map(\.0).min()!
    let maxX = points.map(\.0).max()!
    let minY = points.map(\.1).min()!
    let maxY = points.map(\.1).max()!
    for y in minY...maxY {
      for x in minX...maxX {
        var inside = false
        var previous = points.count - 1
        for current in 0..<points.count {
          let a = points[current]
          let b = points[previous]
          if (a.1 > y) != (b.1 > y) {
            let crossingX = Double(b.0 - a.0) * Double(y - a.1) / Double(b.1 - a.1) + Double(a.0)
            if Double(x) < crossingX { inside.toggle() }
          }
          previous = current
        }
        if inside { set(x, y, color) }
      }
    }
  }

  mutating func line(from start: (Int, Int), to end: (Int, Int), thickness: Int, color: PixelColor) {
    var x = start.0
    var y = start.1
    let dx = abs(end.0 - start.0)
    let sx = start.0 < end.0 ? 1 : -1
    let dy = -abs(end.1 - start.1)
    let sy = start.1 < end.1 ? 1 : -1
    var error = dx + dy
    while true {
      fillRect(x: x - thickness / 2, y: y - thickness / 2, width: thickness, height: thickness, color: color)
      if x == end.0 && y == end.1 { break }
      let twice = 2 * error
      if twice >= dy { error += dy; x += sx }
      if twice <= dx { error += dx; y += sy }
    }
  }

  mutating func polyline(_ points: [(Int, Int)], thickness: Int, color: PixelColor) {
    for index in 1..<points.count {
      line(from: points[index - 1], to: points[index], thickness: thickness, color: color)
    }
  }

  func image() -> CGImage {
    let provider = CGDataProvider(data: Data(pixels) as CFData)!
    let bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)
      .union(.byteOrder32Big)
    return CGImage(
      width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 32,
      bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
      bitmapInfo: bitmapInfo, provider: provider, decode: nil,
      shouldInterpolate: false, intent: .defaultIntent
    )!
  }
}

let pixelBlack: PixelColor = (0, 0, 0, 255)
let pixelNavy: PixelColor = (0, 0, 85, 255)
let pixelBlue: PixelColor = (0, 0, 170, 255)
let pixelMidBlue: PixelColor = (0, 85, 170, 255)
let pixelCyan: PixelColor = (0, 255, 255, 255)
let pixelLightCyan: PixelColor = (85, 255, 255, 255)
let pixelWhite: PixelColor = (255, 255, 255, 255)
let pixelGray: PixelColor = (85, 85, 85, 255)
let pixelGold: PixelColor = (255, 170, 0, 255)
let pixelYellow: PixelColor = (255, 255, 85, 255)

func drawPhone(on canvas: inout PixelCanvas, x: Int, y: Int) {
  // A deliberately simple MS Paint-like silhouette: broad, consistent
  // stair-steps and no isolated antialiasing pixels.
  canvas.fillPolygon([
    (x + 12, y), (x + 38, y), (x + 61, y + 43), (x + 61, y + 53),
    (x + 36, y + 69), (x + 26, y + 69), (x, y + 20), (x, y + 11)
  ], color: pixelBlack)
  canvas.fillPolygon([
    (x + 13, y + 4), (x + 36, y + 4), (x + 57, y + 44), (x + 57, y + 51),
    (x + 34, y + 65), (x + 28, y + 65), (x + 4, y + 19), (x + 4, y + 13)
  ], color: pixelGray)
  canvas.fillPolygon([
    (x + 14, y + 7), (x + 35, y + 7), (x + 53, y + 42),
    (x + 31, y + 58), (x + 8, y + 18)
  ], color: pixelBlack)
  canvas.fillPolygon([
    (x + 16, y + 11), (x + 33, y + 11), (x + 48, y + 41),
    (x + 31, y + 53), (x + 12, y + 17)
  ], color: pixelNavy)
  canvas.fillPolygon([
    (x + 18, y + 13), (x + 31, y + 13), (x + 43, y + 37),
    (x + 31, y + 47), (x + 16, y + 18)
  ], color: pixelBlue)
  canvas.fillRect(x: x + 17, y: y + 5, width: 13, height: 3, color: pixelWhite)
  canvas.fillRect(x: x + 6, y: y + 14, width: 3, height: 8, color: pixelWhite)
  canvas.fillRect(x: x + 9, y: y + 23, width: 3, height: 7, color: pixelWhite)
  canvas.fillRect(x: x + 12, y: y + 31, width: 3, height: 6, color: pixelWhite)
  canvas.fillPolygon([
    (x + 31, y + 59), (x + 39, y + 54), (x + 46, y + 54),
    (x + 38, y + 60)
  ], color: pixelWhite)
}

func drawWave(on canvas: inout PixelCanvas, points: [(Int, Int)], color: PixelColor) {
  canvas.polyline(points, thickness: 7, color: pixelNavy)
  canvas.polyline(points, thickness: 3, color: color)
}

func makeIdlePhone() -> CGImage {
  var canvas = PixelCanvas(width: 62, height: 70)
  drawPhone(on: &canvas, x: 0, y: 0)
  return canvas.image()
}

func makeRingingPhone(width: Int, height: Int, phoneY: Int, waveCount: Int) -> CGImage {
  var canvas = PixelCanvas(width: width, height: height)
  drawPhone(on: &canvas, x: 0, y: phoneY)
  let waves: [([(Int, Int)], PixelColor)] = [
    ([(65, 30), (70, 25), (71, 19), (68, 14)], pixelMidBlue),
    ([(78, 33), (84, 26), (85, 17), (81, 8)], pixelMidBlue),
    ([(92, 35), (99, 27), (101, 17), (98, 7), (93, 1)], pixelCyan),
  ]
  for index in 0..<waveCount { drawWave(on: &canvas, points: waves[index].0, color: waves[index].1) }
  return canvas.image()
}

func makeHiddenPhone() -> CGImage {
  var canvas = PixelCanvas(width: 104, height: 60)
  canvas.fillPolygon([(25, 2), (73, 2), (84, 11), (82, 30), (20, 30), (20, 10)], color: pixelBlack)
  canvas.fillPolygon([(28, 6), (70, 6), (79, 13), (78, 26), (24, 26), (24, 12)], color: pixelGray)
  canvas.fillRect(x: 29, y: 10, width: 45, height: 12, color: pixelNavy)
  canvas.fillRect(x: 43, y: 7, width: 17, height: 3, color: pixelWhite)
  canvas.fillRect(x: 25, y: 13, width: 3, height: 8, color: pixelWhite)
  canvas.fillPolygon([(17, 22), (82, 22), (100, 32), (100, 43), (90, 50), (18, 50), (4, 44), (4, 35)], color: pixelBlack)
  canvas.fillPolygon([(18, 26), (79, 26), (96, 34), (96, 40), (87, 44), (19, 44), (8, 40), (8, 35)], color: pixelYellow)
  canvas.fillPolygon([(14, 41), (93, 41), (87, 47), (19, 47), (8, 43)], color: pixelGold)
  canvas.fillRect(x: 20, y: 44, width: 67, height: 3, color: pixelBlack)
  return canvas.image()
}

typealias PaletteColor = (UInt8, UInt8, UInt8)

let backgroundPalette: [PaletteColor] = [
  (0, 0, 0), (0, 0, 85), (0, 85, 85), (0, 85, 170), (0, 85, 255),
  (0, 170, 170), (0, 170, 255), (85, 85, 170), (85, 85, 255),
  (85, 170, 255), (170, 170, 255), (170, 255, 255), (85, 85, 85), (170, 170, 170),
]
let phonePalette: [PaletteColor] = [
  (0, 0, 0), (0, 0, 85), (0, 0, 170), (0, 0, 255), (0, 85, 170),
  (0, 170, 255), (0, 255, 255), (85, 255, 255), (85, 85, 85), (170, 170, 170), (255, 255, 255),
]
let cushionPalette: [PaletteColor] = [
  (0, 0, 0), (170, 85, 0), (255, 85, 0), (255, 170, 0), (255, 255, 85), (255, 255, 255),
]
let successPalette: [PaletteColor] = [
  (0, 0, 0), (0, 85, 0), (0, 170, 0), (0, 255, 85), (85, 255, 170), (255, 255, 255),
]
let errorPalette: [PaletteColor] = [
  (0, 0, 0), (170, 0, 0), (255, 0, 0), (255, 85, 85), (255, 255, 255),
]

func quantize(_ image: CGImage, transparent: Bool, palette: [PaletteColor]) -> CGImage {
  let canvas = Canvas(width: image.width, height: image.height, opaque: !transparent)
  canvas.context.draw(image, in: CGRect(x: 0, y: 0, width: image.width, height: image.height))
  guard let bytes = canvas.context.data?.assumingMemoryBound(to: UInt8.self) else {
    fatalError("Unable to access image pixels")
  }
  for pixel in 0..<(image.width * image.height) {
    let offset = pixel * 4
    let alpha = bytes[offset + 3]
    if transparent && alpha < 180 {
      bytes[offset] = 0
      bytes[offset + 1] = 0
      bytes[offset + 2] = 0
      bytes[offset + 3] = 0
      continue
    }
    let source: PaletteColor
    if transparent && alpha < 255 {
      source = (
        UInt8(min(255, Int(bytes[offset]) * 255 / Int(alpha))),
        UInt8(min(255, Int(bytes[offset + 1]) * 255 / Int(alpha))),
        UInt8(min(255, Int(bytes[offset + 2]) * 255 / Int(alpha)))
      )
    } else {
      source = (bytes[offset], bytes[offset + 1], bytes[offset + 2])
    }
    let nearest = palette.min { left, right in
      let ld = pow(Double(Int(source.0) - Int(left.0)), 2) + pow(Double(Int(source.1) - Int(left.1)), 2) + pow(Double(Int(source.2) - Int(left.2)), 2)
      let rd = pow(Double(Int(source.0) - Int(right.0)), 2) + pow(Double(Int(source.1) - Int(right.1)), 2) + pow(Double(Int(source.2) - Int(right.2)), 2)
      return ld < rd
    }!
    bytes[offset] = nearest.0
    bytes[offset + 1] = nearest.1
    bytes[offset + 2] = nearest.2
    bytes[offset + 3] = 255
  }
  return canvas.image()
}

func normalizeBackgroundSky(_ image: CGImage) -> CGImage {
  let canvas = Canvas(width: image.width, height: image.height, opaque: true)
  canvas.context.draw(image, in: CGRect(x: 0, y: 0, width: image.width, height: image.height))
  guard let bytes = canvas.context.data?.assumingMemoryBound(to: UInt8.self) else {
    fatalError("Unable to normalize background")
  }
  for pixel in 0..<(image.width * image.height) {
    let offset = pixel * 4
    if bytes[offset] == 170 && bytes[offset + 1] == 170 &&
       (bytes[offset + 2] == 170 || bytes[offset + 2] == 255) {
      bytes[offset] = 170
      bytes[offset + 1] = 170
      bytes[offset + 2] = 255
      bytes[offset + 3] = 255
    }
  }
  return canvas.image()
}

func audit(_ image: CGImage, name: String, transparent: Bool, palette: [PaletteColor]) {
  let canvas = Canvas(width: image.width, height: image.height)
  canvas.context.draw(image, in: CGRect(x: 0, y: 0, width: image.width, height: image.height))
  guard let bytes = canvas.context.data?.assumingMemoryBound(to: UInt8.self) else { fatalError("Unable to audit \(name)") }
  var used = Set<String>()
  for pixel in 0..<(image.width * image.height) {
    let offset = pixel * 4
    let alpha = bytes[offset + 3]
    guard alpha == 0 || alpha == 255 else { fatalError("\(name): non-binary alpha \(alpha)") }
    if alpha == 0 { continue }
    if !transparent && alpha != 255 { fatalError("\(name): background must be opaque") }
    let color = (bytes[offset], bytes[offset + 1], bytes[offset + 2])
    let redIsValid = color.0 == 0 || color.0 == 85 || color.0 == 170 || color.0 == 255
    let greenIsValid = color.1 == 0 || color.1 == 85 || color.1 == 170 || color.1 == 255
    let blueIsValid = color.2 == 0 || color.2 == 85 || color.2 == 170 || color.2 == 255
    guard redIsValid && greenIsValid && blueIsValid else {
      fatalError("\(name): color is outside Pebble 64 palette")
    }
    guard palette.contains(where: { $0.0 == color.0 && $0.1 == color.1 && $0.2 == color.2 }) else {
      fatalError("\(name): color is outside its curated subset")
    }
    used.insert(String(format: "#%02X%02X%02X", color.0, color.1, color.2))
  }
  print("verified \(name): \(used.count) Pebble colors, alpha \(transparent ? "0/255" : "255")")
}

let backgroundSource = load("background-v5-imagegen.png")
let foregroundSource = load("foreground-v4-imagegen.png")
let atlas = load("sprite-atlas-v2-generated.png")
let cellWidth = atlas.width / 4
let cellHeight = atlas.height / 3

func atlasCell(column: Int, row: Int) -> CGImage {
  let rect = CGRect(x: column * cellWidth, y: row * cellHeight, width: cellWidth, height: cellHeight)
  guard let cell = atlas.cropping(to: rect) else { fatalError("Unable to crop atlas cell") }
  return cell
}

let background = normalizeBackgroundSky(
  quantize(renderAveraged(backgroundSource, width: 200, height: 228), transparent: false, palette: backgroundPalette)
)
audit(background, name: "background-sofa-v5.png", transparent: false, palette: backgroundPalette)
save(background, name: "background-sofa-v5.png")

let specs: [(String, Int, Int, Int, Int, [PaletteColor])] = [
  ("cushion-low-v3.png", 0, 1, 96, 50, cushionPalette),
  ("cushion-high-v3.png", 1, 1, 96, 58, cushionPalette),
  ("badge-success-v3.png", 2, 1, 30, 30, successPalette),
  ("badge-error-v3.png", 3, 1, 30, 30, errorPalette),
  ("badge-auth-v3.png", 0, 2, 30, 30, cushionPalette),
  ("badge-offline-v3.png", 1, 2, 30, 30, phonePalette),
  ("menu-icon-v3.png", 2, 2, 25, 25, phonePalette),
]

var images: [String: CGImage] = [:]
for (name, column, row, width, height, palette) in specs {
  let output = quantize(fit(atlasCell(column: column, row: row), width: width, height: height), transparent: true, palette: palette)
  audit(output, name: name, transparent: true, palette: palette)
  images[name] = output
  save(output, name: name)
}

let foregroundPalette = phonePalette + cushionPalette
let foreground = quantize(
  fitWithPadding(foregroundSource, width: 176, height: 120, padding: 8),
  transparent: true,
  palette: foregroundPalette
)
audit(foreground, name: "scene-foreground-v4.png", transparent: true, palette: foregroundPalette)
images["scene-foreground-v4.png"] = foreground
save(foreground, name: "scene-foreground-v4.png")

func makeWaveOverlay(waveCount: Int) -> CGImage {
  var canvas = PixelCanvas(width: 60, height: 64)
  let waves: [([(Int, Int)], PixelColor)] = [
    ([(5, 43), (11, 37), (12, 30), (8, 25)], pixelCyan),
    ([(18, 51), (27, 42), (29, 30), (25, 18)], pixelLightCyan),
    ([(32, 58), (43, 47), (46, 31), (42, 15), (35, 7)], pixelWhite),
  ]
  for index in 0..<waveCount { drawWave(on: &canvas, points: waves[index].0, color: waves[index].1) }
  return canvas.image()
}

for waveCount in 1...3 {
  let name = "wave-\(waveCount)-v4.png"
  let wave = makeWaveOverlay(waveCount: waveCount)
  audit(wave, name: name, transparent: true, palette: phonePalette)
  images[name] = wave
  save(wave, name: name)
}

// Four exact 200x228 screens for quick visual regression review.
let preview = Canvas(width: 800, height: 228, opaque: true)
for frame in 0..<4 {
  preview.context.draw(background, in: CGRect(x: frame * 200, y: 0, width: 200, height: 228))
}
let scene = images["scene-foreground-v4.png"]!
let wave1 = images["wave-1-v4.png"]!
let wave2 = images["wave-2-v4.png"]!
let wave3 = images["wave-3-v4.png"]!
let success = images["badge-success-v3.png"]!
func previewY(_ nativeY: Int, height: Int) -> Int { 228 - nativeY - height }
for frame in 0..<4 {
  preview.context.draw(scene, in: CGRect(x: frame * 200 + 24, y: previewY(67, height: 99), width: 145, height: 99))
}
preview.context.draw(wave1, in: CGRect(x: 200 + 140, y: previewY(111, height: 64), width: 60, height: 64))
preview.context.draw(wave2, in: CGRect(x: 400 + 140, y: previewY(111, height: 64), width: 60, height: 64))
preview.context.draw(wave3, in: CGRect(x: 600 + 140, y: previewY(111, height: 64), width: 60, height: 64))
preview.context.draw(success, in: CGRect(x: 762, y: previewY(12, height: 30), width: 30, height: 30))

let previewURL = assetDir.appendingPathComponent("states-preview-v4.png")
guard let destination = CGImageDestinationCreateWithURL(
  previewURL as CFURL, UTType.png.identifier as CFString, 1, nil
) else { fatalError("Unable to create preview") }
CGImageDestinationAddImage(destination, preview.image(), nil)
guard CGImageDestinationFinalize(destination) else { fatalError("Unable to write preview") }
print("wrote assets/states-preview-v4.png [800x228]")
