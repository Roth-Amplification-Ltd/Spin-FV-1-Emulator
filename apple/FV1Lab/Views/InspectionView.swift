import Foundation
import SwiftUI

struct InspectionView: View {
    @ObservedObject var model: FV1WorkspaceModel

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                HStack { Text("Virtual Chip").font(.headline); Spacer(); Button("Refresh") { model.refreshInspection() } }
                if let s = model.snapshot {
                    Grid(alignment: .leading, horizontalSpacing: 18, verticalSpacing: 6) {
                        row("Sample", "\(s.sampleCounter)"); row("PC", "\(s.programCounter)")
                        row("Instruction", "\(s.instructionCounter)"); row("ACC", hex(s.acc)); row("PACC", hex(s.pacc))
                        row("LR", hex(s.lr)); row("ADDR_PTR", hex(s.addressPointer)); row("DACL", hex(s.dacLeft)); row("DACR", hex(s.dacRight))
                    }
                } else { Text("Load a program to inspect chip state.").foregroundStyle(.secondary) }
                Divider()
                Text("Resources").font(.headline)
                if let r = model.resources {
                    Grid(alignment: .leading, horizontalSpacing: 18, verticalSpacing: 6) {
                        row("Instructions", "\(r.usedInstructions)"); row("Worst path", "\(r.worstCasePath)")
                        row("Delay reads", "\(r.staticDelayReads)"); row("Delay writes", "\(r.staticDelayWrites)")
                        row("Dynamic delay", "\(r.dynamicDelayReads)"); row("Highest delay", "\(r.highestStaticDelayAddress)")
                        row("General regs", "\(r.generalRegistersUsed)"); row("POTs", "\(r.potsUsed)")
                        row("Sine LFOs", "\(r.sineLFOsUsed)"); row("Ramp LFOs", "\(r.rampLFOsUsed)"); row("SKPs", "\(r.skipInstructions)")
                    }
                }
            }.padding()
        }
    }

    @ViewBuilder private func row(_ label: String, _ value: String) -> some View {
        GridRow { Text(label).foregroundStyle(.secondary); Text(value).monospacedDigit().textSelection(.enabled) }
    }
    private func hex(_ value: Int32) -> String { String(format: "0x%08X", UInt32(bitPattern: value)) }
}
