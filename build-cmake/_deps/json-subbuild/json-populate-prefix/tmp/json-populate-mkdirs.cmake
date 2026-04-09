# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/data/.openclaw/workspace/io-bench-cpp/build-cmake/_deps/json-src")
  file(MAKE_DIRECTORY "/data/.openclaw/workspace/io-bench-cpp/build-cmake/_deps/json-src")
endif()
file(MAKE_DIRECTORY
  "/data/.openclaw/workspace/io-bench-cpp/build-cmake/_deps/json-build"
  "/data/.openclaw/workspace/io-bench-cpp/build-cmake/_deps/json-subbuild/json-populate-prefix"
  "/data/.openclaw/workspace/io-bench-cpp/build-cmake/_deps/json-subbuild/json-populate-prefix/tmp"
  "/data/.openclaw/workspace/io-bench-cpp/build-cmake/_deps/json-subbuild/json-populate-prefix/src/json-populate-stamp"
  "/data/.openclaw/workspace/io-bench-cpp/build-cmake/_deps/json-subbuild/json-populate-prefix/src"
  "/data/.openclaw/workspace/io-bench-cpp/build-cmake/_deps/json-subbuild/json-populate-prefix/src/json-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/data/.openclaw/workspace/io-bench-cpp/build-cmake/_deps/json-subbuild/json-populate-prefix/src/json-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/data/.openclaw/workspace/io-bench-cpp/build-cmake/_deps/json-subbuild/json-populate-prefix/src/json-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
