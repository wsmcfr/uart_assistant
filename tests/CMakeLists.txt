# 测试模块 CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

# 启用测试
enable_testing()

# 启用 Qt 自动化工具
set(CMAKE_AUTOMOC ON)

# 查找 Qt5 Test 模块
# 主工程固定使用 Qt 5.12.9，测试工程必须复用同一套 Qt 版本，避免
# 在启用 BUILD_TESTS 后同时链接 Qt5/Qt6 导致配置失败或运行时 DLL 混用。
find_package(Qt5 REQUIRED COMPONENTS Test Core Gui Widgets OpenGL PrintSupport Network SerialPort)

# 测试源文件
set(TEST_SOURCES
    main.cpp
    unit/TestChecksumUtils.cpp
    unit/TestConversionUtils.cpp
    unit/TestEncodingUtils.cpp
    unit/TestAnsiParser.cpp
    unit/TestFFTUtils.cpp
    unit/TestFilterUtils.cpp
    unit/TestDisplaySettingsPolicy.cpp
    unit/TestMemoryUtils.cpp
    unit/TestMemoryAwareUiBehavior.cpp
    unit/TestMacroRecorderMemoryPolicy.cpp
    unit/TestPlotterDataPolicy.cpp
    unit/TestPlotRenderQuality.cpp
    unit/TestPlotterRenderCoordinator.cpp
    unit/TestPlotterAnalysisService.cpp
    unit/TestPlotterStatisticsCalculator.cpp
    unit/TestPlotterManagerLifecycle.cpp
    unit/TestTabbedReceiveWidget.cpp
    unit/TestTerminalModeWidget.cpp
    unit/TestTranslationCompleteness.cpp
    unit/TestHidCommunication.cpp
    unit/TestCommunicationWorkspaces.cpp
    unit/TestReleaseMetadata.cpp
    unit/TestThemeCoverage.cpp
    unit/TestSettingsDialogLayout.cpp
    unit/TestToolboxDialogLayout.cpp
    unit/TestFileTransfer.cpp
    unit/TestDocumentationLinks.cpp
    unit/TestSendQueue.cpp
    unit/TestLuaSandbox.cpp
    unit/TestScriptEditorDialog.cpp
    unit/TestLuaScriptProtocol.cpp
    unit/TestProtocolConfigSchema.cpp
    unit/TestProtocolConfigEditor.cpp
    unit/TestProtocolDiagnostics.cpp
    unit/TestProtocolRegistry.cpp
    unit/TestMainWindowCommunicationController.cpp
    unit/TestMainWindowCommunicationWorkspaceCoordinator.cpp
    unit/TestMainWindowProtocolState.cpp
    unit/TestMainWindowPlotDataRouter.cpp
    unit/TestMainWindowSessionCoordinator.cpp
    unit/TestMainWindowLazyLoading.cpp
)

