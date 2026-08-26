#!/usr/bin/env swift

import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

guard CommandLine.arguments.count == 3 else {
  fputs("Usage: build-round-background.swift INPUT OUTPUT\n", stderr)
  exit(2)
}

let inputURL = URL(fileURLWithPath: CommandLine.arguments[1])
let outputURL = URL(fileURLWithPath: CommandLine.arguments[2])
let outputSize = 260

guard
  let source = CGImageSourceCreateWithURL(inputURL as CFURL, nil),
  let inputImage = CGImageSourceCreateImageAtIndex(source, 0, nil)
else {
  fputs("Unable to read input image\n", stderr)
  exit(1)
}

let cropSize = min(inputImage.width, inputImage.height)
let cropRect = CGRect(
  x: (inputImage.width - cropSize) / 2,
  y: (inputImage.height - cropSize) / 2,
  width: cropSize,
  height: cropSize
)

guard let croppedImage = inputImage.cropping(to: cropRect) else {
  fputs("Unable to crop input image\n", stderr)
  exit(1)
}

let bytesPerRow = outputSize * 4
var pixels = [UInt8](repeating: 0, count: bytesPerRow * outputSize)
let colorSpace = CGColorSpaceCreateDeviceRGB()
let bitmapInfo = CGBitmapInfo(
  rawValue: CGImageAlphaInfo.premultipliedLast.rawValue
    | CGBitmapInfo.byteOrder32Big.rawValue
)

guard let context = CGContext(
  data: &pixels,
  width: outputSize,
  height: outputSize,
  bitsPerComponent: 8,
  bytesPerRow: bytesPerRow,
  space: colorSpace,
  bitmapInfo: bitmapInfo.rawValue
) else {
  fputs("Unable to create output context\n", stderr)
  exit(1)
}

context.interpolationQuality = .none
context.setShouldAntialias(false)
context.draw(
  croppedImage,
  in: CGRect(x: 0, y: 0, width: outputSize, height: outputSize)
)

@inline(__always)
func pebbleChannel(_ value: UInt8) -> UInt8 {
  UInt8(min(3, (Int(value) + 42) / 85) * 85)
}

for index in stride(from: 0, to: pixels.count, by: 4) {
  pixels[index] = pebbleChannel(pixels[index])
  pixels[index + 1] = pebbleChannel(pixels[index + 1])
  pixels[index + 2] = pebbleChannel(pixels[index + 2])
  pixels[index + 3] = 255
}

guard
  let provider = CGDataProvider(data: Data(pixels) as CFData),
  let outputImage = CGImage(
    width: outputSize,
    height: outputSize,
    bitsPerComponent: 8,
    bitsPerPixel: 32,
    bytesPerRow: bytesPerRow,
    space: colorSpace,
    bitmapInfo: bitmapInfo,
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

print(
  "\(inputImage.width)x\(inputImage.height) -> \(outputSize)x\(outputSize), "
    + "center crop, nearest-neighbor, Pebble 64-color palette"
)
