import SwiftUI
import UniformTypeIdentifiers

struct WorkspaceView: View {
    @Binding var document: FV1Document
    @StateObject private var model = FV1WorkspaceModel()
    @State private var selection: Panel? = .program
    @State private var importingProgram = false

    enum Panel: String, CaseIterable, Identifiable {
        case program = "Program", scope = "Scope", controls = "Controls", chip = "Chip", console = "Console"
        var id: String { rawValue }
        var icon: String {
            switch self { case .program: "text.alignleft"; case .scope: "waveform"; case .controls: "slider.horizontal.3"; case .chip: "cpu"; case .console: "terminal" }
        }
    }

    var body: some View {
        NavigationSplitView {
            List(Panel.allCases, selection: $selection) { panel in Label(panel.rawValue, systemImage: panel.icon).tag(panel) }
                .navigationTitle("FV-1 Lab")
        } detail: {
            detail
                .navigationTitle(selection?.rawValue ?? "FV-1 Lab")
                .toolbar { toolbar }
        }
        .focusedSceneValue(\.fv1CompileAction) { model.compileAndLoad(source: document.text) }
        .fileImporter(isPresented: $importingProgram, allowedContentTypes: [.data], allowsMultipleSelection: false) { result in
            do {
                guard let url = try result.get().first else { return }
                let access = url.startAccessingSecurityScopedResource(); defer { if access { url.stopAccessingSecurityScopedResource() } }
                model.loadRawProgram(try Data(contentsOf: url))
            } catch { }
        }
        .frame(minWidth: 900, minHeight: 620)
    }

    @ViewBuilder private var detail: some View {
        switch selection ?? .program {
        case .program:
            VStack(spacing: 0) {
                HStack { Text(model.compileSummary).font(.caption).foregroundStyle(.secondary); Spacer() }.padding(.horizontal).padding(.vertical, 8)
                TextEditor(text: $document.text).font(.system(.body, design: .monospaced)).textEditorStyle(.plain).padding(10)
            }
        case .scope:
            VStack(alignment: .leading, spacing: 12) {
                ScopeView(left: model.audio.scopeLeft, right: model.audio.scopeRight).frame(minHeight: 320)
                HStack { Text("Input frames: \(model.audio.inputFrames)"); Spacer(); Text("FV-1 frames: \(model.audio.chipFrames)") }.font(.caption).monospacedDigit()
            }.padding()
        case .controls: ControlsView(model: model)
        case .chip: InspectionView(model: model)
        case .console: ScrollView { Text(model.console).font(.system(.caption, design: .monospaced)).textSelection(.enabled).frame(maxWidth: .infinity, alignment: .leading).padding() }
        }
    }

    @ToolbarContentBuilder private var toolbar: some ToolbarContent {
        ToolbarItemGroup {
            Button { model.compileAndLoad(source: document.text) } label: { Label("Compile & Load", systemImage: "hammer") }
            Button { importingProgram = true } label: { Label("Open Program", systemImage: "shippingbox") }
            Button { model.audio.toggle() } label: { Label(model.audio.isRunning ? "Stop Audio" : "Start Audio", systemImage: model.audio.isRunning ? "stop.circle.fill" : "play.circle.fill") }
        }
    }
}
