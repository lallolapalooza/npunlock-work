# Intel NPU — omp chat archives

Exported 2026-10-06 with the `omp-archive` plugin core (`buildSessionArchive`).
Each `omp-archive-<timestamp>_<session-id>.zip` contains:

| Path | Contents |
| --- | --- |
| `session.jsonl` | the main transcript, byte-for-byte |
| `artifacts/` | every file under the session artifact dir: subagent transcripts, `*.bash.log` / `*.eval.log` / `*.read.log` (full untruncated output), tombstones |
| `viewer.html` | standalone HTML viewer (from `omp --export`) |
| `manifest.json` / `agents.json` / `jobs.json` | index, recovered async jobs |

## touching-folders/

Chats that actually operated on the two Intel-NPU work folders
`~/projects/r2gvp04w-re` (later `r2gvp04w-re.1`) and `~/projects/npunlock-work`
(now trashed to `/tmp/trash/…`): the npunlock mega-session, the repo pushes,
and the archive/cleanup sessions that zipped, moved, restored or flattened them.

## other-npu/

Other Intel-NPU chats that touch neither folder — the OpenVINO/llama.cpp NPU
bug hunts and the Intel NPU driver-DLL reverse-engineering sessions.

> Archives are not redacted: they can contain secrets, raw context and images.
