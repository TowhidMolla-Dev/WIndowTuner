import { Plugin } from "@opencode/plugin/tui"
import { readFileSync, unwatchFile, watchFile } from "node:fs"
import { join } from "node:path"
import { For, createSignal, onCleanup } from "solid-js"

type TodoRow = { kind: "heading" | "task" | "text"; text: string; done?: boolean }

function readTodo(path: string): { rows: TodoRow[]; complete: number; total: number } {
  try {
    const rows: TodoRow[] = []
    let complete = 0
    let total = 0
    for (const line of readFileSync(path, "utf8").split(/\r?\n/)) {
      const heading = line.match(/^##\s+(.+)$/)
      if (heading) {
        rows.push({ kind: "heading", text: heading[1] })
        continue
      }
      const task = line.match(/^\s*- \[([ xX])\]\s+(.+)$/)
      if (task) {
        const done = task[1].toLowerCase() === "x"
        total += 1
        if (done) complete += 1
        rows.push({ kind: "task", text: task[2], done })
      }
    }
    return { rows, complete, total }
  } catch {
    return { rows: [{ kind: "text", text: "TODO.md not found in this project." }], complete: 0, total: 0 }
  }
}

function TodoSidebar(props: { path: string; theme: any }) {
  const [todo, setTodo] = createSignal(readTodo(props.path))
  const refresh = () => setTodo(readTodo(props.path))
  watchFile(props.path, { interval: 800 }, refresh)
  onCleanup(() => unwatchFile(props.path, refresh))

  return (
    <box flexDirection="column" padding={1}>
      <text fg={props.theme.text.base}>WindowTuner tasks ({todo().complete}/{todo().total})</text>
      <For each={todo().rows}>
        {(row) =>
          row.kind === "heading" ? (
            <text fg={props.theme.text.base}>{`\n${row.text}`}</text>
          ) : row.kind === "task" ? (
            <text fg={props.theme.text.base}>
              {`${row.done ? "✓" : "○"} ${row.text}`}
            </text>
          ) : (
            <text fg={props.theme.text.base}>{row.text}</text>
          )
        }
      </For>
    </box>
  )
}

export default Plugin.define({
  id: "windowtuner.tasks",
  setup(context) {
    const directory = context.location?.directory ?? process.cwd()
    return context.ui.slot({
      append: "sidebar.content",
      render: () => <TodoSidebar path={join(directory, "TODO.md")} theme={context.theme} />,
    })
  },
})
