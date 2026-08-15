#if os(macOS)
import AppKit

@MainActor
enum MacAboutPresenter {
    static func show() {
        let credits = NSAttributedString(
            string: "Created & engineered by Adam Vadala-Roth\nRoth Amplification LTD\nMPL-2.0",
            attributes: [.font: NSFont.systemFont(ofSize: NSFont.smallSystemFontSize)]
        )
        NSApplication.shared.orderFrontStandardAboutPanel(options: [
            .applicationName: "FV-1 Lab",
            .applicationVersion: FV1Engine.versionString,
            .credits: credits
        ])
    }
}
#endif
