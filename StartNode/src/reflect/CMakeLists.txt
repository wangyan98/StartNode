add_library(Reflect STATIC
    src/TypeRegistry.cpp
    src/Json.cpp
    src/Serialize.cpp
)
target_include_directories(Reflect PUBLIC include)
target_link_libraries(Reflect PUBLIC MetaCore)