# 创建测试可执行文件
add_executable(ComAssistant_tests
    ${TEST_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/ICommunication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/HidDevice.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/HidWorker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/HidReportCodec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/SendQueue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/SendDispatcher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/script/LuaSandbox.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/LuaScriptProtocol.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/CommunicationFactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/SerialPort.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/TcpClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/TcpServer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/UdpSocket.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/IProtocol.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolConfigSchema.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolDiagnostics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolDescriptor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/transfer/FileTransfer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/modes/IModeWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/modes/TerminalModeWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/modes/SerialModeWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/modes/FrameModeWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/modes/DebugModeWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/CommunicationWorkspaceWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/ProtocolConfigEditor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/TcpClientWorkspaceWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/TcpServerWorkspaceWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/UdpWorkspaceWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/HidReportWorkspaceWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ProtocolConfigDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ProtocolDiagnosticsDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ScriptEditorDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ScriptExecutionWorker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/syntax/LuaSyntaxHighlighter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MainWindowCommunicationController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MainWindowCommunicationWorkspaceCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MainWindowProtocolState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MainWindowPlotDataRouter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MainWindowSessionCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MainWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/PlotterRenderCoordinator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/PlotterAnalysisService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/controls/IInteractiveControl.h
)

# 包含目录
target_include_directories(ComAssistant_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${spdlog_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/include
    ${LUA_INCLUDE_DIR}
)

target_compile_definitions(ComAssistant_tests PRIVATE
    COMASSISTANT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
)

# 链接库
target_link_libraries(ComAssistant_tests PRIVATE
    Qt5::Test
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::OpenGL
    Qt5::PrintSupport
    Qt5::Network
    Qt5::SerialPort
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${LUA_LIBRARIES}
)

if(WIN32)
    target_link_libraries(ComAssistant_tests PRIVATE
        opengl32
        psapi
        dbghelp
    )
endif()

# 添加被测试的源文件（工具类）
target_sources(ComAssistant_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/utils/ChecksumUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/utils/ConversionUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/utils/EncodingUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/utils/CrashHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/utils/AutoSaveManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/config/DisplaySettingsPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/utils/MemoryUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/utils/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/utils/FilterUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/theme/ThemeManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/script/LuaSandbox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/script/LuaEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/export/DataExporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/macro/MacroRecorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/PlotterDataPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/PlotRenderQuality.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/PlotterRenderCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/PlotterAnalysisService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/PlotterStatisticsCalculator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/PlotterManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/PlotterWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/SpectrumWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/RealTimeFFTWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/FFTSettingsDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/PlotControlPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/WaterfallWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/third_party/qcustomplot/qcustomplot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/modes/AnsiParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/modes/TerminalBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/modes/TerminalModeWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/modes/SerialModeWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/modes/FrameModeWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/modes/DebugModeWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/TabbedReceiveWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/CommunicationWorkspaceWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/ProtocolConfigEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/TcpClientWorkspaceWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/TcpServerWorkspaceWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/UdpWorkspaceWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/HidReportWorkspaceWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/SerialSettingsWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/NetworkSettingsWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/QuickSendWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/SettingsDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ToolboxDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ExportDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/RecoveryDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/FileTransferDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/DataWindowConfigDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ProtocolConfigDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ProtocolDiagnosticsDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ScriptEditorDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ScriptExecutionWorker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/syntax/LuaSyntaxHighlighter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MainWindowCommunicationController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MainWindowCommunicationWorkspaceCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MainWindowProtocolState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MainWindowPlotDataRouter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MainWindowSessionCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/HelpDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/MainWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/utils/FFTUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/config/ConfigManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/session/SessionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/session/SessionSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/HidDevice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/HidWorker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/HidReportCodec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/SendQueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/SendDispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/CommunicationFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/SerialPort.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/SerialPortMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/TcpClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/TcpServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/UdpSocket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/communication/MultiPortManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/FrameParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/AsciiProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/HexProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ModbusProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/CustomProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/EasyHexProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/TextProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/StampProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/CsvProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/JustFloatProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/LuaScriptProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolConfigSchema.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolDiagnostics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/PlotProtocolDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/transfer/FileTransfer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/upgrade/IAPUpgrader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/upgrade/AppUpdateChecker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/SendWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/DataStatistics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/SearchWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/MultiGroupSendWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/ModbusAnalyzerWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/MacroWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/MultiPortWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/DataWindowManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/DataWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/DataTableWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/SideNavigationBar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/widgets/ThemeButton.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/controls/ControlPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/controls/controls/SliderControl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/controls/controls/Attitude3DControl.cpp
)

if(COMASSISTANT_HIDAPI_TARGET)
    target_compile_definitions(ComAssistant_tests PRIVATE COMASSISTANT_ENABLE_HIDAPI)
    target_link_libraries(ComAssistant_tests PRIVATE ${COMASSISTANT_HIDAPI_TARGET})
endif()

# 注册测试
add_test(NAME ComAssistant_tests COMMAND ComAssistant_tests)

# Windows 下部署测试运行时（Qt DLL）
if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

    add_custom_command(TARGET ComAssistant_tests POST_BUILD
        COMMAND ${QT_BIN_DIR}/windeployqt.exe
            $<$<CONFIG:Debug>:--debug>
            $<$<NOT:$<CONFIG:Debug>>:--release>
            --no-translations
            --no-system-d3d-compiler
            --no-opengl-sw
            "$<TARGET_FILE:ComAssistant_tests>"
        COMMENT "Deploying Qt runtime for tests..."
    )
endif()

# 设置测试属性
set_tests_properties(ComAssistant_tests PROPERTIES
    TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:ComAssistant>;$ENV{PATH}"
)

# 输出信息
message(STATUS "=== Test Configuration ===")
message(STATUS "Test executable: ComAssistant_tests")
