#!/usr/bin/env swift

import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

guard CommandLine.arguments.count == 3 else {
  fputs("Usage: clean-background.swift INPUT OUTPUT\n", stderr)
  exit(2)
}

let inputURL = URL(fileURLWithPath: CommandLine.arguments[1])
let outputURL = URL(fileURLWithPath: CommandLine.arguments[2])

guard
  let source = CGImageSourceCreateWithURL(inputURL as CFURL, nil),
  let inputImage = CGImageSourceCreateImageAtIndex(source, 0, nil)
else {
  fputs("Unable to read input image\n", stderr)
  exit(1)
}

let width = inputImage.width
let height = inputImage.height
let bytesPerRow = width * 4
var pixels = [UInt8](repeating: 0, count: bytesPerRow * height)
let colorSpace = CGColorSpaceCreateDeviceRGB()
let bitmapInfo = CGBitmapInfo(
  rawValue: CGImageAlphaInfo.premultipliedLast.rawValue
    | CGBitmapInfo.byteOrder32Big.rawValue
)

guard let context = CGContext(
  data: &pixels,
  width: width,
  height: height,
  bitsPerComponent: 8,
  bytesPerRow: bytesPerRow,
  space: colorSpace,
  bitmapInfo: bitmapInfo.rawValue
) else {
  fputs("Unable to create image context\n", stderr)
  exit(1)
}

context.interpolationQuality = .none
context.draw(inputImage, in: CGRect(x: 0, y: 0, width: width, height: height))

@inline(__always)
func pixelIndex(_ x: Int, _ y: Int) -> Int {
  y * bytesPerRow + x * 4
}

@inline(__always)
func luminance(_ red: UInt8, _ green: UInt8, _ blue: UInt8) -> Double {
  0.2126 * Double(red) + 0.7152 * Double(green) + 0.0722 * Double(blue)
}

@inline(__always)
func isBackgroundCandidate(_ x: Int, _ y: Int) -> Bool {
  let index = pixelIndex(x, y)
  let red = pixels[index]
  let green = pixels[index + 1]
  let blue = pixels[index + 2]
  return min(red, green, blue) >= 145 && luminance(red, green, blue) >= 205
}

var background = [Bool](repeating: false, count: width * height)
var queue = [Int]()
queue.reserveCapacity(width * height)

func enqueueBackground(_ x: Int, _ y: Int) {
  guard x >= 0, x < width, y >= 0, y < height else {
    return
  }
  let offset = y * width + x
  guard !background[offset], isBackgroundCandidate(x, y) else {
    return
  }
  background[offset] = true
  queue.append(offset)
}

for x in 0..<width {
  enqueueBackground(x, 0)
  enqueueBackground(x, height - 1)
}
for y in 0..<height {
  enqueueBackground(0, y)
  enqueueBackground(width - 1, y)
}

var queueHead = 0
while queueHead < queue.count {
  let offset = queue[queueHead]
  queueHead += 1
  let x = offset % width
  let y = offset / width
  enqueueBackground(x - 1, y)
  enqueueBackground(x + 1, y)
  enqueueBackground(x, y - 1)
  enqueueBackground(x, y + 1)
}

@inline(__always)
func touchesBackground(_ x: Int, _ y: Int, radius: Int) -> Bool {
  let minX = max(0, x - radius)
  let maxX = min(width - 1, x + radius)
  let minY = max(0, y - radius)
  let maxY = min(height - 1, y + radius)
  for sampleY in minY...maxY {
    for sampleX in minX...maxX where background[sampleY * width + sampleX] {
      return true
    }
  }
  return false
}

@inline(__always)
func touchesForeground(_ x: Int, _ y: Int) -> Bool {
  let minX = max(0, x - 1)
  let maxX = min(width - 1, x + 1)
  let minY = max(0, y - 1)
  let maxY = min(height - 1, y + 1)
  for sampleY in minY...maxY {
    for sampleX in minX...maxX where !background[sampleY * width + sampleX] {
      return true
    }
  }
  return false
}

struct RGB {
  let red: UInt8
  let green: UInt8
  let blue: UInt8
}

let black = RGB(red: 0, green: 0, blue: 0)
let lightGray = RGB(red: 170, green: 170, blue: 170)
let white = RGB(red: 255, green: 255, blue: 255)

