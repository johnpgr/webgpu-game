---
name: entity-systems-and-zii
description: "Reference for entity system architecture (sparse/flat-struct entities, property bitflags, entity trees, packing/unpacking) and ZII (Zero Is Initialization) memory allocation patterns (arena allocators, group allocation, RAII critique). Sourced from Casey Muratori's Handmade Hero, Ryan Fleury's forum writings, and related Handmade Network discussions."
---

# Entity Systems & ZII Memory Allocation — Detailed Reference

## Table of Contents

1. [Source Material Index](#1-source-material-index)
2. [The Sparse Entity System](#2-the-sparse-entity-system)
3. [Discriminated Unions vs. Flat Structs](#3-discriminated-unions-vs-flat-structs)
4. [Property Bitflags & Feature Composition](#4-property-bitflags--feature-composition)
5. [Entity Trees & Structural Composition](#5-entity-trees--structural-composition)
6. [Entity Packing & Unpacking (World Streaming)](#6-entity-packing--unpacking-world-streaming)
7. [ZII — Zero Is Initialization](#7-zii--zero-is-initialization)
8. [Arena Allocators](#8-arena-allocators)
9. [RAII Critique & the N+2 Programmer](#9-raii-critique--the-n2-programmer)
10. [Practical Code Patterns](#10-practical-code-patterns)
11. [Extended References & Further Reading](#11-extended-references--further-reading)

---

## 1. Source Material Index

### Primary Sources (provided URLs)

| # | Source | Content |
|---|--------|---------|
| S1 | [Handmade Network Forum — Discriminated Union vs. Sparse System](https://hero.handmade.network/forums/code-discussion/t/7896-why_don%27t_use_discriminated_union_rather_than_sparse_system_for_entity_system) | Extended discussion between Ryan Fleury, Simon Anciaux (mrmixer), Miles (notnullnotvoid), ratchetfreak, and longtran2904. Core arguments for flat structs, property flags, entity trees, and against discriminated unions for entity data. Ryan's game code posted at `hatebin.com/oykukwtnoo`. |
| S2 | [Handmade Hero Day 277 — The Sparse Entity System](https://www.youtube.com/watch?v=wqpxe-s9xyw) / [Episode Guide](https://guide.handmadehero.org/code/day277/) | Casey's blackboard-heavy episode introducing the sparse entity concept. Key timestamps: **10:27** entity systems overview, **14:47** current compositional approach, **21:28** problems with AoS approach, **24:45** sparse entity system, **27:06** inheritance, **36:44** when inheritance falls apart, **40:25** "inheritance is compression", **46:47** one huge entity struct, **51:24** sparse matrix solver analogy, **56:09** dispatch, **1:07:30** why no dynamic dispatch. |
| S3 | [Handmade Hero Day 663 — Simplifying Entity Storage Part I](https://www.youtube.com/watch?v=-m7lhJ_Mzdg) / [Episode Guide](https://guide.handmadehero.org/code/day663/) | First part of entity storage simplification. Refactoring from the earlier sim-region model toward direct world-chunk entity management. |
| S4 | [Casey Muratori — Smart-Pointers, RAII, ZII? Becoming an N+2 Programmer](https://www.youtube.com/watch?v=xt1KNDmOYqA) | Casey's standalone talk on the progression of memory management thinking. First ~10 minutes: core argument about per-object vs. group allocation. Introduces the term ZII (Zero Is Initialization) as the counterpart to RAII. |

### Secondary Sources (referenced within primary sources or closely related)

| # | Source | Content |
|---|--------|---------|
| S5 | [Handmade Hero Day 278 — Moving Entity Storage into World Chunks](https://guide.handmadehero.org/code/day278/) | How the simulation region streams in entities as you traverse the world. Packed entities in world chunks, unpacking into sim region. |
| S6 | [Handmade Hero Day 341 — Dynamically Growing Arenas](https://guide.handmadehero.org/code/day341/) | Dynamic arena growth, `BeginTemporaryMemory`/`EndTemporaryMemory`, and the source code comment: `NOTE(casey): PROGRAMMING! RAII = bad :( ZII = good :)` |
| S7 | [Handmade Hero Days 543–544 — Moving Unpacked Entities / Caching](https://guide.handmadehero.org/code/day543/) | `EnsureRegionIsUnpacked()`, entity reference tables, `RepackEntitiesAsNecessary()`, free entity slot management. |
| S8 | [Handmade Hero Days 664–666 — Simplifying Entity Storage II, Packing/Unpacking](https://guide.handmadehero.org/code/day665/) | `entity_block_storage` containing `packed_entity_block` and `unpacked_entity_block`. `EntityFlag_Unpacked`. Chunk-based entity lifecycle. |
| S9 | [Ryan Fleury — Entity Memory Contiguity: A Story About Simplicity](https://ryanfleury.net/blog_entity_memory_contiguity) | Blog post exploring contiguous entity storage, flat structs, and why uniformity of memory layout simplifies everything downstream. |
| S10 | [GingerBill — Memory Allocation Strategies (6-part series)](https://www.gingerbill.org/series/memory-allocation-strategies/) | Comprehensive tutorial on arena/linear allocators, stack allocators, pool allocators, free list allocators, buddy allocators. Concrete C implementations. |
| S11 | [Karl Zylinski — Mistakes and Cool Things to Do with Arena Allocators](https://zylinski.se/posts/dynamic-arrays-and-arenas/) | Practical lessons from using arenas in the Odin language. Dynamic arrays backed by arenas. |
| S12 | [Luna's Blog — Systems Languages Should Support Zero Is Initialization](https://blog.xoria.org/zii/) | In-depth analysis of ZII at the language level. Examines Go, Odin, and Jai. Discusses enum design, zeroed allocations, interaction with virtual memory, and optional types. |
| S13 | [Loris Cro — RAII and the Rust/Linux Drama](https://kristoff.it/blog/raii-rust-linux/) | Links Casey's ZII talk as evidence that RAII conflicts with batch-oriented, performance-sensitive programming. Arena allocation reduces N lifetimes to 1. |
| S14 | [Handmade Opinions (excalamus.com)](https://excalamus.com/2023-02-23-handmade-wisdom.html) | Collected Casey Muratori opinions from Handmade Hero, including zero-initialization philosophy, resource cleanup in waves, memory arenas, and the static/internal convention. |
| S15 | [Handmade Hero Day 35 — Basic Sparse Tilemap Storage](https://guide.handmadehero.org/code/day035/) | Early introduction to sparse storage via hash-based tile chunks. Precursor concepts to the entity system. |
| S16 | [Handmade Hero Day 55 — Hash-based World Storage](https://hero.handmade.network/episode/code/day055) | Hash maps for world chunks, arena allocation for chunks, discussion of octree alternatives. |
| S17 | [Handmade Network Forum — Memory Arenas](https://hero.handmade.network/forums/code-discussion/t/878-memory_arenas) | Community discussion of permanent vs. transient storage. Casey's explanation: "The line is mostly just 'what would I need to save if I wanted to restore the state of the program?'" |
| S18 | [Handmade Network Forum — Anyone else adopted data-oriented design?](https://hero.handmade.network/forums/code-discussion/t/386-anyone_else_adopted_data-oriented_design) | Discussion of big-block allocation, separated allocation/initialization, data-oriented design in non-game contexts (DSP, GUI apps). |

---

## 2. The Sparse Entity System

**Origin:** Handmade Hero Day 277 [S2].

### The Problem Space

Every game needs to represent "things in the world." The challenge is organizing the *data* that describes heterogeneous objects (players, enemies, trees, doors, projectiles) in a way that makes *code* simple to write and maintain.

### The Insight: Inheritance Is Compression

Casey frames traditional OOP inheritance as a *compression technique* — it compresses identical code across similar types into base classes. But the axis of compression is fixed at compile time, and the real problem space of entity behaviors is not a tree — it's a combinatoric space. [S2, timestamp 40:25]

If you have N orthogonal features, there are **2^N** possible entity configurations. Inheritance asks you to pick a single decomposition axis. The moment two features cross-cut in a way the hierarchy didn't anticipate, you need diamond inheritance or duplicated code.

### The Solution: One Huge Flat Struct

Instead of a type hierarchy, use a single `struct Entity` containing every field any entity might ever need. All entities share the same memory layout. [S2, timestamp 46:47]

**Key properties:**
- Uniform interpretation — a pointer to `Entity` always means the same thing
- No discriminator checks required for field access
- Any codepath written over the struct works for all entities
- Debugger-friendly — all fields visible at a glance [S1, ratchetfreak]

### The Sparse Matrix Analogy

Casey draws an analogy to sparse matrices [S2, timestamp 51:24]. A sparse matrix is mostly zeros, and you only store the non-zero entries. Similarly, a flat entity struct is "sparse" — most fields are zero for any given entity, and only the relevant ones contain meaningful data. The ZII principle makes this work: zero means "unused/inactive/default."

---

## 3. Discriminated Unions vs. Flat Structs

**Source:** Handmade Network Forum [S1]. Key participants: Ryan Fleury (Delix), Simon Anciaux (mrmixer), Miles (notnullnotvoid), ratchetfreak, longtran2904.

### Arguments Against Discriminated Unions for Entities

| Concern | Detail | Source |
|---------|--------|--------|
| **Code bloat** | You cannot assume anything about data layout. Every access needs a discriminator check. With a flat struct, codepaths work for all entities without checking. | Ryan Fleury [S1] |
| **Debugger friction** | Unions only show the active variant. Flat structs show everything. | ratchetfreak [S1] |
| **Corruption risk** | Forgetting to check the tag means silently reinterpreting overlapping data. With flat structs, writing one field can never corrupt another. | ratchetfreak [S1] |
| **Data serves multiple behaviors** | Example: a "toggled" bit maps to electricity, collision, and lighting simultaneously. A union forces data into one variant; a flat struct lets data serve multiple roles. | Ryan Fleury [S1] |
| **O(2^N) entity-kind enumeration** | Discriminated unions encourage defining explicit "kinds." With N features, there are 2^N possible kinds to manage. Property flags reduce this to O(N). | Ryan Fleury [S1] |

### Ryan Fleury's Counterargument to "Wasted Space"

Three rebuttals [S1]:

1. **Practically irrelevant:** Entity data is tiny compared to textures, audio, and render buffers.
2. **Natural compression:** When features are orthogonalized, the same fields encode multiple things. Flat structs compress organically as you understand your data better.
3. **Entity trees:** A single entity becomes smaller when you decompose complex objects into trees of small uniform entities, each contributing one set of features.

### Miles's Cache Observation

Even unioned entity structs routinely exceed 64 bytes, meaning one new cache line per entity regardless of layout. If cache is truly the bottleneck, SOA (Struct of Arrays) is the right transformation — not unions. [S1, Miles]

---

## 4. Property Bitflags & Feature Composition

**Source:** Ryan Fleury [S1], Casey Muratori Day 277 [S2].

### The Model

Each feature/codepath maps to a single bit. There is a direct 1:1 mapping between one bit and one codepath.

```c
typedef U64 EntityProperties;
enum {
    ENTITY_PROPERTY_RENDER_SPRITE      = (1 << 0),
    ENTITY_PROPERTY_CHASE_PLAYER       = (1 << 1),
    ENTITY_PROPERTY_CONTROLLED_BY_USER = (1 << 2),
    ENTITY_PROPERTY_HAS_PHYSICS_SHAPE  = (1 << 3),
    ENTITY_PROPERTY_IS_STATIC          = (1 << 4),
    ENTITY_PROPERTY_HOSTILE            = (1 << 5),
    // etc.
};
```

### For Larger Property Counts (Bit Array)

When you exceed 64 properties, use an array of U64s with bit indexing [S1, Ryan Fleury]:

```c
enum EntityProperty {
    ENTITY_PROPERTY_RENDER_SPRITE,
    ENTITY_PROPERTY_CHASE_PLAYER,
    ENTITY_PROPERTY_CONTROLLED_BY_USER,
    ENTITY_PROPERTY_HAS_PHYSICS_SHAPE,
    // etc.
    ENTITY_PROPERTY_COUNT
};

struct Entity {
    U64 properties[(ENTITY_PROPERTY_COUNT + 63) / 64];
    // data fields ...
};

internal b32 EntityHasProperty(Entity *entity, EntityProperty prop) {
    return !!(entity->properties[prop / 64] & (1ull << (prop % 64)));
}

internal Entity *SetEntityProperty(Entity *entity, EntityProperty prop) {
    entity->properties[prop / 64] |= 1ull << (prop % 64);
    return entity;
}
```

The `(EntityProperty_COUNT + 63) / 64` calculation gives the ceiling division — the number of U64s needed to hold one bit per property.

### "Kinds" Are Helper Functions, Not Types

```c
Entity *MakePlayer(void) {
    Entity *entity = AllocateEntity();
    SetEntityProperty(entity, ENTITY_PROPERTY_CONTROLLED_BY_USER);
    SetEntityProperty(entity, ENTITY_PROPERTY_RENDER_SPRITE);
    SetEntityProperty(entity, ENTITY_PROPERTY_HAS_PHYSICS_SHAPE);
    entity->sprite_info = /* ... */;
    return entity;
}
```

This gives you the full 2^N combinatoric space to explore as a designer, and you can trivially build a UI to toggle properties (just loop over the enum). [S1, Ryan Fleury]

### Ryan Fleury's Trigger Example

From his game code, every entity has a `Trigger` struct:

```c
struct Trigger {
    TriggerKind kind;
    Input input;
    Shape shape;
    ItemKind collect_item_kind;
    Item *collect_item;
    EntityHandle source_entity;
    f32 seconds_to_allow_source_entity;
    AttackFlags attack_flags;
};
```

The `TriggerKind` is a sub-discriminator, but it's still one-to-one with codepaths (hitbox, crafting, chest, item-collection). The trigger data is *always present* on every entity — you just check the kind to decide which codepath fires. No union needed. [S1, Ryan Fleury]

---

## 5. Entity Trees & Structural Composition

**Source:** Ryan Fleury [S1].

### The Structure

```c
struct Entity {
    Entity *first_child;
    Entity *last_child;
    Entity *next;
    Entity *prev;
    Entity *parent;
    // data fields ...
};
```

### Why Trees?

1. **Combine features by combining entities.** A complex boss is a tree of entities, each contributing behaviors. The root entity might handle AI; children handle individual limbs, weapons, trigger volumes.
2. **Entities become smaller.** Instead of one massive entity per world object, you have small uniform entities composed into structures.
3. **Flexible granularity.** Need per-body-part health? Make each body part its own entity. Don't need it? Use one entity for the whole character. The tree allows this choice at runtime. [S1, Ryan Fleury]

### Requirement: No "Kinds"

Entity trees only make sense once you abandon the "kinds" model. A tree where a `Tree` entity has a `Player` parent is nonsensical under the kinds model. But when entities are just bags of features identified by property bits, tree composition becomes natural. [S1, Ryan Fleury]

---

## 6. Entity Packing & Unpacking (World Streaming)

**Source:** Handmade Hero Days 277–280, 543–544, 663–666 [S2, S5, S7, S8].

### The Architecture

The open world in Handmade Hero is divided into **world chunks**. Each chunk stores entities in a **packed** (compact, serialized) format. When the camera gets close to a chunk, its entities are **unpacked** into full `Entity` structs for simulation.

```
World Chunks (packed_entity_block)
    │
    ├── Chunk at (0,0): [packed entity data...]
    ├── Chunk at (1,0): [packed entity data...]
    └── ...
         │
         ▼  (unpack when camera is near)
    Unpacked Entities (full Entity structs, sim-ready)
         │
         ▼  (repack when camera moves away)
    Back to packed format in chunk
```

### Key Data Structures (Days 665–666) [S8]

- **`entity_block_storage`**: A union-like container holding either a `packed_entity_block` or an `unpacked_entity_block`, sized to be roughly equivalent.
- **`EntityFlag_Unpacked`**: Flag indicating an entity has been expanded into its full simulation-ready form.
- **`EnsureChunkIsUnpacked()`**: Called when entering a chunk's region. Walks packed blocks, expands entities.
- **`FillUnpackedEntity()`**: Populates the full entity struct from packed data.
- **`RepackEntitiesAsNecessary()`**: Repacks entities that have moved out of the camera region.
- **Free entity slots**: Unpacked entity slots are managed via a free list (`free_entity`), chained up in `CreateWorld()`. [S7]

### Why Flat Structs Help

Because every entity has the same layout, packing/unpacking is straightforward — no polymorphic serializers, no virtual dispatch, no type-specific handling. You memcpy (possibly with delta compression) between packed and unpacked forms.

---

## 7. ZII — Zero Is Initialization

**Source:** Casey Muratori [S4, S6, S14], Luna's Blog [S12].

### Definition

Design your data types so that a zero-filled instance is already in a valid default state. All-zeros means "empty," "inactive," "default," or "off." No constructor needed.

### Design Rules

**Enums: put the "nothing/empty/default" variant at position 0.**

```c
enum file_state {
    FILE_STATE_NO_FILE,   // = 0
    FILE_STATE_OPEN,
    FILE_STATE_CLOSED,
};
```

A `calloc`'d or `mmap`'d array of these is immediately usable — every slot is `FILE_STATE_NO_FILE`. [S12]

**Structs: zero means "unused."**

```c
struct Entity {
    U64 properties[...]; // zero = no properties = inert
    V3  position;        // zero = origin
    f32 health;          // zero = dead or unused
    Entity *parent;      // NULL = root or unattached
};
```

An entity pushed from a zeroed arena has no properties set. No codepath will activate. It's a valid, safe no-op entity.

**Booleans: design so `false` (0) is the safe default.**

```c
// GOOD: false means "not yet ready" — safe to check
b32 is_initialized;

// BAD: false means "enabled" — zero-init would silently enable
b32 is_disabled;
```

**Pointers: NULL (0) means "nothing."** Design APIs so NULL pointers are handled gracefully (early return, no-op). [S14]

### ZII and Virtual Memory

When memory comes from `VirtualAlloc` (Windows) or `mmap` (Unix), the OS guarantees it's zero-filled. This makes ZII *free* — you get valid default state without paying for any initialization. [S12]

When arenas recycle memory, they must re-zero it. GingerBill's Odin allocators track which memory is "fresh from the OS" vs. "recycled," only zeroing the recycled portions. [S12]

### Language-Level ZII Support

Languages that embrace ZII: **Go**, **Odin**, **Jai**. Key properties [S12]:
- Local variables without initializers default to zero
- Struct literal unspecified fields are zero
- No concept of "invalid byte sequence for a type" (unlike Rust's niche optimization)
- Memory allocation defaults to zeroed memory

---

## 8. Arena Allocators

**Source:** Casey Muratori [S6, S17], GingerBill [S10].

### Core Concept

An arena is a contiguous block of memory with an offset that advances on each allocation. Deallocation is wholesale — reset the offset or free the entire block.

### Minimal Implementation (from GingerBill [S10])

```c
typedef struct Arena Arena;
struct Arena {
    unsigned char *buf;
    size_t         buf_len;
    size_t         prev_offset;
    size_t         curr_offset;
};

void *arena_alloc_align(Arena *a, size_t size, size_t align) {
    uintptr_t curr_ptr = (uintptr_t)a->buf + (uintptr_t)a->curr_offset;
    uintptr_t offset = align_forward(curr_ptr, align);
    offset -= (uintptr_t)a->buf;

    if (offset + size <= a->buf_len) {
        void *ptr = &a->buf[offset];
        a->prev_offset = offset;
        a->curr_offset = offset + size;
        memset(ptr, 0, size);  // ZII: zero new memory by default
        return ptr;
    }
    return NULL;
}

void arena_free_all(Arena *a) {
    a->curr_offset = 0;
    a->prev_offset = 0;
}
```

**O(1) allocation. O(1) total deallocation. Zero per-object tracking.**

### Handmade Hero's Two-Arena Model [S6, S17]

| Arena | Contents | Lifetime | Saveable? |
|-------|----------|----------|-----------|
| **Permanent** | Entity state, world chunks, game progression | Entire game session | Yes — this *is* your save game |
| **Transient** | Loaded bitmaps, computed meshes, scratch buffers | Regenerable at any time | No — can be rebuilt from permanent state + asset files |

Casey's rule: "The line is mostly just 'what would I need to save if I wanted to restore the state of the program?'" [S17]

### Temporary Memory Savepoints [S6, S10]

```c
typedef struct Temp_Arena_Memory Temp_Arena_Memory;
struct Temp_Arena_Memory {
    Arena *arena;
    size_t prev_offset;
    size_t curr_offset;
};

Temp_Arena_Memory temp_arena_memory_begin(Arena *a) {
    Temp_Arena_Memory temp;
    temp.arena       = a;
    temp.prev_offset = a->prev_offset;
    temp.curr_offset = a->curr_offset;
    return temp;
}

void temp_arena_memory_end(Temp_Arena_Memory temp) {
    temp.arena->prev_offset = temp.prev_offset;
    temp.arena->curr_offset = temp.curr_offset;
}
```

Allocate scratch space for a single frame or computation, then rewind the arena. No individual frees. [S6]

### Dynamically Growing Arenas (Day 341) [S6]

When the current block fills up, allocate a new block from the OS. Blocks are chained. `EndTemporaryMemory` may need to free blocks that were allocated after the savepoint.

### Handmade Hero's Push Macros

```c
#define PushStruct(Arena, type)        (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type)  (type *)PushSize_(Arena, (Count) * sizeof(type))
```

---

## 9. RAII Critique & the N+2 Programmer

**Source:** Casey Muratori [S4], Loris Cro [S13], Handmade Opinions [S14].

### The Three Stages of Memory Management Thinking

Casey describes a progression [S4]:

**Stage N (beginner): Per-object allocation.** `malloc`/`free` for every thing. Smart pointers. Reference counting. Every variable has its own lifetime. Enormous mental overhead tracking ownership.

**Stage N+1: Recognizing patterns.** You start to see that lifetimes cluster. Many objects live and die together. You start thinking about scopes of data.

**Stage N+2: Group allocation.** You allocate a big block. Push things into it. Free the whole block at once. N lifetimes become 1 lifetime. Ownership is trivial. Smart pointers, reference counting, move semantics — all unnecessary.

### Specific RAII Criticisms

| Criticism | Detail | Source |
|-----------|--------|--------|
| **Forces per-object thinking** | Every object has its own constructor/destructor/lifetime. Fundamentally at odds with group allocation. | Casey [S4] |
| **Slow shutdown** | RAII dutifully destroys every object on exit. The OS will reclaim all memory in milliseconds. Result: applications that take forever to close. | Casey [S4, S14] |
| **Mental overhead** | Every RAII variable is a potential failure point. Ownership graphs, move semantics, reference counting add complexity. | Casey [S4], Loris Cro [S13] |
| **Anti-batch** | Performance code operates in batches. RAII's per-object semantics (construct, destruct, move) are friction against batch processing. | Casey [S4], Mike Acton (CppCon 2014) [S13] |
| **Reduces to ZII + arenas** | When you allocate from arenas and design for ZII, there is no resource management problem left to solve. RAII becomes a solution looking for a problem. | Casey [S4] |

### Loris Cro's Framing [S13]

"Operating in batches by using memory arenas is also a way to reduce memory ownership complexity, since you are turning orchestration of N lifetimes into 1."

---

## 10. Practical Code Patterns

### Pattern: Creating an Entity from a Zeroed Arena

```c
Entity *MakeTree(Arena *arena) {
    Entity *e = PushStruct(arena, Entity);
    // 'e' is already all zeros thanks to arena_alloc's memset (or mmap)
    // ZII: no properties = inert, position = origin, etc.
    SetEntityProperty(e, ENTITY_PROPERTY_RENDER_SPRITE);
    SetEntityProperty(e, ENTITY_PROPERTY_HAS_PHYSICS_SHAPE);
    SetEntityProperty(e, ENTITY_PROPERTY_IS_STATIC);
    e->sprite_info = TreeSprite;
    e->collision_shape = TreeCollisionBox;
    return e;
}
```

### Pattern: Feature Loop

```c
void UpdateEntities(Entity *entities, u32 count) {
    for (u32 i = 0; i < count; i++) {
        Entity *e = &entities[i];
        if (EntityHasProperty(e, ENTITY_PROPERTY_HAS_PHYSICS_SHAPE)) {
            ApplyPhysics(e);
        }
        if (EntityHasProperty(e, EntityProperty_ChasePlayer)) {
            ChasePlayer(e);
        }
        if (EntityHasProperty(e, EntityProperty_RenderSprite)) {
            RenderSprite(e);
        }
    }
}
```

Each feature is O(1) per entity. Total work is O(N × F) where F = number of features checked — but F is constant and small, so effectively O(N).

### Pattern: Entity Tree Assembly

```c
Entity *MakeBoss(Arena *arena) {
    Entity *root = MakeEnemy(arena);
    
    Entity *left_arm  = MakeBodyPart(arena);
    Entity *right_arm = MakeBodyPart(arena);
    Entity *weapon    = MakeWeapon(arena);
    
    AddChild(root, left_arm);
    AddChild(root, right_arm);
    AddChild(right_arm, weapon);
    
    return root;
}
```

### Pattern: Frame-Scoped Scratch Arena

```c
void GameUpdate(Arena *transient_arena, /* ... */) {
    Temp_Arena_Memory scratch = temp_arena_memory_begin(transient_arena);
    
    // All allocations in here are temporary
    CollisionPair *pairs = PushArray(transient_arena, max_pairs, CollisionPair);
    u32 pair_count = FindCollisionPairs(entities, entity_count, pairs, max_pairs);
    ResolveCollisions(pairs, pair_count);
    
    temp_arena_memory_end(scratch);
    // 'pairs' memory is now reclaimed — no per-object free needed
}
```

---

## 11. Extended References & Further Reading

### Video Sources

| Title | URL | Key Takeaway |
|-------|-----|--------------|
| Handmade Hero Day 277 — The Sparse Entity System | `youtube.com/watch?v=wqpxe-s9xyw` | Blackboard introduction of flat-struct entities, sparse matrix analogy, property flags |
| Handmade Hero Day 663 — Simplifying Entity Storage I | `youtube.com/watch?v=-m7lhJ_Mzdg` | Refactoring entity storage toward simplified chunk-based management |
| Casey Muratori — Smart-Pointers, RAII, ZII? | `youtube.com/watch?v=xt1KNDmOYqA` | The N+2 programmer progression; group allocation vs. per-object lifetime management |
| Handmade Hero Day 341 — Dynamically Growing Arenas | `guide.handmadehero.org/code/day341/` | Dynamic arena growth, temporary memory, `RAII = bad, ZII = good` |
| Mike Acton — Data-Oriented Design and C++ (CppCon 2014) | `youtube.com/watch?v=rX0ItVEVjHc` | RAII and other C++ patterns as anti-performance; data transformation pipelines |
| Andrew Kelley — Practical DOD (Handmade Seattle) | `youtube.com/watch?v=IroPQ150F6c` | Zig perspective on data-oriented memory management |

### Written Sources

| Title | URL | Key Takeaway |
|-------|-----|--------------|
| Handmade Network Forum — Disc. Union vs. Sparse System | `hero.handmade.network/forums/code-discussion/t/7896-...` | Full debate: Ryan Fleury's flat-struct arguments, entity tree pattern, trigger example |
| Ryan Fleury — Entity Memory Contiguity | `ryanfleury.net/blog_entity_memory_contiguity` | Contiguous entity storage, uniformity simplifying downstream code |
| GingerBill — Memory Allocation Strategies (6 parts) | `gingerbill.org/series/memory-allocation-strategies/` | Complete C implementations of arena, stack, pool, free-list, buddy allocators |
| Karl Zylinski — Arena Allocators in Odin | `zylinski.se/posts/dynamic-arrays-and-arenas/` | Practical arena patterns in Odin; dynamic arrays backed by arenas |
| Luna — Systems Languages Should Support ZII | `blog.xoria.org/zii/` | Language-level ZII analysis; Go, Odin, Jai comparison; enum design; virtual memory interaction |
| Loris Cro — RAII and the Rust/Linux Drama | `kristoff.it/blog/raii-rust-linux/` | RAII as friction against batch processing; arena allocation as ownership simplification |
| Handmade Opinions (excalamus.com) | `excalamus.com/2023-02-23-handmade-wisdom.html` | Compiled Casey opinions on zero-init, resource cleanup in waves, memory arenas, static/internal |
| Handmade Network — Memory Arenas | `hero.handmade.network/forums/code-discussion/t/878-memory_arenas` | Permanent vs. transient storage discussion; Casey's "save game" litmus test |

### Related Handmade Hero Episodes

| Day | Title | Relevance |
|-----|-------|-----------|
| 35 | Basic Sparse Tilemap Storage | Early sparse storage via hash-based tile chunks |
| 52 | Entity Movement in Camera Space | First questions about sparse entity storage |
| 55 | Hash-based World Storage | Hash maps for chunks, arena allocation for chunks |
| 56 | Switch from Tiles to Entities | Entity compression/decompression, high/low entity split |
| 277 | The Sparse Entity System | **Core episode.** Full blackboard design session. |
| 278–280 | Moving Entity Storage / Streaming | World-chunk entity storage, streaming simulation |
| 286–294 | Decoupling Entity Behavior / Brains | Separating behavior from entity types, "brains" system |
| 341 | Dynamically Growing Arenas | Dynamic arenas, ZII, temporary memory |
| 445 | Cleaning Up Entity Creation | Constraint-based room layout, entity creation refactor |
| 543–544 | Unpacked Entity Caching | Entity reference tables, repacking, free slots |
| 655 | Revisiting Entity Movement | Late-series entity movement and storage refinements |
| 663–666 | Simplifying Entity Storage | **Major refactor.** entity_block_storage, packing/unpacking overhaul |
