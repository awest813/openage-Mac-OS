# macOS packaging helpers

| File | Purpose |
| ---- | ------- |
| `Import Game Assets.command` | Double-clickable converter: opens Finder (or uses a dropped folder) and runs `./run convert --force …` |

Place `Import Game Assets.command` next to the `run` launcher in a portable
release tree, or invoke it from a built repo (it walks upward to find `run`).
