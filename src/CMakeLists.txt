cmake_minimum_required(VERSION 3.20)
project(VAGEDC15Logger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(vagedc15core STATIC
	Core/CommandInterpreter.cpp
	Core/Config.cpp
	Core/Utilities.cpp
	EDC15/EDC15File.cpp
	Logging/Logger.cpp
)

target_include_directories(vagedc15core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
	target_link_libraries(vagedc15core PRIVATE winmm)
endif()
