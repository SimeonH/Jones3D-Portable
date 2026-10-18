project(sound C)

file(GLOB_RECURSE SOURCES *.c)
file(GLOB_RECURSE HEADERS *.h)

# Driver: DirectSound (Driver.c) or OpenAL Soft (Driver_openal.c)
if(WIN32)
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Driver_openal.c)
else()
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Driver.c)
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(${PROJECT_NAME} PUBLIC j3dcore)
target_link_libraries(${PROJECT_NAME} PUBLIC rdroid)
target_link_libraries(${PROJECT_NAME} PUBLIC std)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC dsound)
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC OpenAL::OpenAL)
endif()

# Organize source files in Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})
