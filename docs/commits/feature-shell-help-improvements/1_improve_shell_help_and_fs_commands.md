# Improve shell help and filesystem commands

branch : feature/shell-help-improvements
message :
Improve PenOS shell help and filesystem commands

- Make the shell help output more user-friendly with aligned descriptions
  and example usages of common commands.
- Add explicit mention of demo tasks (counter|spinner) to guide spawn usage.
- Integrate filesystem commands (ls, cat) into the shell, guarded by FS_FS_H
  so builds without the in-memory FS still work.
- Improve line editing by handling both backspace keycodes (8 and 127) and
  clearly reporting ESC / Ctrl+C cancellation.
- Print a short banner when the shell starts to guide first-time users.

## Summary

This change makes the PenOS shell more user-friendly by enhancing the built-in
help output, adding examples, and exposing the in-memory filesystem via
`ls` and `cat` commands (when the FS is enabled).

## Changes

- **Help output**

  - Replaced the single-line command listing with a multi-line, descriptive help
    screen.
  - Each command now has a short explanation.
  - Added an "Examples" section to guide new users.

- **Filesystem integration**

  - Added `ls` to list files in the in-memory filesystem.
  - Added `cat <file>` to display file contents.
  - The commands are compiled conditionally under `#ifdef FS_FS_H` so the shell
    still builds if the FS is not present.

- **Line editing & control keys**

  - Handle both backspace key codes (`8` and `127`) for better keyboard
    compatibility.
  - Treat `ESC` as a line cancel with a clear `[ESC]` message.
  - Treat `Ctrl+C` as a line interrupt with a `^C` message and stop demo tasks.

- **Shell startup UX**
  - Print a short banner: `PenOS shell. Type 'help' for a list of commands.`

## Rationale

The previous shell interface assumed prior knowledge of available commands and
lacked guidance for new users. This change makes the shell self-discoverable,
improves the first-run experience, and provides a simple way to inspect the
in-memory filesystem when present.

## Notes

- Filesystem commands require the in-memory FS (`fs/fs.h` + `fs.c`) and that
  `FS_FS_H` is visible at compile time.
- Behaviour for existing commands (`ticks`, `sysinfo`, `ps`, `spawn`, `kill`,
  `halt`, `shutdown`) is unchanged, but they are now documented more clearly.
