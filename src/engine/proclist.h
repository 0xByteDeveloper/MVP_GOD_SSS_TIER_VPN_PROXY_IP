// ============================================================================
//  GOD_SSS_TIER_VPN — src/engine/proclist.h
//  Süreç listesi (PID + isim + komut satırı) — UI'daki search_textbox'ın kaynağı.
//    Linux  : /proc/<pid>/stat (isim) + /proc/<pid>/cmdline
//    Windows: CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)
// ============================================================================
#pragma once
#include <string>
#include <vector>

namespace gt {

struct ProcInfo {
    long pid = 0;
    std::string name;      // chrome, curl, game.exe ...
    std::string cmdline;   // (varsa) tam komut satırı
    std::string owner;     // (Linux) kullanıcı adı — MVP: boş
};

std::vector<ProcInfo> list_processes();

} // namespace gt
