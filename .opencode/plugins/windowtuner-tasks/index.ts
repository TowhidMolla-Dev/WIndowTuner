import { Plugin } from "@opencode/plugin"

// The server part makes this project-local plugin discoverable to the TUI,
// which renders the companion ./tui.tsx contribution in sidebar.content.
export default Plugin.define({
  id: "windowtuner.tasks",
  setup() {},
})
