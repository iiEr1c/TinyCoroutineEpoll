set(HEADER_LIST
    "awaitable.hpp"
    "io_scheduler.hpp"
    "poll_info.hpp"
    "poll.hpp"
    "sync_wait.hpp"
    "task.hpp"
    "socket/acceptor.hpp"
    "socket/ip_v4_address.hpp"
    "socket/ip_v4_address.cpp"
    "socket/tcp_conn.hpp"
    "socket/tcp_server.hpp"
    "ignoreSigPipe.hpp")

add_library(TinyCoroutine ${HEADER_LIST})

    
# We need this directory, and users of our library will need it too
target_include_directories(TinyCoroutine PUBLIC ./)

# All users of this library will need at least C++11
target_compile_features(TinyCoroutine PUBLIC cxx_std_20)

target_compile_options(TinyCoroutine PRIVATE
            ${CXX_FLAGS}
            "$<$<CONFIG:Debug>:${CXX_FLAGS_DEBUG}>"
            "$<$<CONFIG:Release>:${CXX_FLAGS_RELEASE}>")
#target_compile_options(TinyNet PRIVATE "-fsanitize=address" "-fsanitize=undefined")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("CMAKE_BUILD_TYPE == " ${CMAKE_BUILD_TYPE})
    target_link_options(TinyCoroutine PRIVATE ${SANITIZE_FLAG})
endif()

target_link_libraries(TinyCoroutine PUBLIC fmt::fmt)