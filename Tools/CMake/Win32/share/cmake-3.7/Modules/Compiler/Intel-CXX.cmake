include(Compiler/Intel)
__compiler_intel(CXX)

string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL_INIT " -DNDEBUG")
string(APPEND CMAKE_CXX_FLAGS_RELEASE_INIT " -DNDEBUG")
string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT " -DNDEBUG")

set(CMAKE_DEPFILE_FLAGS_CXX "-MD -MT <OBJECT> -MF <DEPFILE>")

if("x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
  set(_std -Qstd)
  set(_ext c++)
else()
  set(_std -std)
  set(_ext gnu++)
endif()

if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0.2)
  set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "${_std}=c++14")
  # todo: there is no gnu++14 value supported; figure out what to do
  set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "${_std}=c++14")
elseif (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0.0)
  set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "${_std}=c++1y")
  # todo: there is no gnu++14 value supported; figure out what to do
  set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "${_std}=c++1y")
endif()

if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13.0)
  set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "${_std}=c++11")
  set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "${_std}=${_ext}11")
elseif (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.1)
  set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "${_std}=c++0x")
  set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "${_std}=${_ext}0x")
endif()

if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.1)
  set(CMAKE_CXX98_STANDARD_COMPILE_OPTION "${_std}=c++98")
  set(CMAKE_CXX98_EXTENSION_COMPILE_OPTION "${_std}=${_ext}98")
endif()

if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.1)
  if (NOT CMAKE_CXX_COMPILER_FORCED)
    if (NOT CMAKE_CXX_STANDARD_COMPUTED_DEFAULT)
      message(FATAL_ERROR "CMAKE_CXX_STANDARD_COMPUTED_DEFAULT should be set for ${CMAKE_CXX_COMPILER_ID} (${CMAKE_CXX_COMPILER}) version ${CMAKE_CXX_COMPILER_VERSION}")
    else()
      set(CMAKE_CXX_STANDARD_DEFAULT ${CMAKE_CXX_STANDARD_COMPUTED_DEFAULT})
    endif()
  elseif (NOT CMAKE_CXX_STANDARD_COMPUTED_DEFAULT)
    # Compiler id was forced so just guess the default standard level.
    set(CMAKE_CXX_STANDARD_DEFAULT 98)
  endif()
endif()

unset(_std)
unset(_ext)

macro(cmake_record_cxx_compile_features)
  macro(_get_intel_features std_version list)
    record_compiler_features(CXX "${std_version}" ${list})
  endmacro()

  set(_result 0)
  if(NOT "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC" AND
      NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.1)
    if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0)
      _get_intel_features(${CMAKE_CXX14_STANDARD_COMPILE_OPTION} CMAKE_CXX14_COMPILE_FEATURES)
    endif()
    if (_result EQUAL 0)
      _get_intel_features(${CMAKE_CXX11_STANDARD_COMPILE_OPTION} CMAKE_CXX11_COMPILE_FEATURES)
    endif()
    if (_result EQUAL 0)
      _get_intel_features(${CMAKE_CXX98_STANDARD_COMPILE_OPTION} CMAKE_CXX98_COMPILE_FEATURES)
    endif()
  endif()
endmacro()

set(CMAKE_CXX_CREATE_PREPROCESSED_SOURCE "<CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -E <SOURCE> > <PREPROCESSED_SOURCE>")
set(CMAKE_CXX_CREATE_ASSEMBLY_SOURCE "<CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -S <SOURCE> -o <ASSEMBLY_SOURCE>")