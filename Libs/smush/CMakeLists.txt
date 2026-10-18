project(smush C CXX)

file(GLOB_RECURSE SOURCES *.c *.cpp)
file(GLOB_RECURSE HEADERS *.h)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})
target_link_libraries(${PROJECT_NAME} PUBLIC j3dcore)
target_link_libraries(${PROJECT_NAME} PUBLIC std)
target_link_libraries(${PROJECT_NAME} PUBLIC sound)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC dsound)
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)


# Organize source files in Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})
