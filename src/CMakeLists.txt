file(GLOB_RECURSE SOURCES 
    "*.cpp"
)
if(WIN32)
    set(APP_ICON "${CMAKE_SOURCE_DIR}/assets/icons/app_icon.ico")
    set(RESOURCE_FILE "${CMAKE_SOURCE_DIR}/resources.rc")
    
    add_executable(${PROJECT_NAME} WIN32
        ${SOURCES}
        ${RESOURCE_FILE}
        ${APP_ICON}
    )
else()
    add_executable(${PROJECT_NAME}
        ${SOURCES}
    )
endif()

add_library(glad STATIC ${GLAD_SRC})
target_include_directories(glad PUBLIC ${GLAD_INCLUDE})

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
    glad
    gdi32
)

if(MINGW)
    target_link_libraries(${PROJECT_NAME} mingw32)
endif()