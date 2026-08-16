#if os(macOS)
import AppKit
import Foundation
import UniformTypeIdentifiers

@MainActor
enum MacExportPresenter {
    static func chooseRecordingURL(suggestedName: String) -> URL? {
        let panel = NSSavePanel()
        panel.title = "Record FV-1 Lab Audio"
        panel.nameFieldStringValue = suggestedName
        panel.canCreateDirectories = true
        if let wav = UTType(filenameExtension: "wav") {
            panel.allowedContentTypes = [wav]
        }
        return panel.runModal() == .OK ? panel.url : nil
    }

    static func saveCSV(suggestedName: String, contents: String) {
        let panel = NSSavePanel()
        panel.title = "Export FV-1 Lab Analyzer Data"
        panel.nameFieldStringValue = suggestedName
        panel.canCreateDirectories = true
        if let csv = UTType(filenameExtension: "csv") {
            panel.allowedContentTypes = [csv]
        }

        guard panel.runModal() == .OK, let url = panel.url else { return }

        do {
            try contents.write(to: url, atomically: true, encoding: .utf8)
        } catch {
            NSAlert(error: error).runModal()
        }
    }
}
#endif
