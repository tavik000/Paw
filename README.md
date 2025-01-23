<p align="center">
  <h1 align="center">Shadow Bubble: Hide and Seek Chaos </h1>
  <p align="center">Global Game Jam 2025 Unreal Engine 5 Project</p>
</p>

### Keyword

> `UE` `Unreal Engine 5` `C++` `Hide and Seek` `Asymmetric Multiplayer` `FPS`

---

## Game Overview

**Shadow Bubble: Hide and Seek Chaos** is a 3-5 player asymmetric multiplayer game combining stealth, strategy, and light mechanics.
Players split into two roles—**Seekers** and **Hiders**—and compete in a shadowy arena filled with interactive objects like bubbles and dynamic lighting.
Hider can turn invisible in the shadow. but the lighting and shadow are dynamic and always moving and changing.

---

## Roles and Mechanics

### **Seeker**

-   **Abilities**:
    -   **Flashlight**: Illuminate and damage Hiders.
    -   **FPS Bubble Gun**: Fires a single shot with a long cooldown to trap Hiders in bubbles.
        -   **Bubble Effect**: Trapped Hiders remain in the bubble unless rescued by teammates within a set time.
-   **Character**: Bubble Ghost (TBD).

### **Hider**

-   **Abilities**:
    -   **Shadow Cloak**: Become invisible and regenerate health while in shadows.
    -   **Sprint**: Speed boost for 3-5 seconds (leaves a trail of small bubbles).
    -   **Rescue**: Free teammates trapped in bubbles.
    -   **Bubble Vulnerability**: Popping bubble.
-   **Character**: Black Cat.

---

## Gameplay Elements

-   **Breakable Objects**: Provide temporary cover but can be destroyed.
-   **Pushable Objects**: Manipulate shadows or block paths strategically.
-   **Bubble Lightbulbs**: Pop to progress as Hiders. It provide light source that shadow other object as hiding place. and it is movable.
-   **Bubble Blocks**: Movable bubble, provide block in the air that generate shadow when the lighting source are around. if break, the block drop down.
-   **Point Lights**: Additional light sources to avoid as Hiders. might be switchable.

---

## Victory Conditions

-   **Hiders Win**: Pop all bubble lightbulbs.
-   **Seeker Wins**: Capture all Hiders.
-   **Draw**: Time runs out before either condition is met.

---

## Core Features

1. **Dynamic Lighting**:

    - Utilize Unreal Engine 5's **Lumen** for realistic, shifting shadows that define the gameplay.
    - Shadows move as light sources dynamically rotate or shift across the map.

2. **Stealth & Strategy**:

    - Hiders must skillfully use shadows, movement, and environment interactions to avoid detection.

3. **Interactive Environment**:

    - Breakable and pushable objects add depth and creativity to gameplay.
    - Bubble objects and light sources provide unique challenges and opportunities.

4. **Multiplayer Gameplay**:
    - High-tension hide-and-seek dynamics, where strategy and quick thinking are key.

---

## Why Unreal Engine 5?

-   **Lighting**: Stunning real-time dynamic lighting and shadow effects with GI and reflections using **Lumen** with Real-Time Ray Tracing!!! Let's go! Thanks to Nvidia RTX series, Microsoft DirectX12, and Epic Games Unreal Engine 5.
-   **Physics(Chaos Destruction)**: Advanced object interactions for breakable and pushable elements.
-   **Multiplayer Framework**: Robust tools for creating seamless multiplayer experiences. Online Subsystem.
-   **Virtual Shadow Map**: High-resolution real-time shadowing.

---
