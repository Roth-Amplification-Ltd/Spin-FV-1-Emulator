import SwiftUI

#if os(macOS)
struct MacAudioSettingsView: View {
    @ObservedObject var audio:
        AppleAudioController
    @ObservedObject var devices:
        MacAudioDeviceManager

    @Environment(\.dismiss)
    private var dismiss

    var body: some View {
        VStack(
            alignment: .leading,
            spacing: 14
        ) {
            Text("Audio Settings")
                .font(.title2.bold())

            Text(
                audio.isRunning
                    ? "The current audio session keeps its active route. Changes below apply the next time Start is pressed."
                    : "Configure the native AVAudioEngine / Core Audio host. Device choices and timing preferences are remembered between launches."
            )
            .font(.caption)
            .foregroundStyle(.secondary)
            .fixedSize(
                horizontal: false,
                vertical: true
            )

            Form {
                LabeledContent("Backend") {
                    Text(
                        "AVAudioEngine / Core Audio"
                    )
                }

                Picker(
                    "I/O device",
                    selection:
                        $devices.ioDeviceUID
                ) {
                    Text("OS Default")
                        .tag("")

                    ForEach(
                        devices.ioDevices
                    ) { device in
                        Text(
                            "\(device.displayName) — \(device.capabilityLabel)"
                        )
                        .tag(device.uid)
                    }
                }

                Text(
                    "For different physical input and output interfaces, create a macOS Aggregate Device in Audio MIDI Setup and select that aggregate here."
                )
                .font(.caption2)
                .foregroundStyle(.secondary)
                .fixedSize(
                    horizontal: false,
                    vertical: true
                )

                Picker(
                    "Preferred sample rate",
                    selection:
                        $devices.preferredSampleRate
                ) {
                    ForEach(
                        [
                            44_100.0,
                            48_000.0,
                            88_200.0,
                            96_000.0,
                            192_000.0
                        ],
                        id: \.self
                    ) { rate in
                        Text(
                            "\(Int(rate)) Hz"
                        )
                        .tag(rate)
                    }
                }

                Picker(
                    "Preferred buffer",
                    selection:
                        $devices.preferredBufferFrames
                ) {
                    ForEach(
                        [
                            64,
                            128,
                            256,
                            512,
                            1024,
                            2048
                        ],
                        id: \.self
                    ) { frames in
                        Text(
                            "\(frames) frames"
                        )
                        .tag(frames)
                    }
                }

                LabeledContent(
                    "Virtual FV-1 clock"
                ) {
                    Text("32768 Hz")
                        .monospacedDigit()
                }

                if audio.inputSampleRate > 0 {
                    LabeledContent(
                        "Active input rate"
                    ) {
                        Text(
                            "\(Int(audio.inputSampleRate)) Hz"
                        )
                    }

                    LabeledContent(
                        "Active output rate"
                    ) {
                        Text(
                            "\(Int(audio.outputSampleRate)) Hz"
                        )
                    }
                }
            }

            HStack {
                Button(
                    "Refresh Audio Devices"
                ) {
                    devices.refresh()
                }

                Spacer()

                Button("Done") {
                    dismiss()
                }
                .keyboardShortcut(
                    .defaultAction
                )
            }
        }
        .padding(20)
        .frame(
            width: 560,
            height: 500
        )
        .onAppear {
            devices.refresh()
        }
    }
}
#endif


struct TestGeneratorSettingsView: View {
    @ObservedObject var audio:
        AppleAudioController

    @Environment(\.dismiss)
    private var dismiss

