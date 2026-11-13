# The JUCE part of the build: the plug-in (AU MIDI FX, VST3, Standalone) and the developer tools.
# JUCE_DIR: the path of a JUCE 8 checkout (developed with tag 8.0.8).
#   cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
# Without it, the environment variable JUCE_DIR is used, then a JUCE folder next to this project,
# then JUCE in the Developer folder of the home directory.
if(NOT JUCE_DIR)
  foreach(candidate "$ENV{JUCE_DIR}" "${CMAKE_SOURCE_DIR}/JUCE" "${CMAKE_SOURCE_DIR}/../JUCE" "$ENV{HOME}/Developer/JUCE")
    if(candidate AND EXISTS "${candidate}/CMakeLists.txt")
      set(JUCE_DIR "${candidate}")
      break()
    endif()
  endforeach()
endif()
set(JUCE_DIR "${JUCE_DIR}" CACHE PATH "Path of the JUCE 8 checkout")
if(NOT JUCE_DIR OR NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "JUCE not found. Get JUCE 8.0.8 (git clone --branch 8.0.8 https://github.com/juce-framework/JUCE.git) and configure with -DJUCE_DIR=<path>.")
endif()
message(STATUS "JUCE: ${JUCE_DIR}")
add_subdirectory("${JUCE_DIR}" JUCE)

# Audio Unit only on macOS. Everywhere: VST3 and the standalone application.
if(APPLE)
  set(KINDRED_FORMATS AU VST3 Standalone)
else()
  set(KINDRED_FORMATS VST3 Standalone)
endif()
set(KINDRED_ROOT ${CMAKE_SOURCE_DIR})

juce_add_plugin(${KINDRED_TARGET}
  COMPANY_NAME "${KINDRED_COMPANY}"
  PLUGIN_MANUFACTURER_CODE ${KINDRED_MFR}
  PLUGIN_CODE ${KINDRED_CODE}
  FORMATS ${KINDRED_FORMATS}
  PRODUCT_NAME "${KINDRED_PRODUCT_NAME}"
  BUNDLE_ID ${KINDRED_BUNDLE}
  VERSION ${KINDRED_VERSION}
  BUILD_VERSION "${KINDRED_VERSION}.${KINDRED_BUILD}"
  IS_SYNTH FALSE
  IS_MIDI_EFFECT TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE          # installed by tools/build.sh, only after validation
  AU_MAIN_TYPE kAudioUnitType_MIDIProcessor
  VST3_CATEGORIES Fx Generator
  MICROPHONE_PERMISSION_ENABLED FALSE)

file(GLOB KINDRED_STYLES CONFIGURE_DEPENDS ${KINDRED_ROOT}/styles/*.json)
juce_add_binary_data(KindredData SOURCES
  ${KINDRED_STYLES}
  ${KINDRED_ROOT}/fonts/Inter-Regular.ttf ${KINDRED_ROOT}/fonts/Inter-Medium.ttf ${KINDRED_ROOT}/fonts/Inter-SemiBold.ttf
  ${KINDRED_ROOT}/fonts/InterDisplay-SemiBold.ttf ${KINDRED_ROOT}/fonts/InstrumentSerif-Italic.ttf)

set(KINDRED_PLUGIN_SOURCES
  ${KINDRED_ROOT}/src/plugin/Session.cpp ${KINDRED_ROOT}/src/plugin/PluginProcessor.cpp
  ${KINDRED_ROOT}/src/ui/PluginEditor.cpp)
set(KINDRED_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_MODAL_LOOPS_PERMITTED=1)

target_sources(${KINDRED_TARGET} PRIVATE ${KINDRED_PLUGIN_SOURCES})
target_include_directories(${KINDRED_TARGET} PRIVATE ${KINDRED_ROOT}/src)
target_compile_definitions(${KINDRED_TARGET} PUBLIC ${KINDRED_DEFS})
target_link_libraries(${KINDRED_TARGET} PRIVATE
  kindred_engine KindredData juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ---- developer tools (not shipped) --------------------------------------------------------------
# KindredSnapshot: builds the processor and editor in-process and writes PNGs of the editor at
# several sizes and states, with paint timings.
juce_add_console_app(KindredSnapshot PRODUCT_NAME "KindredSnapshot")
target_sources(KindredSnapshot PRIVATE ${KINDRED_ROOT}/tools/Snapshot.cpp ${KINDRED_PLUGIN_SOURCES})
target_include_directories(KindredSnapshot PRIVATE ${KINDRED_ROOT}/src)
target_compile_definitions(KindredSnapshot PRIVATE ${KINDRED_DEFS}
  JucePlugin_Name="${KINDRED_PRODUCT_NAME}" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=1
  JucePlugin_ProducesMidiOutput=1 JucePlugin_IsMidiEffect=1 JucePlugin_Build_Standalone=0)
target_link_libraries(KindredSnapshot PRIVATE kindred_engine KindredData juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags)

# KindredHostTest: loads the BUILT plug-in (.vst3 or .component) through JUCE's hosting, runs a
# transport (start, stop, loop, locate, tempo change) and checks every MIDI message it sends.
juce_add_console_app(KindredHostTest PRODUCT_NAME "KindredHostTest")
target_sources(KindredHostTest PRIVATE ${KINDRED_ROOT}/tools/HostTest.cpp)
target_compile_definitions(KindredHostTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
  JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
if(APPLE)
  target_compile_definitions(KindredHostTest PRIVATE JUCE_PLUGINHOST_AU=1)
endif()
target_link_libraries(KindredHostTest PRIVATE juce::juce_audio_processors juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags)

# KindredUiTest: clicks through the editor with synthetic mouse and key events.
juce_add_console_app(KindredUiTest PRODUCT_NAME "KindredUiTest")
target_sources(KindredUiTest PRIVATE ${KINDRED_ROOT}/tools/UiTest.cpp ${KINDRED_PLUGIN_SOURCES})
target_include_directories(KindredUiTest PRIVATE ${KINDRED_ROOT}/src)
target_compile_definitions(KindredUiTest PRIVATE ${KINDRED_DEFS}
  JucePlugin_Name="${KINDRED_PRODUCT_NAME}" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=1
  JucePlugin_ProducesMidiOutput=1 JucePlugin_IsMidiEffect=1 JucePlugin_Build_Standalone=0)
target_link_libraries(KindredUiTest PRIVATE kindred_engine KindredData juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags)
