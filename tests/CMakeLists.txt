# 测试模块 CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

# 启用测试
enable_testing()

# 启用 Qt 自动化工具
set(CMAKE_AUTOMOC ON)

# 查找 Qt6 Test 模块
find_package(Qt6 REQUIRED COMPONENTS Test Core)

# 测试源文件
set(TEST_SOURCES
    main.cpp
    unit/TestChecksumUtils.cpp
    unit/TestConversionUtils.cpp
    unit/TestEncodingUtils.cpp
    unit/TestPlotRenderQuality.cpp
    unit/TestTranslationCompleteness.cpp
)

# 创建测试可执行文件
add_executable(ComAssistant_tests
    ${TEST_SOURCES}
)

# 包含目录
target_include_directories(ComAssistant_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(ComAssistant_tests PRIVATE
    COMASSISTANT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
)

# 链接库
target_link_libraries(ComAssistant_tests PRIVATE
    Qt6::Test
    Qt6::Core
)

# 添加被测试的源文件（工具类）
target_sources(ComAssistant_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/utils/ChecksumUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/utils/ConversionUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/utils/EncodingUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/PlotRenderQuality.cpp
)

# 注册测试
add_test(NAME ComAssistant_tests COMMAND ComAssistant_tests)

# Windows 下部署测试运行时（Qt DLL）
if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

    add_custom_command(TARGET ComAssistant_tests POST_BUILD
        COMMAND ${QT_BIN_DIR}/windeployqt.exe
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
