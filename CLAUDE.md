# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Shadow Bubble: Hide and Seek Chaos** is a 3-5 player asymmetric multiplayer hide-and-seek game built with Unreal Engine 5.6. This is a Global Game Jam 2025 project featuring dynamic lighting mechanics where hiders (cats) can become invisible in shadows while seekers (ghosts) hunt them using flashlights and bubble guns.

## Development Commands

### Building the Project
```bash
# Build the project (requires UE5.6 and Visual Studio)
# Open Paw.uproject in Unreal Editor to build automatically
# Or use Unreal Build Tool from command line:
# UnrealBuildTool.exe Paw Win64 Development -Project="Paw.uproject"
```

### Opening the Project
```bash
# Open in Unreal Editor 5.6
# Double-click Paw.uproject or use:
# UnrealEditor.exe "Paw.uproject"
```

### Compilation
- The project uses both C++ and Blueprints
- C++ code must be compiled through Visual Studio or Unreal Editor
- Blueprint changes are compiled automatically in the editor

## Core Architecture

### Project Structure
- **Source/Paw/**: C++ source code for core game systems
- **Content/Main/**: Primary game content including Blueprints, maps, and assets
- **Config/**: Unreal Engine configuration files
- **Plugins/**: Third-party plugins (OnlineSubsystemSteam, BlueprintAssist, etc.)

### Key Systems

#### Character System (Asymmetric Roles)
- **Base Classes**: `PawCharacterBase`, `PawPlayerBase`, `PawBattleCharacter`
- **Seeker Role**: `PawPlayerSeeker`, `PawPlayerSeeker_Ghost` - Ghost characters with weapons
- **Hider Role**: Implemented primarily in `BP_Player_Hider_Cat` - Cat characters with stealth abilities

#### Weapon System
- **Base**: `PawWeaponBase` with component-based architecture
- **Components**: `PawWeaponComponent`, `PawCharacterWeaponComponent`, `PawGunComponent`
- **Primary Weapon**: `PawWeapon_BubbleBlaster` - Fires bubble projectiles to capture hiders
- **Utility**: `PawFlashLightComponent` - Light source for seekers

#### Bubble Mechanics (Core Game Feature)
- **Base**: `PawBubbleBase` - Foundation for all bubble systems
- **Types**: 
  - `PawBubbleLight` - Interactive light sources that hiders must pop to win
  - `PawBubbleHiderCapture` - Traps that capture hiders when hit by bubble gun
  - `PawBubblePhysicsObject` - Movable bubble blocks that create shadows

#### Multiplayer Architecture
- **Session Management**: `PawMultiplayerSessionSubsystem` - Handles Steam lobby creation/joining
- **Game Mode**: `BP_DemoGameMode` - Main game logic with role assignment
- **Online Subsystem**: Steam integration for matchmaking and lobbies

#### Environmental Interaction
- **Interface**: `PawCollideBreakableInterface` - Defines breakable object behavior
- **Base Class**: `PawGameplayElementBase` - Foundation for interactive environment objects
- **Destruction**: Uses Chaos Destruction system for breakable objects

### Key Maps
- **L_MainMenu**: Main menu and lobby system
- **L_Demo**: Primary gameplay level with dynamic lighting setup
- **LI_Light**: Lighting reference level showcasing Lumen effects

### Technical Features
- **Lighting**: Uses UE5 Lumen for real-time global illumination and dynamic shadows
- **Physics**: Chaos Destruction for breakable objects and bubble physics
- **Rendering**: Virtual Shadow Maps enabled for high-quality shadows
- **Graphics**: DirectX 12 with shader model 6 support

## Development Guidelines

### C++ Code Patterns
- Use component-based architecture following UE5 patterns
- Inherit from appropriate Paw base classes (`PawCharacterBase`, `PawWeaponBase`, etc.)
- Implement interfaces for common behaviors (`PawCollideBreakableInterface`)
- Follow Unreal naming conventions (classes prefixed with `Paw`, interfaces with `I`)

### Blueprint Integration
- C++ classes expose functionality via `UPROPERTY` and `UFUNCTION` macros
- Core logic in C++, gameplay tweaking and content creation in Blueprints
- Animation logic handled primarily in Animation Blueprints (`ABP_Player_Hider_Cat`, etc.)

### Multiplayer Considerations
- All gameplay elements must work in networked environment
- Use proper replication for multiplayer state
- Steam Online Subsystem handles lobby and session management
- Player roles (Seeker/Hider) assigned by game mode

### Asset Organization
- Core blueprints in `Content/Main/`
- Input mappings in `Content/Main/Input/`
- UI widgets prefixed with `WBP_`
- Maps prefixed with `L_` (levels) or `LI_` (level instances)

## Testing and Debugging

### Multiplayer Testing
- Use Standalone Game mode for network testing
- Steam integration requires valid Steam app ID
- Test both LAN and online connectivity

### Performance Considerations
- Dynamic lighting (Lumen) is performance-intensive
- Virtual Shadow Maps require modern graphics hardware
- Physics-heavy bubble interactions need optimization for multiplayer

## Git LFS Safety Guidelines

### CRITICAL: Protecting LFS Files
This project uses Git LFS extensively for Unreal Engine assets (.uasset, .umap, etc.). **ALWAYS** follow these guidelines:

#### Before Any Git Operations
1. **Check LFS status first**: `git lfs status`
2. **Verify .gitattributes** is properly configured with explicit LFS attributes
3. **Never use shorthand LFS attributes** - always use full specification

#### Safe Git Commands for UE Projects
- Use `git lfs pull` instead of `git pull` when fetching
- Use `git lfs checkout` after switching branches
- Run `git lfs fsck` periodically to verify LFS integrity
- **Avoid `git add .`** on large UE projects without checking what's being added

#### If LFS Files Become Corrupted
1. Check what files lost LFS tracking: `git lfs ls-files`
2. Remove corrupted files from index: `git rm --cached [files]`
3. Re-add them to restore LFS tracking: `git add [files]`
4. Commit the fix: `git commit -m "Fix LFS tracking"`

#### Prevention
- Always set up LFS hooks: `git lfs install --local`
- Use explicit LFS attributes in .gitattributes (not shorthand like `lock`)
- Never run operations that might trigger LFS conversion on UE asset files

## Dependencies

### Engine Version
- Unreal Engine 5.6 (required for Lumen features)

### Key Plugins
- OnlineSubsystemSteam (multiplayer)
- BlueprintAssist (development tool)
- BlockoutToolsPlugin (level design)
- AutoSizeComments (blueprint organization)

### Build Requirements
- Visual Studio 2022 with C++ development tools
- Windows SDK
- Steam SDK (for online features)