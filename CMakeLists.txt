cmake_minimum_required(VERSION 3.14)

# 项目信息
project(ComAssistant
    VERSION 1.3.3
    DESCRIPTION "串口调试助手 - Serial Port Debug Assistant"
    LANGUAGES CXX
)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 单配置生成器默认使用 Release，避免 MinGW/Ninja 未设置 CMAKE_BUILD_TYPE
# 时，windeployqt 和编译产物配置不一致。
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Qt 自动化工具
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Windows 特定设置
if(WIN32)
    # 使用 UTF-8 编码（兼容 MSVC 和 MinGW）
    if(MSVC)
        add_compile_options(/utf-8)
    else()
        add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
    # 定义 Windows 版本
    add_definitions(-D_WIN32_WINNT=0x0A00)  # Windows 10
endif()

# QCustomPlot OpenGL 支持
add_definitions(-DQCUSTOMPLOT_USE_OPENGL)

# 查找 Qt5 包
find_package(Qt5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    SerialPort
    Network
    OpenGL
    PrintSupport
    LinguistTools
)

# 第三方库：优先使用 vcpkg，否则通过 FetchContent 自动下载
include(FetchContent)

# spdlog（header-only 日志库）
find_package(spdlog CONFIG QUIET)
if(NOT spdlog_FOUND)
    message(STATUS "spdlog not found via find_package, fetching via FetchContent...")
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.12.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# nlohmann-json（header-only JSON 库）
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found via find_package, fetching via FetchContent...")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Lua（脚本引擎）
find_package(Lua QUIET)
if(NOT LUA_FOUND)
    message(STATUS "Lua not found via find_package, fetching via FetchContent...")
    FetchContent_Declare(
        lua
        GIT_REPOSITORY https://github.com/walterschell/Lua.git
        GIT_TAG        v5.4.6
    )
    # 该 Lua CMake 包默认注册 lua-testsuite，但作为依赖下载时测试数据目录名
    # 会误用主工程版本号，导致 ctest 出现不存在的 lua-<项目版本>-tests。
    # 主工程只需要 lua_static，因此在引入前关闭第三方测试。
    set(LUA_ENABLE_TESTING OFF CACHE BOOL "Disable bundled Lua tests" FORCE)
    FetchContent_MakeAvailable(lua)
    set(LUA_INCLUDE_DIR "${lua_SOURCE_DIR}/include")
    set(LUA_LIBRARIES lua_static)
endif()

# 第三方库源文件 (QCustomPlot)
set(THIRD_PARTY_SOURCES
    src/third_party/qcustomplot/qcustomplot.cpp
)

set(THIRD_PARTY_HEADERS
    src/third_party/qcustomplot/qcustomplot.h
)

# 源文件
set(CORE_SOURCES
    src/core/utils/Logger.cpp
    src/core/config/ConfigManager.cpp
    src/core/config/DisplaySettingsPolicy.cpp
    src/core/utils/CrashHandler.cpp
    src/core/utils/AutoSaveManager.cpp
    src/core/utils/FFTUtils.cpp
    src/core/utils/FilterUtils.cpp
    src/core/utils/ChecksumUtils.cpp
    src/core/utils/ConversionUtils.cpp
    src/core/utils/EncodingUtils.cpp
    src/core/utils/MemoryUtils.cpp
    src/core/script/LuaEngine.cpp
    src/core/theme/ThemeManager.cpp
    src/core/session/SessionSerializer.cpp
    src/core/session/SessionManager.cpp
    src/core/export/DataExporter.cpp
    src/core/macro/MacroRecorder.cpp
    src/core/transfer/FileTransfer.cpp
    src/core/upgrade/IAPUpgrader.cpp
    src/core/upgrade/AppUpdateChecker.cpp
)

set(CORE_HEADERS
    src/version.h
    src/core/utils/Logger.h
    src/core/config/ConfigManager.h
    src/core/config/DisplaySettingsPolicy.h
    src/core/config/AppConfig.h
    src/core/utils/CrashHandler.h
    src/core/utils/AutoSaveManager.h
    src/core/utils/FFTUtils.h
    src/core/utils/FilterUtils.h
    src/core/utils/ChecksumUtils.h
    src/core/utils/ConversionUtils.h
    src/core/utils/EncodingUtils.h
    src/core/utils/MemoryUtils.h
    src/core/script/LuaEngine.h
    src/core/theme/ThemeManager.h
    src/core/session/SessionData.h
    src/core/session/SessionSerializer.h
    src/core/session/SessionManager.h
    src/core/export/DataExporter.h
    src/core/macro/MacroRecorder.h
    src/core/transfer/FileTransfer.h
    src/core/upgrade/IAPUpgrader.h
    src/core/upgrade/AppUpdateChecker.h
)

# 通信模块源文件
set(COMMUNICATION_SOURCES
    src/core/communication/SerialPort.cpp
    src/core/communication/TcpClient.cpp
    src/core/communication/TcpServer.cpp
    src/core/communication/UdpSocket.cpp
    src/core/communication/CommunicationFactory.cpp
    src/core/communication/SerialPortMonitor.cpp
    src/core/communication/MultiPortManager.cpp
)

