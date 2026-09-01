#!/usr/bin/env swift

import AppKit
import CoreGraphics
import Foundation

let root = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
let portal = root.appendingPathComponent("developer-portal")
let source = portal.appendingPathComponent("source")
let icons = portal.appendingPathComponent("icons")
let banner = portal.appendingPathComponent("banner")
let screenshots = portal.appendingPathComponent("screenshots/emery")

try FileManager.default.createDirectory(at: icons, withIntermediateDirectories: true)
try FileManager.default.createDirectory(at: banner, withIntermediateDirectories: true)
try FileManager.default.createDirectory(at: screenshots, withIntermediateDirectories: true)

func load(_ url: URL) -> NSImage {
  guard let image = NSImage(contentsOf: url) else {
    fatalError("Unable to load \(url.path)")
  }
  return image
}

func render(width: Int, height: Int, draw: () -> Void) -> NSImage {
  guard let bitmap = NSBitmapImageRep(
    bitmapDataPlanes: nil,
    pixelsWide: width,
    pixelsHigh: height,
    bitsPerSample: 8,
    samplesPerPixel: 4,
    hasAlpha: true,
    isPlanar: false,
    colorSpaceName: .deviceRGB,
    bytesPerRow: 0,
    bitsPerPixel: 0
  ), let context = NSGraphicsContext(bitmapImageRep: bitmap) else {
    fatalError("Unable to create \(width)x\(height) bitmap")
  }
  bitmap.size = NSSize(width: width, height: height)
  NSGraphicsContext.saveGraphicsState()
  NSGraphicsContext.current = context
  context.imageInterpolation = .none
  draw()
  context.flushGraphics()
  NSGraphicsContext.restoreGraphicsState()
  let image = NSImage(size: NSSize(width: width, height: height))
  image.addRepresentation(bitmap)
  return image
}

func save(_ image: NSImage, to url: URL) {
  guard
    let tiff = image.tiffRepresentation,
    let bitmap = NSBitmapImageRep(data: tiff),
    let data = bitmap.representation(using: .png, properties: [:])
  else { fatalError("Unable to encode \(url.lastPathComponent)") }
  try! data.write(to: url)
  print("wrote \(url.path) [\(bitmap.pixelsWide)x\(bitmap.pixelsHigh)]")
}

func drawImage(_ image: NSImage, in rect: NSRect) {
  image.draw(
    in: rect,
    from: NSRect(origin: .zero, size: image.size),
    operation: .copy,
    fraction: 1,
    respectFlipped: true,
    hints: [.interpolation: NSImageInterpolation.none]
  )
}

let iconMaster = load(source.appendingPathComponent("icon-master-imagegen.png"))
for size in [48, 80, 144, 512] {
  let output = render(width: size, height: size) {
    drawImage(iconMaster, in: NSRect(x: 0, y: 0, width: size, height: size))
  }
  save(output, to: icons.appendingPathComponent("icon-\(size).png"))
}

let menuIcon = load(root.appendingPathComponent("resources/images/menu-icon-v3.png"))
let menuOutput = render(width: 25, height: 25) {
  drawImage(menuIcon, in: NSRect(x: 0, y: 0, width: 25, height: 25))
}
save(menuOutput, to: icons.appendingPathComponent("menu-icon-25.png"))

let bannerMaster = load(source.appendingPathComponent("banner-master-imagegen.png"))
let bannerOutput = render(width: 720, height: 320) {
  drawImage(bannerMaster, in: NSRect(x: 0, y: 0, width: 720, height: 320))

  NSColor(calibratedWhite: 0, alpha: 0.22).setFill()
  NSBezierPath(rect: NSRect(x: 0, y: 0, width: 306, height: 320)).fill()

  let labelStyle: [NSAttributedString.Key: Any] = [
    .font: NSFont.monospacedSystemFont(ofSize: 14, weight: .bold),
    .foregroundColor: NSColor(calibratedRed: 0.35, green: 0.88, blue: 1, alpha: 1),
    .kern: 1.2,
  ]
  let titleStyle: [NSAttributedString.Key: Any] = [
    .font: NSFont.monospacedSystemFont(ofSize: 38, weight: .bold),
    .foregroundColor: NSColor.white,
    .kern: -1.2,
  ]
  let subtitleStyle: [NSAttributedString.Key: Any] = [
    .font: NSFont.systemFont(ofSize: 21, weight: .semibold),
    .foregroundColor: NSColor(calibratedRed: 0.78, green: 0.87, blue: 1, alpha: 1),
  ]
  let noteStyle: [NSAttributedString.Key: Any] = [
    .font: NSFont.systemFont(ofSize: 15, weight: .medium),
    .foregroundColor: NSColor(calibratedWhite: 1, alpha: 0.9),
  ]

  NSAttributedString(string: "PEBBLE TIME 2", attributes: labelStyle)
    .draw(at: NSPoint(x: 34, y: 246))
  NSAttributedString(string: "FIND MY\niPHONE", attributes: titleStyle)
    .draw(in: NSRect(x: 31, y: 139, width: 260, height: 104))
  NSAttributedString(string: "Ring your iPhone\nfrom your wrist", attributes: subtitleStyle)
    .draw(in: NSRect(x: 34, y: 80, width: 260, height: 54))
  NSAttributedString(string: "Hold Select · No extra app", attributes: noteStyle)
    .draw(in: NSRect(x: 34, y: 43, width: 270, height: 24))
}
save(bannerOutput, to: banner.appendingPathComponent("marketing-banner-720x320.png"))

let screenshotNames = [
  "01-auth-required",
  "02-ready",
  "03-requesting",
  "04-signal-sent",
  "05-offline",
]

for name in screenshotNames {
  let raw = load(source.appendingPathComponent("screenshots/\(name).png"))
  guard
    let rawData = raw.tiffRepresentation,
    let rawBitmap = NSBitmapImageRep(data: rawData),
    let rawCG = rawBitmap.cgImage,
    let screenCG = rawCG.cropping(to: CGRect(x: 0, y: 0, width: 140, height: 160))
  else { fatalError("Unable to crop \(name)") }
  let cropped = NSImage(cgImage: screenCG, size: NSSize(width: 140, height: 160))
  let output = render(width: 200, height: 228) {
    drawImage(cropped, in: NSRect(x: 0, y: 0, width: 200, height: 228))
  }
  save(output, to: screenshots.appendingPathComponent("\(name)-200x228.png"))
}

let preview = render(width: 1120, height: 620) {
  NSColor(calibratedRed: 0.94, green: 0.95, blue: 0.98, alpha: 1).setFill()
  NSBezierPath(rect: NSRect(x: 0, y: 0, width: 1120, height: 620)).fill()
  drawImage(bannerOutput, in: NSRect(x: 24, y: 276, width: 720, height: 320))
  let icon144 = load(icons.appendingPathComponent("icon-144.png"))
  let icon48 = load(icons.appendingPathComponent("icon-48.png"))
  drawImage(icon144, in: NSRect(x: 786, y: 452, width: 144, height: 144))
  drawImage(icon48, in: NSRect(x: 962, y: 548, width: 48, height: 48))
  for (index, name) in screenshotNames.enumerated() {
    let shot = load(screenshots.appendingPathComponent("\(name)-200x228.png"))
    drawImage(shot, in: NSRect(x: 24 + index * 216, y: 24, width: 200, height: 228))
  }
}
save(preview, to: portal.appendingPathComponent("store-assets-preview.png"))
