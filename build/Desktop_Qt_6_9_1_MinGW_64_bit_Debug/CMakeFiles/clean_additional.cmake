# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\Uart_controller_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\Uart_controller_autogen.dir\\ParseCache.txt"
  "Uart_controller_autogen"
  )
endif()
