file(GLOB_RECURSE SOURCES 
    "*.cpp"
)

option(BUILD_WITH_CONSOLE "Build as a console application (for debugging output)" ON)

if(WIN32 AND NOT BUILD_WITH_CONSOLE)
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
    Dune2kMapApi
    GenCore
)


FetchContent_Declare(
  dune2000-map-api
  GIT_REPOSITORY https://github.com/he1kor/dune-map-api
  GIT_TAG        0c5c454145f0c86a06e7d0b125fed4243bd90b6a
)
FetchContent_MakeAvailable(dune2000-map-api)

if(MINGW)
    target_link_libraries(${PROJECT_NAME} mingw32)
endif()