# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Standard Workflow
1. First think through the problem, read the codebase for relevant files, and write a plan to tasks/todo.md.
2. The plan should have a list of todo items that you can check off as you complete them
3. Before you begin working, check in with me and I will verify the plan.
4. Then, begin working on the todo items, marking them as complete as you go.
5. Please every step of the way just give me a high level explanation of what changes you made
6. Make every task and code change you do as simple as possible. We want to avoid making any massive or complex changes. Every change should impact as little code as possible. Everything is about simplicity.
7. Finally, add a review section to the [todo.md](http://todo.md/) file with a summary of the changes you made and any other relevant information.

## Project Overview

**Shadow Bubble: Hide and Seek Chaos** is a 5 player asymmetric multiplayer hide-and-seek game built with Unreal Engine 5.6. This is a Global Game Jam 2025 project featuring dynamic lighting mechanics where hiders (cats) can become invisible in shadows while seekers (ghosts) hunt them using flashlights and bubble guns.

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

### Technical Features
- **Physics**: Chaos Destruction for breakable objects and bubble physics
- **Rendering**: Virtual Shadow Maps enabled for high-quality shadows
- **Graphics**: DirectX 12 with shader model 6 support

## Development Guidelines

### C++ Code Patterns
- Use component-based architecture following UE5 patterns
- Inherit from appropriate Paw base classes (`PawCharacterBase`, `PawWeaponBase`, etc.)
- Implement interfaces for common behaviors (`PawCollideBreakableInterface`)
- Follow Unreal naming conventions (classes prefixed with `Paw`, interfaces with `I`)

### C++ Code Style Rules
- **Always use braces**: All if statements, loops, and control structures must use `{}` braces, even for single-line statements
- **Always use newline braces**: All opening braces `{` must be placed on a new line (Allman/BSD brace style)
- **Use early returns**: Prefer early validation returns over nested if statements to improve readability
- **UObject validation**: Use `IsValid(Object)` instead of simple null checks (`if (Object)`) for all UObject-derived pointers
- **Examples**:
  ```cpp
  // Correct: Newline braces and early return
  void SomeFunction()
  {
      if (!IsValid(SomeObject))
      {
          return;
      }
      
      if (!bSomeCondition)
      {
          UE_LOG(LogTemp, Error, TEXT("Condition failed"));
          return;
      }
      
      // Main logic here
      DoSomething();
  }
  
  // Correct: Use IsValid for UObject pointers
  if (IsValid(SomeActor))
  {
      SomeActor->DoSomething();
  }
  
  // Incorrect: Same-line braces and nested ifs
  void SomeFunction() {
      if (IsValid(SomeObject)) {
          if (bSomeCondition) {
              DoSomething();
          } else {
              UE_LOG(LogTemp, Error, TEXT("Condition failed"));
          }
      }
  }
  
  // Incorrect: Missing braces
  if (bCondition)
      DoSomething();
  ```

### Code Modification and Collaboration Rules
- **Respect user modifications**: Never automatically revert user renames, refactoring, or code changes to Claude-generated versions
- **Preserve user intent**: Always maintain user naming conventions, architectural decisions, and code structure choices
- **Ask before conflicting changes**: When user modifications conflict with suggestions, ask for clarification rather than overwriting
- **Prevent code duplication**: Always check for existing similar functionality before creating new code
- **Promote reusability**: Extract common patterns into reusable helper functions and shared systems
- **Use shared resources**: Prefer shared timers, shared logic, and shared systems over duplicated implementations
- **Blueprint exposure**: Ask user if they want configurable values exposed to Blueprint before adding UPROPERTY macros
- **Modern pointers**: Always use TObjectPtr instead of raw pointers for UObject references
- **Examples**:
  ```cpp
  // Good: User renamed function, respect it
  void StartHealthEffectTimer() { /* keep user's improved name */ }
  
  // Good: Shared timer for related functionality
  FTimerHandle HealthEffectTimerHandle; // One timer for damage+healing
  
  // Bad: Duplicate similar functionality
  void StartDamageTimer() { /* ... */ }
  void StartHealTimer() { /* same interval, separate timer */ }
  
  // Good: Reusable helper function
  bool IsValidForHealthEffect() const
  {
      return HasAuthority() && IsAlive();
  }
  
  // Bad: Duplicated validation code
  if (HasAuthority() && IsAlive()) { /* repeated everywhere */ }
  ```

### Code Organization by Class Size

#### Classes Under 250 Lines:
- **Use minimal commenting** with clean structure
- **No section header comments** (remove detailed headers like "public: // Constructor & Public Engine Overrides")
- **No interface grouping comments** (remove all "//~ Interface" and "//~ End Interface" comments)
- **Maintain logical organization** without verbose headers
- **Use two separate `protected:` sections** (functions first, then properties)

#### Classes Over 250 Lines:
- **Use detailed section headers** for navigation
- **No interface grouping comments** (remove all "//~ Interface" and "//~ End Interface" comments)
- **Clear section boundaries** with comment headers
- **Use two separate `protected:` sections** with headers

#### Standard Class Structure (All Sizes):
1. **public:** Constructor & Public Engine Overrides
2. **public:** Blueprint-Callable API
3. **public:** C++ Public Helpers
4. **public:** Blueprint Events & Delegates
5. **protected:** Protected Engine Overrides (functions only)
6. **protected:** Properties (UPROPERTY variables only)
7. **protected:** Networking (RPCs & RepNotifies)
8. **private:** Internal Helper Methods
9. **private:** Cached State

#### Example for Classes Under 250 Lines:
```cpp
UCLASS()
class PAW_API APawTPPlayer : public APawPlayerBase
{
	GENERATED_BODY()

public:
	APawTPPlayer();
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	UCameraComponent* GetThirdPersonCameraComponent() const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* CameraBoom;
};
```

#### Example for Classes Over 250 Lines:
```cpp
UCLASS()
class PAW_API APawPlayerHider : public APawTPPlayer
{
	GENERATED_BODY()

public: // Constructor & Public Engine Overrides
	APawPlayerHider();
	virtual void Tick(float DeltaTime) override;

public: // Blueprint-Callable API
	UFUNCTION(BlueprintCallable, Category = "Health")
	void TakeHealthDamage(float DamageAmount);

protected: // Protected Engine Overrides
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected: // Properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float Health;

protected: // Networking (RPCs & RepNotifies)
	UFUNCTION(Server, Reliable)
	void ServerTakeHealthDamage(float DamageAmount);

private: // Internal Helper Methods
	void InitializeMaterials();

private: // Cached State
	FTimerHandle HealthEffectTimerHandle;
};
```

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

## Development Best Practices
- **Always expose value to Blueprints**: Ensure important variables and functions are accessible in Blueprint
- **Avoid hardcoding in C++**: Use configurable parameters and expose them to Blueprints
- **Use modern UE pointers**: Prefer UE smart pointers (TSharedPtr, TUniquePtr) over raw pointers

## Unreal Engine C++ Code Organization Standard

### Class Structure Guidelines
You are a code style enforcer for Unreal Engine C++ projects. Whenever you generate or refactor an Unreal Engine class, organize it exactly as follows—mirroring each override's engine-declared access level and grouping by purpose:

1. public: Constructor & Public Engine Overrides  
   - **Constructor declaration**  
   - All engine overrides declared **public** in the base class, grouped by interface with "//~ … interface" comments.  
     ```cpp
     //~ AActor interface
     virtual void Tick(float DeltaTime) override;
     //~ APawn interface
     virtual void Jump() override;
     //~ ACharacter interface
     virtual void Landed(const FHitResult& Hit) override;
     //…other public engine hooks
     ```

2. public: Blueprint-Callable API  
   - All `UFUNCTION(BlueprintCallable)` methods, grouped by subsystem (Health, Light Detection, Stealth, UI, etc.).

3. public: C++ Public Helpers  
   - Any plain C++ methods (non-UFUNCTION) exposed only to C++ callers:
     ```cpp
     void DoSomethingHeavy();
     FVector CalculateSpawnLocation() const;
     ```

4. public: Blueprint Events & Delegates  
   - All `UPROPERTY(BlueprintAssignable)` delegates.  
   - All `UFUNCTION(BlueprintImplementableEvent)` hooks.

5. protected: Protected Engine Overrides  
   - All engine overrides declared **protected** in the base class:
     ```cpp
     virtual void BeginPlay() override;
     virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
     //…other protected engine hooks
     ```

6. protected: Properties  
   - All `UPROPERTY` member variables (`EditAnywhere`, `Replicated`, etc.), grouped by subsystem, under a "// Properties" header:
     ```cpp
     // Properties: Health System
     UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Health, Category="Health")
     float Health;
     //…other properties
     ```

7. protected: Networking (RPCs & RepNotifies)  
   - All `UFUNCTION(Server/Client/NetMulticast, Reliable)` RPC declarations.  
   - All `UFUNCTION()` replication-notify handlers (`OnRep_*`).

8. private: Internal Helper Methods  
   - All non-UFUNCTION helper methods:
     ```cpp
     void InitializeMaterials();
     void StartHealthEffectTimer();
     //…etc.
     ```

9. private: Cached State  
   - Any private `UPROPERTY` or raw member variables (TimerHandles, cached materials, asset pointers, streamable handles, etc.):
     ```cpp
     FTimerHandle HealthEffectTimerHandle;
     TArray<TObjectPtr<UMaterialInterface>> CachedBaseMaterials;
     //…etc.
     ```

**Formatting rules:**  
- Prefix each section with a comment header (e.g. `public: // Constructor & Public Engine Overrides`).  
- Keep related functions and variables together; sub-group by subsystem when needed.  
- Maintain the engine-declared access level for every override so the code compiles.  
- Follow this ordering strictly for readability, merge-friendliness, and consistency with Epic's own code style.  

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
- Unreal Engine 5.6 

### Key Plugins
- OnlineSubsystemSteam (multiplayer)
- BlueprintAssist (development tool)
- BlockoutToolsPlugin (level design)
- AutoSizeComments (blueprint organization)

### Build Requirements
- Visual Studio 2022 with C++ development tools
- Windows SDK
- Steam SDK (for online features)