file(GLOB_RECURSE SOURCES 
    "*.cpp"
)

option(BUILD_WITH_CONSOLE "Build as a console application (for debugging output)" ON)

foreach(template IN ITEMS "habbanya erg true 2.json" "grid3x3 magnetic.json")
    configure_file(
        "${CMAKE_SOURCE_DIR}/assets/templates/${template}"
        "${CMAKE_CURRENT_BINARY_DIR}/${template}"
        COPYONLY
    )
endforeach()

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
    ${CMAKE_SOURCE_DIR}/include/generation
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
    nfd
    nlohmann_json::nlohmann_json
)

FetchContent_Declare(
  dune2000-map-api
  GIT_REPOSITORY https://github.com/he1kor/dune-map-api
  GIT_TAG        0c5c454145f0c86a06e7d0b125fed4243bd90b6a
)

if(NOT TARGET nlohmann_json::nlohmann_json)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()


FetchContent_MakeAvailable(dune2000-map-api)

if(MINGW)
    target_link_libraries(${PROJECT_NAME} mingw32)
endif()

option(MAPGENERATOR_BUILD_TESTS "Build map generation regression tests" OFF)
if(MAPGENERATOR_BUILD_TESTS)
    enable_testing()
    add_executable(resource_generation_regression
        ../tests/resource_generation_regression.cc
        state/template_picker.cpp
        state/generation.cpp
    )
    target_include_directories(resource_generation_regression PRIVATE
        ${CMAKE_SOURCE_DIR}/include/state
        ${CMAKE_SOURCE_DIR}/include/global
    )
    target_link_libraries(resource_generation_regression PRIVATE GenCore nlohmann_json::nlohmann_json)
    add_test(NAME resource_generation_regression COMMAND resource_generation_regression
        "${CMAKE_SOURCE_DIR}/assets/templates/habbanya erg true 2.json")
    add_executable(magnetic_grid_regression
        ../tests/magnetic_grid_regression.cc
        state/template_picker.cpp
        state/generation.cpp
    )
    target_include_directories(magnetic_grid_regression PRIVATE
        ${CMAKE_SOURCE_DIR}/include/state
        ${CMAKE_SOURCE_DIR}/include/global
    )
    target_link_libraries(magnetic_grid_regression PRIVATE GenCore nlohmann_json::nlohmann_json)
    add_test(NAME magnetic_grid_regression COMMAND magnetic_grid_regression
        "${CMAKE_SOURCE_DIR}/assets/templates/grid3x3 magnetic.json")
    add_executable(template_validation_regression
        ../tests/template_validation_regression.cc
        state/template_picker.cpp
        state/generation.cpp
    )
    target_include_directories(template_validation_regression PRIVATE
        ${CMAKE_SOURCE_DIR}/include/state
        ${CMAKE_SOURCE_DIR}/include/global
    )
    target_link_libraries(template_validation_regression PRIVATE GenCore nlohmann_json::nlohmann_json)
    add_test(NAME template_validation_regression COMMAND template_validation_regression
        "${CMAKE_SOURCE_DIR}/assets/templates/habbanya erg true 2.json"
        "${CMAKE_SOURCE_DIR}/assets/templates/grid3x3 magnetic.json"
        "${CMAKE_CURRENT_BINARY_DIR}/template-validation-regression.json")
endif()
