import Foundation
import SwiftUI
import UniformTypeIdentifiers

#if os(macOS)
import AppKit
#endif

struct ValidationView: View {
    @State private var referenceURL: URL?
    @State private var captureURL: URL?
    @State private var reportDirectory: URL?

    @State private var maxAlignmentMS = 100.0
    @State private var gainMatchResidual = false
    @State private var fftSize = 16_384
    @State private var spectralFloorDB = -90.0
    @State private var minimumCorrelation = 0.995
    @State private var maximumResidualRMSDBFS = -45.0
    @State private var maximumResidualPeakDBFS = -24.0

    @State private var packSampleRate = 48_000
    @State private var packSeconds = 5.0
    @State private var packLevel = 0.25

    @State private var outcome: FV1ValidationOutcome?
    @State private var status = "Choose a reference WAV and physical/emulator capture WAV."
    @State private var busy = false

    var body: some View {
        #if os(macOS)
        ScrollView {
            VStack(
                alignment: .leading,
                spacing: 14
            ) {
                fileSelection
                Divider()
                validationConfiguration
                Divider()
                comparisonControls

                if let outcome {
                    Divider()
                    resultPanel(outcome)
                }

                Divider()
                validationPackPanel
            }
            .padding(12)
        }
        #else
        VStack(spacing: 12) {
            Image(
                systemName:
                    "checkmark.seal"
            )
            .font(.system(size: 40))

            Text("Hardware Validation")
                .font(.headline)

            Text(
                "The full file-based validation workflow is enabled on macOS. iPad document-provider integration is completed separately from the Mac parity gate."
            )
            .foregroundStyle(.secondary)
            .multilineTextAlignment(
                .center
            )
            .frame(maxWidth: 500)
        }
        .padding()
        #endif
    }

    #if os(macOS)
    private var fileSelection:
        some View {
        GroupBox("REFERENCE / CAPTURE") {
            VStack(
                alignment: .leading,
                spacing: 9
            ) {
                fileRow(
                    title: "Reference WAV",
                    url: referenceURL
                ) {
                    referenceURL =
                        chooseWAV(
                            title:
                                "Choose Reference WAV"
                        )
                }

                fileRow(
                    title: "Capture WAV",
                    url: captureURL
                ) {
                    captureURL =
                        chooseWAV(
                            title:
                                "Choose Capture WAV"
                        )
                }

                fileRow(
                    title: "Report Folder",
                    url: reportDirectory
                ) {
                    reportDirectory =
                        chooseDirectory(
                            title:
                                "Choose Validation Report Folder"
                        )
                }

                Text(
                    "Report export is optional. When selected, FV-1 Lab writes the shared JSON, Markdown, frequency-response CSV and residual WAV bundle."
                )
                .font(.caption2)
                .foregroundStyle(
                    .secondary
                )
            }
            .padding(4)
        }
    }

    private var validationConfiguration:
        some View {
        GroupBox("VALIDATION THRESHOLDS") {
            Grid(
                alignment: .leading,
                horizontalSpacing: 14,
                verticalSpacing: 8
            ) {
                GridRow {
                    Text("Max alignment")
                    HStack {
                        TextField(
                            "ms",
                            value:
                                $maxAlignmentMS,
                            format: .number
                        )
                        .frame(width: 80)
                        Text("ms")
                    }
                }

                GridRow {
                    Text("Gain-match residual")
                    Toggle(
                        "",
                        isOn:
                            $gainMatchResidual
                    )
                    .labelsHidden()
                }

                GridRow {
                    Text("FFT size")
                    Picker(
                        "",
                        selection: $fftSize
                    ) {
                        ForEach(
                            [
                                1024,
                                2048,
                                4096,
                                8192,
                                16384
                            ],
                            id: \.self
                        ) {
                            Text("\($0)")
                                .tag($0)
                        }
                    }
                    .labelsHidden()
                    .frame(width: 120)
                }

                GridRow {
                    Text("Spectral floor")
                    numberField(
                        $spectralFloorDB,
                        suffix: "dBFS"
                    )
                }

                GridRow {
                    Text("Minimum correlation")
                    TextField(
                        "",
                        value:
                            $minimumCorrelation,
                        format: .number
                    )
                    .frame(width: 90)
                }

                GridRow {
                    Text("Max residual RMS")
                    numberField(
                        $maximumResidualRMSDBFS,
                        suffix: "dBFS"
                    )
                }

                GridRow {
                    Text("Max residual peak")
                    numberField(
                        $maximumResidualPeakDBFS,
                        suffix: "dBFS"
                    )
                }
            }
            .padding(4)
        }
    }

