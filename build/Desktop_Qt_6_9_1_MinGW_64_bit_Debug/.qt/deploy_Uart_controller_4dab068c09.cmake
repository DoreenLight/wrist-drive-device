include("E:/0-Motor/9_wanguanjie/Uart_controller/build/Desktop_Qt_6_9_1_MinGW_64_bit_Debug/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Uart_controller-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase;qtserialport")

qt6_deploy_runtime_dependencies(
    EXECUTABLE E:/0-Motor/9_wanguanjie/Uart_controller/build/Desktop_Qt_6_9_1_MinGW_64_bit_Debug/Uart_controller.exe
    GENERATE_QT_CONF
)
