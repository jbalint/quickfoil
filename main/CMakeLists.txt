add_library(quickfoil_main_Configuration Configuration.cpp Configuration.hpp)

target_link_libraries(quickfoil_main_Configuration
                      folly
                      glog
                      quickfoil_schema_TypeDefs
                      quickfoil_utility_Macros)
