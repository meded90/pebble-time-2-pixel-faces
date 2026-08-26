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
  let red = Double(inputPixels[index])
  let green = Double(inputPixels[index + 1])
  let blue = Double(inputPixels[index + 2])
  let luminance = UInt8(min(255, max(0, (0.2126 * red + 0.7152 * green + 0.0722 * blue).rounded())))
  let darkness = 255 - luminance

  inputPixels[index] = 0
  inputPixels[index + 1] = 0
  inputPixels[index + 2] = 0
  if darkness < 20 {
    inputPixels[index + 3] = 0
  } else {
    inputPixels[index + 3] = UInt8(min(3, (Int(darkness) + 42) / 85) * 85)
  }
}

struct DigitCell {
  let digit: Int
  let rect: CGRect
}

let cells = [
  DigitCell(digit: 9, rect: CGRect(x: 4, y: 4, width: 49, height: 64)),
  DigitCell(digit: 8, rect: CGRect(x: 53, y: 4, width: 45, height: 64)),
  DigitCell(digit: 7, rect: CGRect(x: 98, y: 4, width: 34, height: 64)),
  DigitCell(digit: 6, rect: CGRect(x: 138, y: 4, width: 44, height: 64)),
  DigitCell(digit: 5, rect: CGRect(x: 5, y: 68, width: 48, height: 61)),
  DigitCell(digit: 4, rect: CGRect(x: 53, y: 68, width: 44, height: 61)),
  DigitCell(digit: 3, rect: CGRect(x: 99, y: 68, width: 40, height: 61)),
  DigitCell(digit: 2, rect: CGRect(x: 139, y: 68, width: 45, height: 61)),
  DigitCell(digit: 1, rect: CGRect(x: 5, y: 129, width: 48, height: 62)),
  DigitCell(digit: 0, rect: CGRect(x: 53, y: 129, width: 49, height: 62)),
]

let outputWidth = 50
let outputHeight = 64

try FileManager.default.createDirectory(
  at: outputDirectory,
  withIntermediateDirectories: true
)

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
  guard let bounds = visibleBounds(in: cell.rect) else {
    fputs("Unable to isolate digit \(cell.digit)\n", stderr)
    exit(1)
  }

  let sourceX = Int(bounds.minX)
  let sourceY = Int(bounds.minY)
  let drawWidth = Int(bounds.width)
  let drawHeight = Int(bounds.height)
  let drawX = (outputWidth - drawWidth) / 2
  let drawY = (outputHeight - drawHeight) / 2
  let outputBytesPerRow = outputWidth * 4
  var outputPixels = [UInt8](repeating: 0, count: outputBytesPerRow * outputHeight)

  for y in 0..<drawHeight {
    for x in 0..<drawWidth {
      let inputIndex = (sourceY + y) * inputBytesPerRow + (sourceX + x) * 4
            let outputIndex = (drawY + y) * outputBytesPerRow + (drawX + x) * 4
      outputPixels[outputIndex] = inputPixels[inputIndex]
      outputPixels[outputIndex + 1] = inputPixels[inputIndex + 1]
      outputPixels[outputIndex + 2] = inputPixels[inputIndex + 2]
      outputPixels[outputIndex + 3] = inputPixels[inputIndex + 3]
    }
  }

  for index in stride(from: 0, to: outputPixels.count, by: 4) {
    if outputPixels[index + 3] < 43 {
      outputPixels[index] = 0
      outputPixels[index + 1] = 0
      outputPixels[index + 2] = 0
      outputPixels[index + 3] = 0
    } else {
      outputPixels[index] = 0
      outputPixels[index + 1] = 0
      outputPixels[index + 2] = 0
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

print("Created 10 transparent digit sprites at original glyph scale in \(outputWidth)x\(outputHeight) cells")
