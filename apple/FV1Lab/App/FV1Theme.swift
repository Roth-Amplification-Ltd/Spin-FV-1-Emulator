import Foundation
import SwiftUI

#if os(macOS)
import AppKit
#endif

struct FV1RGB: Equatable {
    let r: Double
    let g: Double
    let b: Double

    init(_ r: Int, _ g: Int, _ b: Int) {
        self.r = Double(r) / 255.0
        self.g = Double(g) / 255.0
        self.b = Double(b) / 255.0
    }

    init(r: Double, g: Double, b: Double) {
        self.r = r
        self.g = g
        self.b = b
    }

    var color: Color {
        Color(red: r, green: g, blue: b)
    }

    func mixed(
        with other: FV1RGB,
        amount: Double
    ) -> FV1RGB {
        let t = max(0, min(1, amount))
        return FV1RGB(
            r: r * (1 - t) + other.r * t,
            g: g * (1 - t) + other.g * t,
            b: b * (1 - t) + other.b * t
        )
    }
}

struct FV1ThemePalette {
    let name: String
    let window: Color
    let panel: Color
    let raised: Color
    let border: Color
    let text: Color
    let muted: Color
    let accent: Color
    let success: Color
    let warning: Color
    let error: Color
    let gridMajor: Color
    let gridMinor: Color
    let rawTrace: Color
    let colorScheme: ColorScheme
}

enum FV1ThemeCatalog {
    static let themes = [
        "Dark",
        "Light",
        "Midnight",
        "Amber CRT",
        "Green Phosphor",
        "Slate",
        "High Contrast"
    ]

    static let accents = [
        "Cyan",
        "Blue",
        "Green",
        "Amber",
        "Orange",
        "Red",
        "Purple",
        "Magenta"
    ]

    static let icons = [
        "Silver",
        "Dark Cyan",
        "Blue",
        "Amber"
    ]

    static func accentRGB(_ name: String) -> FV1RGB {
        switch name {
        case "Blue":
            return FV1RGB(75, 138, 255)
        case "Green":
            return FV1RGB(76, 214, 132)
        case "Amber":
            return FV1RGB(244, 182, 63)
        case "Orange":
            return FV1RGB(245, 132, 55)
        case "Red":
            return FV1RGB(238, 82, 82)
        case "Purple":
            return FV1RGB(156, 112, 255)
        case "Magenta":
            return FV1RGB(232, 93, 205)
        default:
            return FV1RGB(66, 208, 232)
        }
    }

    static func palette(
        theme name: String,
        accent accentName: String
    ) -> FV1ThemePalette {
        let selectedAccent = accentRGB(accentName)

        if name == "Light" {
            return makePalette(
                name: name,
                window: FV1RGB(237, 240, 244),
                panel: FV1RGB(250, 251, 252),
                raised: FV1RGB(255, 255, 255),
                border: FV1RGB(190, 198, 208),
                text: FV1RGB(28, 34, 42),
                muted: FV1RGB(94, 105, 118),
                accent: selectedAccent,
                success: FV1RGB(35, 142, 79),
                warning: FV1RGB(180, 116, 20),
                error: FV1RGB(190, 53, 53),
                gridMajor: FV1RGB(185, 193, 204),
                gridMinor: FV1RGB(216, 222, 229),
                scheme: .light
            )
        }

        if name == "Midnight" {
            return darkBase(
                name: name,
                window: FV1RGB(8, 13, 24),
                panel: FV1RGB(15, 23, 38),
                text: FV1RGB(225, 234, 248),
                accent: selectedAccent
            )
        }

        if name == "Amber CRT" {
            return darkBase(
                name: name,
                window: FV1RGB(12, 9, 5),
                panel: FV1RGB(22, 16, 8),
                text: FV1RGB(245, 194, 95),
                accent: FV1RGB(255, 184, 58)
            )
        }

        if name == "Green Phosphor" {
            return darkBase(
                name: name,
                window: FV1RGB(4, 11, 7),
                panel: FV1RGB(7, 22, 13),
                text: FV1RGB(133, 245, 151),
                accent: FV1RGB(79, 255, 113)
            )
        }

        if name == "Slate" {
            return darkBase(
                name: name,
                window: FV1RGB(31, 35, 40),
                panel: FV1RGB(43, 48, 55),
                text: FV1RGB(229, 233, 238),
                accent: selectedAccent
            )
        }

        if name == "High Contrast" {
            return makePalette(
                name: name,
                window: FV1RGB(0, 0, 0),
                panel: FV1RGB(0, 0, 0),
                raised: FV1RGB(18, 18, 18),
                border: FV1RGB(255, 255, 255),
                text: FV1RGB(255, 255, 255),
                muted: FV1RGB(215, 215, 215),
                accent: FV1RGB(255, 230, 0),
                success: FV1RGB(88, 211, 122),
                warning: FV1RGB(245, 184, 76),
                error: FV1RGB(242, 92, 92),
                gridMajor: FV1RGB(150, 150, 150),
                gridMinor: FV1RGB(80, 80, 80),
                scheme: .dark
            )
        }

        return darkBase(
            name: "Dark",
            window: FV1RGB(18, 20, 24),
            panel: FV1RGB(27, 30, 35),
            text: FV1RGB(228, 233, 239),
            accent: selectedAccent
        )
    }

