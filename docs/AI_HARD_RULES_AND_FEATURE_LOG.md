# PenOS – AI CONTRIBUTING RULES (MANDATORY FOR ALL LLMs)

This document defines the _contract_ that any AI/LLM (Codex, GPT, etc.) MUST follow
when generating code or documentation for PenOS.

Existance of AI will not be included in git history. Any file which have mention of AI, will be .gitignore d

---

## 1. Always Read These Files First

Before generating any change, AI MUST read:

1. `README.md`
2. `docs/product/ASSET_MAP.md`
3. `docs/product/FEATURE_LOG.md`
4. This file: `docs/CONTRIBUTING_AI.md` will be .gitignore d

---

## 2. Modular Product Rule

Every subsystem is a stand-alone brandable product:

| Asset Type | Product Name | Description                                |
| ---------- | ------------ | ------------------------------------------ |
| Kernel     | PenCore      | Boot, PMM, paging, scheduler, syscalls     |
| Shell      | PenShell     | CLI, commands, REPL                        |
| Drivers    | PenDrivers   | Input/output drivers, HAL-style components |
| GUI        | PenDesk      | Framebuffer UI, desktop, windows, widgets  |
| Scripting  | PenScript    | Future automation / scripting language     |

When modifying a subsystem, AI MUST classify the work under one of these modules
and clearly state it in any commit / docs it generates.

---

## 3. Requirements for ANY Commit (AI or Human)

For **every** code or docs change proposal, AI MUST output:

- **Branch name** – e.g. `feature/…` or `fix/…`
- **Commit message** – one clean line
- **Short commit description** – 1–2 lines (for `git log` / release notes)
- **Feature log entry** – a new entry added (or to-be-added) to  
  `docs/product/FEATURE_LOG.md` using the standard template there

AI MUST also state which **product asset** the change belongs to:

- `PenCore` / `PenShell` / `PenDrivers` / `PenDesk` / `PenScript` / newly added product with newly invented feature

---

## 4. Allowed Files Per Feature

AI MUST NOT alter files outside the requested layers.

**Examples:**

- GUI work → `src/ui/`, `include/ui/`, `assets/gui/`, GUI docs
- Memory work → `src/mem/`, `include/mem/`, memory docs
- Kernel core → `src/kernel.c`, `src/boot.s`, `src/arch/x86/*`
- Shell → `src/shell/*`
- Drivers → `src/drivers/*`, related docs

If a prompt explicitly lists **allowed files**, AI MUST stay within that list.

---

## 5. Architecture Consistency Rules

AI MUST NOT break:

- Boot sequence (GRUB → `boot.s` → `kernel_main`)
- Paging invariants (higher-half mapping assumptions)
- Scheduler contracts (task states, tick rate, ISR usage)
- Console fallback (text console must keep working if framebuffer fails)
- GUI degradation (if no framebuffer, system must still boot to shell)

Any change that _might_ affect these areas MUST clearly describe:

- What invariants are touched
- Why it is safe
- How to roll back or guard behind a flag

---

## 6. Business Output Rules

Every new feature MUST include:

- **Product naming** – which Pen\* module this strengthens
- **Business proposition** – who could use/buy/license this (even hypothetically)
- **Reuse potential** – can this module be sold or reused separately from the OS?

Examples:

- “PenDesk Core can be sold as a tiny GUI kit for retro laptops.”
- “PenDrivers/PS2 could be reused in educational kernel labs.”

This belongs in:

- The `docs/product/FEATURE_LOG.md` entry
- Any new/updated commit doc under `docs/commits/...` (see Section 8)

---

## 7. Forbidden Actions

AI MUST NEVER:

- Invent unsupported hardware features
- Silently refactor entire subsystems
- Delete or collapse documentation sections
- Break backward compatibility without:
  - A clear migration note
  - A versioned commit doc

---

## 8. Commit Docs (`docs/commits/*`) – HARD RULE

PenOS uses **per-feature commit docs** to explain architecture and evolution.

The layout today includes examples like:

