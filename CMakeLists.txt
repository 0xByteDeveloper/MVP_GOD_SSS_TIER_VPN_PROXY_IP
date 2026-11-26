cmake_minimum_required(VERSION 3.16)
project(MVP_GOD_SSS_TIER VERSION 2.0.0 LANGUAGES C CXX)

# ============================================================================
#  MVP_GOD_SSS_TIER — ücretsiz ip/proxy/vpn sistemi (C++ motor + WebGUI)
#    src/       : motor (Hola trial istemcisi, Tor/zincir, VPN Gate, SOCKS5
#                 router, PID izleyici + OTO-YAKALA, gömülü HTML sunucusu)
#    web/       : statik WebGUI (vanilla JS — derleme gerektirmez; exe'ye
#                 src/web_embedded.h üzerinden gömülür)
#    hook/      : süreç kancası (Linux LD_PRELOAD / Windows DLL)
#  Linux   : ./build.sh (veya cmake)  — OpenSSL gerekir (yalnız test derlemesi)
#  Windows : build.bat (MSVC) — SıFIR dış bağımlılık (WinHTTP+SChannel);
#            ya da zig ile çapraz derleme: scripts/build_win.sh
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if (NOT MSVC)
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-result)
endif()

find_package(Threads REQUIRED)

set(MOTOR_SOURCES
    src/main.cpp
    src/core/util.cpp
    src/core/log.cpp
    src/net/net.cpp
    src/net/httpclient.cpp
    src/net/tls.cpp
    src/providers/hola.cpp
    src/providers/more.cpp
    src/engine/router.cpp
    src/engine/instance.cpp
    src/engine/proclist.cpp
    src/engine/monitor.cpp
    src/httpserver.cpp
    src/api.cpp
)

if (UNIX)
    find_package(OpenSSL REQUIRED)
    add_executable(mvp_god_sss_tier ${MOTOR_SOURCES})
    target_include_directories(mvp_god_sss_tier PRIVATE src)
    target_link_libraries(mvp_god_sss_tier PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)

    # LD_PRELOAD kancası
    add_library(godtier_hook SHARED hook/hook_linux.c)
    set_target_properties(godtier_hook PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/hook
        OUTPUT_NAME "godtier_hook")
    target_link_libraries(godtier_hook PRIVATE ${CMAKE_DL_LIBS})
else()
    # ---------------- Windows (MSVC) ----------------
    add_executable(MVP_GOD_SSS_TIER ${MOTOR_SOURCES})
    target_include_directories(MVP_GOD_SSS_TIER PRIVATE src)
    target_link_libraries(MVP_GOD_SSS_TIER PRIVATE ws2_32 winhttp secur32 crypt32 iphlpapi advapi32)
    # kancayı ayrı DLL olarak derle + exe ile aynı dizine koy
    add_library(godtier_hook SHARED hook/hook_windows.cpp)
    target_link_libraries(godtier_hook PRIVATE ws2_32 user32)
    add_custom_command(TARGET godtier_hook POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:godtier_hook> ${CMAKE_BINARY_DIR}/bin/godtier_hook.dll)
endif()
