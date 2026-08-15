import SwiftUI

struct ScopeView: View {
    let left: [Float]
    let right: [Float]

    var body: some View {
        Canvas { context, size in
            draw(samples: left, in: &context, size: size, offset: -0.25)
            draw(samples: right, in: &context, size: size, offset: 0.25)
        }
        .background(.black.opacity(0.88))
        .clipShape(RoundedRectangle(cornerRadius: 10))
        .accessibilityLabel("Realtime stereo output oscilloscope")
    }

    private func draw(samples: [Float], in context: inout GraphicsContext, size: CGSize, offset: CGFloat) {
        guard samples.count > 1 else { return }
        var path = Path()
        let mid = size.height * (0.5 + offset)
        let scale = size.height * 0.20
        for (index, sample) in samples.enumerated() {
            let x = size.width * CGFloat(index) / CGFloat(samples.count - 1)
            let y = mid - CGFloat(sample) * scale
            if index == 0 { path.move(to: CGPoint(x: x, y: y)) } else { path.addLine(to: CGPoint(x: x, y: y)) }
        }
        context.stroke(path, with: .color(.green), lineWidth: 1.25)
    }
}
