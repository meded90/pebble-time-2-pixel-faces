#!/usr/bin/env swift

import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

guard CommandLine.arguments.count == 3 else {
  fputs("Usage: quantize-pebble-palette.swift INPUT.png OUTPUT.png\n", stderr)
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

guard
  let context = CGContext(
    data: &pixels,
    width: width,
    height: height,
    bitsPerComponent: 8,
    bytesPerRow: bytesPerRow,
    space: colorSpace,
    bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
      | CGBitmapInfo.byteOrder32Big.rawValue
  )
else {
  fputs("Unable to create bitmap context\n", stderr)
  exit(1)
}

context.interpolationQuality = .none
context.draw(
  inputImage,
  in: CGRect(x: 0, y: 0, width: width, height: height)
)

@inline(__always)
func pebbleChannel(_ value: UInt8) -> UInt8 {
  UInt8(min(3, (Int(value) + 42) / 85) * 85)
}

var colors = Set<UInt32>()

for index in stride(from: 0, to: pixels.count, by: 4) {
  let red = pebbleChannel(pixels[index])
  let green = pebbleChannel(pixels[index + 1])
  let blue = pebbleChannel(pixels[index + 2])

  pixels[index] = red
  pixels[index + 1] = green
  pixels[index + 2] = blue
  pixels[index + 3] = 255

  colors.insert(
    UInt32(red) << 16 | UInt32(green) << 8 | UInt32(blue)
  )
}

let imageData = Data(pixels) as CFData

guard
  let provider = CGDataProvider(data: imageData),
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

print("\(width)x\(height), \(colors.count) colors")
