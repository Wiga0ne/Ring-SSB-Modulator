cmake_minimum_required(VERSION 3.22)
project(WigaRingThink VERSION 2.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# the pedal picture is embedded into the plugin (no external file needed at runtime)
juce_add_binary_data(RingAssets SOURCES background.png pitchshift_white.png pitchshift_yellow.png)

juce_add_plugin(RingThingSSB
  COMPANY_NAME "Wiga"
  PLUGIN_MANUFACTURER_CODE Wiga
  PLUGIN_CODE WgRt
  FORMATS VST3
  VST3_CATEGORIES Fx Modulation
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  PRODUCT_NAME "Wiga Ring Think"
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(RingThingSSB PRIVATE
  PluginProcessor.cpp
  PluginEditor.cpp)

target_compile_definitions(RingThingSSB PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RingThingSSB
  PRIVATE
    RingAssets
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
