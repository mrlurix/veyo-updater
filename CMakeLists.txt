cmake_minimum_required(VERSION 3.16)
project(VeyoUpdater VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# جلوگیری از بیلد سنگین
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# اگر Qt پیدا نشد راهنما نمایش بده
if(NOT Qt6_FOUND)
    message(FATAL_ERROR "Qt6 not found! Install Qt 6.5+ from https://www.qt.io/download or via: pip install aqtinstall  &  aqt install-qt windows desktop 6.7.0 win64_msvc2019_64")
endif()

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/WingetManager.cpp
)

set(HEADERS
    src/MainWindow.h
    src/WingetManager.h
)

# ریسورس‌ها (Qt)
qt_add_resources(RESOURCES resources/resources.qrc)

# آیکون + VersionInfo + Manifest ویندوز (برای پرتابل تک‌فایل)
set(WIN_RESOURCES "")
if(WIN32)
    set(WIN_RESOURCES resources/app.rc)
endif()

add_executable(${PROJECT_NAME} WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
    ${WIN_RESOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

# بهینه‌سازی برای حجم کم + پرتابل
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /O2 /GL /EHsc)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "/LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO"
        INTERPROCEDURAL_OPTIMIZATION TRUE
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
    # حذف کنسول اضافی و کاهش حجم
    target_link_options(${PROJECT_NAME} PRIVATE "/MANIFEST:NO")
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O2 -s -ffunction-sections -fdata-sections)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--gc-sections -s)
endif()

# خروجی در پوشه dist برای پرتابل
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/dist"
)

# نصب
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# windeployqt بعد از بیلد (اگر در PATH باشد)
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --no-system-d3d-compiler --no-opengl-sw "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "Running windeployqt..."
        )
    endif()
endif()

message(STATUS "VeyoUpdater ${PROJECT_VERSION} configured | Qt ${Qt6_VERSION}")