set(COMMUNICATION_HEADERS
    src/core/communication/ICommunication.h
    src/core/communication/SerialPort.h
    src/core/communication/TcpClient.h
    src/core/communication/TcpServer.h
    src/core/communication/UdpSocket.h
    src/core/communication/CommunicationFactory.h
    src/core/communication/SerialPortMonitor.h
    src/core/communication/MultiPortManager.h
)

# 协议模块源文件
set(PROTOCOL_SOURCES
    src/core/protocol/FrameParser.cpp
    src/core/protocol/AsciiProtocol.cpp
    src/core/protocol/HexProtocol.cpp
    src/core/protocol/ModbusProtocol.cpp
    src/core/protocol/CustomProtocol.cpp
    src/core/protocol/TextProtocol.cpp
    src/core/protocol/StampProtocol.cpp
    src/core/protocol/CsvProtocol.cpp
    src/core/protocol/ProtocolFactory.cpp
    src/core/protocol/EasyHexProtocol.cpp
    src/core/protocol/JustFloatProtocol.cpp
    src/core/protocol/PlotProtocolDetector.cpp
)

set(PROTOCOL_HEADERS
    src/core/protocol/IProtocol.h
    src/core/protocol/FrameParser.h
    src/core/protocol/AsciiProtocol.h
    src/core/protocol/HexProtocol.h
    src/core/protocol/ModbusProtocol.h
    src/core/protocol/CustomProtocol.h
    src/core/protocol/TextProtocol.h
    src/core/protocol/StampProtocol.h
    src/core/protocol/CsvProtocol.h
    src/core/protocol/ProtocolFactory.h
    src/core/protocol/EasyHexProtocol.h
    src/core/protocol/JustFloatProtocol.h
    src/core/protocol/PlotProtocolDetector.h
)

# UI模块源文件
set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/HelpDialog.cpp
    src/ui/PlotRenderQuality.cpp
    src/ui/PlotterWindow.cpp
    src/ui/PlotterManager.cpp
    src/ui/SpectrumWindow.cpp
    src/ui/RealTimeFFTWindow.cpp
    src/ui/dialogs/ToolboxDialog.cpp
    src/ui/dialogs/ScriptEditorDialog.cpp
    src/ui/dialogs/ExportDialog.cpp
    src/ui/dialogs/RecoveryDialog.cpp
    src/ui/dialogs/SettingsDialog.cpp
    src/ui/dialogs/FFTSettingsDialog.cpp
    src/ui/dialogs/FileTransferDialog.cpp
    src/ui/dialogs/DataWindowConfigDialog.cpp
    src/ui/syntax/LuaSyntaxHighlighter.cpp
    src/ui/widgets/SerialSettingsWidget.cpp
    src/ui/widgets/NetworkSettingsWidget.cpp
    src/ui/widgets/SendWidget.cpp
    src/ui/widgets/TabbedReceiveWidget.cpp
    src/ui/widgets/DataStatistics.cpp
    src/ui/widgets/QuickSendWidget.cpp
    src/ui/widgets/WaterfallWidget.cpp
    src/ui/widgets/SearchWidget.cpp
    src/ui/widgets/MultiGroupSendWidget.cpp
    src/ui/widgets/ModbusAnalyzerWidget.cpp
    src/ui/widgets/MacroWidget.cpp
    src/ui/widgets/MultiPortWidget.cpp
    src/ui/widgets/DataWindowManager.cpp
    src/ui/widgets/DataWindow.cpp
    src/ui/widgets/DataTableWidget.cpp
    src/ui/widgets/SideNavigationBar.cpp
    src/ui/widgets/ThemeButton.cpp
    src/ui/widgets/PlotControlPanel.cpp
    src/ui/modes/SerialModeWidget.cpp
    src/ui/modes/TerminalModeWidget.cpp
    src/ui/modes/TerminalBuffer.cpp
    src/ui/modes/AnsiParser.cpp
    src/ui/modes/FrameModeWidget.cpp
    src/ui/modes/DebugModeWidget.cpp
    src/ui/controls/ControlPanel.cpp
    src/ui/controls/controls/SliderControl.cpp
    src/ui/controls/controls/Attitude3DControl.cpp
)

