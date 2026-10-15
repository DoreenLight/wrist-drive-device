cmake_minimum_required(VERSION 3.19)
project(Uart_controller LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets SerialPort Charts)

qt_standard_project_setup()

qt_add_executable(Uart_controller
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    protocolworker.cpp
    protocolworker.h
    floatiddefs.h
)

target_link_libraries(Uart_controller
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::SerialPort
        Qt::Charts
)

install(TARGETS Uart_controller
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Uart_controller
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
