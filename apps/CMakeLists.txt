add_executable(app app.cpp)
target_compile_features(app PRIVATE cxx_std_20)

# target_link_libraries(app PRIVATE TinyJson)
target_link_libraries(app PRIVATE TinyCoroutine spdlog::spdlog)

target_compile_options(app PRIVATE
            ${CXX_FLAGS}
            "$<$<CONFIG:Debug>:${CXX_FLAGS_DEBUG}>"
            "$<$<CONFIG:Release>:${CXX_FLAGS_RELEASE}>")

#target_compile_options(app PRIVATE "-fsanitize=address" "-fsanitize=undefined")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("CMAKE_BUILD_TYPE == " ${CMAKE_BUILD_TYPE})
    target_link_options(app PRIVATE ${SANITIZE_FLAG})
endif()

