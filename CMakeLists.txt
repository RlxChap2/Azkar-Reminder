cmake_minimum_required(VERSION 3.20)
project(DhikrReminder VERSION 1.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Windows subsystem - GUI app, no console window
if(MSVC)
    add_compile_options(/W3 /MP /utf-8 /permissive-)
    add_compile_definitions(
        _UNICODE
        UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
    )
endif()

# ─── Third-party: SQLite3 ──────────────────────────────────────────────────
add_library(sqlite3 STATIC
    third_party/sqlite3.c
)
target_include_directories(sqlite3 PUBLIC third_party)
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
endif()

# ─── Source files ─────────────────────────────────────────────────────────
set(CORE_SOURCES
    src/core/Logger.cpp
    src/core/Settings.cpp
    src/core/Database.cpp
    src/core/ContentEngine.cpp
    src/core/Scheduler.cpp
    src/core/Localization.cpp
    src/core/Application.cpp
    src/core/FullScreenDetector.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/PopupWindow.cpp
    src/ui/TrayIcon.cpp
    src/ui/OnboardingFlow.cpp
    src/ui/core/WindowChrome.cpp
    src/ui/core/LogoRenderer.cpp
    src/ui/core/ResourceLoader.cpp
    src/ui/core/UIState.cpp
    src/ui/components/TimePickerPopup.cpp
    src/ui/pages/HomePage.cpp
    src/ui/pages/RemindersPage.cpp
    src/ui/pages/SchedulePage.cpp
    src/ui/pages/LibraryPage.cpp
    src/ui/pages/StatisticsPage.cpp
    src/ui/pages/SettingsPage.cpp
    src/ui/pages/AboutPage.cpp
)

set(AUDIO_SOURCES
    src/audio/AudioEngine.cpp
)


set(UPDATE_SOURCES
    src/update/Updater.cpp
)

set(ALL_SOURCES
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${AUDIO_SOURCES}
    ${UPDATE_SOURCES}
    DhikrReminder.rc
)

# ─── Executable ───────────────────────────────────────────────────────────
add_executable(AzkarReminder WIN32 ${ALL_SOURCES})

target_include_directories(AzkarReminder PRIVATE
    src
    third_party
)

# ─── Link libraries ───────────────────────────────────────────────────────
target_link_libraries(AzkarReminder PRIVATE
    sqlite3
    d2d1
    dwrite
    shell32
    user32
    gdi32
    ole32
    oleaut32
    comctl32
    comdlg32
    shlwapi
    xaudio2
    dwmapi
    mfplat
    mfreadwrite
    mfuuid
    uxtheme
    version
)

# ─── Linker settings ──────────────────────────────────────────────────────
if(MSVC)
    set_target_properties(AzkarReminder PROPERTIES
        VS_DPI_AWARE "PerMonitor"
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup"
    )
endif()

# ─── Output ───────────────────────────────────────────────────────────────
set_target_properties(AzkarReminder PROPERTIES
    OUTPUT_NAME "AzkarReminder"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ─── Copy audio to output ─────────────────────────────────────────────────
add_custom_command(TARGET AzkarReminder POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/audio"
        "$<TARGET_FILE_DIR:AzkarReminder>/audio"
    COMMENT "Copying audio assets to output directory"
)

# ─── Install rules ────────────────────────────────────────────────────────
install(TARGETS AzkarReminder
    RUNTIME DESTINATION bin
)
install(DIRECTORY assets/audio
    DESTINATION bin
)
