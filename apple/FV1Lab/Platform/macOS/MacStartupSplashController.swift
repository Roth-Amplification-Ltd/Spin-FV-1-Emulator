#if os(macOS)
import AppKit
import Combine
import QuartzCore
import SwiftUI

@MainActor
private final class FV1MacSplashState: ObservableObject {
    @Published var progress = 8.0
    @Published var status = "Initializing Spin FV-1 Emulator…"

    let version = FV1Engine.versionString
}

private struct FV1MacSplashHostView: View {
    @ObservedObject var state: FV1MacSplashState

    var body: some View {
        FV1StartupSplashView(
            progress: state.progress,
            status: state.status,
            version: state.version
        )
        .frame(width: 720, height: 405)
    }
}

@MainActor
final class FV1MacStartupCoordinator: NSObject {
    static let shared = FV1MacStartupCoordinator()

    private static let splashIdentifier =
        NSUserInterfaceItemIdentifier(
            "com.rothamplification.fv1lab.startup-splash"
        )

    private weak var mainWindow: NSWindow?
    private var splashWindow: NSPanel?
    private var splashState: FV1MacSplashState?
    private var launchSuppressionActive = false

    private var didStart = false
    private var didFinish = false

    private override init() {
        super.init()
    }

    func beginLaunchSuppression() {
        guard !launchSuppressionActive else {
            return
        }

        launchSuppressionActive = true

        /*
         * Use AppKit's selector-based notification API here instead of the
         * block-based API. In Swift 6 the block form is @Sendable and can
         * introduce actor-isolation diagnostics for this @MainActor
         * coordinator. NSWindow key notifications are delivered on the UI
         * thread, which is exactly where this AppKit lifecycle code belongs.
         */
        NotificationCenter.default.addObserver(
            self,
            selector:
                #selector(
                    handleWindowDidBecomeKey(_:)
                ),
            name:
                NSWindow.didBecomeKeyNotification,
            object: nil
        )
    }

    @objc
    private func handleWindowDidBecomeKey(
        _ notification: Notification
    ) {
        guard let window =
            notification.object as? NSWindow else {
            return
        }

        suppressWindowBeforeInitialSplash(
            window
        )
    }

    func presentInitialSplashIfNeeded() {
        guard !didStart else { return }
        didStart = true
        presentSplash(replay: false)
    }

    func attachMainWindow(_ window: NSWindow) {
        guard !isSplashWindow(window) else { return }

        mainWindow = window
        configureMainWindowGeometry(window)

        if didFinish {
            window.alphaValue = 1
            window.makeKeyAndOrderFront(nil)
            return
        }

        window.alphaValue = 0
        window.orderOut(nil)

        if !didStart {
            presentInitialSplashIfNeeded()
        }
    }

    private func suppressWindowBeforeInitialSplash(
        _ window: NSWindow
    ) {
        guard !didFinish,
              !isSplashWindow(window) else {
            return
        }

        if mainWindow == nil {
            mainWindow = window
            configureMainWindowGeometry(window)
        }

        window.alphaValue = 0
        window.orderOut(nil)
    }

    private func isSplashWindow(
        _ window: NSWindow
    ) -> Bool {
        window === splashWindow
            || window.identifier
                == Self.splashIdentifier
    }

    func showStartupSplashAgain() {
        if let existing = splashWindow {
            existing.alphaValue = 1
            existing.orderFrontRegardless()
            return
        }

        guard didFinish else {
            presentInitialSplashIfNeeded()
            return
        }

        presentSplash(replay: true)
    }

    private func presentSplash(
        replay: Bool
    ) {
        guard splashWindow == nil else {
            splashWindow?.orderFrontRegardless()
            return
        }

        let state = FV1MacSplashState()
        splashState = state

        let panel = NSPanel(
            contentRect: NSRect(
                x: 0,
                y: 0,
                width: 720,
                height: 405
            ),
            styleMask: [.borderless],
            backing: .buffered,
            defer: false
        )

        panel.identifier = Self.splashIdentifier
        panel.title = "FV-1 Lab"
        panel.isOpaque = false
        panel.backgroundColor = .clear
        panel.hasShadow = true
        panel.level = .floating
        panel.hidesOnDeactivate = false
        panel.isReleasedWhenClosed = false
        panel.collectionBehavior = [
            .canJoinAllSpaces,
            .fullScreenAuxiliary
        ]

        panel.contentViewController =
            NSHostingController(
                rootView:
                    FV1MacSplashHostView(
                        state: state
                    )
            )

        panel.setContentSize(
            NSSize(
                width: 720,
                height: 405
            )
        )

        if replay,
           let mainWindow {
            let frame = mainWindow.frame
            let splashFrame = panel.frame

            panel.setFrameOrigin(
                NSPoint(
                    x:
                        frame.midX
                        - splashFrame.width * 0.5,
                    y:
                        frame.midY
                        - splashFrame.height * 0.5
                )
            )
        } else {
            panel.center()
        }

        panel.alphaValue = 1
        splashWindow = panel

        NSApplication.shared.activate(
            ignoringOtherApps: true
        )
        panel.orderFrontRegardless()

        Task { @MainActor [weak self] in
            await self?.runSplashSequence(
                replay: replay
            )
        }
    }

    private func sleep(
        milliseconds: UInt64
    ) async {
        try? await Task.sleep(
            nanoseconds:
                milliseconds * 1_000_000
        )
    }

    private func runSplashSequence(
        replay: Bool
    ) async {
        guard let state = splashState else {
            return
        }

        state.progress = 8
        state.status =
            "Initializing Spin FV-1 Emulator…"
        await sleep(milliseconds: 180)

        _ = FV1Engine.versionString
        _ = FV1Engine.abiVersion

        withAnimation(
            .easeOut(duration: 0.16)
        ) {
            state.progress = 26
        }
        state.status =
            "Loading FV-1 SDK and application resources…"
        await sleep(milliseconds: 310)

        withAnimation(
            .easeOut(duration: 0.18)
        ) {
            state.progress = 48
        }
        state.status =
            "Preparing native Apple testbench…"
        await sleep(milliseconds: 420)

        withAnimation(
            .easeOut(duration: 0.20)
        ) {
            state.progress = 76
        }
        state.status =
            "Initializing FV-1 Lab workspace…"
        await sleep(milliseconds: 430)

        withAnimation(
            .easeOut(duration: 0.18)
        ) {
            state.progress = 98
        }

        state.status =
            replay
                ? "FV-1 Lab ready"
                : "FV-1 Lab ready — preparing workspace…"

        await sleep(milliseconds: 230)

        withAnimation(
            .easeOut(duration: 0.12)
        ) {
            state.progress = 100
        }

        state.status = "Ready"

        await sleep(milliseconds: 180)

        if replay {
            closeSplash(
                revealMainWindow: false
            )
        } else {
            revealMainWindow()
        }
    }

    private func closeSplash(
        revealMainWindow: Bool
    ) {
        guard let panel = splashWindow else {
            if revealMainWindow {
                showMainWindowNow()
            }
            return
        }

        NSAnimationContext.runAnimationGroup {
            context in

            context.duration = 0.28
            context.timingFunction =
                CAMediaTimingFunction(
                    name: .easeOut
                )

            panel.animator().alphaValue = 0
        } completionHandler: { [weak self] in
            Task { @MainActor in
                panel.orderOut(nil)
                panel.close()

                self?.splashWindow = nil
                self?.splashState = nil

                if revealMainWindow {
                    self?.showMainWindowNow()
                }
            }
        }
    }

    private func revealMainWindow() {
        guard !didFinish else { return }
        didFinish = true

        endLaunchSuppression()

        closeSplash(
            revealMainWindow: true
        )
    }

    private func endLaunchSuppression() {
        guard launchSuppressionActive else {
            return
        }

        NotificationCenter.default.removeObserver(
            self,
            name:
                NSWindow.didBecomeKeyNotification,
            object: nil
        )

        launchSuppressionActive = false
    }

    private func configureMainWindowGeometry(
        _ window: NSWindow
    ) {
        let preferred =
            NSSize(
                width: 1500,
                height: 860
            )

        let baseMinimum =
            NSSize(
                width: 1360,
                height: 760
            )

        guard let visibleFrame =
            window.screen?.visibleFrame
                ?? NSScreen.main?.visibleFrame else {
            window.contentMinSize =
                baseMinimum

            let current =
                window.contentLayoutRect.size

            let target =
                NSSize(
                    width:
                        max(
                            current.width,
                            preferred.width
                        ),
                    height:
                        max(
                            current.height,
                            preferred.height
                        )
                )

            if abs(
                current.width
                    - target.width
            ) > 1
                || abs(
                    current.height
                        - target.height
                ) > 1 {
                window.setContentSize(
                    target
                )
                window.center()
            }
            return
        }

        let availableWidth =
            max(
                960,
                visibleFrame.width - 40
            )

        let availableHeight =
            max(
                640,
                visibleFrame.height - 40
            )

        window.contentMinSize =
            NSSize(
                width:
                    min(
                        baseMinimum.width,
                        availableWidth
                    ),
                height:
                    min(
                        baseMinimum.height,
                        availableHeight
                    )
            )

        let desired =
            NSSize(
                width:
                    min(
                        preferred.width,
                        availableWidth
                    ),
                height:
                    min(
                        preferred.height,
                        availableHeight
                    )
            )

        let current =
            window.contentLayoutRect.size

        let target =
            NSSize(
                width:
                    min(
                        availableWidth,
                        max(
                            current.width,
                            desired.width
                        )
                    ),
                height:
                    min(
                        availableHeight,
                        max(
                            current.height,
                            desired.height
                        )
                    )
            )

        if abs(
            current.width - target.width
        ) > 1
            || abs(
                current.height - target.height
            ) > 1 {
            window.setContentSize(
                target
            )
            window.center()
        }
    }

    private func showMainWindowNow() {
        guard let window = mainWindow else {
            return
        }

        configureMainWindowGeometry(
            window
        )

        window.alphaValue = 1
        window.makeKeyAndOrderFront(nil)

        NSApplication.shared.activate(
            ignoringOtherApps: true
        )
    }
}

@MainActor
final class FV1MacAppDelegate:
    NSObject,
    NSApplicationDelegate {

    func applicationWillFinishLaunching(
        _ notification: Notification
    ) {
        FV1MacStartupCoordinator.shared
            .beginLaunchSuppression()
    }

    func applicationDidFinishLaunching(
        _ notification: Notification
    ) {
        FV1MacStartupCoordinator.shared
            .presentInitialSplashIfNeeded()
    }
}

struct FV1MacMainWindowCapture:
    NSViewRepresentable {

    func makeNSView(
        context: Context
    ) -> NSView {
        FV1MacWindowCaptureView()
    }

    func updateNSView(
        _ nsView: NSView,
        context: Context
    ) {}
}

private final class FV1MacWindowCaptureView:
    NSView {

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()

        guard let window else { return }

        Task { @MainActor in
            FV1MacStartupCoordinator.shared
                .attachMainWindow(
                    window
                )
        }
    }
}
#endif
