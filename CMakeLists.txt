cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(game_model STATIC
	src/model.cpp
	src/collision_detector.cpp
	src/loot_generator.cpp
	src/geom.h
	src/model_serialization.h
	src/model.h
	src/collision_detector.h
	src/loot_generator.h
	src/tagged.h
)

target_include_directories(game_model PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(game_model PUBLIC CONAN_PKG::boost Threads::Threads)

add_library(game_lib STATIC
	src/application.cpp
	src/json_loader.cpp
	src/boost_json.cpp
	src/serializing_listener.cpp
	src/retirement_db.cpp
)

target_include_directories(game_lib PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(game_lib PUBLIC game_model Threads::Threads CONAN_PKG::libpqxx)

add_executable(game_server
	src/main.cpp
	src/http_server.cpp
	src/request_handler.cpp
)

target_link_libraries(game_server PRIVATE game_lib Threads::Threads)

option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
	enable_testing()

	add_executable(game_server_tests
		tests/state-serialization-tests.cpp
		tests/collision-detector-tests.cpp
		tests/loot_generator_tests.cpp
	)

	target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 game_model)

	add_test(NAME game_server_tests COMMAND game_server_tests)
endif()
