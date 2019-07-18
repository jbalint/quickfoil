add_library(quickfoil_schema_FoilClause FoilClause.cpp FoilClause.hpp)
add_library(quickfoil_schema_FoilLiteral ../empty_src.cpp FoilLiteral.hpp)
add_library(quickfoil_schema_FoilParser ../empty_src.cpp FoilParser.hpp)
add_library(quickfoil_schema_FoilPredicate ../empty_src.cpp FoilPredicate.hpp)
add_library(quickfoil_schema_FoilVariable ../empty_src.cpp FoilVariable.hpp)
add_library(quickfoil_schema_TypeDefs ../empty_src.cpp TypeDefs.hpp)

target_link_libraries(quickfoil_schema_FoilClause
                      glog
                      quickfoil_memory_Buffer
                      quickfoil_schema_FoilLiteral
                      quickfoil_types_TypeID
                      quickfoil_types_TypeTraits
                      quickfoil_utility_Macros
                      quickfoil_utility_Vector)
target_link_libraries(quickfoil_schema_FoilLiteral
                      glog
                      quickfoil_schema_FoilPredicate
                      quickfoil_schema_FoilVariable
                      quickfoil_utility_Hash
                      quickfoil_utility_Macros
                      quickfoil_utility_Vector)
target_link_libraries(quickfoil_schema_FoilParser
                      glog
                      quickfoil_schema_FoilClause
                      quickfoil_schema_FoilLiteral
                      quickfoil_schema_FoilPredicate
                      quickfoil_schema_FoilVariable
                      quickfoil_utility_Macros
                      quickfoil_utility_StringUtil
                      quickfoil_utility_Vector)
target_link_libraries(quickfoil_schema_FoilPredicate
                      glog
                      quickfoil_storage_TableView
                      quickfoil_utility_Macros
                      quickfoil_utility_Vector)
target_link_libraries(quickfoil_schema_FoilVariable
                      quickfoil_utility_Macros)

add_executable(quickfoil_schema_FoilLiteral_test
               FoilLiteral_test.cpp)
               
target_link_libraries(quickfoil_schema_FoilLiteral_test
                      gtest
                      gtest_main
                      quickfoil_schema_FoilLiteral
                      quickfoil_schema_FoilParser)

add_test(quickfoil_schema_FoilLiteral_test quickfoil_schema_FoilLiteral_test)
