cmake_minimum_required(VERSION 3.3...3.12 FATAL_ERROR)
foreach(p
    CMP0071 # 3.10: Let AUTOMOC and AUTOUIC process GENERATED files
    )
  if(POLICY ${p})
    cmake_policy(SET ${p} NEW)
  endif()
endforeach()

project(QtVTKPenPaintWindows)

find_package(VTK COMPONENTS
  vtkCommonCore
  vtkFiltersSources
  vtkGUISupportQt
  vtkIOImage
  vtkInteractionImage
  vtkTestingCore
)
include(${VTK_USE_FILE})


if("${VTK_QT_VERSION}" STREQUAL "")
  message(FATAL_ERROR "VTK was not built with Qt")
endif()

# Set your files and resources here
set( Srcs QtVTKPenPaintWindowsApp.cxx QtVTKPenPaintWindows.cxx)

set( Hdrs QtVTKPenPaintWindows.h)

set( MOC_Hdrs QtVTKPenPaintWindows.h)

set( UIs QtVTKPenPaintWindows.ui )

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED QUIET)
qt5_wrap_ui(UI_Srcs ${UIs})

# CMAKE_AUTOMOC in ON so the MocHdrs will be automatically wrapped.
add_executable(QtVTKPenPaintWindows ${Srcs} ${Hdrs} ${UI_Srcs} ${MOC_Hdrs})
qt5_use_modules(QtVTKPenPaintWindows Core Gui Widgets)
target_link_libraries(QtVTKPenPaintWindows ${VTK_LIBRARIES})
