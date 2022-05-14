cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)

# This only needs macos 15 due to std::filesystem, not clap
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "Minimum macOS version")

project(micro-clap-host VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(libs/clap)

# set up rtaudio statically
set(RTAUDIO_BUILD_STATIC_LIBS TRUE)
set(RTAUDIO_IS_STATIC TRUE)
set(RTAUDIO_STATIC_MSVCRT TRUE)
add_subdirectory(libs/rtaudio)

# use asan on mac
set(SANITIZE TRUE)

add_executable(${PROJECT_NAME} src/main.cpp src/resolve_entrypoint.cpp src/host-impl.cpp)

if (APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MAC=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl "-framework CoreFoundation")
    target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<BOOL:${SANITIZE}>:-fsanitize=address>
            $<$<BOOL:${SANITIZE}>:-fsanitize=undefined>
    )
    target_link_options(${PROJECT_NAME} PRIVATE
            $<$<BOOL:${SANITIZE}>:-fsanitize=address>
            $<$<BOOL:${SANITIZE}>:-fsanitize=undefined>
    )
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Werror)
elseif(UNIX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE LIN=1)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Werror)
else()

    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN=1)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE clap-core rtaudio)
