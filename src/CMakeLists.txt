file(GLOB_RECURSE SOURCES 
    "*.cpp"
)
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include/global
    ${CMAKE_SOURCE_DIR}/include/ui
    ${CMAKE_SOURCE_DIR}/include/state
    ${CMAKE_SOURCE_DIR}/libs/imgui
)

target_link_libraries(${PROJECT_NAME}
    imgui
    ${GLFW_LIBRARY}
    opengl32
    gdi32
)

if(MINGW)
    target_link_libraries(${PROJECT_NAME} mingw32)
endif()