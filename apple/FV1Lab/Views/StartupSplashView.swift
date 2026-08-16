import Foundation
import SwiftUI

struct FV1StartupSplashView: View {
    let progress: Double
    let status: String
    let version: String

    private let accent = Color(
        red: 35.0 / 255.0,
        green: 220.0 / 255.0,
        blue: 245.0 / 255.0
    )

    var body: some View {
        GeometryReader { proxy in
            let scale = min(proxy.size.width / 960.0, proxy.size.height / 540.0)

            splashCard
                .frame(width: 960, height: 540)
                .scaleEffect(scale)
                .position(x: proxy.size.width * 0.5, y: proxy.size.height * 0.5)
        }
        .background(Color(red: 4 / 255, green: 7 / 255, blue: 9 / 255))
        .ignoresSafeArea()
        .accessibilityElement(children: .combine)
        .accessibilityLabel("FV-1 Lab")
        .accessibilityValue(status)
    }

    private var splashCard: some View {
        ZStack {
            backgroundPanel
            engineeringOverlay
            productTypography
            startupProgress
        }
        .frame(width: 960, height: 540)
    }

    private var backgroundPanel: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 18, style: .continuous)
                .fill(
                    LinearGradient(
                        colors: [
                            Color(red: 24 / 255, green: 30 / 255, blue: 35 / 255),
                            Color(red: 9 / 255, green: 15 / 255, blue: 20 / 255),
                            Color(red: 5 / 255, green: 9 / 255, blue: 12 / 255)
                        ],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    )
                )
                .shadow(color: .black.opacity(0.58), radius: 18, x: 0, y: 7)

            Image("Splash")
                .resizable()
                .scaledToFill()
                .saturation(0.05)
                .contrast(1.08)
                .frame(width: 928, height: 508)
                .clipped()
                .overlay(accent.opacity(0.10).blendMode(.color))
                .overlay(Color(red: 2 / 255, green: 6 / 255, blue: 9 / 255).opacity(0.46))
                .clipShape(RoundedRectangle(cornerRadius: 16, style: .continuous))

            RoundedRectangle(cornerRadius: 18, style: .continuous)
                .stroke(
                    Color(red: 135 / 255, green: 145 / 255, blue: 152 / 255),
                    lineWidth: 1.2
                )
        }
        .padding(16)
    }

    private var engineeringOverlay: some View {
        Canvas { context, _ in
            drawScanLines(context: &context)
            drawScope(context: &context)
            drawChip(context: &context)
        }
        .frame(width: 960, height: 540)
        .allowsHitTesting(false)
    }

    private func drawScanLines(context: inout GraphicsContext) {
        for y in stride(from: 28.0, through: 512.0, by: 3.0) {
            var line = Path()
            line.move(to: CGPoint(x: 24, y: y))
            line.addLine(to: CGPoint(x: 936, y: y))
            context.stroke(line, with: .color(.white.opacity(0.018)), lineWidth: 0.55)
        }
    }

    private func drawScope(context: inout GraphicsContext) {
        let scope = CGRect(x: 88, y: 132, width: 784, height: 118)

        for i in 1..<9 {
            let x = scope.minX + scope.width * Double(i) / 9.0
            var line = Path()
            line.move(to: CGPoint(x: x, y: scope.minY))
            line.addLine(to: CGPoint(x: x, y: scope.maxY))
            context.stroke(
                line,
                with: .color(accent.opacity(0.16)),
                style: StrokeStyle(lineWidth: 0.7, dash: [3.5, 4.5])
            )
        }

        for i in 1..<4 {
            let y = scope.minY + scope.height * Double(i) / 4.0
            var line = Path()
            line.move(to: CGPoint(x: scope.minX, y: y))
            line.addLine(to: CGPoint(x: scope.maxX, y: y))
            context.stroke(
                line,
                with: .color(accent.opacity(0.16)),
                style: StrokeStyle(lineWidth: 0.7, dash: [3.5, 4.5])
            )
        }

        var wave = Path()
        let points = 420
        for i in 0..<points {
            let u = Double(i) / Double(points - 1)
            let x = scope.minX + u * scope.width
            let envelope = 0.38 + 0.62 * sin(Double.pi * u)
            let value =
                sin(7.0 * Double.pi * u)
                + 0.43 * sin(17.0 * Double.pi * u + 0.65)
                + 0.20 * sin(37.0 * Double.pi * u)
            let y = scope.midY - value * envelope * scope.height * 0.31
            if i == 0 {
                wave.move(to: CGPoint(x: x, y: y))
            } else {
                wave.addLine(to: CGPoint(x: x, y: y))
            }
        }

        context.stroke(
            wave,
            with: .color(accent.opacity(0.16)),
            style: StrokeStyle(lineWidth: 9, lineCap: .round, lineJoin: .round)
        )
        context.stroke(
            wave,
            with: .color(accent),
            style: StrokeStyle(lineWidth: 2.2, lineCap: .round, lineJoin: .round)
        )
    }

    private func drawChip(context: inout GraphicsContext) {
        let body = CGRect(x: 570, y: 166, width: 245, height: 48)
        let bodyPath = Path(roundedRect: body, cornerRadius: 8)

        context.fill(
            bodyPath,
            with: .linearGradient(
                Gradient(colors: [
                    Color(red: 70 / 255, green: 74 / 255, blue: 79 / 255),
                    Color(red: 30 / 255, green: 33 / 255, blue: 36 / 255),
                    Color(red: 8 / 255, green: 10 / 255, blue: 12 / 255)
                ]),
                startPoint: CGPoint(x: body.midX, y: body.minY),
                endPoint: CGPoint(x: body.midX, y: body.maxY)
            )
        )
        context.stroke(bodyPath, with: .color(accent.opacity(0.52)), lineWidth: 1.1)

        let dimple = Path(
            ellipseIn: CGRect(x: body.minX + 7, y: body.midY - 5, width: 10, height: 10)
        )
        context.fill(dimple, with: .color(.black.opacity(0.82)))
        context.stroke(dimple, with: .color(.white.opacity(0.30)), lineWidth: 0.7)

        let first = body.minX + body.width * 0.16
        let last = body.maxX - body.width * 0.08
        let step = (last - first) / 6.0

        for i in 0..<7 {
            let x = first + Double(i) * step
            var pin = Path()
            pin.move(to: CGPoint(x: x - 4.4, y: body.maxY - 1))
            pin.addLine(to: CGPoint(x: x + 4.4, y: body.maxY - 1))
            pin.addLine(to: CGPoint(x: x + 3.2, y: body.maxY + 10))
            pin.addLine(to: CGPoint(x: x + 1.8, y: body.maxY + 18))
            pin.addLine(to: CGPoint(x: x - 1.8, y: body.maxY + 18))
            pin.addLine(to: CGPoint(x: x - 3.2, y: body.maxY + 10))
            pin.closeSubpath()
            context.fill(
                pin,
                with: .linearGradient(
                    Gradient(colors: [
                        Color.white.opacity(0.92),
                        Color(red: 132 / 255, green: 139 / 255, blue: 145 / 255),
                        Color(red: 62 / 255, green: 68 / 255, blue: 74 / 255)
                    ]),
                    startPoint: CGPoint(x: x, y: body.maxY),
                    endPoint: CGPoint(x: x, y: body.maxY + 18)
                )
            )
        }
    }

    private var productTypography: some View {
        ZStack {
            Text("FV-1")
                .font(.system(size: 52, weight: .bold))
                .foregroundStyle(Color(red: 222 / 255, green: 226 / 255, blue: 230 / 255))
                .frame(width: 300, height: 76)
                .position(x: 242, y: 96)

            Text("Spin FV-1 Emulator")
                .font(.system(size: 34, weight: .light))
                .foregroundStyle(Color(red: 205 / 255, green: 211 / 255, blue: 216 / 255))
                .frame(width: 650, height: 64)
                .position(x: 480, y: 298)

            Rectangle().fill(accent).frame(width: 115, height: 1.1).position(x: 342.5, y: 337)
            Rectangle().fill(accent).frame(width: 115, height: 1.1).position(x: 617.5, y: 337)

            Text("FV-1 Lab")
                .font(.system(size: 16, weight: .medium))
                .foregroundStyle(accent)
                .frame(width: 160, height: 36)
                .position(x: 480, y: 338)

            Text("Virtual DSP Testbench for Spin FV-1")
                .font(.system(size: 12.5, weight: .regular))
                .foregroundStyle(Color(red: 170 / 255, green: 178 / 255, blue: 185 / 255))
                .frame(width: 390, height: 34)
                .position(x: 480, y: 369)
        }
        .allowsHitTesting(false)
    }

    private var startupProgress: some View {
        ZStack {
            progressTrack

            Text(status)
                .font(.system(size: 9.5, weight: .regular))
                .foregroundStyle(Color(red: 175 / 255, green: 183 / 255, blue: 190 / 255))
                .lineLimit(1)
                .frame(width: 720, height: 24, alignment: .leading)
                .position(x: 426, y: 445)

            Text("\(Int(progress.rounded()))%")
                .font(.system(size: 12, weight: .medium, design: .monospaced))
                .foregroundStyle(accent)
                .frame(width: 84, height: 28, alignment: .trailing)
                .position(x: 852, y: 444)

            Text("Created & engineered by Adam Vadala-Roth")
                .font(.system(size: 9.5, weight: .medium))
                .foregroundStyle(Color(red: 158 / 255, green: 166 / 255, blue: 173 / 255))
                .frame(width: 460, height: 22)
                .position(x: 480, y: 473)

            Rectangle()
                .fill(Color(red: 90 / 255, green: 100 / 255, blue: 108 / 255).opacity(0.78))
                .frame(width: 230, height: 0.8)
                .position(x: 170, y: 508)

            Rectangle()
                .fill(Color(red: 90 / 255, green: 100 / 255, blue: 108 / 255).opacity(0.78))
                .frame(width: 230, height: 0.8)
                .position(x: 790, y: 508)

            Text("© 2026 Roth Amplification LTD  •  MPL-2.0  •  \(version)")
                .font(.system(size: 8.8, weight: .regular))
                .foregroundStyle(Color(red: 115 / 255, green: 124 / 255, blue: 132 / 255))
                .frame(width: 390, height: 34)
                .position(x: 480, y: 503)
        }
        .allowsHitTesting(false)
    }

    private var progressTrack: some View {
        ZStack(alignment: .leading) {
            RoundedRectangle(cornerRadius: 6, style: .continuous)
                .fill(Color(red: 3 / 255, green: 7 / 255, blue: 9 / 255).opacity(0.90))
                .overlay(
                    RoundedRectangle(cornerRadius: 6, style: .continuous)
                        .stroke(Color(red: 75 / 255, green: 85 / 255, blue: 92 / 255), lineWidth: 1)
                )

            GeometryReader { proxy in
                let fraction = min(1, max(0, progress / 100.0))
                RoundedRectangle(cornerRadius: 4, style: .continuous)
                    .fill(accent)
                    .frame(
                        width: max(0, (proxy.size.width - 4) * fraction),
                        height: max(0, proxy.size.height - 4)
                    )
                    .padding(2)
            }
        }
        .frame(width: 828, height: 12)
        .position(x: 480, y: 420)
    }
}
