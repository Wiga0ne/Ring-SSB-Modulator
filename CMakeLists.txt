cmake_minimum_required(VERSION 3.22)
project(RingThingSSB VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(RingThingSSB
  COMPANY_NAME "Custom"
  PLUGIN_MANUFACTURER_CODE Cstm
  PLUGIN_CODE RtSb
  FORMATS VST3
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  PRODUCT_NAME "Ring SSB Modulator"
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(RingThingSSB PRIVATE
  Source/PluginProcessor.cpp Source/PluginEditor.cpp)

target_compile_definitions(RingThingSSB PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RingThingSSB
  PRIVATE juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
