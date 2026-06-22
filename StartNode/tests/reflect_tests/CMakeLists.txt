add_executable(reflect_tests
    src/TestMain.cpp
    src/TestMetaCore.cpp
    src/TestTypeRegistry.cpp
    src/TestJson.cpp
    src/TestSerialize.cpp
    src/TestFormat.cpp
    src/TestTypeId.cpp
    src/TestTypeIdCrossTU_A.cpp
    src/TestTypeIdCrossTU_B.cpp
)

target_link_libraries(reflect_tests PRIVATE Reflect GTest::gtest_main)

# Reflection flag is applied transitively via MetaCore, but the test TU
# itself directly uses ^^T reflection, so ensure -freflection is on.
target_compile_options(reflect_tests PRIVATE -freflection)

include(GoogleTest)
gtest_discover_tests(reflect_tests)
