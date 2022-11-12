add_executable(long long.cpp)

find_package(fmt REQUIRED)

target_link_libraries(long PRIVATE fmt::fmt)
target_link_libraries(long PRIVATE ubsan)

set_target_properties(long PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

