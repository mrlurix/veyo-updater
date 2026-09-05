cmake_minimum_required(VERSION 3.16)
project(VeyoUpdater VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# فقط Win32 API — بدون Qt، بدون وابستگی خارجی
# خروجی یک فایل exe پرتابل ~400KB

add_executable(VeyoUpdater WIN32
    src/main.cpp
    ../resources/app.rc
)

target_link_libraries(VeyoUpdater PRIVATE
    comctl32
    dwmapi
    uxtheme
    advapi32
    shell32
)

# بهینه‌سازی حجم
if(MSVC)
    # /MT = static CRT: zero prerequisites on any Windows (no VC++ Redist needed)
    target_compile_options(VeyoUpdater PRIVATE /utf-8 /MT /O2 /GL /EHsc)
    set_target_properties(VeyoUpdater PROPERTIES
        LINK_FLAGS "/LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO /SUBSYSTEM:WINDOWS"
        INTERPROCEDURAL_OPTIMIZATION TRUE
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
    )
    # حذف کنسول و کم کردن حجم
    target_link_options(VeyoUpdater PRIVATE "/MANIFEST:NO")
else()
    # MinGW
    target_compile_options(VeyoUpdater PRIVATE -O2 -s -ffunction-sections -fdata-sections -municode -mwindows)
    target_link_options(VeyoUpdater PRIVATE -Wl,--gc-sections -s -municode -mwindows)
    set_target_properties(VeyoUpdater PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
    )
endif()

message(STATUS "VeyoUpdater — Ultra-light native (Win32 only, single EXE ~400KB)")
