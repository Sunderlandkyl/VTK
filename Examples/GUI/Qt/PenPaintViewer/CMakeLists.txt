cmake_minimum_required(VERSION 3.3...3.12 FATAL_ERROR)
project(QtVTKPenPaintWindows)

find_package(VTK COMPONENTS
  CommonCore
  CommonSystem
  FiltersSources
  GUISupportQt
  IOImage
  ImagingColor
  ImagingGeneral
  ImagingSources
  InteractionImage
  InteractionStyle
  InteractionWidgets
  TestingCore)
if (NOT VTK_FOUND)
  message("Skipping example: ${VTK_NOT_FOUND_MESSAGE}")
  return ()
endif ()

find_package(Qt5 COMPONENTS Widgets)
if (NOT TARGET Qt5::Widgets)
  message("Skipping example: ${Qt5_NOT_FOUND_MESSAGE}")
  return ()
endif ()

# Set your files and resources here
set( Srcs QtVTKPenPaintWindowsApp.cxx)


include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${VTK_INCLUDE_DIR}
)

# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# CMAKE_AUTOMOC in ON so the MocHdrs will be automatically wrapped.
add_executable(QtVTKPenPaintWindows ${Srcs})
  target_link_libraries(QtVTKPenPaintWindows
  PRIVATE
    ${VTK_LIBRARIES}
    Qt5::Widgets)
vtk_module_autoinit(
  TARGETS QtVTKPenPaintWindows
  MODULES ${VTK_LIBRARIES})
