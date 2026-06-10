cmake_minimum_required(VERSION 3.10)
project(OSCoreSimulator
    VERSION 1.0.0
    DESCRIPTION "可持久化的操作系统核心模拟器 — 操作系统A课程设计"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── MSVC / MinGW specifics ──
if(MSVC)
    add_compile_options(/W4 /utf-8)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
elseif(MINGW)
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# ── Collect sources ──
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# ── Output directory ──
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_SOURCE_DIR}/bin)
endforeach()

# ── Main executable ──
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# ── Threads library (pthread for MinGW, built-in for MSVC) ──
find_package(Threads QUIET)
if(Threads_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()

# ── Install ──
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
