if (STATIC_BUILD)
  find_library(JEMALLOC_LIBRARIES
               NAMES libjemalloc.a
               PATHS ${JEMALLOC_ROOT}/lib
  )
else ()
  find_library(JEMALLOC_LIBRARIES
               NAMES jemalloc
               PATHS ${JEMALLOC_ROOT}/lib
  )
endif ()

find_path(JEMALLOC_INCLUDE_DIR
          NAMES jemalloc/jemalloc.h
          PATHS ${JEMALLOC_ROOT}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JeMalloc DEFAULT_MSG JEMALLOC_LIBRARIES JEMALLOC_INCLUDE_DIR)

mark_as_advanced(
  JEMALLOC_LIBRARIES
  JEMALLOC_INCLUDE_DIR
)
