#
# Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

# Variables
set(STREAMLINE_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "SL SDK Root Directory")
find_path(STREAMLINE_INCLUDE_DIR sl.h HINTS "${STREAMLINE_SDK_ROOT}/include" NO_CACHE)
find_path(STREAMLINE_PLUGIN_DLL_DIR sl.interposer.dll HINTS "${STREAMLINE_SDK_ROOT}/bin/x64" "${STREAMLINE_SDK_ROOT}/bin/x64/development" NO_CACHE)
find_path(STREAMLINE_PLUGIN_JSON_DIR sl.interposer.json HINTS "${STREAMLINE_SDK_ROOT}/scripts" "${STREAMLINE_SDK_ROOT}/bin/x64" NO_CACHE)
find_path(STREAMLINE_SUPPORT_DLL_DIR nvngx_dlss.dll HINTS "${STREAMLINE_SDK_ROOT}/bin/x64" NO_CACHE)
find_library(STREAMLINE_INTERPOSER_LIB sl.interposer HINTS "${STREAMLINE_SDK_ROOT}/lib/x64" NO_CACHE)
find_file(STREAMLINE_INTERPOSER_DLL sl.interposer.dll HINTS "${STREAMLINE_SDK_ROOT}/bin/x64" "${STREAMLINE_SDK_ROOT}/bin/x64/development" NO_CACHE)
set(STREAMLINE_INSTALL_DIR "." CACHE STRING "Streamline Install Dir")
 
# Sort Features
option(STREAMLINE_FEATURE_DLSS_SR   "Include DLSS-SR dll"         OFF)
option(STREAMLINE_FEATURE_NRD       "Include NRD dll"             OFF)
option(STREAMLINE_FEATURE_IMGUI     "Include Imgui dll"           OFF)
option(STREAMLINE_FEATURE_NVPERF    "Include NSight Perf SDK dll" OFF)
option(STREAMLINE_FEATURE_REFLEX    "Include Reflex dll"          OFF)
option(STREAMLINE_FEATURE_NIS       "Include NIS dll"             OFF)
option(STREAMLINE_FEATURE_DLSS_FG   "Include DLSS-FG dll"         OFF)
option(STREAMLINE_FEATURE_DEEPDVC   "Include DEEPDVC dll"   OFF)
option(STREAMLINE_FEATURE_DIRECTSR  "Include DirectSR dll"  OFF)

option(STREAMLINE_IMPORT_AS_INTERFACE "Import Streamline as an Interface without lib" OFF)

file(GLOB STREAMLINE_DLLS "${STREAMLINE_PLUGIN_DLL_DIR}sl.interposer.dll" "${STREAMLINE_PLUGIN_DLL_DIR}sl.common.dll")

if (STREAMLINE_FEATURE_DLSS_SR)
    list (APPEND STREAMLINE_DLLS "${STREAMLINE_PLUGIN_DLL_DIR}sl.dlss.dll" "${STREAMLINE_SUPPORT_DLL_DIR}nvngx_dlss.dll")
endif()

if (STREAMLINE_FEATURE_NRD)
    list (APPEND STREAMLINE_DLLS "${STREAMLINE_PLUGIN_DLL_DIR}sl.nrd.dll" "${STREAMLINE_SUPPORT_DLL_DIR}NRD.dll")
endif()

if (STREAMLINE_FEATURE_IMGUI AND EXISTS "${STREAMLINE_PLUGIN_DLL_DIR}sl.imgui.dll")
    list (APPEND STREAMLINE_DLLS "${STREAMLINE_PLUGIN_DLL_DIR}sl.imgui.dll")
endif()

if (STREAMLINE_FEATURE_NVPERF AND EXISTS "${STREAMLINE_PLUGIN_DLL_DIR}sl.nvperf.dll")
    list (APPEND STREAMLINE_DLLS "${STREAMLINE_PLUGIN_DLL_DIR}sl.nvperf.dll")
endif()

if (STREAMLINE_FEATURE_REFLEX)
    list (APPEND STREAMLINE_DLLS "${STREAMLINE_PLUGIN_DLL_DIR}sl.reflex.dll" "${STREAMLINE_PLUGIN_DLL_DIR}sl.pcl.dll")
    if (EXISTS "${STREAMLINE_PLUGIN_DLL_DIR}NvLowLatencyVk.dll")
        list (APPEND STREAMLINE_DLLS "${STREAMLINE_PLUGIN_DLL_DIR}NvLowLatencyVk.dll")
    else()
        message(WARNING "Missing NvLowLatencyVk.dll: Reflex will fail to run with Vulkan.")
    endif()
endif()

if (STREAMLINE_FEATURE_NIS)
    list (APPEND STREAMLINE_DLLS "${STREAMLINE_PLUGIN_DLL_DIR}sl.nis.dll")
endif()

if (STREAMLINE_FEATURE_DLSS_FG)
    list (APPEND STREAMLINE_DLLS "${STREAMLINE_PLUGIN_DLL_DIR}sl.dlss_g.dll" "${STREAMLINE_SUPPORT_DLL_DIR}nvngx_dlssg.dll")
endif()


if (STREAMLINE_FEATURE_DEEPDVC)
    list (APPEND STREAMLINE_DLLS "${STREAMLINE_PLUGIN_DLL_DIR}sl.deepdvc.dll" "${STREAMLINE_SUPPORT_DLL_DIR}nvngx_DeepDVC.dll")
endif()

if (STREAMLINE_FEATURE_DIRECTSR)
    list (APPEND STREAMLINE_DLLS "${STREAMLINE_PLUGIN_DLL_DIR}sl.directsr.dll")
endif()

# We copy all the Jsons that may be present. 
if(NOT STREAMLINE_PLUGIN_JSON_DIR)
    message(STATUS "No sl.interposer.json found. Cmake cannot find the json files.")
    file(GLOB STREAMLINE_JSONS "")
else()
    file(GLOB STREAMLINE_JSONS ${STREAMLINE_PLUGIN_JSON_DIR}/*.json)
endif()

#Library

if (STREAMLINE_IMPORT_AS_INTERFACE)
add_library(streamline INTERFACE)
else()
add_library(streamline IMPORTED SHARED GLOBAL)
set_target_properties(streamline PROPERTIES IMPORTED_IMPLIB ${STREAMLINE_INTERPOSER_LIB})
endif()

target_include_directories(streamline INTERFACE ${STREAMLINE_INCLUDE_DIR})



# DLLS copy function
if (WIN32)
    add_custom_target(CopyStreamlineDLLs ALL DEPENDS ${STREAMLINE_DLLS})
    add_custom_command(TARGET CopyStreamlineDLLs
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMENT "Created Dir: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    add_custom_command(TARGET CopyStreamlineDLLs
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${STREAMLINE_DLLS} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMENT "Copied DLLs: ${STREAMLINE_DLLS}")
    
    if (STREAMLINE_JSONS)
        add_custom_command(TARGET CopyStreamlineDLLs
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${STREAMLINE_JSONS} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            COMMENT "Copied JSONs: ${STREAMLINE_JSONS}")
    endif()
    add_dependencies(streamline CopyStreamlineDLLs)

    set_target_properties(CopyStreamlineDLLs PROPERTIES FOLDER "Streamline")
endif ()

install(FILES ${STREAMLINE_DLLS} ${STREAMLINE_JSONS} DESTINATION "${STREAMLINE_INSTALL_DIR}")
