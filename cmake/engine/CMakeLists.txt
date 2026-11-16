# The engine as a static library per define flavour. Definitions and JUCE
# modules are PUBLIC: JUCE needs a module's compile definitions to match
# between a library and what links it. The JUCE module sources travel with the
# modules to every target that links them, so the executables compile them
# (as they always did) and the libraries here are told not to: they hold the
# engine only, and the module symbols resolve when the executable links.
# (Source file properties are per directory, which is why this is its own.)
get_property(ilana_juce_modules GLOBAL PROPERTY _juce_module_names)
foreach (module IN LISTS ilana_juce_modules)
    get_target_property(module_sources ${module} INTERFACE_JUCE_MODULE_SOURCES)
    if (module_sources)
        set_source_files_properties(${module_sources} PROPERTIES HEADER_FILE_ONLY TRUE)
    endif()
endforeach()

function (ilana_engine name pluginName)
    add_library(${name} STATIC ${ILANA_ENGINE_SOURCES})
    target_include_directories(${name} PUBLIC "${PROJECT_SOURCE_DIR}/src" "${ILANA_GENERATED_DIR}")
    target_compile_definitions(${name} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Name="${pluginName}"
        ${ARGN}
    )
    target_link_libraries(${name}
        PUBLIC
            ilanaStretch
            ilanaWavetables
            juce::juce_dsp
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_recommended_config_flags
    )
    set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()


# Test-build engine flavours (createEditor returns nullptr, no GUI):
#   Test = the unit tests, Fp = TEST + fixed random seeds (fingerprints and the
#   tools that must repeat), FxBench = TEST as the effect (ilanaFxCpuBench).
ilana_engine(ilanaEngineTest   "ilanaSynth"    ILANA_TEST_BUILD=1)
ilana_engine(ilanaEngineFp     "ilanaSynth"    ILANA_TEST_BUILD=1 ILANA_FINGERPRINT_BUILD=1)
ilana_engine(ilanaEngineFxBench "ilanaSynth FX" ILANA_TEST_BUILD=1 ILANA_FX=1)

