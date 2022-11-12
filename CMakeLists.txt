cmake_minimum_required(VERSION 3.23)
project(long-arithmetic)

# --------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra)

add_compile_options(-fsanitize=address,undefined)
add_link_options(-fsanitize=address,undefined)

# --------------------------------------------------------------------

add_subdirectory(src)

# --------------------------------------------------------------------
