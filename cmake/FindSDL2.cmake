# Minimal SDL2 finder for Windows and generic setups

find_path(SDL2_INCLUDE_DIR SDL.h
    HINTS
        $ENV{SDL2_DIR}/include
        ${SDL2_DIR}/include
        C:/vcpkg/installed/x64-windows/include
)

if (WIN32)
    find_library(SDL2_LIBRARY NAMES SDL2 SDL2d SDL2.lib
        HINTS 
            $ENV{SDL2_DIR}/lib 
            ${SDL2_DIR}/lib 
            ${SDL2_DIR}/lib/x64 
            ${SDL2_DIR}/lib/Release
            C:/vcpkg/installed/x64-windows/lib)
    find_library(SDL2MAIN_LIBRARY NAMES SDL2main SDL2main.lib
        HINTS 
            $ENV{SDL2_DIR}/lib 
            ${SDL2_DIR}/lib 
            ${SDL2_DIR}/lib/x64 
            ${SDL2_DIR}/lib/Release
            C:/vcpkg/installed/x64-windows/lib)
else()
    find_library(SDL2_LIBRARY NAMES SDL2)
    set(SDL2MAIN_LIBRARY "")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2 REQUIRED_VARS SDL2_INCLUDE_DIR SDL2_LIBRARY)

if (SDL2_FOUND)
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
    set(SDL2_LIBRARIES ${SDL2_LIBRARY} ${SDL2MAIN_LIBRARY})

    if (NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2 PROPERTIES
            IMPORTED_LOCATION "${SDL2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
        )
    endif()
    if (SDL2MAIN_LIBRARY AND NOT TARGET SDL2::SDL2main)
        add_library(SDL2::SDL2main UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2main PROPERTIES
            IMPORTED_LOCATION "${SDL2MAIN_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
        )
    endif()
endif()