    private static func darkBase(
        name: String,
        window: FV1RGB,
        panel: FV1RGB,
        text: FV1RGB,
        accent: FV1RGB
    ) -> FV1ThemePalette {
        let white = FV1RGB(255, 255, 255)

        return makePalette(
            name: name,
            window: window,
            panel: panel,
            raised: panel.mixed(with: white, amount: 0.07),
            border: panel.mixed(with: white, amount: 0.18),
            text: text,
            muted: text.mixed(with: panel, amount: 0.43),
            accent: accent,
            success: FV1RGB(88, 211, 122),
            warning: FV1RGB(245, 184, 76),
            error: FV1RGB(242, 92, 92),
            gridMajor: panel.mixed(with: text, amount: 0.20),
            gridMinor: panel.mixed(with: text, amount: 0.10),
            scheme: .dark
        )
    }

    private static func makePalette(
        name: String,
        window: FV1RGB,
        panel: FV1RGB,
        raised: FV1RGB,
        border: FV1RGB,
        text: FV1RGB,
        muted: FV1RGB,
        accent: FV1RGB,
        success: FV1RGB,
        warning: FV1RGB,
        error: FV1RGB,
        gridMajor: FV1RGB,
        gridMinor: FV1RGB,
        scheme: ColorScheme
    ) -> FV1ThemePalette {
        FV1ThemePalette(
            name: name,
            window: window.color,
            panel: panel.color,
            raised: raised.color,
            border: border.color,
            text: text.color,
            muted: muted.color,
            accent: accent.color,
            success: success.color,
            warning: warning.color,
            error: error.color,
            gridMajor: gridMajor.color,
            gridMinor: gridMinor.color,
            rawTrace:
                text.mixed(
                    with: panel,
                    amount: 0.28
                ).color,
            colorScheme: scheme
        )
    }
}

private struct FV1ThemePaletteKey: EnvironmentKey {
    static let defaultValue =
        FV1ThemeCatalog.palette(
            theme: "Dark",
            accent: "Cyan"
        )
}

extension EnvironmentValues {
    var fv1ThemePalette: FV1ThemePalette {
        get { self[FV1ThemePaletteKey.self] }
        set { self[FV1ThemePaletteKey.self] = newValue }
    }
}

struct FV1ThemeRootModifier: ViewModifier {
    @AppStorage("appearance/theme")
    private var themeName = "Dark"

    @AppStorage("appearance/accent")
    private var accentName = "Cyan"

    @AppStorage("appearance/icon")
    private var iconName = "Silver"

    private var palette: FV1ThemePalette {
        FV1ThemeCatalog.palette(
            theme: themeName,
            accent: accentName
        )
    }

    func body(content: Content) -> some View {
        content
            .environment(
                \.fv1ThemePalette,
                palette
            )
            .tint(palette.accent)
            .foregroundStyle(palette.text)
            .background(
                palette.window
                    .ignoresSafeArea()
            )
            .preferredColorScheme(
                palette.colorScheme
            )
            #if os(macOS)
            .onAppear {
                FV1MacAppearanceManager.apply(
                    themeName: themeName,
                    iconName: iconName
                )
            }
            .onChange(of: themeName) { _, newValue in
                FV1MacAppearanceManager
                    .applyAppearance(
                        themeName: newValue
                    )
            }
            .onChange(of: iconName) { _, newValue in
                FV1MacApplicationIconManager
                    .apply(name: newValue)
            }
            #endif
    }
}

extension View {
    func fv1Theme() -> some View {
        modifier(FV1ThemeRootModifier())
    }
}

#if os(macOS)
@MainActor
enum FV1MacAppearanceManager {
    static func apply(
        themeName: String,
        iconName: String
    ) {
        applyAppearance(themeName: themeName)
        FV1MacApplicationIconManager
            .apply(name: iconName)
    }

    static func applyAppearance(
        themeName: String
    ) {
        NSApplication.shared.appearance =
            NSAppearance(
                named:
                    themeName == "Light"
                    ? .aqua
                    : .darkAqua
            )
    }
}

@MainActor
enum FV1MacApplicationIconManager {
    private static let assets = [
        "Silver": "FV1IconSilver",
        "Dark Cyan": "FV1IconDarkCyan",
        "Blue": "FV1IconBlue",
        "Amber": "FV1IconAmber"
    ]

    static func apply(name: String) {
        let asset =
            assets[name]
            ?? assets["Silver"]!

        guard let image =
            NSImage(named: NSImage.Name(asset)) else {
            return
        }

        NSApplication.shared
            .applicationIconImage = image
    }
}
#endif