set(UI_HEADERS
    src/ui/MainWindow.h
    src/ui/HelpDialog.h
    src/ui/PlotRenderQuality.h
    src/ui/PlotterWindow.h
    src/ui/PlotterManager.h
    src/ui/SpectrumWindow.h
    src/ui/RealTimeFFTWindow.h
    src/ui/dialogs/ToolboxDialog.h
    src/ui/dialogs/ScriptEditorDialog.h
    src/ui/dialogs/ExportDialog.h
    src/ui/dialogs/RecoveryDialog.h
    src/ui/dialogs/SettingsDialog.h
    src/ui/dialogs/FFTSettingsDialog.h
    src/ui/dialogs/FileTransferDialog.h
    src/ui/dialogs/DataWindowConfigDialog.h
    src/ui/syntax/LuaSyntaxHighlighter.h
    src/ui/widgets/SerialSettingsWidget.h
    src/ui/widgets/NetworkSettingsWidget.h
    src/ui/widgets/SendWidget.h
    src/ui/widgets/TabbedReceiveWidget.h
    src/ui/widgets/DataStatistics.h
    src/ui/widgets/QuickSendWidget.h
    src/ui/widgets/WaterfallWidget.h
    src/ui/widgets/SearchWidget.h
    src/ui/widgets/MultiGroupSendWidget.h
    src/ui/widgets/ModbusAnalyzerWidget.h
    src/ui/widgets/MacroWidget.h
    src/ui/widgets/MultiPortWidget.h
    src/ui/widgets/DataWindowManager.h
    src/ui/widgets/DataWindow.h
    src/ui/widgets/DataTableWidget.h
    src/ui/widgets/SideNavigationBar.h
    src/ui/widgets/PlotControlPanel.h
    src/ui/modes/IModeWidget.h
    src/ui/modes/SerialModeWidget.h
    src/ui/modes/TerminalModeWidget.h
    src/ui/modes/TerminalBuffer.h
    src/ui/modes/AnsiParser.h
    src/ui/modes/FrameModeWidget.h
    src/ui/modes/DebugModeWidget.h
    src/ui/controls/IInteractiveControl.h
    src/ui/controls/ControlPanel.h
    src/ui/controls/controls/SliderControl.h
    src/ui/controls/controls/Attitude3DControl.h
)

# 主程序源文件
set(MAIN_SOURCES
    src/main.cpp
)

# 资源文件
set(RESOURCES
    resources/app.qrc
    resources/help/help.qrc
)

# 翻译文件
set(TS_FILES
    resources/translations/en_US.ts
)

# 创建可执行文件
add_executable(${PROJECT_NAME} WIN32
    ${MAIN_SOURCES}
    ${CORE_SOURCES}
    ${CORE_HEADERS}
    ${COMMUNICATION_SOURCES}
    ${COMMUNICATION_HEADERS}
    ${PROTOCOL_SOURCES}
    ${PROTOCOL_HEADERS}
    ${UI_SOURCES}
    ${UI_HEADERS}
    ${THIRD_PARTY_SOURCES}
    ${THIRD_PARTY_HEADERS}
    ${RESOURCES}
)

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party
    ${LUA_INCLUDE_DIR}
)

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::SerialPort
    Qt5::Network
    Qt5::OpenGL
    Qt5::PrintSupport
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${LUA_LIBRARIES}
)

# Windows 特定链接
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        dbghelp    # 用于崩溃转储
        opengl32   # OpenGL 库
        psapi      # 工作集回收
    )
endif()

# 编译翻译文件 (.ts -> .qm) - Qt5 使用 qt5_add_translation。
qt5_add_translation(QM_FILES ${TS_FILES})

# 计算运行时翻译目录。
# 单配置生成器（MinGW Makefiles/Ninja）默认把 ComAssistant.exe 放在构建根目录；
# 多配置生成器（Visual Studio/Ninja Multi-Config）默认放在构建根目录下的配置目录。
# 这里不用 $<TARGET_FILE_DIR:${PROJECT_NAME}>，避免翻译目标和主程序目标形成 CMake 依赖环。
if(CMAKE_CONFIGURATION_TYPES)
    set(COMASSISTANT_RUNTIME_TRANSLATION_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/translations")
else()
    set(COMASSISTANT_RUNTIME_TRANSLATION_DIR "${CMAKE_CURRENT_BINARY_DIR}/translations")
endif()

# 翻译文件必须独立成为构建目标，不能只挂在主程序 PRE_BUILD/POST_BUILD 上。
# 原因：只修改 .ts 时主程序可能不会重新链接，导致 PRE_BUILD/POST_BUILD 不执行，
# 运行时仍加载旧的 translations/en_US.qm，语言切换后菜单文本会继续显示中文。
add_custom_target(${PROJECT_NAME}_translations ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${COMASSISTANT_RUNTIME_TRANSLATION_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QM_FILES} "${COMASSISTANT_RUNTIME_TRANSLATION_DIR}/"
    DEPENDS ${QM_FILES}
    COMMENT "Refreshing and copying translation files..."
    VERBATIM
)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_translations)

# 安装规则
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# 复制 Qt DLL (仅 Windows Debug/Release)
if(WIN32)
    # 获取 Qt bin 目录
    get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

    # 部署 Qt 运行时
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${QT_BIN_DIR}/windeployqt.exe
            $<$<CONFIG:Debug>:--debug>
            $<$<NOT:$<CONFIG:Debug>>:--release>
            --no-translations
            --no-system-d3d-compiler
            --no-opengl-sw
            "$<TARGET_FILE:${PROJECT_NAME}>"
        COMMENT "Deploying Qt runtime..."
    )
endif()

# 输出配置信息
message(STATUS "=== ComAssistant Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Qt Version: ${Qt5_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# 添加测试子目录（默认开启，确保配置和基础工具类问题在本地构建阶段暴露）
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
