# This is where we actually wrap the modules that have requested it.
include(vtkPythonWrapping)

get_property(VTK_PYTHON_MODULES GLOBAL PROPERTY VTK_PYTHON_WRAPPED)

# Get the include directories for the module and all its dependencies.
macro(vtk_include_recurse module)
  _vtk_module_config_recurse("${module}_PYTHON" ${module})
  include_directories(${${module}_PYTHON_INCLUDE_DIRS})
endmacro()


set(VTK_PYTHON_MODULES_AND_KITS ${VTK_PYTHON_MODULES})
set(_vtk_python_modules_only ${VTK_PYTHON_MODULES})
if(VTK_ENABLE_KITS)
  set(_vtk_kits_with_suffix)
  set(VTK_KIT_SUFFIX "Kit") # Required to avoid conflict with targets like vtkFiltersPython
  # Create list of module that do not belong to any kits
  foreach(kit IN LISTS vtk_kits)
    # XXX Variable '_${kit}_modules' is set in vtkModuleTop.cmake
    list(REMOVE_ITEM _vtk_python_modules_only ${_${kit}_modules})
    list(APPEND _vtk_kits_with_suffix ${kit}${VTK_KIT_SUFFIX})
  endforeach()
  set(VTK_PYTHON_MODULES_AND_KITS ${_vtk_kits_with_suffix} ${_vtk_python_modules_only})
  # Loop through all kits that should wrapped, and wrap them.
  foreach(kit IN LISTS vtk_kits)
    set(kit_srcs)
    set(_module_depends)
    foreach(module IN LISTS _${kit}_modules)
      vtk_module_load(${module})
      vtk_module_headers_load(${module})
      vtk_include_recurse(${module})
      list(APPEND _module_depends ${${module}_DEPENDS})
    endforeach()
    vtk_add_python_wrapping("${_${kit}_modules}" kit_srcs ${kit}${VTK_KIT_SUFFIX})
    # Compose kit python module dependency list.
    set(_kit_depends)
    foreach(dep IN LISTS _module_depends)
      if(${dep}_KIT)
        list(APPEND _kit_depends ${${dep}_KIT}${VTK_KIT_SUFFIX})
      else()
        list(APPEND _kit_depends ${dep})
      endif()
    endforeach()
    list(REMOVE_DUPLICATES _kit_depends)
    set(${kit}${VTK_KIT_SUFFIX}_DEPENDS ${_kit_depends})
    # Wrap kit.
    vtk_add_python_wrapping_library(${kit}${VTK_KIT_SUFFIX} kit_srcs ${kit})
  endforeach()
endif()
# Loop through all modules that should be wrapped, and wrap them.
foreach(module IN LISTS _vtk_python_modules_only)
  vtk_module_load(${module})
  vtk_module_headers_load(${module})
  vtk_include_recurse(${module})
  vtk_add_python_wrapping(${module} module_srcs)
  set(_saved_${module}_DEPENDS ${${module}_DEPENDS})
  # Update module dependency to consider dependent kits if any.
  if(VTK_ENABLE_KITS)
    set(_module_depends_with_kits)
    foreach(dep IN LISTS ${module}_DEPENDS)
      if(${dep}_KIT)
        list(APPEND _module_depends_with_kits ${${dep}_KIT}${VTK_KIT_SUFFIX})
      else()
        list(APPEND _module_depends_with_kits ${dep})
      endif()
    endforeach()
    list(REMOVE_DUPLICATES _module_depends_with_kits)
    set(${module}_DEPENDS ${_module_depends_with_kits})
  endif()
  vtk_add_python_wrapping_library(${module} module_srcs ${module})
  # Restore regular list of dependency so that it can be used later.
  set(${module}_DEPENDS ${_saved_${module}_DEPENDS})
  unset(_saved_${module}_DEPENDS)
endforeach()

option(VTK_ENABLE_VTKPYTHON "Enable vtkpython and pvtkpython binaries" ON)
mark_as_advanced(VTK_ENABLE_VTKPYTHON)
if(VTK_ENABLE_VTKPYTHON)

vtk_module_load(vtkWrappingPythonCore)
vtk_module_load(vtkPython)
include_directories(${CMAKE_CURRENT_BINARY_DIR}
  ${VTK_SOURCE_DIR}/Utilities
  ${vtkPython_INCLUDE_DIRS}
  ${vtkWrappingPythonCore_INCLUDE_DIRS})
