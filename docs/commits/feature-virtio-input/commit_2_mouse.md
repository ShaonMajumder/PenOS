# Git Commit Documentation - VirtIO-Mouse Support

## Branch Name
```
feature/virtio-input-mouse
```

## Commit Message
```
feat: Add VirtIO-Mouse support to VirtIO-Input driver

- Extend VirtIO-Input to handle mouse events (movement and buttons)
- Add mouse state tracking (position and button states)
- Implement mouse API: mouse_get_position() and mouse_get_buttons()
- Add 'mouse' shell command to display cursor position
- Enable virtio-mouse-pci device in QEMU configuration
```

## Detailed Description

### Feature: VirtIO-Mouse Support

This commit extends the existing VirtIO-Input driver to support mouse input alongside keyboard input. The implementation handles relative mouse movement (EV_REL events) and button clicks (BTN_LEFT, BTN_RIGHT, BTN_MIDDLE).

---

## Changes by Component

### 1. VirtIO Input Driver Enhancement

**Files Modified:**
- `src/drivers/virtio_input.c`

**Changes:**
- Added support for multiple VirtIO input devices (Keyboard and Mouse)
- Added `mouse_dev` structure and `mouse_events` buffer
- Added mouse event type definitions (`EV_REL`, `REL_X`, `REL_Y`, `BTN_LEFT`, `BTN_RIGHT`, `BTN_MIDDLE`)
- Added `mouse_state_t` structure to track cumulative position and button states
- Enhanced `virtio_input_poll()` to poll both keyboard and mouse devices separately
- Implemented `mouse_get_position()` and `mouse_get_buttons()` API functions

**Technical Details:**
- QEMU exposes Keyboard and Mouse as separate PCI devices
- The driver now scans and initializes both devices independently
- Mouse position is tracked cumulatively (relative deltas are summed)
- Button states use bitmask: Bit 0=Left, Bit 1=Right, Bit 2=Middle

---

### 2. Mouse API

**Files Modified:**
- `include/drivers/mouse.h`

**Changes:**
- Replaced `mouse_get_state()` with two simpler functions:
  - `void mouse_get_position(int *x, int *y)`
  - `int mouse_get_buttons(void)`

---

###3. Shell Integration

**Files Modified:**
- `src/shell/shell.c`

**Changes:**
- Added `cmd_mouse()` command to display current mouse state
- Updated `help` command to include `mouse`
- Added command parser entry for `mouse`

**Example Output:**
```
PenOS:/> mouse
Mouse Position: X=142 Y=87 Buttons=[L]
```

---

### 4. QEMU Configuration

**Files Modified:**
- `Makefile`

**Changes:**
- Added `-device virtio-mouse-pci,disable-modern=on` to QEMU flags
- Works alongside existing keyboard device

---

## Architecture Update

### Input Event Flow

```
[VirtIO Mouse Device]
        ↓
   [VirtQueue]
        ↓
[virtio_input_poll()] ← Called from shell idle loop
        ↓
    ┌───────────────────┐
    │ Event Dispatcher  │
    └───────────────────┘
         ↓         ↓         ↓
    [EV_KEY]  [EV_REL]  [EV_KEY + BTN_*]
         ↓         ↓         ↓
  [Keyboard] [Mouse Pos] [Mouse Btns]
```

### State Management

**Mouse State:**
- **Position (X, Y)**: Cumulative relative movement
- **Buttons**: Bitmask of pressed buttons

**Note:** Position is relative and unbounded. In a GUI, this would be clamped to screen dimensions.

---

## Testing Performed

### Manual Verification
- ✅ Boot PenOS with VirtIO mouse device
- ✅ Move mouse: position values change
- ✅ Click left/right/middle buttons: button states update
- ✅ Run `mouse` command: displays current state
- ✅ Keyboard input still works (no regression)

### Test Procedure
1. `make clean && make iso && make run`
2. At shell prompt: `mouse`
3. Move mouse, run `mouse` again
4. Click buttons, verify state in output

---

## Limitations

- **Polling-based**: Mouse events are polled, not interrupt-driven
- **No cursor rendering**: Text-mode console, no visual cursor
- **Unbounded position**: Position can go negative or very large
- **No acceleration**: Raw relative input, no mouse acceleration curve

---

## Future Enhancements

1. **Interrupt-driven**: Use VirtIO interrupts instead of polling
2. **Cursor rendering**: Show cursor in text mode (character block)
3. **Position clamping**: Limit to screen bounds (80x25)
4. **Mouse acceleration**: Apply sensitivity/acceleration curve
5. **GUI integration**: Use mouse for window manager (future)

---

## Related Features

**Implemented:**
- ✅ VirtIO-Input (Keyboard) - commit_1
- ✅ VirtIO-Input (Mouse) - commit_2 (this commit)

**Unimplemented:**
- ❌ VirtIO-Tablet (absolute positioning)
- ❌ GUI/Windowing system
- ❌ Mouse cursor rendering