    private var comparisonControls:
        some View {
        HStack {
            Button {
                runComparison()
            } label: {
                Label(
                    busy
                        ? "Comparing…"
                        : "Compare Recordings",
                    systemImage:
                        "waveform.badge.magnifyingglass"
                )
            }
            .buttonStyle(
                .borderedProminent
            )
            .disabled(
                busy
                    || referenceURL == nil
                    || captureURL == nil
            )

            if busy {
                ProgressView()
                    .controlSize(.small)
            }

            Spacer()

            Text(status)
                .font(.caption)
                .foregroundStyle(
                    .secondary
                )
        }
    }

    @ViewBuilder
    private func resultPanel(
        _ outcome: FV1ValidationOutcome
    ) -> some View {
        let result = outcome.summary

        GroupBox(
            result.passed
                ? "VALIDATION PASSED"
                : "VALIDATION FAILED"
        ) {
            VStack(
                alignment: .leading,
                spacing: 10
            ) {
                HStack {
                    Image(
                        systemName:
                            result.passed
                            ? "checkmark.seal.fill"
                            : "xmark.octagon.fill"
                    )
                    .font(.title2)
                    .foregroundStyle(
                        result.passed
                            ? .green
                            : .red
                    )

                    Text(
                        result.passed
                            ? "Capture is within configured thresholds."
                            : "\(result.failureCount) acceptance threshold(s) failed."
                    )
                    .font(.headline)
                }

                Grid(
                    alignment: .leading,
                    horizontalSpacing: 14,
                    verticalSpacing: 6
                ) {
                    metric(
                        "Sample rate",
                        "\(result.sampleRate) Hz"
                    )
                    metric(
                        "Compared",
                        "\(result.comparedFrames) frames"
                    )
                    metric(
                        "Delay",
                        String(
                            format:
                                "%.3f ms (%lld frames)",
                            result.captureDelayMS,
                            result.captureDelayFrames
                        )
                    )
                    metric(
                        "Applied gain",
                        formatDB(
                            result.appliedCaptureGainDB
                        )
                    )

                    Divider()

                    metric(
                        "L correlation",
                        format3(
                            result.left.correlation
                        )
                    )
                    metric(
                        "R correlation",
                        format3(
                            result.right.correlation
                        )
                    )
                    metric(
                        "L gain error",
                        formatDB(
                            result.left.gainErrorDB
                        )
                    )
                    metric(
                        "R gain error",
                        formatDB(
                            result.right.gainErrorDB
                        )
                    )
                    metric(
                        "L residual RMS",
                        formatDBFS(
                            result.left.residualRMSDBFS
                        )
                    )
                    metric(
                        "R residual RMS",
                        formatDBFS(
                            result.right.residualRMSDBFS
                        )
                    )
                    metric(
                        "L residual peak",
                        formatDBFS(
                            result.left.residualPeakDBFS
                        )
                    )
                    metric(
                        "R residual peak",
                        formatDBFS(
                            result.right.residualPeakDBFS
                        )
                    )
                    metric(
                        "L SNR",
                        formatDB(
                            result.left.snrDB
                        )
                    )
                    metric(
                        "R SNR",
                        formatDB(
                            result.right.snrDB
                        )
                    )

                    Divider()

                    metric(
                        "Spectral RMS error",
                        formatDB(
                            result.spectralRMSMagnitudeErrorDB
                        )
                    )
                    metric(
                        "Worst magnitude error",
                        formatDB(
                            result.spectralWorstMagnitudeErrorDB
                        )
                    )
                    metric(
                        "Worst phase error",
                        String(
                            format:
                                "%.2f°",
                            result.spectralWorstPhaseErrorDegrees
                        )
                    )
                }

                if !outcome.failureText.isEmpty {
                    Divider()

                    Text("FAILURES")
                        .font(
                            .caption.bold()
                        )

                    Text(
                        outcome.failureText
                    )
                    .font(
                        .caption.monospaced()
                    )
                    .textSelection(
                        .enabled
                    )
                }
            }
            .padding(4)
        }
    }

