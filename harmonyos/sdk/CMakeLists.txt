cmake_minimum_required(VERSION 3.20)
project(PicoSdkExample LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PICO_LLAMA_SOURCE "$ENV{PICO_LLAMA_SOURCE}" CACHE PATH "Pinned upstream source")
set(PICO_LLAMA_BUILD "$ENV{PICO_LLAMA_BUILD}" CACHE PATH "Qualified static-library build")
if(NOT EXISTS "${PICO_LLAMA_SOURCE}/include/llama.h" OR NOT EXISTS "${PICO_LLAMA_BUILD}/src/libllama.a")
  message(FATAL_ERROR "Explicit pinned llama source and compatible static libraries required")
endif()
add_library(pico-sdk-example STATIC session_example.cpp)
set_target_properties(pico-sdk-example PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(pico-sdk-example PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${PICO_LLAMA_SOURCE}/include" "${PICO_LLAMA_SOURCE}/ggml/include" PRIVATE "${PICO_LLAMA_SOURCE}/vendor/nlohmann")
target_compile_options(pico-sdk-example PRIVATE -Wall -Wextra -Werror -Wno-unused-command-line-argument)
target_link_libraries(pico-sdk-example PUBLIC
  -Wl,--start-group
  "${PICO_LLAMA_BUILD}/src/libllama.a"
  "${PICO_LLAMA_BUILD}/ggml/src/libggml.a"
  "${PICO_LLAMA_BUILD}/ggml/src/libggml-cpu.a"
  "${PICO_LLAMA_BUILD}/ggml/src/libggml-base.a"
  -Wl,--end-group pthread dl m)
# The matched Ollama runner adds a hash helper; ordinary upstream llama.cpp
# does not. Do not make that downstream archive a dependency of a plain SDK.
if(EXISTS "${PICO_LLAMA_BUILD}/vendor/hash/libvendor-hash.a")
  target_link_libraries(pico-sdk-example PUBLIC "${PICO_LLAMA_BUILD}/vendor/hash/libvendor-hash.a")
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  add_executable(pico-sdk-cli-example cli_example.cpp)
  target_link_libraries(pico-sdk-cli-example PRIVATE pico-sdk-example)
endif()
