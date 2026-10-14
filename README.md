# Ring SSB Modulator (VST3, Windows)

## Obtenir le .vst3 (sans rien installer en local)
1. Créez un dépôt GitHub vide, poussez ce dossier (git init / add / commit / push).
2. Onglet Actions > "Build VST3 Windows" (se lance seul au push).
3. Téléchargez l'artifact "RingSSB-VST3-Windows" : il contient "Ring SSB Modulator.vst3".
4. Copiez ce dossier dans C:\Program Files\Common Files\VST3\ puis rescannez les plugins dans votre DAW.

## Compiler en local
Prérequis : Visual Studio 2022 (charge de travail C++), CMake, Git.
    cmake -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release
Résultat : build\RingThingSSB_artefacts\Release\VST3\
