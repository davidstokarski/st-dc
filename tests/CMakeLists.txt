# Whole-plugin tests: engine curves / timing / nulls, processor stress, state, Drum Multiview (with the installed
# ST Sat / ST EQ AUs loaded in-process) and editor snapshots (PNG).
#   StDcPluginTests [--snapshot <dir>] [--video <dir> [scene ...]]
# Links the plugin's shared-code library, so it exercises the real processor.
juce_add_console_app(StDcPluginTests PRODUCT_NAME "StDcPluginTests")
target_sources(StDcPluginTests PRIVATE PluginTests.cpp VideoRender.cpp)
target_compile_features(StDcPluginTests PRIVATE cxx_std_20)
target_link_libraries(StDcPluginTests PRIVATE STDC)
target_include_directories(StDcPluginTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../Source")
target_include_directories(StDcPluginTests PRIVATE $<TARGET_PROPERTY:STDC,INCLUDE_DIRECTORIES>)
target_compile_definitions(StDcPluginTests PRIVATE $<TARGET_PROPERTY:STDC,COMPILE_DEFINITIONS> $<TARGET_PROPERTY:STDC,INTERFACE_COMPILE_DEFINITIONS>)
if(APPLE)
    target_link_libraries(StDcPluginTests PRIVATE "-framework AudioToolbox")
endif()
