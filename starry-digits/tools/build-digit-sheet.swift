#!/usr/bin/env swift

import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

guard CommandLine.arguments.count == 3 else {
  fputs("Usage: build-digit-sheet.swift INPUT OUTPUT_DIRECTORY\n", stderr)
  exit(2)
}

let inputURL = URL(fileURLWithPath: CommandLine.arguments[1])
let outputDirectoryURL = URL(fileURLWithPath: CommandLine.arguments[2])

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

@inline(__always)
func pebbleChannel(_ value: UInt8) -> UInt8 {
  UInt8(min(3, (Int(value) + 42) / 85) * 85)
}

for index in stride(from: 0, to: inputPixels.count, by: 4) {
  let brightest = max(
    inputPixels[index],
    inputPixels[index + 1],
    inputPixels[index + 2]
  )
  if brightest < 30 {
    inputPixels[index] = 0
    inputPixels[index + 1] = 0
    inputPixels[index + 2] = 0
    inputPixels[index + 3] = 0
  } else {
    inputPixels[index] = pebbleChannel(inputPixels[index])
    inputPixels[index + 1] = pebbleChannel(inputPixels[index + 1])
    inputPixels[index + 2] = pebbleChannel(inputPixels[index + 2])
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

// The source is ordered 9,8,7,6 / 5,4,3,2 / 1,0.
let sourceCells = [
  DigitCell(digit: 9, rect: CGRect(x: 8, y: 2, width: 46, height: 58)),
  DigitCell(digit: 8, rect: CGRect(x: 52, y: 2, width: 46, height: 58)),
  DigitCell(digit: 7, rect: CGRect(x: 97, y: 2, width: 37, height: 58)),
  DigitCell(digit: 6, rect: CGRect(x: 136, y: 2, width: 44, height: 58)),
  DigitCell(digit: 5, rect: CGRect(x: 10, y: 60, width: 46, height: 58)),
  DigitCell(digit: 4, rect: CGRect(x: 54, y: 60, width: 43, height: 58)),
  DigitCell(digit: 3, rect: CGRect(x: 98, y: 60, width: 40, height: 58)),
  DigitCell(digit: 2, rect: CGRect(x: 138, y: 60, width: 44, height: 58)),
  DigitCell(digit: 1, rect: CGRect(x: 8, y: 116, width: 48, height: 58)),
  DigitCell(digit: 0, rect: CGRect(x: 54, y: 116, width: 48, height: 58)),
]

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

  return CGRect(
    x: minX,
    y: minY,
    width: maxX - minX + 1,
    height: maxY - minY + 1
  )
}

let digitWidth = 48
let digitHeight = 58
let sheetWidth = digitWidth * 10
let sheetBytesPerRow = sheetWidth * 4
var sheetPixels = [UInt8](
  repeating: 0,
  count: sheetBytesPerRow * digitHeight
)

guard let sheetContext = CGContext(
  data: &sheetPixels,
  width: sheetWidth,
  height: digitHeight,
  bitsPerComponent: 8,
  bytesPerRow: sheetBytesPerRow,
  space: colorSpace,
  bitmapInfo: bitmapInfo.rawValue
) else {
  fputs("Unable to create digit sheet context\n", stderr)
  exit(1)
}

sheetContext.interpolationQuality = .none

for cell in sourceCells {
  guard
    let bounds = visibleBounds(in: cell.rect),
    let cropped = transparentInput.cropping(to: bounds)
  else {
    fputs("Unable to isolate digit \(cell.digit)\n", stderr)
    exit(1)
  }

  guard cropped.width <= digitWidth && cropped.height <= digitHeight else {
    fputs("Digit \(cell.digit) does not fit without resizing\n", stderr)
    exit(1)
  }

  let drawX = cell.digit * digitWidth + (digitWidth - cropped.width) / 2
  let drawY = (digitHeight - cropped.height) / 2
  sheetContext.draw(
    cropped,
    in: CGRect(
      x: drawX,
      y: drawY,
      width: cropped.width,
      height: cropped.height
    )
  )

  print("digit \(cell.digit): source \(cropped.width)x\(cropped.height), scale 1:1")
}

guard
  let sheetProvider = CGDataProvider(data: Data(sheetPixels) as CFData),
  let sheetImage = CGImage(
    width: sheetWidth,
    height: digitHeight,
    bitsPerComponent: 8,
    bitsPerPixel: 32,
    bytesPerRow: sheetBytesPerRow,
    space: colorSpace,
    bitmapInfo: bitmapInfo,
    provider: sheetProvider,
    decode: nil,
    shouldInterpolate: false,
    intent: .defaultIntent
  )
else {
  fputs("Unable to create digit sheet image\n", stderr)
  exit(1)
}

try FileManager.default.createDirectory(
  at: outputDirectoryURL,
  withIntermediateDirectories: true
)

func writePNG(_ image: CGImage, to url: URL) {
  guard let destination = CGImageDestinationCreateWithURL(
    url as CFURL,
    UTType.png.identifier as CFString,
    1,
    nil
  ) else {
    fputs("Unable to create PNG destination for \(url.path)\n", stderr)
    exit(1)
  }

  CGImageDestinationAddImage(destination, image, nil)
  guard CGImageDestinationFinalize(destination) else {
    fputs("Unable to write \(url.path)\n", stderr)
    exit(1)
  }
}

writePNG(
  sheetImage,
  to: outputDirectoryURL.appendingPathComponent("digit-sheet.png")
)

for digit in 0..<10 {
  guard let image = sheetImage.cropping(to: CGRect(
    x: digit * digitWidth,
    y: 0,
    width: digitWidth,
    height: digitHeight
  )) else {
    fputs("Unable to crop digit \(digit) from generated sheet\n", stderr)
    exit(1)
  }
  writePNG(
    image,
    to: outputDirectoryURL.appendingPathComponent("digit-\(digit).png")
  )
}

print("Created ten 48x58 digit resources and a 480x58 QA sheet, no resizing")
