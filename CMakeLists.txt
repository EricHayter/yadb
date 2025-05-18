cmake_minimum_required(VERSION 3.25)

project(MyExample)

find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)

add_executable(data_types data_types.cpp)
add_executable(io io.cpp)

target_link_libraries(data_types PRIVATE Arrow::arrow_shared)
target_link_libraries(io PRIVATE Arrow::arrow_shared Parquet::parquet_shared)