- `docs/commits/feature-core/1_bootstrap.md`
- `docs/commits/feature-pmm/1_bitmap_pmm.md`
- `docs/commits/feature-paging/1_dynamic_vmm.md`
- `docs/commits/feature-heap/1_kernel_heap.md`, `2_heap_freelist.md`, `3_heap_trim.md`
- `docs/commits/feature-scheduler/1_preemptive_rr.md`, `2_task_lifecycle.md`
- `docs/commits/feature-mouse/1_ps2_mouse.md`
- `docs/commits/feature-framebuffer/...`
- `docs/commits/feature-gui/...`
- `docs/commits/feature-gui-double-buffering/...`
- `docs/commits/feature-identify-paging/...`
- `docs/commits/feature-syscall/...`
- `docs/commits/feature-branding/...`
- Fixes:
  - `docs/commits/fix-boot-splash-and-opt-in-demo-tasks/...`
  - `docs/commits/fix-shell-enter-key/...`
  - `docs/commits/fix-wsl-multiboot-scheduler-stability/...`
- Meta:
  - `docs/commits/branch-cleanup.md`
  - `docs/commits/docs/` (docs-related changes)

### 8.1 Feature Branches (`feature/*`)

For ANY new **feature** branch (e.g. `feature/gui-desktop-wm-terminal`), AI MUST:

1. Propose/assume a commit-doc directory:

   ```text
   docs/commits/feature-<slug>/
   ```

---

## 4. Template for New Feature / Product Entry

> **AI MUST USE THIS TEMPLATE** for every committable change it proposes.

When you add or modify a feature, append a new entry in Section 5 using this shape:

```markdown
### [FEATURE_ID] Short technical + product name

- **Date:** YYYY-MM-DD
- **Branch:** `feature/...` or `fix/...`
- **Commit message:** `<one-line git commit message>`
- **Short commit description:** `<one sentence suitable for git log>`

- **Asset type:** `<kernel | shell | drivers | gui | scripting | docs | tooling | other>`
- **Product name (module):** `<PenCore | PenShell | PenDrivers | PenDesk | PenScript | new brand>`

- **Technical summary:**

  - What changed? (1–3 bullets)
  - Which files / subsystems were primarily touched?

- **Architecture notes:**

  - How does this feature fit into the existing architecture?
  - Any new invariants or important design choices?

- **Business proposition:**

  - What could this be sold / licensed as?
  - Who is the target user/customer? (e.g., OS course, embedded vendor, tooling company)
  - Example use cases.

- **Brand & packaging ideas:**

  - Brandable feature/module name(s).
  - Possible SKUs or “editions” this could be part of.

- **Docs / links to create or update:**
  - `docs/commits/feature-.../*.md` (new or updated)
  - `docs/architecture.md` section(s) to touch
  - Any README / samples that should be updated.
```

7. Branch & Cleanup Rules (AI) – INCLUDING docs/commits/branch-cleanup.md

Branch lifecycle is part of the product story. AI tools MUST treat branches as first-class artifacts and document cleanup decisions.

7.1 Protected / Core Branches

The following branches are considered core and MUST NOT be deleted or renamed by any AI suggestion:

main

develop

feature/text-mode

feature/gui-mode

AI may propose merges into these branches, but:

Must NOT propose deleting them.

Must NOT propose force-pushing / rewriting their history.

7.2 Branch Cleanup Policy

When suggesting that a branch be deleted (locally or remotely), the AI MUST:

Check branch ancestry / ahead-behind status (conceptually).

Identify whether the branch is:

Fully merged into main or develop.

Ahead of main/develop with unique commits.

If the branch has commits that are not in main/develop, AI MUST either:

Propose how those changes were integrated (e.g., “already cherry-picked into feature/gui-mode”), or

Explicitly state why it’s safe to drop them (e.g., experimental, superseded by another feature).

Confirm functionality status.

Before branch deletion is recommended, the AI MUST:

Propose a build/test sequence (e.g., make, make iso, qemu-system-i386 ...) to verify that:

The feature the branch introduced is either:

fully merged and functional in another maintained branch, or

explicitly abandoned and no longer part of the roadmap.

Clearly state the expected result (e.g., “GUI boots and gui command works from develop”).

Require manual human approval.

AI MUST NOT assume the branch is actually deleted.

Always phrase branch cleanup as:

“After running these tests and manually confirming everything looks good, you MAY run:
git branch -d <branch> …”

Deletion of any branch MUST be explicitly left to a human user.

7.3 Rollback / Recovery Requirement

For any branch that AI recommends deleting, it MUST also provide a rollback plan, including:

Git commands or steps to restore:

Example:

# if you deleted the remote branch:

git checkout -b <branch> <backup-commit-sha>
git push origin <branch>

A short note on:

Which commit SHA(s) contain the original feature work.

How to re-integrate that work if the deletion was premature (e.g., cherry-pick, revert, or new feature branch).

7.4 docs/commits/branch-cleanup.md – Central Log

Any AI-generated suggestion that removes or sunsets a branch MUST update
docs/commits/branch-cleanup.md with a new entry containing:

Branch name being cleaned up (cleanup_target_branch).

Status of where its work lives now (superseded_by_branch or “abandoned”).

Short technical summary of what the branch was about.

Functional status:

“Feature verified in develop,” or

“Experimental, never shipped.”

Rollback information:

Key commit SHA(s) for recovery.

Short instructions on restoring the branch if needed.

The AI MUST:

Propose the exact markdown snippet to append to docs/commits/branch-cleanup.md.

Reference this cleanup entry from the relevant Section 6 feature/product log entry if it’s related to deprecating a feature.

7.5 AI Scope Limitations

AI MUST only propose git commands (e.g., git branch -d ..., git push origin --delete ...), not assume execution.

All destructive actions (branch deletion, force-push, history rewrite) MUST be:

Clearly labeled as dangerous.

Guarded with language like: “Only do this if you are sure and have backups.”

8. README & Marketing / Tech Stack Rules (AI)

The top-level README.md is the public, investor-ready face of PenOS.
Any AI-generated feature or fix that is user-visible MUST ensure README.md stays:

Accurate (technically correct),

Marketable (clear value proposition),

Up-to-date (features + versions reflected).

8.1 README as a “Living Pitch Deck”

Whenever you add a feature that:

Changes user-visible behavior (boot flow, shell commands, GUI, etc.), or

Introduces a new product-level asset (e.g. new driver, GUI module, scripting hook),

the AI MUST:

Decide whether this feature is significant enough to surface in README.md.

Rule of thumb: If it deserves a commit doc in docs/commits/feature-\* or an entry in Section 6, it should be mentioned in README.

Update at least one of the following README sections:

“What PenOS Gives You”

“Quick Start”

“Architecture at a Glance”

“Feature Map”

“Tech Stack & Build Targets” (see 8.3)

“Roadmap & Future Work”

Keep the tone:

Accessible to non-experts (recruiters, investors, engineers in other fields).

Focused on benefits and capabilities, not just implementation details.

8.2 Version & Release Information

README.md MUST always expose the current version of PenOS.

AI MUST:

Ensure there is a clearly labeled version line near the top, e.g.:

**Current version:** v0.8.0

Whenever a version bump is proposed (e.g. new docs/versions/v0.9.0.md):

Update the version string in README to the new version.

Add a brief bullet or short paragraph summarizing what’s new in that version (linking to the version doc).

Ensure that any new docs/versions/vX.Y.Z.md file is referenced from:

The “Documentation Guide” or equivalent section.

Optionally a “Release history” or “Changelog” bullet list.

8.3 Tech Stack & Build Table (Mandatory Structure)

README.md MUST contain a Tech Stack & Build table that gives a one-glance view to technical readers and funders.

AI MUST maintain a section like:

## Tech Stack & Build Targets

| Area       | Choice / Tooling                    | Notes                               |
| ---------- | ----------------------------------- | ----------------------------------- |
| CPU/Arch   | 32-bit x86 (i386)                   | Protected mode, no long mode yet    |
| Language   | C + a small amount of x86 assembly  | Freestanding, no libc at runtime    |
| Toolchain  | gcc, ld, make                       | 32-bit cross-compile setup          |
| Bootloader | GRUB (BIOS)                         | Custom PenOS theme + splash         |
| Emulator   | qemu-system-i386                    | Also works in VirtualBox/VMware     |
| Graphics   | Multiboot framebuffer (1024×768×32) | VGA text fallback if no framebuffer |
| Memory     | Bitmap PMM + paging + kernel heap   | Higher-half kernel design           |
| Scheduler  | Preemptive round-robin              | Demo tasks launched from PenShell   |

Rules:

When the underlying tech changes (e.g., new supported resolution, new toolchain, new architecture):

AI MUST update this table accordingly.

AI MUST keep the descriptions concise and understandable.

New major subsystems (e.g., networking stack, filesystem) MUST be surfaced here with a new row once they are actually implemented, not just planned.

8.4 Mapping Features to README Sections

For each new feature entry in Section 6 and the corresponding commit doc:

If it is user-facing (e.g. new shell command, GUI behavior, better splash), AI MUST:

Update the “What PenOS Gives You” section:

Add a bullet or short paragraph summarizing the new capability in plain language.

Optionally, update the “Feature Map” table to reference the deeper docs.

If it is architecture-facing (e.g. new heap strategy, new scheduler tuning) but not obvious to users:

Update “Architecture at a Glance” and/or “Feature Map” with a short, non-jargony summary.

If it is experimental or internal-only, AI SHOULD:

Mention it lightly in README (or not at all) but must not oversell incomplete features.

8.5 README Tone & Audience

AI MUST treat README.md as:

A marketing/overview document, not just a technical dump.

Target audience includes:

Engineers,

Recruiters/hiring managers,

Potential funders or partners,

Learners interested in OS dev.

Therefore:

Use clear headings, short paragraphs, and bullet lists.

Explain why a feature matters (e.g. “Frame buffer splash makes PenOS feel like a real OS from the first frame”) not just how it’s coded.

Avoid:

Very low-level assembler detail in README (keep that for docs/architecture.md and commit docs).

Overly casual language; aim for confident, readable, and professional.

8.6 Alignment With Business/Product Story

Whenever README is updated for a feature, AI MUST ensure:

The wording is consistent with:

The asset type and product name in Section 2 (PenCore, PenShell, PenDesk, etc.).

The Feature / Product entry in Section 6 for that feature.

The associated commit doc under docs/commits/feature-_ or docs/commits/fix-_.

If a README change materially repositions a module (e.g. turning PenDesk into a stand-alone product), the AI MUST:

Call that out explicitly in its response.

Not silently rewrite the overall business story.

### [BOOT-MODES-001] Dual text/GUI boot modes

- **Date:** 2025-11-29
- **Branch:** `feature/boot-modes-gui-text`
- **Commit message:** `Add boot-time GUI/text mode selection via Multiboot cmdline`
- **Short commit description:** Parse `penos.mode=` from GRUB so the same kernel can boot directly into the shell or auto-launch the desktop.

- **Asset type:** kernel, gui
- **Product name (module):** PenCore, PenDesk

- **Technical summary:**

  - Added `kernel/boot_mode` helper to parse the Multiboot command line.
  - Updated `kernel_main` to call `desktop_start()` automatically only when `penos.mode=gui`.
  - Expanded `grub/grub.cfg` and README to document the two menu entries and shared binary story.

- **Architecture notes:**

  - Boot mode parsing happens before subsystem init so later code can query a simple enum.
  - GUI autostart is guarded by framebuffer presence and still restores the console on ESC thanks to existing desktop behavior.
  - Shell `gui` command and scheduler/driver flows remain untouched, preserving compatibility.

- **Business proposition:**

  - Ship one ISO that acts as both **PenOS Core Text** (console-first) and **PenOS Desktop GUI** (auto-GUI) SKUs.
  - Licensing partners can differentiate offerings (headless classroom server vs. kiosk demo) without diverging code.
  - Highlights PenOS’s modularity for investors: a single kernel underpins multiple branded experiences.

- **Brand & packaging ideas:**

  - PenCore Core Text Edition (default GRUB entry) and PenDesk Desktop Edition (GUI entry).
  - Potential OEM menu branding (“PenOS Control”, “PenOS Experience”) leveraging the same binary but different boot parameters.

- **Docs / links to create or update:**
  - `docs/commits/feature-boot-modes-gui-text/1_gui_text_boot_modes.md`
  - `README.md` (Boot & Splash, Quick Start, What PenOS Gives You)
  - `grub/grub.cfg`