    private var validationPackPanel:
        some View {
        GroupBox(
            "HARDWARE VALIDATION PACK"
        ) {
            VStack(
                alignment: .leading,
                spacing: 9
            ) {
                Text(
                    "Generate the same deterministic laboratory stimuli used by the Linux testbench for physical FV-1 capture."
                )
                .font(.caption)
                .foregroundStyle(
                    .secondary
                )

                HStack {
                    LabeledContent(
                        "Rate"
                    ) {
                        Picker(
                            "",
                            selection:
                                $packSampleRate
                        ) {
                            ForEach(
                                [
                                    44_100,
                                    48_000,
                                    96_000
                                ],
                                id: \.self
                            ) {
                                Text(
                                    "\($0) Hz"
                                )
                                .tag($0)
                            }
                        }
                        .labelsHidden()
                    }

                    LabeledContent(
                        "Duration"
                    ) {
                        HStack {
                            TextField(
                                "",
                                value:
                                    $packSeconds,
                                format: .number
                            )
                            .frame(width: 60)
                            Text("s")
                        }
                    }

                    LabeledContent(
                        "Level"
                    ) {
                        TextField(
                            "",
                            value:
                                $packLevel,
                            format: .number
                        )
                        .frame(width: 60)
                    }

                    Spacer()

                    Button(
                        "Generate Pack…"
                    ) {
                        generatePack()
                    }
                    .disabled(busy)
                }
            }
            .padding(4)
        }
    }

    private func fileRow(
        title: String,
        url: URL?,
        choose: @escaping () -> Void
    ) -> some View {
        HStack {
            Text(title)
                .frame(
                    width: 100,
                    alignment: .leading
                )

            Text(
                url?.lastPathComponent
                    ?? "Not selected"
            )
            .font(
                .caption.monospaced()
            )
            .lineLimit(1)
            .frame(
                maxWidth: .infinity,
                alignment: .leading
            )

            Button("Choose…") {
                choose()
            }
        }
    }

    private func numberField(
        _ value: Binding<Double>,
        suffix: String
    ) -> some View {
        HStack {
            TextField(
                "",
                value: value,
                format: .number
            )
            .frame(width: 80)
            Text(suffix)
        }
    }

    @ViewBuilder
    private func metric(
        _ label: String,
        _ value: String
    ) -> some View {
        GridRow {
            Text(label)
                .foregroundStyle(
                    .secondary
                )
            Text(value)
                .monospacedDigit()
                .textSelection(.enabled)
        }
    }

