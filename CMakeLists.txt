cmake_minimum_required(VERSION 3.22)
project(PsyRhythm VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PSY_BUILD_TESTS "Build engine tests (no JUCE needed)" ON)
option(PSY_BUILD_PLUGIN "Build the VST3 plugin (needs JUCE via network)" ON)

# ---- Testes do motor (C++ puro) ----
if(PSY_BUILD_TESTS)
  enable_testing()
  add_executable(engine_tests Tests/engine_tests.cpp)
  target_include_directories(engine_tests PRIVATE Source/Engine)
  add_test(NAME engine_tests COMMAND engine_tests)
endif()

# ---- Plugin ----
if(PSY_BUILD_PLUGIN)
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)

  juce_add_plugin(PsyRhythm
    COMPANY_NAME "YourCompany"            # <- troque
    PLUGIN_MANUFACTURER_CODE Psyr
    PLUGIN_CODE Prh1
    FORMATS VST3
    PRODUCT_NAME "PsyRhythm"              # nome de trabalho; troque por um nome próprio
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Generator
    COPY_PLUGIN_AFTER_BUILD FALSE)

  target_sources(PsyRhythm PRIVATE Source/Plugin/PluginProcessor.cpp Source/Plugin/PluginEditor.cpp)
  target_include_directories(PsyRhythm PRIVATE Source/Engine)

  # Logo oficial: usada somente se assets/logo.png existir (nunca é redesenhada/distorcida)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets/logo.png)
    juce_add_binary_data(PsyAssets SOURCES assets/logo.png)
    target_link_libraries(PsyRhythm PRIVATE PsyAssets)
    target_compile_definitions(PsyRhythm PRIVATE PSY_HAS_LOGO=1)
  else()
    target_compile_definitions(PsyRhythm PRIVATE PSY_HAS_LOGO=0)
    message(WARNING "assets/logo.png não encontrado: a build usa um espaço reservado no lugar da logo.")
  endif()

  target_compile_definitions(PsyRhythm PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
  target_link_libraries(PsyRhythm PRIVATE juce::juce_audio_utils juce::juce_recommended_warning_flags)
  set_target_properties(PsyRhythm PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
  message(STATUS "PsyRhythm: createPluginFilter esta em Source/Plugin/PluginProcessor.cpp; sem /GL")
  if(MSVC)
    target_compile_options(PsyRhythm PRIVATE /O2)
  endif()
endif()
