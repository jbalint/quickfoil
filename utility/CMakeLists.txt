add_library(quickfoil_utility_BitVector BitVector.cpp BitVector.hpp)
add_library(quickfoil_utility_BitVectorBuilder ../empty_src.cpp BitVector.hpp)
add_library(quickfoil_utility_BitVectorIterator ../empty_src.cpp BitVectorIterator.hpp)
add_library(quickfoil_utility_ElementDeleter ../empty_src.cpp ElementDeleter.hpp)
add_library(quickfoil_utility_Hash ../empty_src.cpp Hash.hpp)
add_library(quickfoil_utility_Macros ../empty_src.cpp Macros.hpp)
add_library(quickfoil_utility_Vector ../empty_src.cpp Vector.hpp)
add_library(quickfoil_utility_StringUtil StringUtil.cpp StringUtil.hpp)

target_link_libraries(quickfoil_utility_BitVectorBuilder
                      glog
                      quickfoil_utility_BitVector
                      quickfoil_utility_Macros)
target_link_libraries(quickfoil_utility_BitVectorIterator
                      quickfoil_utility_BitVector
                      quickfoil_utility_Macros)
target_link_libraries(quickfoil_utility_ElementDeleter
                      quickfoil_utility_Macros
                      quickfoil_utility_Vector)
target_link_libraries(quickfoil_utility_Hash
                      quickfoil_schema_TypeDefs
                      quickfoil_utility_Vector)
target_link_libraries(quickfoil_utility_Macros
                      glog)
target_link_libraries(quickfoil_utility_Vector
                      folly)
target_link_libraries(quickfoil_utility_StringUtil
                      quickfoil_utility_Vector)
