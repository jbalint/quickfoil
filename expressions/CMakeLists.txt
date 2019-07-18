add_library(quickfoil_expressions_AttributeReference
            ../empty_src.cpp
            AttributeReference.hpp)
add_library(quickfoil_expressions_ComparisonOperators
            ../empty_src.cpp
            ComparisonOperators.hpp)
add_library(quickfoil_expressions_ComparisonPredicate
            ../empty_src.cpp
            ComparisonPredicate.hpp)
add_library(quickfoil_expressions_OperatorTraits ../empty_src.cpp OperatorTraits.hpp)

target_link_libraries(quickfoil_expressions_AttributeReference
                      glog
                      quickfoil_memory_Buffer
                      quickfoil_utility_Macros
                      quickfoil_utility_Vector)
target_link_libraries(quickfoil_expressions_ComparisonPredicate
                      quickfoil_expressions_AttributeReference
                      quickfoil_expressions_OperatorTraits
                      quickfoil_schema_TypeDefs
                      quickfoil_types_TypeID
                      quickfoil_types_TypeTraits
                      quickfoil_utility_BitVector
                      quickfoil_utility_BitVectorBuilder
                      quickfoil_utility_Macros)
target_link_libraries(quickfoil_expressions_OperatorTraits
                      quickfoil_expressions_ComparisonOperators)
