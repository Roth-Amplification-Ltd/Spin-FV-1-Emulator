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

} // namespace fv1::gui
