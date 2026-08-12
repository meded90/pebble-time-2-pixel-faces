#!/usr/bin/env swift

import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

guard CommandLine.arguments.count == 4 else {
  fputs("Usage: build-publishing-assets.swift ICON_SOURCE BANNER_SOURCE REPO_ROOT\n", stderr)
  exit(2)
}

let iconSourceURL = URL(fileURLWithPath: CommandLine.arguments[1])
let bannerSourceURL = URL(fileURLWithPath: CommandLine.arguments[2])
let rootURL = URL(fileURLWithPath: CommandLine.arguments[3], isDirectory: true)
let colorSpace = CGColorSpaceCreateDeviceRGB()
let bitmapInfo = CGBitmapInfo(
  rawValue: CGImageAlphaInfo.premultipliedLast.rawValue
    | CGBitmapInfo.byteOrder32Big.rawValue
)

func load(_ url: URL) -> CGImage {
  guard
    let source = CGImageSourceCreateWithURL(url as CFURL, nil),
    let image = CGImageSourceCreateImageAtIndex(source, 0, nil)
  else {
    fputs("Unable to read \(url.path)\n", stderr)
    exit(1)
  }
  return image
}

@inline(__always)
func pebbleChannel(_ value: UInt8) -> UInt8 {
  UInt8(min(3, (Int(value) + 42) / 85) * 85)
}

func render(_ source: CGImage, width: Int, height: Int) -> CGImage {
  let targetAspect = CGFloat(width) / CGFloat(height)
  let sourceAspect = CGFloat(source.width) / CGFloat(source.height)
  let crop: CGRect
  if sourceAspect > targetAspect {
    let cropWidth = Int((CGFloat(source.height) * targetAspect).rounded())
    crop = CGRect(
      x: (source.width - cropWidth) / 2,
      y: 0,
      width: cropWidth,
      height: source.height
    )
  } else {
    let cropHeight = Int((CGFloat(source.width) / targetAspect).rounded())
    crop = CGRect(
      x: 0,
      y: (source.height - cropHeight) / 2,
      width: source.width,
      height: cropHeight
    )
  }

  guard let cropped = source.cropping(to: crop) else {
    fputs("Unable to crop source image\n", stderr)
    exit(1)
  }

  let bytesPerRow = width * 4
  var pixels = [UInt8](repeating: 0, count: bytesPerRow * height)
  guard let context = CGContext(
    data: &pixels,
    width: width,
    height: height,
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
  context.draw(cropped, in: CGRect(x: 0, y: 0, width: width, height: height))

  for index in stride(from: 0, to: pixels.count, by: 4) {
    pixels[index] = pebbleChannel(pixels[index])
    pixels[index + 1] = pebbleChannel(pixels[index + 1])
    pixels[index + 2] = pebbleChannel(pixels[index + 2])
    pixels[index + 3] = 255
  }

  guard
    let provider = CGDataProvider(data: Data(pixels) as CFData),
    let image = CGImage(
      width: width,
      height: height,
      bitsPerComponent: 8,
      bitsPerPixel: 32,
      bytesPerRow: bytesPerRow,
      space: colorSpace,
      bitmapInfo: bitmapInfo,
      provider: provider,
      decode: nil,
      shouldInterpolate: false,
      intent: .defaultIntent
    )
  else {
    fputs("Unable to create output image\n", stderr)
    exit(1)
  }
  return image
}

func write(_ image: CGImage, to url: URL) {
  try? FileManager.default.createDirectory(
    at: url.deletingLastPathComponent(),
    withIntermediateDirectories: true
  )
  guard let destination = CGImageDestinationCreateWithURL(
    url as CFURL,
    UTType.png.identifier as CFString,
    1,
    nil
  ) else {
    fputs("Unable to create \(url.path)\n", stderr)
    exit(1)
  }
  CGImageDestinationAddImage(destination, image, nil)
  guard CGImageDestinationFinalize(destination) else {
    fputs("Unable to write \(url.path)\n", stderr)
    exit(1)
  }
  print(url.path)
}

let iconSource = load(iconSourceURL)
for size in [32, 80, 144, 512] {
  write(
    render(iconSource, width: size, height: size),
    to: rootURL.appendingPathComponent("assets/icons/starry-digits-icon-\(size).png")
  )
}
write(
  render(iconSource, width: 25, height: 25),
  to: rootURL.appendingPathComponent(
    "starry-digits/resources/images/starry-digits-menu-icon.png"
  )
)

let bannerSource = load(bannerSourceURL)
write(
  render(bannerSource, width: 720, height: 320),
  to: rootURL.appendingPathComponent(
    "assets/banners/starry-digits-banner-720x320.png"
  )
)
