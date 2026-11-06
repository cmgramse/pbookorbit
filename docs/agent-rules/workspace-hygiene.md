# Workspace Hygiene

## What this is about

While working, an agent produces two very different kinds of file. Some are
*deliverables*: source code, documentation, configuration — things the project
keeps. Others are *scratch*: a plan it wrote to think with, a half-finished
analysis, a sample output it inspected once. Scratch is the agent's
whiteboard. It is useful during the task and misleading afterwards, because a
later reader cannot tell a discarded draft from a current decision.

This file says where scratch goes and when it disappears.

## The scratch path

All temporary working files go in:

```
.work/<YYYY-MM-DD-short-slug>/
```

One directory per working session. The date prefix keeps sessions sortable;
the slug says what the session was about, for example
`.work/2026-01-15-login-timeout/`. Loose files dropped directly in
`.work/` are not acceptable — they lose the "which session was this?"
information that makes scratch readable at all.

This path is listed in `.gitignore`, so scratch is never committed. If you
find committed scratch, that is a bug to clean up, not a precedent.

## Distil or delete, at the end of every task

When a task or session ends, every scratch directory it created must reach one
of three outcomes:

1. **Distilled, then deleted.** The default. Anything worth keeping is written
   into its real home — the code, the documentation, a decision record, the
   task tracker — and the scratch directory is removed.
2. **Deleted.** Nothing in it was worth keeping.
3. **Deliberately kept.** Rare. Put a `KEEP.md` inside the directory saying
   what makes deleting it premature and what would later make it safe to
   delete. Keeping scratch without that note is the same as forgetting it.

"I will tidy up later" is outcome 4, and it does not exist. Do the distilling
while you still remember what the files meant.

## Never put scratch inside something that ships

Do not create scratch, notes or work-in-progress directories inside any
directory that is packaged, published or installed elsewhere — a library
package, a plugin directory, a build output, a deployable bundle. Scratch
inside a shipped directory travels to whoever installs it, which is a
correctness problem and sometimes a disclosure one. Use the scratch path
above instead, every time.

## Data, not just files

The same rule covers temporary data: sample exports, scratch databases,
downloaded fixtures, captured responses. Keep them under the scratch path,
delete them with it, and never let a temporary copy of real user data outlive
the task that needed it. If temporary data cannot be deleted for some reason,
say so explicitly rather than leaving it in place quietly.

## Secrets

Credentials never belong in scratch, in project files, or in the conversation
— not even briefly, and not even in a file you intend to delete. If a task
seems to need one, stop and ask how this project supplies it.
