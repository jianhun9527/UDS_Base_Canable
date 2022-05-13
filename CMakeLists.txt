cmake_minimum_required(VERSION 3.0.0)

project(UDS_DEMO_CODE)

# set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_BUILD_TYPE "Release")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDBG_RX_STREAM -DDBG_TX_STREAM")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_ENABLE_LOG_FUNC_ -D_OS_SYSTEM_=_WIN_SYS2_")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -O3")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -D_ENABLE_LOG_FUNC_ -D_OS_SYSTEM_=_WIN_SYS2_")

set(ICON_FILE ./images/logo.rc)

include_directories(./library ./source/inc ./source/component/iniparser)
link_directories(./library)

aux_source_directory(./source SRC1_LIST)
aux_source_directory(./source/src SRC2_LIST)
aux_source_directory(./source/component/iniparser SRC3_LIST)
SET(SRC_LIST ${SRC1_LIST} ${SRC2_LIST} ${SRC3_LIST})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/debug)
add_executable(CANbootloader ${SRC_LIST} ${ICON_FILE})
target_link_libraries(CANbootloader cando.dll)
