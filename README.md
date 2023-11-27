# Convai Unreal Engine SDK Plugin

## Overview
Convai brings advanced AI-driven interaction to Unreal Engine, empowering NPCs to engage in realistic conversations with players and respond intelligently to verbal commands and environmental events. Convai integrates seamlessly with various character assets and multiple language support.

### Key Highlights
- **Intelligent Conversation**: Real-time, open-ended conversation capabilities that make interactions more natural and dynamic.
- **Multilingual Support**: Engage with players in multiple languages, broadening the reach of your game.
- **Advanced Actions**: NPCs can interpret their surroundings and respond to conversations, verbal commands, and events, paving the way for innovative gameplay scenarios.
- **Knowledge Base**: Equip your characters with extensive knowledge, making them lore experts or embedding them deeply into the game's world.
- **Multiplayer Interaction**: Facilitates voice chat between multiple players and characters, enhancing the multiplayer experience.
- **Intelligent Animations**: Integration with lip-sync and comprehensive facial and body animations, synchronized with conversation flow.

### Resources for Ease of Use
- [Marketplace Link](https://www.unrealengine.com/marketplace/en-US/product/Convai)
- [Extensive Documentation](https://docs.Convai.com/api-docs-restructure/plugins-and-integrations/unreal-engine)
- [Quick Start Video](https://www.youtube.com/watch?v=HHJvY9dmwwg)
- [Metahuman Demo Project](https://drive.google.com/drive/u/4/folders/1HNcghI9SG1NpCUaJWRX9Yh28HUF00-U0)

### Compatibility and Integration
The plugin is compatible with a wide range of character and avatar assets, including Metahumans, Daz3D, Reallusion, ReadyPlayerMe, Avaturn and more. Both humanoid and non-humanoid assets can be seamlessly integrated.

### Standalone Components
- **Text to Speech & Speech to Text**: Empower your characters with realistic speech capabilities.
- **Dynamic Character Updates**: Modify characters’ personalities, backstories, and knowledge in real-time, based on in-game events.

### Coming Soon
- **Expansion to Other Platforms**: Including iOS and Linux.
- **Long-term Memory for Characters**: Enabling NPCs to remember past interactions for more immersive gameplay.

# Installation

## Overview
This guide provides detailed instructions on how to build and install the Convai Unreal Engine plugin.

## Prerequisites
Before starting, ensure you have the following installed:
- Python: [Download Python](https://www.python.org/downloads/)
- Git: [Download Git](https://git-scm.com/downloads)
- Unreal Engine 5.x: Ensure you have Unreal Engine 5.0 or later installed.
- For Windows:
  - Visual Studio: Required for building the plugin. Refer to Unreal Engine's [Visual Studio Setup Guide](https://docs.unrealengine.com/en-US/Programming/Development/VisualStudioSetup) for detailed instructions.
  - For Android builds: Android Studio and the corresponding NDK. Refer to the [Setting Up Android SDK and NDK for Unreal](https://docs.unrealengine.com/4.27/en-US/SharingAndReleasing/Mobile/Android/Setup/) guide.
- For macOS:
  - Xcode: Required for building the plugin. Refer to the official Unreal Engine documentation for specific macOS development setup instructions.

## Cloning and Building the Plugin
1. Open a command prompt or terminal.
2. Clone the repository:
   ```
   git clone https://github.com/Conv-AI/Convai-UnrealEngine-SDK.git
   ```
3. Change directory to the cloned repository:
   ```
   cd Convai-UnrealEngine-SDK
   ```
4. Run the build script with the Unreal Engine directory as the first argument:

     ```
     python Build.py [Unreal Engine Directory] [Additional Flags]
     ```

   Add `-TargetPlatforms=Win64` to build specifically for Windows. For building on Windows with support for both Windows and Android, no extra flags are needed.
     ```
	 // Example: Windows only Build for Unreal Engine 5.3.
     python Build.py "C:\Program Files\Epic Games\UE_5.3" -TargetPlatforms=Win64 
     ```

## Installing the Plugin
1. After building the plugin, locate the output files in the `Output` folder within the repository directory.
2. Copy the entire plugin folder to the Unreal Engine's plugins directory:
   - For a vanilla installation, this is typically located at:
     ```
     C:\Program Files\Epic Games\UE_5.x\Engine\Plugins
     ```
     or for macOS:
     ```
     /Users/your_user/UnrealEngine/UE_5.x/Engine/Plugins
     ```
3. Ensure the folder hierarchy in the plugins directory mirrors the structure in the `Output` folder. It should look like this:
   ```
   UE_5.x
   ├── Engine
       ├── Plugins
           ├── Convai
               ├── (plugin files and folders)
   ```

## Need Support or Have Questions?
If you encounter any issues or have questions, feel free to reach out to us at support@convai.com or join the community on [Discord](https://discord.gg/UVvBgV3xQ5) Our team is dedicated to providing you with the support you need to make the most out of Convai.