# Needed for the vtkpython executable.
if(VTK_USE_CARBON)
  find_program(VTK_APPLE_RESOURCE Rez "${OSX_DEVELOPER_ROOT}/usr/bin")
endif()

if(UNIX)
  find_library(PYTHON_UTIL_LIBRARY
    NAMES util
    DOC "Utility library needed for vtkpython")
  mark_as_advanced(PYTHON_UTIL_LIBRARY)
endif()

# Generate the header which initializes Python modules when BUILD_SHARED_LIBS is
# OFF. The py_module_dependencies will be set to the libraries we should link
# against when we use the vtkpythonmodules.h file.
vtk_write_python_modules_header_for_wrapped_modules(
  "${CMAKE_CURRENT_BINARY_DIR}/vtkpythonmodules.h"
  py_module_dependencies)

# Create the VTK/Python  executable
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/vtkPythonAppInitConfigure.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/vtkPythonAppInitConfigure.h)

add_executable(vtkpython vtkPythonAppInit.cxx vtkpython.rc)

unset(VTKPYTHON_LINK_FLAGS)
unset(VTKPYTHON_LINK_LIBS)

if(CMAKE_SYSTEM_NAME MATCHES "AIX")
  get_filename_component(CMAKE_PYTHON_LIB_PREFIX "${PYTHON_LIBRARY}" PATH)
  find_file(CMAKE_PYTHON_LIBRARY_EXPORT python.exp "${CMAKE_PYTHON_LIB_PREFIX}")
  if(CMAKE_PYTHON_LIBRARY_EXPORT)
    set(VTKPYTHON_LINK_FLAGS "-Wl,-bE:${CMAKE_PYTHON_LIBRARY_EXPORT}")
  endif()
endif()

if(APPLE)
  if(VTK_USE_CARBON AND VTK_APPLE_RESOURCE)
    get_target_property(
      VTKPYTHON_RUNTIME_DIR vtkpython RUNTIME_OUTPUT_DIRECTORY)
    add_custom_command(
      TARGET vtkpython POST_BUILD
      COMMAND ${VTK_APPLE_RESOURCE} Carbon.r -o ${VTKPYTHON_RUNTIME_DIR}/vtkpython
      )
  endif()

  if(VTK_USE_CARBON)
    # MacOS python has a --enable-toolbox-glue option that is on by default,
    # which adds the "-u _PyMac_Error" linker flag to force inclusion of some
    # Carbon extension functions for Python.  It is irrelevant for Cocoa VTK.
    set(VTKPYTHON_LINK_FLAGS "-u _PyMac_Error")
  endif()
endif()

# Link against all the kit wrappers.
list(APPEND VTKPYTHON_LINK_LIBS vtkWrappingPythonCore)

if(PYTHON_UTIL_LIBRARY)
  list(APPEND VTKPYTHON_LINK_LIBS ${PYTHON_UTIL_LIBRARY})
endif()

set(VTKPYTHON_LINK_LIBS ${VTKPYTHON_LINK_LIBS} ${py_module_dependencies})

# Link to rt to prevent undefined symbol 'fdatasync'
if(CMAKE_SYSTEM MATCHES "SunOS.*" AND NOT CMAKE_COMPILER_IS_GNUCXX)
  find_library(VTK_SUNCC_RT_LIBRARY rt)
  if(VTK_SUNCC_RT_LIBRARY)
    set(VTKPYTHON_LINK_LIBS ${VTKPYTHON_LINK_LIBS} ${VTK_SUNCC_RT_LIBRARY})
  endif()
endif()

# If we are wrapping Tcl then the Tk libraries are available and should be used
# in the Python wrapping too.
if(VTK_WRAP_TCL)
  list(APPEND VTKPYTHON_LINK_LIBS ${VTK_TK_LIBRARIES})
endif()

