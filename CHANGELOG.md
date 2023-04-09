# Changelog
All notable changes to this project will be documented in this file.

# Release 2.1.1
- Fixed packaging issue due to out-dated MetaHuman animation blueprint.
- Fixed rare crash on End play in the editor.

# Release 2.1.0
### New Features:
- Added MetaHuman animation blueprints.
- Added demo map for Action API.
- Added compatibility with OVR lip-sync.
- Added `OnVisemesReady` event and `GetVisemes` BP function to Convai Chatbot component.
- Added `GetAPIKey` and `SetAPIKey` BP functions.
- Added `IsTalking` and `IsProcessing` BP functions.
- Added `ConvaiGetLookedAtCharacter` BP function.
- Added Better logs and error messages.
- Character details are automatically fetched whenever a new `character ID` is set inside the Convai Chatbot component.
- Added `Character Name`, `Backstory`, `Voice` and `ReadyPlayerMe link` variables to the chatbot component which are automatically populated every time a new `character ID` is set.
- Environment objects can now be set on the Convai chatbot component without the need to explicitly specify them in `Send Text` or `Start Talking` BP functions.

### Improvements:
- Improved backward compatibility with projects that implemented V1.x of the plugin.

### Bug Fixes:
- Fixed conflicts with the Firebase blueprint library plugin.
- Fixed Session ID not updating properly.
- Fixed bug where stop talking won't be called as intended.
- Fixed warning due to improper initialization of `OptionalPositionVector` variable in the `ConvaiAction` structure.
- Fixed packaging issue on UE 5.1 due to conflicts with the gRPC library.
- Fixed bug where stop talking wont be called as intended.
- Fixed an editor crash with gRPC.

# Release 2.0.0
### New Features:
- Added gRPC streaming for a huge latency reduction.
- Added Convai Chatbot and Player components.
- Added various events

### Deprecations:
- Deprecated all functions that work off the REST API.

# Release 1.0.0
- Added REST API blueprint functions for Convai.