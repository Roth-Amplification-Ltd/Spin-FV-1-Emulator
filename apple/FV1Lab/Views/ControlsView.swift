import SwiftUI

struct ControlsView: View {
    @ObservedObject var model: FV1WorkspaceModel

    var body: some View {
        Form {
            Section("Virtual FV-1") {
                pot("POT0", value: $model.pot0)
                pot("POT1", value: $model.pot1)
                pot("POT2", value: $model.pot2)
                Button("Reset Chip") { model.reset() }
            }
            Section("Audio") {
                HStack {
                    Circle().fill(model.audio.isRunning ? .green : .secondary).frame(width: 9, height: 9)
                    Text(model.audio.routeDescription).lineLimit(2)
                }
                Button(model.audio.isRunning ? "Stop Audio" : "Start Audio") { model.audio.toggle() }
                if model.audio.inputSampleRate > 0 {
                    LabeledContent("Input", value: "\(Int(model.audio.inputSampleRate)) Hz")
                    LabeledContent("Output", value: "\(Int(model.audio.outputSampleRate)) Hz")
                    LabeledContent("FV-1", value: "32768 Hz")
                    LabeledContent("Underflows", value: "\(model.audio.underflows)")
                    LabeledContent("Overflows", value: "\(model.audio.overflows)")
                }
                if !model.audio.lastError.isEmpty { Text(model.audio.lastError).foregroundStyle(.red) }
            }
            #if os(iOS)
            Section("iPad Input") {
                ForEach(model.audio.availableInputs, id: \.uid) { port in
                    Button(port.portName) { model.audio.selectInput(uid: port.uid) }
                }
                Text("Output routing follows the iPadOS system audio route.").font(.caption).foregroundStyle(.secondary)
            }
            #endif
        }
        .formStyle(.grouped)
    }

    private func pot(_ title: String, value: Binding<Double>) -> some View {
        VStack(alignment: .leading) {
            HStack { Text(title); Spacer(); Text(value.wrappedValue, format: .number.precision(.fractionLength(3))).monospacedDigit() }
            Slider(value: value, in: 0...1)
        }
    }
}