if(HAVE_PTHREAD_H AND CMAKE_USE_PTHREADS)
  list(APPEND VTKPYTHON_LINK_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

# FIXME: Is this needed here?
if(VTK_USE_FFMPEG_ENCODER)
  list(APPEND VTKPYTHON_LINK_LIBS ${FFMPEG_BASIC_LIBRARIES})
endif()

target_link_libraries(vtkpython ${VTKPYTHON_LINK_LIBS})

unset(PVTKPYTHON_EXECUTABLE)
# Create the pvtkpython Python wrapper executable with MPI support.
if(TARGET vtkParallelMPI)
  find_package(MPI REQUIRED)
  include_directories(${MPI_C_INCLUDE_PATH})
  set(MPI_LIBRARIES ${MPI_C_LIBRARIES})
  if(MPI_CXX_LIBRARIES)
    set(MPI_LIBRARIES ${MPI_LIBRARIES} ${MPI_CXX_LIBRARIES})
  endif()

  # Needed for mpich 2
  add_definitions("-DMPICH_IGNORE_CXX_SEEK")

  set(PVTKPYTHON_EXECUTABLE pvtkpython)
  add_executable(pvtkpython vtkParaPythonAppInit.cxx)
  target_link_libraries(pvtkpython ${VTKPYTHON_LINK_LIBS} vtkParallelMPI
    ${MPI_LIBRARIES})

  if(VTK_USE_CARBON AND VTK_APPLE_RESOURCE)
    add_custom_command(TARGET pvtkpython POST_BUILD
      COMMAND ${VTK_APPLE_RESOURCE} Carbon.r -o ${VTK_EXECUTABLE_DIR}/pvtkpython)
  endif()
endif()

endif()

# Handle out-of-source builds correctly.
#
#  1. Create a list of Python files to be installed/copied.
#  2. Copy them to VTK_BINARY_DIR if it is different from VTK_SOURCE_DIR.
#  3. Use Python's compileall to compile the copied files.
#
# *** Step 1 has to be done carefully to avoid missing out files ***

if(PYTHON_EXECUTABLE)
  # Make the necessary directories.
  file(MAKE_DIRECTORY
    ${CMAKE_CURRENT_BINARY_DIR}/vtk/gtk
    ${CMAKE_CURRENT_BINARY_DIR}/vtk/qt4
    ${CMAKE_CURRENT_BINARY_DIR}/vtk/tk
    ${CMAKE_CURRENT_BINARY_DIR}/vtk/util
    ${CMAKE_CURRENT_BINARY_DIR}/vtk/wx
    ${CMAKE_CURRENT_BINARY_DIR}/vtk/test
    ${CMAKE_CURRENT_BINARY_DIR}/vtk/numpy_interface)

# Now create a list of Python files.

# Wrapping/Python/vtk/*.py
  unset(VTK_PYTHON_FILES)
  unset(VTK_PYTHON_IMPORT_ALL)
  foreach(module IN LISTS VTK_PYTHON_MODULES_AND_KITS)
    set(VTK_PYTHON_IMPORT_ALL "${VTK_PYTHON_IMPORT_ALL}from ${module} import *\n")
    configure_file(vtk/module.py.in vtk/${module}.py @ONLY)
  endforeach()
  configure_file(vtk/__init__.py.in vtk/__init__.py @ONLY)

# vtk.util package
  list(APPEND VTK_PYTHON_FILES
    vtk/util/__init__
    vtk/util/vtkMethodParser
    vtk/util/misc
    vtk/util/vtkConstants
    vtk/util/vtkImageExportToArray
    vtk/util/vtkImageImportFromArray
    vtk/util/vtkVariant
    vtk/util/colors
    vtk/util/numpy_support
    vtk/util/vtkAlgorithm
    )

# vtk.test package
  list(APPEND VTK_PYTHON_FILES
    vtk/test/BlackBox
    vtk/test/Testing
    vtk/test/__init__
    )

# Tk related files
  list(APPEND VTK_PYTHON_FILES
    vtk/tk/__init__
    vtk/tk/vtkLoadPythonTkWidgets
    vtk/tk/vtkTkImageViewerWidget
    vtk/tk/vtkTkRenderWidget
    vtk/tk/vtkTkRenderWindowInteractor
    vtk/tk/vtkTkPhotoImage
    )

# PyQt v4 related files
  list(APPEND VTK_PYTHON_FILES
    vtk/qt4/__init__
    vtk/qt4/QVTKRenderWindowInteractor
    )

# wxPython related files
  list(APPEND VTK_PYTHON_FILES
    vtk/wx/__init__
    vtk/wx/wxVTKRenderWindow
    vtk/wx/wxVTKRenderWindowInteractor
    )

# pyGTK related files
  list(APPEND VTK_PYTHON_FILES
    vtk/gtk/GtkVTKRenderWindow
    vtk/gtk/__init__
    vtk/gtk/GtkVTKRenderWindowInteractor
    vtk/gtk/GtkGLExtVTKRenderWindow
    vtk/gtk/GtkGLExtVTKRenderWindowInteractor
    )

# vtk.numpy_interface related files
  list(APPEND VTK_PYTHON_FILES
    vtk/numpy_interface/__init__
    vtk/numpy_interface/algorithms
    vtk/numpy_interface/dataset_adapter
    vtk/numpy_interface/internal_algorithms
    )
# Done listing of files.

# Now copy these files if necessary.
  unset(VTK_PYTHON_SOURCE_FILES)
  unset(VTK_PYTHON_OUTPUT_FILES)
  foreach(file ${VTK_PYTHON_FILES})
    set(src "${CMAKE_CURRENT_BINARY_DIR}/${file}.py")
    set(VTK_PYTHON_SOURCE_FILES ${VTK_PYTHON_SOURCE_FILES} ${src})
  endforeach()

  if(NOT "${VTK_BINARY_DIR}" MATCHES "^${VTK_SOURCE_DIR}$")
    foreach(file ${VTK_PYTHON_FILES})
      set(src "${CMAKE_CURRENT_SOURCE_DIR}/${file}.py")
      set(tgt "${CMAKE_CURRENT_BINARY_DIR}/${file}.py")
      add_custom_command(DEPENDS ${src}
        COMMAND ${CMAKE_COMMAND} -E copy ${src} ${tgt}
        OUTPUT ${tgt}
        COMMENT "source copy")
    endforeach()
  endif()

  # Byte compile the Python files.
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/compile_all_vtk.py.in
    ${CMAKE_CURRENT_BINARY_DIR}/compile_all_vtk.py
    @ONLY)
  add_custom_command(
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/compile_all_vtk.py
    DEPENDS ${VTK_PYTHON_SOURCE_FILES} ${CMAKE_CURRENT_BINARY_DIR}/compile_all_vtk.py
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/vtk_compile_complete"
    )

  # Create a target on which custom commands can depend.  When this
  # is up-to-date, it is safe to run VTK python code.  Therefore
  # this should also depend on the vtkpython executable.
  add_custom_target(vtkpython_pyc ALL echo "..."
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/vtk_compile_complete")
  add_dependencies(vtkpython_pyc
    vtkpython
    )

  # If no runtime is to be installed then do not install python modules.
  if(NOT VTK_INSTALL_NO_RUNTIME)

    # Install python modules
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/vtk"
      DESTINATION ${VTK_INSTALL_PYTHON_MODULE_DIR} COMPONENT RuntimeLibraries
      USE_SOURCE_PERMISSIONS)

    # Install python extension library that backs the modules
    if (BUILD_SHARED_LIBS AND NOT VTK_INSTALL_NO_LIBRARIES)
      foreach(module IN LISTS VTK_PYTHON_MODULES_AND_KITS)
        install(TARGETS ${module}Python
          EXPORT ${VTK_INSTALL_EXPORT_NAME}
          RUNTIME DESTINATION ${VTK_INSTALL_RUNTIME_DIR} COMPONENT RuntimeLibraries
          LIBRARY DESTINATION ${VTK_INSTALL_PYTHON_MODULE_DIR}/vtk COMPONENT RuntimeLibraries
          ARCHIVE DESTINATION ${VTK_INSTALL_ARCHIVE_DIR} COMPONENT Development
          )
      endforeach()
    endif()

    # Install the conveniently configured python interpretters
    if(NOT VTK_INSTALL_NO_PYTHON_EXES AND VTK_ENABLE_VTKPYTHON)
      # Install the vtkpython executable
      install(TARGETS vtkpython
        DESTINATION ${VTK_INSTALL_RUNTIME_DIR})

      if(PVTKPYTHON_EXECUTABLE)
        # Install the mpi enabled vtkpython executable
        install(TARGETS pvtkpython
          DESTINATION ${VTK_INSTALL_RUNTIME_DIR})
      endif()
    endif()

  endif()

endif()
