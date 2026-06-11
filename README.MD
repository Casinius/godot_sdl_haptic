# SDL3 Haptic Manager for Godot

A Godot C++ module that provides advanced force feedback (haptic) management using SDL3. Supports constant, periodic, and condition effects with per‑effect update throttling, unique IDs, and fine‑grained control from GDScript.
## 🚀 Features

    - Multiple effect types – Constant force, periodic (sine, square, triangle, sawtooth), condition (spring, damper, inertia, friction)

    - Per‑effect control – Unique 64‑bit IDs for each created effect

    - Runtime updates – Change strength, direction, or coefficients on the fly (200Hz throttled to avoid USB overflow)

    - Start / stop – Independently start, stop, or destroy any effect

    - Hardware‑aware – Auto‑detects number of simultaneous effects the device can play

    - Godot‑friendly – All methods are bound to GDScript with simple int keys

    - Automatic cleanup – All effects are destroyed when the node is freed

## 📦 Dependencies

    - Godot 4.x (C++ bindings)

    - SDL3 (development libraries)

    - A compatible force‑feedback device (e.g., Xbox/PlayStation controller, force‑feedback wheel)

## 🛠️ Building

Place the source files in modules/sdl_haptic_manager/ inside your Godot source tree.

Ensure SDL3 development packages are installed:
    - bash

### Ubuntu/Debian
sudo apt install libsdl3-dev

### Arch Linux
sudo pacman -S sdl3

### Windows – use vcpkg or download SDL3 devel package

Recompile Godot with the module:
bash

scons platform=windows target=editor
### or platform=linuxbsd, macos

## 📖 Usage
Basic setup (GDScript)

```gdscript
var ffb = FFBManager.new()
ffb.joystick_id = 0        # first connected joystick
ffb.init_sdl()

### Create a constant force effect (does not auto‑start)
var key = ffb.new_effect_constforce(
    20000,      # level (-32768..32767)
    0,          # direction (Cartesian X)
    0, 0        # attack/fade length
)
ffb.start_effect(key)
```
### Update parameters later
```gdscript
ffb.update_effect_constforce(key, 30000, 10000)
```
### Stop and destroy when done
```gdscript
ffb.stop_effect(key)
ffb.destroy_effect_constforce(key)
```
### Clean up everything before exit
```gdscript
ffb.deinit_sdl()
```
Periodic effects (sine, square, triangle, sawtooth)
```gdscript

#SDL_HAPTIC_SINE = 1 << 0
var sine_key = ffb.new_effect_periodic(
    1,          # waveform type (SDL_HAPTIC_SINE)
    20000,      # magnitude (0..32767)
    0,          # offset
    500,        # period in milliseconds
    0, 0,       # attack/fade length
    0           # direction
)
ffb.start_effect(sine_key)
ffb.update_effect_periodic(sine_key, 30000, 0, 5000)

Condition effects (spring, damper, inertia, friction)
```
```gdscript

#SDL_HAPTIC_SPRING = 1 << 2
var spring_key = ffb.new_effect_condition(
    1 << 2,     # type
    20000,      # right_sat
    20000,      # left_sat
    10000,      # right_coeff
    10000,      # left_coeff
    1000,       # deadband
    0,          # center
    0, 0        # attack/fade
)
ffb.start_effect(spring_key)
```
## 🔧 API Reference
Initialization / Cleanup
Method	Description
init_sdl() -> bool	Opens joystick and haptic device. Must be called first.

deinit_sdl() -> bool	Destroys all effects, closes devices, and quits SDL.

print_joy_info()	Prints joystick name and max simultaneous effects.


Constant Force
Method	Description
new_effect_constforce(lv, dir, attk_len, fade_len) -> int	Creates a constant force effect. Returns unique key or -1 on error.

update_effect_constforce(key, lv, dir) -> bool	Updates level and direction. Throttled (200Hz max).

destroy_effect_constforce(key) -> bool	Destroys the effect and frees resources.


Periodic Effects
Method	Description
new_effect_periodic(type, magnitude, offset, period, attack_len, fade_len, direction) -> int	Creates a periodic (waveform) effect.

update_effect_periodic(key, magnitude, offset, direction) -> bool	Updates magnitude, offset and direction.


Condition Effects
Method	Description
new_effect_condition(type, right_sat, left_sat, right_coeff, left_coeff, deadband, center, attack_len, fade_len) -> int	Creates a condition effect.

update_effect_condition(key, right_sat, left_sat, right_coeff, left_coeff, deadband, center) -> bool	Updates condition parameters.


Common Control
Method	Description
start_effect(key) -> bool	Begins playing the effect (infinite duration).

stop_effect(key) -> bool	Stops the effect without destroying it.

destroy_effect_constforce(key) -> bool	Destroys any effect (constant, periodic, or condition).



   - Note – The destroy function is named destroy_effect_constforce for backward compatibility, but works for all effect types.

## ⚠️ Important Notes

    - Haptic device must support the effect type – Some low‑end devices only support simple rumble. Use print_joy_info() to check Simultaneous Effects.

    - Update throttling – All updates are limited to 200Hz (5ms interval) to prevent USB buffer overflows.

    - Start after creation – Effects are not automatically started. You must call start_effect(key).

    - Cleanup – Always call deinit_sdl() before quitting the game, or rely on the FFBManager being freed (auto‑cleanup is provided).

## 📝 Example: Racing Game Steering Wheel
```gdscript

extends Node

var ffb: FFBManager

func _ready():
    ffb = FFBManager.new()
    ffb.joystick_id = 0
    if not ffb.init_sdl():
        print("Failed to init FFB")
        return

    # Constant spring effect to simulate centering force
    var center_key = ffb.new_effect_condition(
        1<<2, 20000, 20000, 5000, 5000, 500, 0, 0, 0
    )
    ffb.start_effect(center_key)

func _process(delta):
    # Update effect based on steering wheel position
    var wheel_pos = Input.get_joy_axis(0, JOY_AXIS_0)
    var force = abs(wheel_pos) * 30000
    ffb.update_effect_condition(center_key, force, force, 5000, 5000, 500, 0)

func _exit_tree():
    ffb.deinit_sdl()
```
## 🤝 Contributing

Feel free to open issues or PRs. Additional effect types (ramp, custom) can be added following the same pattern as create_periodic / create_condition.
## 📄 License

Same as Godot – MIT license. SDL3 is also zlib licensed.
