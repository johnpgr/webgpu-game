# Level Editor Plan

## Goal

Build a debug-only in-game level editor for the running game.
Levels are authored live in memory, previewed immediately in the game, and persisted as generated C header files like the sprite atlas.

## Core Rules

- The editor runs inside the game in debug builds.
- The saved format is data-only C headers, not raw runtime entity dumps.
- The source of truth while editing is a mutable in-memory `LevelAsset`.
- The runtime preview world is rebuilt from that asset.
- One level should live in one header file, with a central registry header for discovery.

## Level Model

A level is made of 3 authored lists:

1. `layout`
   - world bounds
   - floor/platform/solid rects
   - camera limits
   - spawn markers if needed

2. `scenery`
   - decorative sprite placements
   - props
   - background pieces
   - non-interactive dressing

3. `entities`
   - player spawn
   - enemies
   - hazards
   - pickups
   - triggers
   - gameplay markers

## Why Not Save Raw Runtime Entities

Runtime `Entity` data contains transient state that should not become authored content:
- generation
- free list links
- animation time
- velocity
- temporary handles
- live camera target state

The editor should save clean authored records and rebuild runtime entities from them.

## Persistence Format

Recommended layout:

- `assets/levels/level_test_room.h`
- `assets/levels/level_forest_01.h`
- `assets/levels/levels.h`

Each level header should contain:
- version
- level name/id
- static arrays for layout/scenery/entities
- counts
- one root `LevelAssetDesc`

Example shape:

```c
typedef struct LevelLayoutItem {
    u32 editor_id;
    u32 kind;
    vec2 position;
    vec2 size;
} LevelLayoutItem;

typedef struct LevelSceneryItem {
    u32 editor_id;
    u32 sprite_id;
    vec2 position;
    vec2 size;
    f32 depth;
    u32 render_layer;
    b32 flip_x;
} LevelSceneryItem;

typedef struct LevelEntityItem {
    u32 editor_id;
    u32 archetype_id;
    vec2 position;
    vec2 size;
    u32 parent_editor_id;
} LevelEntityItem;

typedef struct LevelAssetDesc {
    char const *name;
    vec2 world_min;
    vec2 world_max;
    LevelLayoutItem const *layout;
    u32 layout_count;
    LevelSceneryItem const *scenery;
    u32 scenery_count;
    LevelEntityItem const *entities;
    u32 entity_count;
} LevelAssetDesc;
```

Do not save raw atlas animation ids directly unless they are intentionally stable.
Prefer authored ids that resolve through a registry.

## Runtime Architecture

### In-Memory Authoring State

Add an `EditorState` under `GameState` with:
- editor enabled flag
- current tool
- selected item ids
- hovered item id
- active level asset
- dirty flag
- undo/redo stacks
- editor id -> runtime handle map
- camera controls for editing

### Preview World

Keep the current `GameWorld` as the runtime preview.
When authored data changes:

- rebuild the world from `LevelAsset`, or
- later patch simple changes in place

Default to full rebuild first.
It is simpler and safer.

### Prefab / Archetype Layer

Add a registry that maps authored ids to runtime spawn/apply logic.

Examples:
- `ARCHETYPE_PLAYER_SPAWN`
- `ARCHETYPE_SKELETON_PATROL`
- `ARCHETYPE_SPIKE_HAZARD`
- `ARCHETYPE_PICKUP_HEALTH`

This keeps saved data stable and avoids leaking runtime implementation details into level files.

## Editor Modes

Use 2 modes:

- `Edit`
  - selection
  - placement
  - move/duplicate/delete
  - grid snapping
  - overlay drawing

- `Playtest`
  - rebuild from current authored asset
  - run the level normally
  - switch back without losing editor state

## Rendering Plan

Use the existing render frame split:

- `BackgroundPass`
  - temporary level geometry preview
  - large layout blocks

- `WorldSpritePass`
  - scenery and entity preview

- `DebugPass`
  - grid
  - bounds
  - selection boxes
  - pivots
  - snap guides
  - parent links
  - ids
  - collision shapes

- `UiPass`
  - tool strip
  - palette
  - inspector
  - save/reload buttons
  - current mode

Start with `DebugPass` first.
Make `UiPass` minimal until the basic workflow works.

## Input Plan

The current input struct is too thin.
Expand it for editor use:

- mouse position
- mouse delta
- left/right/middle buttons
- pressed/released edges
- wheel
- shift/ctrl/alt
- tool hotkeys
- editor toggle hotkey
- playtest toggle hotkey

Without this, selection and drag tools will stay awkward.

## First Tools

Implement in this order:

1. editor toggle
2. camera pan/zoom
3. grid + snapping
4. select item
5. move item
6. delete item
7. duplicate item
8. place layout rect
9. place scenery prop
10. place entity archetype

## Save / Reload Workflow

- `Rebuild Preview`
  - rebuild from current in-memory asset
- `Save Header`
  - write deterministic C header output
- `Reload Header`
  - reload authored level from disk
- `Autosave`
  - optional temp export for safety

Saved headers should be deterministic:
- stable ordering
- stable formatting
- stable ids
- no runtime noise

That keeps diffs clean.

## Suggested Milestones

### M1 - Data Foundation
- define `LevelAsset` structs
- define registry headers
- replace hardcoded demo boot with level-driven boot
- support one built-in level

### M2 - Editor Input + Camera
- richer input
- editor toggle
- pan/zoom
- world picking
- selection state

### M3 - Editor Overlays
- implement `DebugPass`
- grid
- bounds
- selection boxes
- placement preview

### M4 - Authoring Tools
- place/move/delete/duplicate
- layout editing
- scenery placement
- entity placement

### M5 - Persistence
- write header files
- reload from disk
- deterministic output
- level registry integration

### M6 - Workflow Polish
- undo/redo
- multi-select
- inspector editing
- parent/child links
- validation warnings

## Risks

- DLL hot reload can preserve stale `GameState` layouts if editor structs change
- saving raw runtime ids would make authored data brittle
- web build should not be the first-class editing target yet
- recompilation should not be required for every small edit during a session
- using rebuild-on-edit means runtime handles are temporary, so editor ids must stay separate

## Definition Of Done

The editor is successful when:

- a level can be opened in the running debug game
- layout, scenery, and entity items can be placed and moved
- the preview updates immediately
- the result can be saved as a C header
- the game can boot from that saved header again
- a designer can go from empty level to playable slice without hand-editing code
