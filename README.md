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

## Prerequisites
Before starting, ensure you have the following installed:
- Python: [Download Python](https://www.python.org/downloads/)
- Git: [Download Git](https://git-scm.com/downloads)
- For Windows:
  - Visual Studio: Required for building the plugin. Refer to Unreal Engine's [Visual Studio Setup Guide](https://docs.unrealengine.com/en-US/Programming/Development/VisualStudioSetup) for detailed instructions.
  - For Android builds: Android Studio and the corresponding NDK. Refer to the [Setting Up Android SDK and NDK for Unreal](https://docs.unrealengine.com/4.27/en-US/SharingAndReleasing/Mobile/Android/Setup/) guide.
- For macOS:
  - Xcode: Required for building the plugin. Refer to the official Unreal Engine documentation for specific macOS development setup instructions.

## Overview
There are multiple ways to install the plugin:
- Install via the [marketplace](https://www.unrealengine.com/marketplace/en-US/product/convai)
- Download one of our ready made [releases](https://github.com/Conv-AI/Convai-UnrealEngine-SDK/releases) and copy them to
	```
	C:\Program Files\Epic Games\UE_5.x\Engine\Plugins\Marketplace
	```
	or for macOS:
	```
	/Users/your_user/UnrealEngine/UE_5.x/Engine/Plugins/Marketplace
	```
 - Build the plugin manually to get the latest updates and support for Custom Unreal Engine Versions, which you can do by [following below steps](#Building-the-plugin-from-source)
   
# Building the plugin from source - Method 1

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
     C:\Program Files\Epic Games\UE_5.x\Engine\Plugins\Marketplace
     ```
     or for macOS:
     ```
     /Users/your_user/UnrealEngine/UE_5.x/Engine/Plugins/Marketplace
     ```
3. Ensure the folder hierarchy in the plugins directory mirrors the structure in the `Output` folder. It should look like this:
   ```
   UE_5.x
   ├── Engine
       ├── Plugins
           ├── Convai
               ├── (plugin files and folders)
   ```

# Building the plugin from source - Method 2

1. Clone the plugin as described in [Method 1](#cloning-and-building-the-plugin) without running the build script.

2. Go to this [drive link](https://drive.google.com/drive/folders/1FPGNIY9qobROUekYnw8Fr1EKCY1RRqen?usp=sharing) and download `Content.zip` and `ThirdParty.zip`.

3. Copy the downloaded files into your cloned plugin folder (e.g., `Convai-UnrealEngine-SDK`) and extract them.

4. Open `\Source\Convai\Convai.Build.cs` with a text editor and change `bUsePrecompiled = true;` to `bUsePrecompiled = false;`.

5. Ensure your project is a C++ project by creating a new C++ class from the `Tools` menu. For conversion help, refer to [this Unreal forum thread](https://forums.unrealengine.com/t/is-it-possible-to-change-my-blueprint-project-to-a-c-project-in-ue5/540618/4).

6. Copy the cloned plugin folder to the `Plugins` folder in your project directory. Create a `Plugins` folder if it doesn't exist.

7. Restart your project to prompt a rebuild of the Convai plugin. Click **Yes** to rebuild the plugin, then after it opens navigate to `Edit->Plugins` - Note: if it gave you any errors then make sure you have the required [Prerequisites](#prerequisites).

8. In the plugins menu, search for `Convai` and enable it.

9. Restart the project.

The Convai plugin should now be installed and ready for use in your Unreal Engine project.


## Need Support or Have Questions?
If you encounter any issues or have questions, feel free to reach out to us at support@convai.com or join the community on [Discord](https://discord.gg/UVvBgV3xQ5) Our team is dedicated to providing you with the support you need to make the most out of Convai.
