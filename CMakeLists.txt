cmake_minimum_required(VERSION 2.8)

PROJECT(dmormpp)

IF (WIN32)  
    MESSAGE(STATUS "Now is windows")

    SET(DMOS_NAME "win")

    SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )
    IF(MSVC)
        ADD_DEFINITIONS(/bigobj)
	if (MSVC_VERSION GREATER_EQUAL "1900")
	        include(CheckCXXCompilerFlag)
	        CHECK_CXX_COMPILER_FLAG("/std:c++latest" _cpp_latest_flag_supported)
	        if (_cpp_latest_flag_supported)
	            add_compile_options("/std:c++latest")
	        endif()
    	endif()
    ENDIF()
    
    
ELSEIF (APPLE)  
    MESSAGE(STATUS "Now is Apple systems")  

    SET(DMOS_NAME "mac") 

    SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread" )
ELSEIF (UNIX)  
    MESSAGE(STATUS "Now is UNIX-like OS")  
 
    SET(DMOS_NAME "lin") 
    
    SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread" )    
ENDIF ()

ADD_SUBDIRECTORY(dmprotobuf/cmake)

IF (WIN32)
    INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/dmprotobuf/src/windows)
ENDIF(WIN32)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/dmprotobuf/src)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/dmprotobuf)
IF (WIN32)
    INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mysql-win/include/mysql)
    INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mysql-win/include)

    LINK_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mysql-win/lib)
ELSE(WIN32)
    INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/FindMYSQL.cmake)
    INCLUDE_DIRECTORIES(${MYSQL_INCLUDE_DIRS})
ENDIF(WIN32)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/test)

LINK_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/lib)

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)


FILE(GLOB_RECURSE dmormpptest_SOURCES
${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)


IF (WIN32)
    ADD_EXECUTABLE(dmormpptest ${dmormpptest_SOURCES})
    TARGET_LINK_LIBRARIES(dmormpptest ${MYSQL_LIBRARIES} sqlite3 pq libprotoc libprotobuf)
    ADD_CUSTOM_COMMAND(
	TARGET dmdbtest
	PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/mysql-win/lib/libmysql.dll
	$<TARGET_FILE_DIR:dmormpptest>
    )
ELSE(WIN32)
    ADD_EXECUTABLE(dmormpptest ${dmormpptest_SOURCES})
    TARGET_LINK_LIBRARIES(dmormpptest ${MYSQL_LIBRARIES} sqlite3 pq libprotoc libprotobuf)
ENDIF(WIN32)

