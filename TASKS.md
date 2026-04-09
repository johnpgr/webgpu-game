# Tasks

See `LEVEL_EDITOR_PLAN.md` for the debug in-game level editor plan, header persistence format, and phased implementation roadmap.

--------------------------

## 1. Add a tiny level slice
Create one hardcoded level struct with bounds, floor/platform rects, and spawn points for the player and enemy.
The code is still running as a free-move demo, so this becomes the base for everything else.

--------------------------

<!-- Im not bothering with physics on this game for now -->
<!-- ## 2. Add real jump input -->
<!-- Extend `GameInput` beyond `move_x`/`move_y` so the game can detect `jump_pressed` and `jump_held`. -->
<!-- Right now the app only feeds movement axes, which blocks proper side-scroller controls. -->

--------------------------

<!-- Im not bothering with physics on this game for now -->
<!-- ## 3. Replace sandbox movement with side-scroller physics -->
<!-- Swap the current 2D velocity mapping for run speed, gravity, jump impulse, and a grounded state. -->
<!-- This is the main step that makes the game feel like a platformer instead of a movement test. -->

--------------------------

## 4. Add collision against floor, platforms, and level bounds
Resolve vertical collision first, stop the player from falling through, and keep movement inside the authored slice.
The code has position and velocity, but no gameplay collision yet.

--------------------------

## 5. Clamp the camera to the level
Keep following the player on X, keep Y fixed for now, and clamp the camera to the level width.
Also clean up camera ownership so the clamp happens in one place instead of being split across systems.

--------------------------

## 6. Draw temporary level geometry
Use simple colored rects for the ground and platforms so the collision layout is visible immediately.
Placeholder visuals are already enough here; do not wait on final tiles or backgrounds.

--------------------------

## 7. Add one real gameplay interaction
Turn the existing patrol enemy into contact damage, or add one simple hazard/pickup if that is faster.
The project already renders a moving enemy, but there is still no actual play loop.

--------------------------

## 8. Verify the slice on desktop and web
Once the room is playable, make sure movement, camera, and interaction behave the same on both targets.
That gives a solid base before expanding art, AI, or broader gameplay systems.
