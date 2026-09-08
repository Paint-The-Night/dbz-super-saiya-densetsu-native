# Small-model reconstruction batches

Use one fresh Luna CLI session per bounded routine. Supply its address, allowed
files, test command, and stopping condition; do not paste the conversation or
the full native source into the prompt. The CLI still uses the signed-in
account's usage allowance.

Example invocation from the repository root:

```sh
mkdir -p .local/luna-workers
codex exec --ignore-user-config --ephemeral \
  -m gpt-5.6-luna -c 'model_reasoning_effort="low"' \
  --sandbox workspace-write --json \
  -o .local/luna-workers/result.txt \
  'Reconstruct one specified routine. Read only relevant source ranges. Edit only src/native.c, src/native_video.inc, and tests/test_native.c. Preserve bank/mode guards, flags, bus cycles and interrupts. Add ROM differential tests and run native_equivalence. No subagents, commits or pushes. Stop if the routine needs broader infrastructure. Report exact address boundaries, counts and test results.' \
  > .local/luna-workers/events.jsonl 2> .local/luna-workers/stderr.log
```

Replace the example prompt with an actual target address before dispatch.
The parent reviews the final diff once, updates inventory only for verified
sites, and runs the relevant gameplay replay before publishing. Workers that
touch the same source files run serially. Prompts asking for limited time or
tokens are guidance, not an enforced spending cap.

Read the final `turn.completed` event for reported token usage. Record input,
cached input, and output separately; these counts are not equivalent to account
quota percentages. Check account limits between batches and stop near the
user's chosen limit. Do not automatically launch another worker after failure.

## First trial: 8 September 2026

A fresh Luna session investigating `$01:C91F` reported 567,279 input tokens
(506,624 cached) and 2,636 output tokens. It made no source changes. Its final
routine boundary/count claim was incorrect: the ROM has an RTL at `$01:C942`
and a separate helper starting at `$01:C943`. Do not add its proposed coverage
counts to the tracker. Nested calls alone do not require translating callees;
the project supports interpreter fallback at those boundaries.

This trial establishes CLI availability, not cost effectiveness. Future worker
prompts should supply a verified instruction listing and the specific existing
helper signatures, explicitly allow callee fallback, and request minimal tool
output. Review the result before treating a worker's claims as evidence.
