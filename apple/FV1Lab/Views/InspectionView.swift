import Foundation
import SwiftUI

struct InspectionView: View {
    @ObservedObject var model:
        FV1WorkspaceModel

    var body: some View {
        ScrollView {
            VStack(
                alignment: .leading,
                spacing: 14
            ) {
                debuggerControls

                if let trace =
                    model.debugTrace {
                    Divider()
                    tracePanel(trace)
                }

                Divider()
                architecturalState

                Divider()
                lfoState

                Divider()
                generalRegisters

                Divider()
                resourceState
            }
            .padding()
        }
    }

    private var debuggerControls:
        some View {
        VStack(
            alignment: .leading,
            spacing: 8
        ) {
            HStack {
                Text(
                    "Offline Virtual Chip"
                )
                .font(.headline)

                Spacer()

                Button("Refresh") {
                    model.refreshInspection()
                }
            }

            Text(
                "The inspector owns a separate FV-1 instance. Stepping never races the realtime audio engine."
            )
            .font(.caption2)
            .foregroundStyle(.secondary)

            Grid(
                alignment: .leading,
                horizontalSpacing: 8,
                verticalSpacing: 5
            ) {
                GridRow {
                    Text("Input L")
                        .foregroundStyle(
                            .secondary
                        )
                    TextField(
                        "0.0",
                        value:
                            $model.debugInputLeft,
                        format: .number
                    )
                    .frame(width: 80)
                }

                GridRow {
                    Text("Input R")
                        .foregroundStyle(
                            .secondary
                        )
                    TextField(
                        "0.0",
                        value:
                            $model.debugInputRight,
                        format: .number
                    )
                    .frame(width: 80)
                }
            }

            HStack {
                Button(
                    "Step Instruction"
                ) {
                    model.stepInstruction()
                }
                .keyboardShortcut(
                    "i",
                    modifiers: [
                        .command,
                        .option
                    ]
                )

                Button(
                    "Step Sample"
                ) {
                    model.stepSample()
                }
                .keyboardShortcut(
                    "s",
                    modifiers: [
                        .command,
                        .option
                    ]
                )

                Button("Reset") {
                    model.reset()
                }
            }
        }
    }

    @ViewBuilder
    private func tracePanel(
        _ trace: FV1DebugTrace
    ) -> some View {
        VStack(
            alignment: .leading,
            spacing: 7
        ) {
            Text("LAST INSTRUCTION")
                .font(.caption.bold())

            Text(
                "\(trace.opcodeName)  \(hex24(trace.rawInstruction))"
            )
            .font(
                .system(
                    .body,
                    design: .monospaced
                )
            )
            .textSelection(.enabled)

            Grid(
                alignment: .leading,
                horizontalSpacing: 12,
                verticalSpacing: 5
            ) {
                row(
                    "PC",
                    "\(trace.pcBefore) → \(trace.pcAfter)"
                )
                row(
                    "Sample",
                    "\(trace.sampleIndex)"
                )
                row(
                    "Instruction",
                    "\(trace.instructionIndex)"
                )
                row(
                    "ACC",
                    "\(hex(trace.accBefore)) → \(hex(trace.accAfter))"
                )
                row(
                    "PACC",
                    hex(trace.paccAfter)
                )
                row(
                    "LR",
                    hex(trace.lrAfter)
                )
                row(
                    "Skipped",
                    trace.skipped
                        ? "yes"
                        : "no"
                )
                row(
                    "Sample finished",
                    trace.sampleFinished
                        ? "yes"
                        : "no"
                )
            }

            if let left =
                trace.outputLeft,
               let right =
                trace.outputRight {
                Text(
                    String(
                        format:
                            "Output  L %.6f   R %.6f",
                        left,
                        right
                    )
                )
                .font(
                    .caption.monospaced()
                )
            }
        }
    }

    private var architecturalState:
        some View {
        VStack(
            alignment: .leading,
            spacing: 7
        ) {
            Text("ARCHITECTURAL STATE")
                .font(.caption.bold())

            if let snapshot =
                model.snapshot {
                Grid(
                    alignment: .leading,
                    horizontalSpacing: 18,
                    verticalSpacing: 6
                ) {
                    row(
                        "Sample",
                        "\(snapshot.sampleCounter)"
                    )
                    row(
                        "PC",
                        "\(snapshot.programCounter)"
                    )
                    row(
                        "Instruction",
                        "\(snapshot.instructionCounter)"
                    )
                    row(
                        "ACC",
                        hex(snapshot.acc)
                    )
                    row(
                        "PACC",
                        hex(snapshot.pacc)
                    )
                    row(
                        "LR",
                        hex(snapshot.lr)
                    )
                    row(
                        "ADDR_PTR",
                        hex(
                            snapshot.addressPointer
                        )
                    )
                    row(
                        "Delay pointer",
                        "\(snapshot.delayPointer)"
                    )
                    row(
                        "DACL",
                        hex(snapshot.dacLeft)
                    )
                    row(
                        "DACR",
                        hex(snapshot.dacRight)
                    )
                    row(
                        "First run",
                        snapshot.firstRun
                            ? "yes"
                            : "no"
                    )
                    row(
                        "Sample active",
                        snapshot.sampleActive
                            ? "yes"
                            : "no"
                    )
                }
            } else {
                Text(
                    "Load a program to inspect chip state."
                )
                .foregroundStyle(
                    .secondary
                )
            }
        }
    }

