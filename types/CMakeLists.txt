add_library(quickfoil_types_FromString ../empty_src.cpp FromString.hpp)
add_library(quickfoil_types_Type Type.cpp Type.hpp)
add_library(quickfoil_types_TypeID ../empty_src.cpp TypeID.hpp)
add_library(quickfoil_types_TypeTraits ../empty_src.cpp TypeTraits.hpp)

target_link_libraries(quickfoil_types_FromString
                      folly
                      glog
                      quickfoil_types_TypeID
                      quickfoil_types_TypeTraits)
target_link_libraries(quickfoil_types_Type
                      glog
                      quickfoil_types_TypeID
                      quickfoil_types_TypeTraits
                      quickfoil_utility_Macros)
target_link_libraries(quickfoil_types_TypeTraits
                      folly
                      quickfoil_types_TypeID)
