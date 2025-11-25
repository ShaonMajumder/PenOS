# Commit 1 - Dynamic higher-half paging

Branch: feature/paging-dynamic
Commit: “Implement dynamic higher-half paging”
Feature summary: Replaced static paging tables with a PMM-backed paging subsystem that identity-maps the first 16 MiB, mirrors the kernel into 0xC0000000+, installs recursive PD access, and exports map/unmap helpers.
Architecture: Page directory and tables are allocated from the bitmap PMM, PDE0 covers a low identity window, PDE768+ cover higher-half kernel mappings, and PDE1023 points back to the PD for future introspection. CR3 is loaded once mappings are built, flipping CR0.PG afterward.
What to learn: Paging should be built atop the PMM so higher-level code can request/free pages dynamically. Establishing higher-half mappings early eases later migrations without breaking the existing low identity execution path.

Problem
: Paging previously relied on two statically placed tables that identity-mapped the first 4 MiB. The layout could not grow with the PMM, offered no higher-half mapping, and exposed no API for other subsystems to map/unmap pages.

Solution
: Rewrote `src/mem/paging.c` to allocate the page directory and tables from the bitmap PMM, identity-map the first 16 MiB, mirror the kernel physical range into 0xC0000000+, and install recursive PD access. Added mapping helpers plus a `paging_virt_to_phys` utility so future heaps/schedulers can stitch in pages dynamically.

Architecture

```
[PMM frames] --> alloc_frame_zero() --> page directory
                                        |-> PDE0  -> identity PT (0-16 MiB)
                                        |-> PDE768+ -> higher-half PTs for kernel
                                        |-> PDE1023 -> recursive map
paging_map()/unmap() -> ensure PT exists -> write PTE -> invlpg
```

Data structures

- `current_pd`/`current_pd_phys`: track the live directory and CR3 target.
- Recursive slot (PDE 1023) reserved for future easy PD/PT manipulation.
- Exported constants (`PAGE_SIZE`, `KERNEL_VIRT_BASE`) for other subsystems.

Tradeoffs

- Keeps an identity window (16 MiB) so low physical memory remains reachable; future work can shrink/remove it once all code is higher-half aware.
- Mapping kernel to higher half without relocating the binary means execution still occurs low, but the mirrored mapping is ready for a later jump.

Interactions

- Paging consumes PMM frames, so allocator exhaustion now halts with an explicit message.
- Shell/console unaffected because identity window remains.
- Future heap/task work can call `paging_map` to grow stacks/heaps.

Scientific difficulty
: Building the new tables before paging is enabled requires careful reasoning about physical vs. virtual addresses, alignment, and CR3 updates to avoid triple faults.

What to learn
: Pair bitmap-based PMM with dynamically built paging structures, and reserve virtual regions early (higher half) even if you have not yet migrated execution there?the groundwork simplifies future architectural shifts.
