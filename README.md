# Wiga Ring Think (VST3, Windows) - personal use

## Repository layout (everything at the ROOT of the repo, no sub-folder except .github)
    CMakeLists.txt
    PluginProcessor.h / PluginProcessor.cpp
    PluginEditor.h / PluginEditor.cpp
    background.png                (finished pedal picture, 993 x 822)
    led_on.png / led_off.png      (preset ladder LEDs)
    led_preset_on/off.png, led_status_on/off.png
    key_rm_on/off.png, key_ub_on/off.png, key_lb_on/off.png   (mode keys with label + LED)
    pitchshift_white.png / pitchshift_yellow.png
    .github/workflows/build.yml

## Get the .vst3
1. Create a NEW empty GitHub repository, then "Add file > Upload files" and drop the files above.
2. Create .github/workflows/build.yml by hand if the upload skipped the hidden folder
   (Add file > Create new file, type the path .github/workflows/build.yml and paste the file content).
3. Actions tab > "Build VST3 Windows" > open the green run > Artifacts > WigaRingThink-VST3-Windows.
4. Unzip, copy "Wiga Ring Think.vst3" to C:\Program Files\Common Files\VST3\ and rescan plugins in your DAW.

## User presets
Saved in %APPDATA%\WigaRingThink\user_presets.xml (shared by every project).
