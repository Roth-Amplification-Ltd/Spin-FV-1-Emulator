import SwiftUI

struct DelayRAMView: View {
    @ObservedObject var model:
        FV1WorkspaceModel

    @State private var jumpAddress = 0

    var body: some View {
        GroupBox("DELAY RAM VIEWER") {
            VStack(
                alignment: .leading,
                spacing: 7
            ) {
                HStack {
                    if let snapshot =
                        model.snapshot {
                        Text(
                            "Physical pointer \(snapshot.delayPointer)"
                        )
                        .font(
                            .caption.monospaced()
                        )
                    } else {
                        Text(
                            "No program loaded"
                        )
                        .font(.caption)
                        .foregroundStyle(
                            .secondary
                        )
                    }

                    Spacer()

                    Button(
                        "Center Pointer"
                    ) {
                        model.centerDelayOnPointer()
                    }
                    .disabled(
                        model.snapshot == nil
                    )
                }

                HStack {
                    TextField(
                        "Address",
                        value: $jumpAddress,
                        format: .number
                    )
                    .frame(width: 90)

                    Button("Jump") {
                        model.jumpDelayWindow(
                            to:
                                UInt32(
                                    max(
                                        0,
                                        jumpAddress
                                    )
                                )
                        )
                    }

                    Button("Refresh") {
                        model.jumpDelayWindow(
                            to:
                                model.delayCenterAddress
                        )
                    }

                    Spacer()

                    Text(
                        "center \(model.delayCenterAddress)"
                    )
                    .font(
                        .caption2.monospaced()
                    )
                    .foregroundStyle(
                        .secondary
                    )
                }

                Divider()

                ScrollView {
                    LazyVStack(
                        alignment: .leading,
                        spacing: 2
                    ) {
                        ForEach(
                            model.delayWords
                        ) { word in
                            delayRow(word)
                        }
                    }
                }
            }
            .padding(2)
        }
        .frame(maxWidth: .infinity)
    }

    @ViewBuilder
    private func delayRow(
        _ word: FV1DelayWord
    ) -> some View {
        let isPointer =
            model.snapshot?.delayPointer
                == word.address

        HStack(spacing: 8) {
            Text(
                isPointer ? "▶" : " "
            )
            .frame(width: 12)

            Text(
                String(
                    format:
                        "%05u",
                    word.address
                )
            )
            .frame(
                width: 54,
                alignment: .trailing
            )

            Text(hex(word.value))
                .frame(
                    width: 78,
                    alignment: .leading
                )

            Text(
                String(
                    format:
                        "%+.6f",
                    word.normalized
                )
            )
            .frame(
                minWidth: 84,
                alignment: .trailing
            )
        }
        .font(
            .caption2.monospaced()
        )
        .padding(
            .horizontal,
            3
        )
        .padding(
            .vertical,
            1
        )
        .background(
            isPointer
                ? Color.accentColor
                    .opacity(0.18)
                : Color.clear
        )
        .clipShape(
            RoundedRectangle(
                cornerRadius: 3
            )
        )
        .textSelection(.enabled)
    }

    private func hex(
        _ value: Int32
    ) -> String {
        String(
            format:
                "0x%06X",
            UInt32(
                bitPattern: value
            ) & 0x00ff_ffff
        )
    }
}
