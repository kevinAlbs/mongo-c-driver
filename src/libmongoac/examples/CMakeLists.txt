cmake_minimum_required(VERSION 3.16)

project(hello-mongoac C)

find_package(mongoac 0.1.0 CONFIG REQUIRED)
find_package(bson CONFIG REQUIRED)

add_executable(hello-mongoac hello-mongoac.c)
target_link_libraries(hello-mongoac PRIVATE mongoac::shared bson::bson)
