# Commit 1 - Dynamic higher-half paging

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