let controlledPalette = [
  black,
  RGB(red: 85, green: 0, blue: 0),
  RGB(red: 170, green: 0, blue: 0),
  RGB(red: 170, green: 85, blue: 85),
  RGB(red: 255, green: 0, blue: 0),
  RGB(red: 255, green: 85, blue: 0),
  RGB(red: 170, green: 85, blue: 0),
  RGB(red: 255, green: 170, blue: 0),
  RGB(red: 255, green: 255, blue: 0),
  RGB(red: 255, green: 255, blue: 85),
  RGB(red: 255, green: 255, blue: 170),
  RGB(red: 85, green: 85, blue: 85),
  lightGray,
  white,
]

@inline(__always)
func nearestControlledColor(_ red: UInt8, _ green: UInt8, _ blue: UInt8) -> RGB {
  var nearest = controlledPalette[0]
  var nearestDistance = Double.greatestFiniteMagnitude
  for color in controlledPalette {
    let redDelta = Double(Int(red) - Int(color.red))
    let greenDelta = Double(Int(green) - Int(color.green))
    let blueDelta = Double(Int(blue) - Int(color.blue))
    let distance = 0.30 * redDelta * redDelta
      + 0.59 * greenDelta * greenDelta
      + 0.11 * blueDelta * blueDelta
    if distance < nearestDistance {
      nearestDistance = distance
      nearest = color
    }
  }
  return nearest
}

@inline(__always)
func cleanedForegroundColor(_ red: UInt8, _ green: UInt8, _ blue: UInt8) -> RGB {
  let brightness = luminance(red, green, blue)
  let maximum = max(red, green, blue)
  let minimum = min(red, green, blue)

  if maximum <= 55 || brightness <= 42 {
    return black
  }

  if minimum >= 205 && brightness >= 225 {
    return white
  }

  if red >= 150 && green >= 125 && blue <= 125 {
    if green >= 205 {
      return RGB(red: 255, green: 255, blue: blue >= 115 ? 170 : 0)
    }
    if green >= 155 {
      return RGB(red: 255, green: 170, blue: 0)
    }
    return RGB(red: 255, green: 85, blue: 0)
  }

  if Int(red) >= Int(green) + 45 && Int(red) >= Int(blue) + 45 {
    if red >= 185 {
      return RGB(red: 255, green: 0, blue: 0)
    }
    if red >= 115 {
      return RGB(red: 170, green: 0, blue: 0)
    }
    return RGB(red: 85, green: 0, blue: 0)
  }

  return nearestControlledColor(red, green, blue)
}

for y in 0..<height {
  for x in 0..<width {
    let offset = y * width + x
    let index = pixelIndex(x, y)
    let red = pixels[index]
    let green = pixels[index + 1]
    let blue = pixels[index + 2]
    let cleaned: RGB

    if background[offset] {
      cleaned = white
    } else if touchesBackground(x, y, radius: 2)
      && luminance(red, green, blue) > 75 {
      cleaned = white
    } else {
      cleaned = cleanedForegroundColor(red, green, blue)
    }

    pixels[index] = cleaned.red
    pixels[index + 1] = cleaned.green
    pixels[index + 2] = cleaned.blue
    pixels[index + 3] = 255
  }
}

guard
  let provider = CGDataProvider(data: Data(pixels) as CFData),
  let outputImage = CGImage(
    width: width,
    height: height,
    bitsPerComponent: 8,
    bitsPerPixel: 32,
    bytesPerRow: bytesPerRow,
    space: colorSpace,
    bitmapInfo: CGBitmapInfo(
      rawValue: CGImageAlphaInfo.noneSkipLast.rawValue
        | CGBitmapInfo.byteOrder32Big.rawValue
    ),
    provider: provider,
    decode: nil,
    shouldInterpolate: false,
    intent: .defaultIntent
  ),
  let destination = CGImageDestinationCreateWithURL(
    outputURL as CFURL,
    UTType.png.identifier as CFString,
    1,
    nil
  )
else {
  fputs("Unable to create output image\n", stderr)
  exit(1)
}

CGImageDestinationAddImage(destination, outputImage, nil)
guard CGImageDestinationFinalize(destination) else {
  fputs("Unable to write output image\n", stderr)
  exit(1)
}

print("Cleaned \(width)x\(height) background with controlled Pebble colors and no outer shadow")