    private var lfoState:
        some View {
        VStack(
            alignment: .leading,
            spacing: 7
        ) {
            Text("LFO STATE")
                .font(.caption.bold())

            if let snapshot =
                model.snapshot {
                Grid(
                    alignment: .leading,
                    horizontalSpacing: 12,
                    verticalSpacing: 5
                ) {
                    row(
                        "SIN0",
                        arrayValue(
                            snapshot.sineLFO,
                            0
                        )
                    )
                    row(
                        "COS0",
                        arrayValue(
                            snapshot.cosineLFO,
                            0
                        )
                    )
                    row(
                        "SIN1",
                        arrayValue(
                            snapshot.sineLFO,
                            1
                        )
                    )
                    row(
                        "COS1",
                        arrayValue(
                            snapshot.cosineLFO,
                            1
                        )
                    )
                    row(
                        "RMP0",
                        arrayValue(
                            snapshot.rampLFO,
                            0
                        )
                    )
                    row(
                        "RMP1",
                        arrayValue(
                            snapshot.rampLFO,
                            1
                        )
                    )
                }
            }
        }
    }

    private var generalRegisters:
        some View {
        VStack(
            alignment: .leading,
            spacing: 7
        ) {
            Text("REG0–REG31")
                .font(.caption.bold())

            if let registers =
                model.snapshot?.registers,
               registers.count >= 64 {
                LazyVGrid(
                    columns: [
                        GridItem(
                            .flexible(),
                            alignment: .leading
                        ),
                        GridItem(
                            .flexible(),
                            alignment: .leading
                        )
                    ],
                    spacing: 4
                ) {
                    ForEach(
                        0..<32,
                        id: \.self
                    ) { generalIndex in
                        let registerIndex =
                            0x20
                            + generalIndex

                        Text(
                            String(
                                format:
                                    "REG%-2d  %@",
                                generalIndex,
                                hex(
                                    registers[
                                        registerIndex
                                    ]
                                )
                            )
                        )
                        .font(
                            .caption2
                                .monospaced()
                        )
                        .textSelection(
                            .enabled
                        )
                    }
                }
            }
        }
    }

    private var resourceState:
        some View {
        VStack(
            alignment: .leading,
            spacing: 7
        ) {
            Text("RESOURCES")
                .font(.caption.bold())

            if let resource =
                model.resources {
                Grid(
                    alignment: .leading,
                    horizontalSpacing: 18,
                    verticalSpacing: 6
                ) {
                    row(
                        "Instructions",
                        "\(resource.usedInstructions)"
                    )
                    row(
                        "Worst path",
                        "\(resource.worstCasePath)"
                    )
                    row(
                        "Delay reads",
                        "\(resource.staticDelayReads)"
                    )
                    row(
                        "Delay writes",
                        "\(resource.staticDelayWrites)"
                    )
                    row(
                        "Dynamic delay",
                        "\(resource.dynamicDelayReads)"
                    )
                    row(
                        "Highest delay",
                        "\(resource.highestStaticDelayAddress)"
                    )
                    row(
                        "General regs",
                        "\(resource.generalRegistersUsed)"
                    )
                    row(
                        "POTs",
                        "\(resource.potsUsed)"
                    )
                    row(
                        "Sine LFOs",
                        "\(resource.sineLFOsUsed)"
                    )
                    row(
                        "Ramp LFOs",
                        "\(resource.rampLFOsUsed)"
                    )
                    row(
                        "SKPs",
                        "\(resource.skipInstructions)"
                    )
                }
            }
        }
    }

    @ViewBuilder
    private func row(
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

    private func arrayValue(
        _ values: [Int32],
        _ index: Int
    ) -> String {
        guard values.indices
            .contains(index) else {
            return "—"
        }
        return hex(values[index])
    }

    private func hex(
        _ value: Int32
    ) -> String {
        String(
            format:
                "0x%08X",
            UInt32(
                bitPattern: value
            )
        )
    }

    private func hex24(
        _ value: UInt32
    ) -> String {
        String(
            format:
                "0x%06X",
            value & 0x00ff_ffff
        )
    }
}
