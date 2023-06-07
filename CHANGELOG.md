# Changelog
All notable changes to this project will be documented in this file.
# Release 2.4.0
**Added**
- Added Unreal Engine 5.2 Support.
- Added 'IsListening' function to detect when the character is actively listening to the player.
- Enhanced Mic Controls: Added the 'SetMicrophoneVolumeMultiplier' and 'GetMicrophoneVolumeMultiplier' functions to allow better control over audio settings. A mic gain slider has also been introduced for easy adjustments to the Mic widget.
- Extended Language Support: Increased Voice Capture Sample Rate to 16kHz, improving support for Korean Speech-To-Text.

**Updated**
- User Interface: The UI is now only initialized if the blueprint is controlled by a player, providing a smoother user experience.
- Blueprint Organization: Reorganized blueprints for easier navigation, complete with improved commentary for better understanding.
- Animation Tuning: The talking animation has been toned down to offer a more natural visual experience.

**Fixed**
- Player Component Acquisition: Resolved an issue where obtaining the player component in the mic widget was problematic.
- Packaging Issues: Addressed packaging issues with ConvaiOVRLipSync and a problem where fonts disappeared after packaging.
- Texture Display: Fixed a grey texture issue with the ReadyPlayerMe character in the demo project.
- Crash Issues: Fixed instances where crashes occurred due to logging an invalid variable and during mic recordings when no audio data was available.
- Component Initialization: Fixed ConvaiAudioComponent initialization errors.
- Missing Player Controller in the Demo Map.

# Release 2.3.0
**Added**
- **Beta** Android support.
- Audio permission on Android.
- Mini-Example demo project.
- Mic input device selection functionalities:
    - SetCaptureDeviceByName.
    - SetCaptureDeviceByIndex.
    - GetActiveCaptureDevice.
    - GetAvailableCaptureDeviceNames.
    - GetAvailableCaptureDeviceDetails.
    - GetCaptureDeviceInfo.
    - GetDefaultCaptureDeviceInfo.
- Mic settings menu widget blueprint with the `ConvaiBasePlayer` blueprint.
- language code and avatar image link outputs to the `ConvaiGetCharacterDetails` function.
- `Convai Download Image` function to download images.
- Convai attribution logo widget.

**Updated**
- Audio capturing by implementing `ConvaiAudioCaptureComponent` to replace `IVoiceModule` for audio capture.
- `glTFRuntime` in the MetaHuman demo project and `ConvaiReadyPlayerMe` plugin bundle.

**Fixed**
- C++ project error on restart.
- Microphone issues on some clients.
- Dark face for ReadyPlayerMe avatars.

# Release 2.2.0
**Added**
- `ConvaiPlayerWithVoiceActivation` blueprint which adds voice activation functionality instead of push-to-talk.

**Updated**
- MetaHuman face animation now looks more natural.
- MetaHuman lip-sync is smoothed out to avoid rough transitions.

# Release 2.1.2
**Added**
- Character voice interrupts with fading.
- `Interrupt Voice Fade Out Duration` variable to control voice fade duration and was added to the `Convai Chatbot` component.
- `Interrupt Speech` BP function to force a speech interruption, and was added to `Convai Chatbot` component.

**Fixed**
- Bug where the character would stop responding when being talked to via text then voice.
- Bug where voice was not being attenuated when traveling far away from the character.

**Updated**
- `ConvaiBasePlayer` blueprint to allow players to talk at their own pace.

# Release 2.1.1
**Fixed**
- Packaging issue due to out-dated MetaHuman animation blueprint.
- Rare crash on End play in the editor.

# Release 2.1.0
**Added**
- MetaHuman animation blueprints.
- demo map for Action API.
- compatibility with OVR lip-sync.
-  `OnVisemesReady` event and `GetVisemes` BP function to Convai Chatbot component.
- `GetAPIKey` and `SetAPIKey` BP functions.
- `IsTalking` and `IsProcessing` BP functions.
- `ConvaiGetLookedAtCharacter` BP function.
- Better logs and error messages.
- `Character Name`, `Backstory`, `Voice` and `ReadyPlayerMe link` variables to the chatbot component which are automatically populated every time a new `character ID` is set.
- Environment objects can now be set on the Convai chatbot component without the need to explicitly specify them in `Send Text` or `Start Talking` BP functions.

# Release 2.0.5
**Fixed**
- Conflicts with the Firebase blueprint library plugin.
- Session ID not updating properly.
- Bug where stop talking won't be called as intended.

# Release 2.0.4
**Fixed**
- Warning due to improper initialization of `OptionalPositionVector` variable in the `ConvaiAction` structure.
- Packaging issue on UE 5.1 due to conflicts with the gRPC library.

# Release 2.0.3
**Fixed**
- Bug where stop talking wont be called as intended.

# Release 2.0.2
**Added**
- Backward compatibility with projects that implemented V1.x of the plugin.

# Release 2.0.1
**Fixed**
- Editor crash with gRPC.

# Release 2.0.0
**Added**
- gRPC streaming for a huge latency reduction.
- Convai Chatbot and Player components.
- Various events and blueprint functions with the new components.

**Deprecated**
- Deprecated all functions that work off the REST API.

# Release 1.0.0
**Added**
- REST API blueprint functions for Convai.