    var body: some View {
        VStack(
            alignment: .leading,
            spacing: 14
        ) {
            Text(
                "Test Generator Settings"
            )
            .font(.title2.bold())

            Text(
                "Configure the same deterministic lab stimuli used by the native testbench."
            )
            .font(.caption)
            .foregroundStyle(.secondary)

            Form {
                Picker(
                    "Signal",
                    selection:
                        $audio.generatorKind
                ) {
                    ForEach(
                        AppleTestSignalKind
                            .allCases
                    ) { signal in
                        Text(signal.rawValue)
                            .tag(signal)
                    }
                }

                LabeledContent(
                    "Start / tone frequency"
                ) {
                    HStack {
                        TextField(
                            "",
                            value:
                                $audio.generatorFrequency,
                            format: .number
                        )
                        .frame(width: 90)
                        Text("Hz")
                    }
                }

                LabeledContent(
                    "Amplitude"
                ) {
                    HStack {
                        Slider(
                            value:
                                $audio.generatorAmplitude,
                            in: 0...1
                        )
                        .frame(width: 180)

                        Text(
                            audio.generatorAmplitude,
                            format:
                                .number
                                .precision(
                                    .fractionLength(
                                        3
                                    )
                                )
                        )
                        .frame(width: 50)
                        .monospacedDigit()
                    }
                }

                LabeledContent(
                    "Sweep end"
                ) {
                    HStack {
                        TextField(
                            "",
                            value:
                                $audio.generatorSweepEnd,
                            format: .number
                        )
                        .frame(width: 90)
                        Text("Hz")
                    }
                }

                LabeledContent(
                    "Sweep duration"
                ) {
                    HStack {
                        TextField(
                            "",
                            value:
                                $audio.generatorSweepSeconds,
                            format: .number
                        )
                        .frame(width: 90)
                        Text("s")
                    }
                }

                LabeledContent(
                    "Impulse period"
                ) {
                    HStack {
                        TextField(
                            "",
                            value:
                                $audio.generatorImpulsePeriod,
                            format: .number
                        )
                        .frame(width: 90)
                        Text("s")
                    }
                }
            }

            HStack {
                Spacer()
                Button("Done") {
                    dismiss()
                }
                .keyboardShortcut(
                    .defaultAction
                )
            }
        }
        .padding(20)
        .frame(
            minWidth: 500,
            minHeight: 430
        )
    }
}


struct AudioLoopRegionView: View {
    @ObservedObject var audio:
        AppleAudioController

    @Environment(\.dismiss)
    private var dismiss

    var body: some View {
        VStack(
            alignment: .leading,
            spacing: 14
        ) {
            Text("Audio Loop Region")
                .font(.title2.bold())

            Text(
                audio.fileLoopName.isEmpty
                    ? "No audio loop loaded."
                    : audio.fileLoopName
            )
            .font(
                .system(
                    .caption,
                    design: .monospaced
                )
            )

            Form {
                Toggle(
                    "Loop playback",
                    isOn:
                        Binding(
                            get: {
                                audio.fileLoopInfo
                                    .looping
                            },
                            set: {
                                audio
                                    .setFileLooping(
                                        $0
                                    )
                            }
                        )
                )

                LabeledContent(
                    "Loop start"
                ) {
                    HStack {
                        TextField(
                            "",
                            value:
                                $audio.fileLoopBegin,
                            format: .number
                        )
                        .frame(width: 100)
                        Text("s")
                    }
                }

                LabeledContent(
                    "Loop end"
                ) {
                    HStack {
                        TextField(
                            "",
                            value:
                                $audio.fileLoopEnd,
                            format: .number
                        )
                        .frame(width: 100)
                        Text("s")
                    }
                }

                LabeledContent(
                    "Boundary crossfade"
                ) {
                    HStack {
                        TextField(
                            "",
                            value:
                                $audio.fileLoopCrossfadeMS,
                            format: .number
                        )
                        .frame(width: 100)
                        Text("ms")
                    }
                }

                LabeledContent(
                    "File duration"
                ) {
                    Text(
                        String(
                            format:
                                "%.3f s",
                            audio.fileLoopInfo
                                .duration
                        )
                    )
                    .monospacedDigit()
                }
            }

            HStack {
                Button(
                    "Use Entire File"
                ) {
                    audio.fileLoopBegin = 0
                    audio.fileLoopEnd =
                        audio.fileLoopInfo
                            .duration
                }

                Spacer()

                Button("Apply") {
                    audio.applyFileLoopRegion()
                    dismiss()
                }
                .keyboardShortcut(
                    .defaultAction
                )
            }
        }
        .padding(20)
        .frame(
            minWidth: 500,
            minHeight: 360
        )
    }
}
