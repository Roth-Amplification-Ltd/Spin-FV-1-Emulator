#pragma once

#include <QString>

#include <filesystem>

namespace fv1::gui {

/*
 * Convert a Qt path into std::filesystem::path without throwing away Unicode.
 *
 * Linux/macOS keep their existing narrow UTF-8-ish filesystem behavior.
 * Windows must use UTF-16 because std::filesystem::path is natively wide there
 * and QString already stores the complete Unicode pathname.
 */
inline std::filesystem::path path_from_qstring(const QString& value) {
#if defined(Q_OS_WIN)
    return std::filesystem::path(value.toStdWString());
#else
    return std::filesystem::path(value.toStdString());
#endif
}

/*
 * Convert filesystem paths back to Qt without narrowing UTF-16 on Windows.
 * This is primarily used for recorder/report paths returned by shared C++ code.
 */
inline QString qstring_from_path(const std::filesystem::path& value) {
#if defined(Q_OS_WIN)
    return QString::fromStdWString(value.wstring());
#else
    return QString::fromStdString(value.string());
#endif
}

} // namespace fv1::gui
