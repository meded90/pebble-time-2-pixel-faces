#!/usr/bin/env swift

import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

guard CommandLine.arguments.count == 3 else {
  fputs("Usage: extract-digits.swift INPUT OUTPUT_DIRECTORY\n", stderr)
  exit(2)
}

let inputURL = URL(fileURLWithPath: CommandLine.arguments[1])
let outputDirectory = URL(fileURLWithPath: CommandLine.arguments[2], isDirectory: true)

guard
  let source = CGImageSourceCreateWithURL(inputURL as CFURL, nil),
  let inputImage = CGImageSourceCreateImageAtIndex(source, 0, nil)
else {
  fputs("Unable to read digit reference\n", stderr)
  exit(1)
}

let inputWidth = inputImage.width
let inputHeight = inputImage.height
let inputBytesPerRow = inputWidth * 4
var inputPixels = [UInt8](repeating: 0, count: inputBytesPerRow * inputHeight)
let colorSpace = CGColorSpaceCreateDeviceRGB()
let bitmapInfo = CGBitmapInfo(
  rawValue: CGImageAlphaInfo.premultipliedLast.rawValue
    | CGBitmapInfo.byteOrder32Big.rawValue
)

guard let inputContext = CGContext(
  data: &inputPixels,
  width: inputWidth,
  height: inputHeight,
  bitsPerComponent: 8,
  bytesPerRow: inputBytesPerRow,
  space: colorSpace,
  bitmapInfo: bitmapInfo.rawValue
) else {
  fputs("Unable to create input context\n", stderr)
  exit(1)
}

inputContext.interpolationQuality = .none
inputContext.draw(
  inputImage,
  in: CGRect(x: 0, y: 0, width: inputWidth, height: inputHeight)
)

for index in stride(from: 0, to: inputPixels.count, by: 4) {
  let brightest = max(inputPixels[index], inputPixels[index + 1], inputPixels[index + 2])
  if brightest < 30 {
    inputPixels[index] = 0
    inputPixels[index + 1] = 0
    inputPixels[index + 2] = 0
    inputPixels[index + 3] = 0
  } else {
    inputPixels[index + 3] = 255
  }
}

guard
  let inputProvider = CGDataProvider(data: Data(inputPixels) as CFData),
  let transparentInput = CGImage(
    width: inputWidth,
    height: inputHeight,
    bitsPerComponent: 8,
    bitsPerPixel: 32,
    bytesPerRow: inputBytesPerRow,
    space: colorSpace,
    bitmapInfo: bitmapInfo,
    provider: inputProvider,
    decode: nil,
    shouldInterpolate: false,
    intent: .defaultIntent
  )
else {
  fputs("Unable to prepare transparent source\n", stderr)
  exit(1)
}

struct DigitCell {
  let digit: Int
  let rect: CGRect
}

let cells = [
  DigitCell(digit: 9, rect: CGRect(x: 8, y: 2, width: 46, height: 58)),
  DigitCell(digit: 8, rect: CGRect(x: 52, y: 2, width: 46, height: 58)),
  DigitCell(digit: 7, rect: CGRect(x: 97, y: 2, width: 40, height: 58)),
  DigitCell(digit: 6, rect: CGRect(x: 136, y: 2, width: 44, height: 58)),
  DigitCell(digit: 5, rect: CGRect(x: 10, y: 60, width: 46, height: 58)),
  DigitCell(digit: 4, rect: CGRect(x: 54, y: 60, width: 44, height: 58)),
  DigitCell(digit: 3, rect: CGRect(x: 98, y: 60, width: 42, height: 58)),
  DigitCell(digit: 2, rect: CGRect(x: 138, y: 60, width: 44, height: 58)),
  DigitCell(digit: 1, rect: CGRect(x: 8, y: 116, width: 48, height: 58)),
  DigitCell(digit: 0, rect: CGRect(x: 54, y: 116, width: 48, height: 58)),
]

let outputWidth = 28
let outputHeight = 40
let contentWidth = 24
let contentHeight = 36

try FileManager.default.createDirectory(
  at: outputDirectory,
  withIntermediateDirectories: true
)

@inline(__always)
func pebbleChannel(_ value: UInt8) -> UInt8 {
  UInt8(min(3, (Int(value) + 42) / 85) * 85)
}