    private func runComparison() {
        guard let referenceURL,
              let captureURL else {
            return
        }

        let referenceAccess =
            referenceURL.startAccessingSecurityScopedResource()
        let captureAccess =
            captureURL.startAccessingSecurityScopedResource()
        let reportAccess =
            reportDirectory?
                .startAccessingSecurityScopedResource()
            ?? false

        busy = true
        outcome = nil
        status =
            "Running shared FV-1 validation analysis…"

        let configuration =
            FV1ValidationConfiguration(
                maxAlignmentMS:
                    maxAlignmentMS,
                gainMatchResidual:
                    gainMatchResidual,
                fftSize:
                    UInt32(
                        max(
                            256,
                            fftSize
                        )
                    ),
                spectralFloorDB:
                    spectralFloorDB,
                minimumCorrelation:
                    minimumCorrelation,
                maximumResidualRMSDBFS:
                    maximumResidualRMSDBFS,
                maximumResidualPeakDBFS:
                    maximumResidualPeakDBFS
            )

        let reportPrefix =
            reportDirectory?
                .appendingPathComponent(
                    "fv1-validation"
                )

        Task { @MainActor in
            do {
                let result =
                    try await Task.detached(
                        priority:
                            .userInitiated
                    ) {
                        try FV1Testbench.validate(
                            referenceURL:
                                referenceURL,
                            captureURL:
                                captureURL,
                            configuration:
                                configuration,
                            reportPrefixURL:
                                reportPrefix
                        )
                    }
                    .value

                outcome = result
                status =
                    result.summary.passed
                    ? "Validation passed."
                    : "Validation completed with threshold failures."
            } catch {
                status =
                    error.localizedDescription
            }

            if referenceAccess {
                referenceURL.stopAccessingSecurityScopedResource()
            }
            if captureAccess {
                captureURL.stopAccessingSecurityScopedResource()
            }
            if reportAccess {
                reportDirectory?
                    .stopAccessingSecurityScopedResource()
            }

            busy = false
        }
    }

    private func generatePack() {
        guard let directory =
            chooseDirectory(
                title:
                    "Choose Validation Pack Destination"
            ) else {
            return
        }

        let directoryAccess =
            directory.startAccessingSecurityScopedResource()

        let output =
            directory
                .appendingPathComponent(
                    "fv1-validation-pack",
                    isDirectory: true
                )

        let rate =
            UInt32(
                max(
                    8_000,
                    packSampleRate
                )
            )
        let seconds =
            max(
                0.1,
                packSeconds
            )
        let level =
            min(
                1.0,
                max(
                    0,
                    packLevel
                )
            )

        busy = true
        status =
            "Generating deterministic validation pack…"

        Task { @MainActor in
            do {
                try await Task.detached(
                    priority:
                        .userInitiated
                ) {
                    try FV1Testbench
                        .writeValidationPack(
                            directoryURL:
                                output,
                            sampleRate:
                                rate,
                            seconds:
                                seconds,
                            level:
                                level
                        )
                }
                .value

                status =
                    "Validation pack generated: \(output.path)"
            } catch {
                status =
                    error.localizedDescription
            }

            if directoryAccess {
                directory.stopAccessingSecurityScopedResource()
            }

            busy = false
        }
    }

    private func chooseWAV(
        title: String
    ) -> URL? {
        let panel = NSOpenPanel()
        panel.title = title
        panel.allowsMultipleSelection =
            false
        panel.canChooseFiles = true
        panel.canChooseDirectories =
            false

        if let wav =
            UTType(
                filenameExtension: "wav"
            ) {
            panel.allowedContentTypes =
                [wav]
        }

        return panel.runModal()
            == .OK
            ? panel.url
            : nil
    }

    private func chooseDirectory(
        title: String
    ) -> URL? {
        let panel = NSOpenPanel()
        panel.title = title
        panel.allowsMultipleSelection =
            false
        panel.canChooseFiles = false
        panel.canChooseDirectories =
            true
        panel.canCreateDirectories =
            true

        return panel.runModal()
            == .OK
            ? panel.url
            : nil
    }

    private func formatDB(
        _ value: Double
    ) -> String {
        String(
            format:
                "%.2f dB",
            value
        )
    }

    private func formatDBFS(
        _ value: Double
    ) -> String {
        String(
            format:
                "%.2f dBFS",
            value
        )
    }

    private func format3(
        _ value: Double
    ) -> String {
        String(
            format:
                "%.5f",
            value
        )
    }
    #endif
}
