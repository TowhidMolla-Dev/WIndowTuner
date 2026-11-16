-- Workspace, monitor and startup placement for WindowTuner.
--
-- This file is embedded in the generated module, not run as a daemon. It
-- delegates matching to Hyprland and only emits rules during config load.
return function(config)
    if type(hl) ~= "table" then return nil end

    config = config or {}
    local workspaces = config.workspaces or {}
    local monitors = config.monitors or {}
    local startup = config.startup or {}
    local autostart = config.autostart or {}

    local state = {subscriptions = {}, keybinds = {}, stopped = false, last_error = nil}

    -- The two state/config roots are overridable so the runtime can be tested
    -- without touching a live Omarchy install.
    local state_home = config.state_home or os.getenv("XDG_STATE_HOME") or
        ((os.getenv("HOME") or "") .. "/.local/state")
    local config_home = config.config_home or os.getenv("XDG_CONFIG_HOME") or
        ((os.getenv("HOME") or "") .. "/.config")

    -- A tool that plans workspaces globally (hyprmoncfg) switches Omarchy's
    -- per-monitor numbering off, but its file loads after this one, so look for
    -- that switch instead of trusting load order.
    local function stands_down()
        local file = io.open(config_home .. "/hypr/hyprmoncfg-monitors.lua", "r")
        if not file then return false end
        local text = file:read("*a") or ""
        file:close()
        return text:find("o.workspaces.enabled = false", 1, true) ~= nil
    end

    local per_monitor = type(o) == "table" and type(o.workspaces) == "table" and
        type(o.workspaces.workspace_id) == "function" and not stands_down()

    local function plain(desc)
        return (tostring(desc or ""):gsub("^desc:", ""))
    end

    local function owner(desc)
        local want = plain(desc)
        if want == "" or type(hl.get_monitors) ~= "function" then return nil end
        for _, monitor in ipairs(hl.get_monitors()) do
            if monitor.description == want then return monitor end
        end
        return nil
    end

    -- Omarchy writes which monitor owns which block of ten in a state file.
    -- Hyprland loads configs before it reports live monitors, so read the file
    -- first and only fall back to the live list.
    local function block_of(desc)
        local want = plain(desc)
        if want == "" then return nil end
        local file = io.open(state_home .. "/omarchy/monitor-workspaces", "r")
        if file then
            local found = nil
            for line in file:lines() do
                local block, identity = line:match("^(%d+)\t[^\t]*\t(.+)$")
                if block and (identity:gsub(" #%d+$", "")) == want then found = tonumber(block); break end
            end
            file:close()
            if found then return found end
        end
        local monitor = owner(desc)
        if monitor and type(o) == "table" and type(o.workspaces) == "table" and
            type(o.workspaces.workspace_id) == "function" then
            -- Ask Omarchy for the id of slot 1 and derive its block.
            local ok, id = pcall(o.workspaces.workspace_id, monitor, 1)
            if ok and type(id) == "number" then return (id - 1) // 10 end
        end
        return nil
    end

    -- The real workspace id of a slot on a monitor. Without per-monitor
    -- workspaces a slot is just its own number.
    local function slot_id(slot, desc)
        slot = tostring(slot or "")
        if not slot:match("^%d+$") then return slot end
        if per_monitor and plain(desc) ~= "" then
            local block = block_of(desc)
            if block then return tostring(block * 10 + tonumber(slot)) end
            local monitor = owner(desc)
            if monitor then
                local ok, id = pcall(o.workspaces.workspace_id, monitor, tonumber(slot))
                if ok and type(id) == "number" then return tostring(id) end
            end
        end
        return slot
    end
    state.workspace_id = slot_id

    local function safe_call(fn, ...)
        if type(fn) ~= "function" then return false end
        local ok = pcall(fn, ...)
        if not ok then state.last_error = "WindowTuner could not apply a workspace or monitor rule." end
        return ok
    end

    -- Display layout. Saved by make, model and serial so a cable moved to
    -- another port keeps the layout.
    if type(hl.monitor) == "function" then
        for desc, entry in pairs(monitors) do
            if plain(desc) ~= "" then
                local spec = {output = "desc:" .. plain(desc)}
                if entry.mode and entry.mode ~= "" then spec.mode = entry.mode end
                if entry.x ~= nil and entry.y ~= nil then
                    spec.position = tostring(entry.x) .. "x" .. tostring(entry.y)
                end
                if entry.scale ~= nil then spec.scale = entry.scale end
                if entry.transform ~= nil then spec.transform = entry.transform end
                if entry.disabled ~= nil then spec.disabled = entry.disabled end
                safe_call(hl.monitor, spec)
            end
        end
    end

    -- Which slot each monitor starts on: its lowest owned slot.
    local lowest = {}
    local owned = {}
    for slot, entry in pairs(workspaces) do
        local name = tostring(slot)
        local monitor = plain(entry.monitor)
        if name:match("^%d+$") and monitor ~= "" then
            owned[#owned + 1] = {slot = tonumber(name), monitor = monitor}
            if not lowest[monitor] or tonumber(name) < lowest[monitor] then lowest[monitor] = tonumber(name) end
        end
    end
    table.sort(owned, function(left, right)
        if left.monitor ~= right.monitor then return left.monitor < right.monitor end
        return left.slot < right.slot
    end)

    if type(hl.workspace_rule) == "function" then
        local slots = {}
        for slot in pairs(workspaces) do slots[#slots + 1] = tostring(slot) end
        table.sort(slots, function(left, right)
            local a, b = tonumber(left) or 0, tonumber(right) or 0
            if a ~= b then return a < b end
            return left < right
        end)
        for _, slot in ipairs(slots) do
            local entry = workspaces[slot]
            local monitor = plain(entry.monitor)
            local rule = {workspace = slot_id(slot, monitor)}
            if monitor ~= "" then
                rule.monitor = "desc:" .. monitor
                if lowest[monitor] == tonumber(slot) then rule.default = per_monitor or nil end
            end
            if entry.persistent then rule.persistent = true end
            safe_call(hl.workspace_rule, rule)
        end
    end

    -- With per-monitor workspaces Super+N means "slot N on the monitor you are
    -- on". A slot assigned to a monitor is made to mean that monitor's slot
    -- from anywhere, so the key always lands where the slot lives.
    if per_monitor and type(o.rebind) == "function" then
        local function move_to(slot, desc, follow)
            return function()
                local window = hl.get_active_window and hl.get_active_window()
                local monitor = owner(desc)
                if not window or not monitor then return end
                hl.dispatch(hl.dsp.window.move({workspace = slot_id(slot, desc), follow = follow,
                    window = "address:" .. window.address}))
            end
        end
        for _, entry in ipairs(owned) do
            if entry.slot >= 1 and entry.slot <= 10 then
                local key = "code:" .. tostring(entry.slot + 9)
                state.keybinds[#state.keybinds + 1] = o.rebind("SUPER + " .. key,
                    "Switch to workspace " .. entry.slot, function()
                        local monitor = owner(entry.monitor)
                        if monitor then o.workspaces.focus(entry.slot, monitor.name)() end
                    end)
                state.keybinds[#state.keybinds + 1] = o.rebind("SUPER + SHIFT + " .. key,
                    "Move window to workspace " .. entry.slot, move_to(entry.slot, entry.monitor, true))
                state.keybinds[#state.keybinds + 1] = o.rebind("SUPER + SHIFT + ALT + " .. key,
                    "Move window silently to workspace " .. entry.slot, move_to(entry.slot, entry.monitor, false))
            end
        end
    end

    -- Startup-only placement: a window the app opens within the chosen seconds
    -- of the compositor starting goes to its workspace; later windows do not.
    if #startup > 0 and type(hl.on) == "function" then
        local function seconds_since_start()
            local up = io.open("/proc/uptime", "r")
            if not up then return nil end
            local uptime = tonumber((up:read("*l") or ""):match("^(%S+)"))
            up:close()
            local stat_file = io.open("/proc/self/stat", "r")
            if not stat_file then return nil end
            local stat = stat_file:read("*l") or ""
            stat_file:close()
            -- The command name may contain spaces and brackets, so count from
            -- the last ")".
            local rest = stat:match("^.*%)%s+(.*)$")
            if not uptime or not rest then return nil end
            local n = 0
            for field in rest:gmatch("%S+") do
                n = n + 1
                if n == 20 then
                    local started = tonumber(field)
                    return started and (uptime - started / 100) or nil
                end
            end
            return nil
        end
        -- `config.now` is a test seam: the real clock is the age of the process.
        local clock = config.now or seconds_since_start
        state.subscriptions[#state.subscriptions + 1] = hl.on("window.open", function(window)
            local elapsed = clock()
            if not elapsed or not window then return end
            for _, placement in ipairs(startup) do
                -- Classes can be spelled differently in a desktop entry and a
                -- window, especially on the first launch of the session.
                if (window.class or ""):lower() == tostring(placement.class or ""):lower() and
                    elapsed <= tonumber(placement.within or 20) then
                    hl.dispatch(hl.dsp.window.move({workspace = slot_id(placement.workspace, placement.monitor),
                        follow = placement.follow ~= false, window = "address:" .. window.address}))
                    return
                end
            end
        end)
    end

    -- Launch at login. Omarchy's helper is used when present; elsewhere the
    -- same command runs once on Hyprland's start event.
    if #autostart > 0 then
        local omarchy = type(o) == "table" and type(o.exec_on_start) == "function"
        for _, entry in ipairs(autostart) do
            local command = entry.command or ""
            if command ~= "" then
                if omarchy and entry.omarchy and entry.omarchy ~= "" then
                    safe_call(o.exec_on_start, entry.omarchy)
                elseif type(hl.on) == "function" then
                    state.subscriptions[#state.subscriptions + 1] = hl.on("hyprland.start", function()
                        hl.dispatch(hl.dsp.exec_cmd(command))
                    end)
                end
            end
        end
    end

    function state.stop()
        if state.stopped then return end
        state.stopped = true
        for _, subscription in ipairs(state.subscriptions) do
            if subscription and type(subscription.remove) == "function" then subscription:remove() end
        end
        for _, keybind in ipairs(state.keybinds) do
            if keybind and type(keybind.remove) == "function" then keybind:remove() end
        end
    end
    return state
end
