// ============================================================================
//  GOD_SSS_TIER_VPN — src/core/log.h
//  Renkli motor logu (referans: MVP_GOD_SSS_TIER_SIMULATOR_UI gt::Log ailesi)
//  LogSetSink ile ConsoleBuffer'a bağlanır — WebGUI debug konsolu akar.
// ============================================================================
#pragma once
#include <functional>
#include <string>

namespace gt {

enum class LogColor { Reset, Red, Green, Yellow, Cyan, Magenta };

void LogInit();
void LogSetSink(std::function<void(const std::string& line)> sink);
void Log(const char* fmt, ...);
void LogLine(LogColor c, const char* fmt, ...);
void LogOk(const char* fmt, ...);
void LogWarn(const char* fmt, ...);
void LogErr(const char* fmt, ...);

} // namespace gt
