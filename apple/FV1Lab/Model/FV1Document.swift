import SwiftUI
import UniformTypeIdentifiers

extension UTType {
    static let fv1SpinASM = UTType(exportedAs: "com.rothamplification.fv1.spinasm", conformingTo: .plainText)
}

struct FV1Document: FileDocument {
    static var readableContentTypes: [UTType] { [.fv1SpinASM, .plainText] }
    var text: String

    init(text: String = "RDAX ADCL, 1.0\nWRAX DACL, 0\nRDAX ADCR, 1.0\nWRAX DACR, 0\n") { self.text = text }

    init(configuration: ReadConfiguration) throws {
        guard let data = configuration.file.regularFileContents,
              let string = String(data: data, encoding: .utf8) else {
            throw CocoaError(.fileReadCorruptFile)
        }
        text = string
    }

    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper {
        FileWrapper(regularFileWithContents: Data(text.utf8))
    }
}
