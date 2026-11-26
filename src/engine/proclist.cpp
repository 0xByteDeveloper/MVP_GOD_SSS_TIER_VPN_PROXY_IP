// ============================================================================
//  GOD_SSS_TIER_VPN — src/engine/proclist.cpp
// ============================================================================
#include "engine/proclist.h"
#include <cstdio>
#include <cstring>

#ifndef _WIN32
#  include <dirent.h>
#  include <sys/types.h>

namespace gt {

std::vector<ProcInfo> list_processes() {
    std::vector<ProcInfo> out;
    DIR* d = opendir("/proc");
    if (!d) return out;
    struct dirent* de;
    while ((de = readdir(d)) != nullptr) {
        // yalnız sayısal girdiler = PID'ler
        if (!de->d_name[0] || de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        long pid = atol(de->d_name);
        if (pid <= 0) continue;
        ProcInfo p;
        p.pid = pid;
        // isim: /proc/<pid>/stat'ın parantez içindeki alanı (boşluklu isimler)
        char path[64];
        snprintf(path, sizeof path, "/proc/%ld/stat", pid);
        FILE* f = fopen(path, "r");
        if (f) {
            char line[512];
            if (fgets(line, sizeof line, f)) {
                char* p1 = strchr(line, '(');
                char* p2 = strrchr(line, ')');
                if (p1 && p2 && p2 > p1) {
                    size_t n = (size_t)(p2 - p1 - 1);
                    if (n >= sizeof line) n = 15;
                    char nm[64];
                    if (n > sizeof nm - 1) n = sizeof nm - 1;
                    memcpy(nm, p1 + 1, n);
                    nm[n] = 0;
                    p.name = nm;
                }
            }
            fclose(f);
        }
        // komut satırı
        snprintf(path, sizeof path, "/proc/%ld/cmdline", pid);
        f = fopen(path, "r");
        if (f) {
            char cmdbuf[1024];
            size_t n = fread(cmdbuf, 1, sizeof cmdbuf - 1, f);
            fclose(f);
            cmdbuf[n] = 0;
            for (size_t k = 0; k < n; k++) if (cmdbuf[k] == 0) cmdbuf[k] = ' ';
            p.cmdline = cmdbuf;
            // cmdline isimden daha bilgilendiriciyse onu kullan
            if (!p.cmdline.empty() && p.name == "python3" && p.cmdline.size() > 8) {}
        }
        if (p.name.empty()) p.name = "pid" + std::to_string(pid);
        out.push_back(p);
    }
    closedir(d);
    return out;
}

} // namespace gt

#else // ------------------------------------------------------------------ Win32
#  include <windows.h>
#  include <tlhelp32.h>

namespace gt {

std::vector<ProcInfo> list_processes() {
    std::vector<ProcInfo> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof pe;
    if (Process32FirstW(snap, &pe)) {
        do {
            ProcInfo p;
            p.pid = (long)pe.th32ProcessID;
            char nm[260];
            int k = 0;
            for (; pe.szExeFile[k] && k < 259; k++) {
                wchar_t w = pe.szExeFile[k];
                nm[k] = (w < 128) ? (char)w : '?';
            }
            nm[k] = 0;
            p.name = nm;
            out.push_back(p);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return out;
}

} // namespace gt
#endif