func visibleBounds(in cell: CGRect) -> CGRect? {
  let minCellX = max(0, Int(cell.minX))
  let maxCellX = min(inputWidth - 1, Int(cell.maxX) - 1)
  let minCellY = max(0, Int(cell.minY))
  let maxCellY = min(inputHeight - 1, Int(cell.maxY) - 1)
  var minX = maxCellX
  var minY = maxCellY
  var maxX = minCellX
  var maxY = minCellY
  var found = false

  for y in minCellY...maxCellY {
    for x in minCellX...maxCellX {
      let index = y * inputBytesPerRow + x * 4
      if inputPixels[index + 3] != 0 {
        found = true
        minX = min(minX, x)
        minY = min(minY, y)
        maxX = max(maxX, x)
        maxY = max(maxY, y)
      }
    }
  }

  guard found else {
    return nil
  }

  minX = max(minCellX, minX - 1)
  minY = max(minCellY, minY - 1)
  maxX = min(maxCellX, maxX + 1)
  maxY = min(maxCellY, maxY + 1)
  return CGRect(x: minX, y: minY, width: maxX - minX + 1, height: maxY - minY + 1)
}

for cell in cells {
  guard
    let bounds = visibleBounds(in: cell.rect),
    let cropped = transparentInput.cropping(to: bounds)
  else {
    fputs("Unable to isolate digit \(cell.digit)\n", stderr)
    exit(1)
  }

  let scale = min(
    CGFloat(contentWidth) / CGFloat(cropped.width),
    CGFloat(contentHeight) / CGFloat(cropped.height)
  )
  let drawWidth = max(1, Int((CGFloat(cropped.width) * scale).rounded()))
  let drawHeight = max(1, Int((CGFloat(cropped.height) * scale).rounded()))
  let drawX = (outputWidth - drawWidth) / 2
  let drawY = (outputHeight - drawHeight) / 2
  let outputBytesPerRow = outputWidth * 4
  var outputPixels = [UInt8](repeating: 0, count: outputBytesPerRow * outputHeight)

  guard let outputContext = CGContext(
    data: &outputPixels,
    width: outputWidth,
    height: outputHeight,
    bitsPerComponent: 8,
    bytesPerRow: outputBytesPerRow,
    space: colorSpace,
    bitmapInfo: bitmapInfo.rawValue
  ) else {
    fputs("Unable to create output context\n", stderr)
    exit(1)
  }

  outputContext.interpolationQuality = .none
  outputContext.draw(
    cropped,
    in: CGRect(x: drawX, y: drawY, width: drawWidth, height: drawHeight)
  )

  for index in stride(from: 0, to: outputPixels.count, by: 4) {
    if outputPixels[index + 3] < 128 {
      outputPixels[index] = 0
      outputPixels[index + 1] = 0
      outputPixels[index + 2] = 0
      outputPixels[index + 3] = 0
    } else {
      outputPixels[index] = pebbleChannel(outputPixels[index])
      outputPixels[index + 1] = pebbleChannel(outputPixels[index + 1])
      outputPixels[index + 2] = pebbleChannel(outputPixels[index + 2])
      outputPixels[index + 3] = 255
    }
  }

  guard
    let outputProvider = CGDataProvider(data: Data(outputPixels) as CFData),
    let outputImage = CGImage(
      width: outputWidth,
      height: outputHeight,
      bitsPerComponent: 8,
      bitsPerPixel: 32,
      bytesPerRow: outputBytesPerRow,
      space: colorSpace,
      bitmapInfo: bitmapInfo,
      provider: outputProvider,
      decode: nil,
      shouldInterpolate: false,
      intent: .defaultIntent
    )
  else {
    fputs("Unable to create digit \(cell.digit) image\n", stderr)
    exit(1)
  }

  let outputURL = outputDirectory.appendingPathComponent("digit-\(cell.digit).png")
  guard let destination = CGImageDestinationCreateWithURL(
    outputURL as CFURL,
    UTType.png.identifier as CFString,
    1,
    nil
  ) else {
    fputs("Unable to create digit \(cell.digit) destination\n", stderr)
    exit(1)
  }

  CGImageDestinationAddImage(destination, outputImage, nil)
  guard CGImageDestinationFinalize(destination) else {
    fputs("Unable to write digit \(cell.digit)\n", stderr)
    exit(1)
  }
}

print("Created 10 transparent digit sprites at \(outputWidth)x\(outputHeight)")
