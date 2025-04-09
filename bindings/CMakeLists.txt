# QuickEd library C++ binding
add_library(quickedcpp STATIC cpp/quicked.cpp)
target_include_directories(quickedcpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
target_link_libraries(quickedcpp PRIVATE quicked)

# QuickEd library Python binding (based on C++ binding)
set(PYBIND11_FINDPYTHON ON)
add_subdirectory(python/pybind11)

pybind11_add_module(pyquicked python/quicked.cpp cpp/quicked.cpp)
set_target_properties(pyquicked PROPERTIES OUTPUT_NAME quicked)
target_include_directories(pyquicked PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
target_link_libraries(pyquicked PRIVATE quicked)

install(TARGETS pyquicked DESTINATION .)