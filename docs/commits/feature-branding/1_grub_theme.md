feature/grub-theme

commit message : boot: add GRUB theme with raster PenOS splash

commit description :

PenOS previously dropped straight into GRUB's default text UI before handing off to the kernel. That meant there was a jarring visual jump between a plain GRUB screen, the ASCII kernel splash, and the later shell. This change introduces a tiny GRUB theme that centers a 1024×768 PNG rendered from `penos-boot-splash.svg`, aligns the menu near the bottom, and keeps the PenOS branding consistent from power-on through the kernel logs. The ISO build step now copies the `grub/themes/penos` assets automatically so `PenOS.iso` always contains the rasterized splash.

What is done in this commit

- Added `grub/themes/penos/theme.txt` and `penos-boot-splash.png`; the theme sets common gfxterm modules, background, and menu colors, and `grub/grub.cfg` loads it via `$prefix/themes/penos/theme.txt`.
- Updated `Makefile`'s `iso` target so the GRUB theme directory is copied into the ISO alongside `grub.cfg`.
- Documented the flow in `README.md` (quick start + feature map) and `docs/architecture.md`, plus noted the `cairosvg` command to regenerate the PNG whenever the SVG artwork changes.

Behavioral result:

- Booting `PenOS.iso` now shows a PenOS-branded GRUB screen immediately, then seamlessly transitions into the kernel's ASCII splash and quiet shell prompt with consistent visuals.
