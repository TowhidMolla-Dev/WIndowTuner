-- Regression scenarios for src/appspace_runtime.lua.
--
-- The runtime is the part that ends up inside the generated Hyprland module,
-- so it is exercised against a fake compositor: no live Hyprland, no writes to
-- the user's config, and no AppSpace files are touched.
local install = assert(loadfile(arg[1] or "src/appspace_runtime.lua"))()

local passed = 0
local function check(name, test)
    local ok, problem = pcall(test)
    if not ok then
        io.stderr:write("FAIL: " .. name .. "\n  " .. tostring(problem) .. "\n")
        os.exit(1)
    end
    passed = passed + 1
    print("PASS: " .. name)
end

local function tempdir()
    local name = os.tmpname()
    os.remove(name)
    assert(os.execute("mkdir -p '" .. name .. "'"))
    return name
end

local function writefile(path, text)
    local directory = path:match("^(.*)/[^/]+$")
    if directory then assert(os.execute("mkdir -p '" .. directory .. "'")) end
    local file = assert(io.open(path, "w"))
    file:write(text)
    file:close()
end

local function monitor(name, description)
    return {name = name, description = description, x = 0, y = 0, width = 1920, height = 1080,
        scale = 1, transform = 0}
end

-- A fake Hyprland/Omarchy surface. Every generated rule and dispatch lands in
-- `calls`, so assertions describe exactly what Hyprland would be asked to do.
local function environment(options)
    options = options or {}
    local env = {calls = {}, events = {}, rebinds = {}, monitors = {}, active = nil}
    env.monitors[1] = monitor("HDMI-A-1", "WAM WD215I09")

    _G.hl = {
        get_monitors = function() return env.monitors end,
        get_active_window = function() return env.active end,
        monitor = function(spec) env.calls[#env.calls + 1] = {kind = "monitor", spec = spec}; return {} end,
        workspace_rule = function(spec)
            env.calls[#env.calls + 1] = {kind = "workspace_rule", spec = spec}
            return {}
        end,
        on = function(event, callback)
            local entry = {callback = callback, removed = false}
            env.events[event] = env.events[event] or {}
            env.events[event][#env.events[event] + 1] = entry
            return {remove = function() entry.removed = true end}
        end,
        dispatch = function(command) env.calls[#env.calls + 1] = {kind = "dispatch", command = command} end,
        dsp = {
            window = {move = function(args) return {kind = "move", args = args} end},
            exec_cmd = function(command) return {kind = "exec", command = command} end,
        },
    }

    local o = {}
    _G.o = o
    if options.omarchy then
        o.exec_on_start = function(command) env.calls[#env.calls + 1] = {kind = "exec_on_start", command = command} end
        o.rebind = function(key, description, callback)
            local entry = {key = key, description = description, callback = callback, removed = false}
            env.rebinds[#env.rebinds + 1] = entry
            return {remove = function() entry.removed = true end}
        end
        o.workspaces = {
            workspace_id = function(selected, slot) return (env.block or 0) * 10 + slot end,
            focus = function(slot, selected)
                return function() env.calls[#env.calls + 1] = {kind = "focus", slot = slot, monitor = selected} end
            end,
        }
    end

    function env:emit(event, ...)
        for _, entry in ipairs(self.events[event] or {}) do entry.callback(...) end
    end
    function env:kinds(kind)
        local found = {}
        for _, call in ipairs(self.calls) do
            if call.kind == kind then found[#found + 1] = call end
        end
        return found
    end
    function env:workspace(slot)
        for _, call in ipairs(self.calls) do
            if call.kind == "workspace_rule" and call.spec.workspace == slot then return call.spec end
        end
        return nil
    end
    function env:rebind(key)
        for _, entry in ipairs(self.rebinds) do
            if entry.key == key then return entry end
        end
        return nil
    end
    return env
end

local function roots(options)
    options = options or {}
    local state = tempdir()
    local config = tempdir()
    if options.blockFile then
        writefile(state .. "/omarchy/monitor-workspaces", options.blockFile)
    end
    if options.hyprmoncfg then
        writefile(config .. "/hypr/hyprmoncfg-monitors.lua", "o.workspaces.enabled = false\n")
    end
    return state, config
end

local function base(config)
    config.workspaces = {
        ["1"] = {monitor = "WAM WD215I09", persistent = true},
        ["2"] = {monitor = "WAM WD215I09"},
    }
    config.monitors = {
        ["WAM WD215I09"] = {mode = "1920x1080@75.00", x = 0, y = 0, scale = 1, transform = 0},
    }
    return config
end

check("an empty configuration installs nothing and leaves no globals", function()
    local env = environment()
    local state = install({})
    assert(#env.calls == 0 and #env.rebinds == 0 and next(env.events) == nil)
    assert(type(state.stop) == "function")
    state.stop()
end)

check("monitor layout is written by description, not by connector", function()
    local env = environment()
    local stateHome, configHome = roots()
    local config = {state_home = stateHome, config_home = configHome}
    base(config)
    config.monitors["WAM WD215I09"] = {mode = "1280x720@60.00", x = 1920, y = 0, scale = 1.25, transform = 1}
    install(config)
    local rules = env:kinds("monitor")
    assert(#rules == 1)
    local spec = rules[1].spec
    assert(spec.output == "desc:WAM WD215I09", spec.output)
    assert(spec.mode == "1280x720@60.00")
    assert(spec.position == "1920x0")
    assert(spec.scale == 1.25 and spec.transform == 1)
end)

check("workspace rules pin a slot to its display and keep it persistent", function()
    local env = environment({omarchy = true})
    local stateHome, configHome = roots()
    local config = {state_home = stateHome, config_home = configHome}
    base(config)
    install(config)
    local spec = env:workspace("1")
    assert(spec, "workspace 1 rule missing")
    assert(spec.monitor == "desc:WAM WD215I09", spec.monitor)
    assert(spec.persistent == true)
    -- Per-monitor numbering is on, so the lowest owned slot is the default.
    assert(spec["default"] == true)
end)

check("only the lowest owned slot becomes the display's default", function()
    local env = environment({omarchy = true})
    local stateHome, configHome = roots()
    local config = {state_home = stateHome, config_home = configHome}
    base(config)
    install(config)
    assert(env:workspace("1")["default"] == true)
    assert(env:workspace("2")["default"] == nil)
end)

check("slot numbers follow the Omarchy block file", function()
    local env = environment({omarchy = true})
    local stateHome, configHome = roots({blockFile = "1\tHDMI-A-1\tWAM WD215I09\n"})
    local config = {state_home = stateHome, config_home = configHome}
    base(config)
    install(config)
    assert(env:workspace("11"), "workspace 11 should be the first slot of block 1")
    assert(env:workspace("12"), "workspace 12 should be the second slot of block 1")
end)

check("without a block file the Omarchy slot API resolves the real workspace id", function()
    local env = environment({omarchy = true})
    env.block = 2
    local stateHome, configHome = roots()
    local config = {state_home = stateHome, config_home = configHome}
    base(config)
    install(config)
    assert(env:workspace("21") and env:workspace("22"))
end)

check("a hyprmoncfg switch keeps plain workspace numbers", function()
    local env = environment({omarchy = true})
    local stateHome, configHome = roots({hyprmoncfg = true})
    local config = {state_home = stateHome, config_home = configHome}
    base(config)
    install(config)
    local spec = env:workspace("1")
    assert(spec and spec.monitor == "desc:WAM WD215I09")
    assert(spec["default"] == nil, "default must not be set when numbering stands down")
end)

check("workspace_id resolves slots for generated window rules", function()
    local env = environment({omarchy = true})
    local stateHome, configHome = roots({blockFile = "1\tHDMI-A-1\tWAM WD215I09\n"})
    local state = install({state_home = stateHome, config_home = configHome,
        workspaces = {["3"] = {monitor = "WAM WD215I09"}}})
    assert(state.workspace_id("3", "desc:WAM WD215I09") == "13")
    -- Unknown or empty monitors fall back to the plain number.
    assert(state.workspace_id("3", "desc:unknown") == "3")
    assert(state.workspace_id("3", "") == "3")
    assert(state.workspace_id("special", "desc:WAM WD215I09") == "special")
end)

check("every owned slot gets Super+N, move and silent-move rebinds", function()
    local env = environment({omarchy = true})
    local stateHome, configHome = roots()
    local config = {state_home = stateHome, config_home = configHome}
    base(config)
    install(config)
    assert(#env.rebinds == 6, #env.rebinds)
    assert(env:rebind("SUPER + code:10"), "Super+1 missing")
    assert(env:rebind("SUPER + SHIFT + code:10"))
    assert(env:rebind("SUPER + SHIFT + ALT + code:10"))
    assert(env:rebind("SUPER + code:11"), "Super+2 missing")
end)

check("Super+N focuses the slot on the owning display", function()
    local env = environment({omarchy = true})
    local stateHome, configHome = roots()
    local config = {state_home = stateHome, config_home = configHome}
    base(config)
    install(config)
    env:rebind("SUPER + code:11").callback()
    local focus = env:kinds("focus")[1]
    assert(focus, "focus dispatch missing")
    assert(focus.slot == 2 and focus.monitor == "HDMI-A-1", focus.monitor)
end)

check("moving a window follows or stays silently", function()
    local env = environment({omarchy = true})
    local stateHome, configHome = roots()
    local config = {state_home = stateHome, config_home = configHome}
    base(config)
    install(config)
    env.active = {address = "0x9"}
    env:rebind("SUPER + SHIFT + code:10").callback()
    env:rebind("SUPER + SHIFT + ALT + code:10").callback()
    local moves = env:kinds("dispatch")
    assert(#moves == 2)
    assert(moves[1].command.args.window == "address:0x9")
    assert(moves[1].command.args.workspace == "1" and moves[1].command.args.follow == true)
    assert(moves[2].command.args.follow == false)
end)

check("an app with no monitor stops the Super+N rebinds from moving anything", function()
    local env = environment({omarchy = true})
    local stateHome, configHome = roots()
    install({state_home = stateHome, config_home = configHome,
        workspaces = {["5"] = {monitor = "gone"}}})
    env.active = {address = "0x9"}
    env:rebind("SUPER + SHIFT + code:14").callback()
    assert(#env:kinds("dispatch") == 0, "must not move a window to a missing display")
end)

check("startup placement only applies inside the app's startup window", function()
    for elapsed, expected in pairs({[5] = true, [120] = false}) do
        local env = environment()
        local stateHome, configHome = roots()
        local state = install({state_home = stateHome, config_home = configHome,
            workspaces = {["1"] = {monitor = "WAM WD215I09"}},
            startup = {{class = "Spotify", workspace = "1", monitor = "WAM WD215I09",
                follow = true, within = 60}},
            now = function() return elapsed end})
        assert(state)
        env:emit("window.open", {class = "spotify", address = "0x4"})
        local moves = env:kinds("dispatch")
        if expected then
            assert(#moves == 1, "a window inside the startup window must be placed")
            assert(moves[1].command.args.workspace == "1")
            assert(moves[1].command.args.window == "address:0x4")
            assert(moves[1].command.args.follow == true)
        else
            assert(#moves == 0, "a window opened later must keep its workspace")
        end
    end
end)

check("startup placement ignores other applications", function()
    local env = environment()
    local stateHome, configHome = roots()
    install({state_home = stateHome, config_home = configHome,
        startup = {{class = "Spotify", workspace = "1", within = 60}},
        now = function() return 1 end})
    env:emit("window.open", {class = "foot", address = "0x5"})
    assert(#env.calls == 0)
end)

check("autostart prefers Omarchy's helper", function()
    local env = environment({omarchy = true})
    local stateHome, configHome = roots()
    install({state_home = stateHome, config_home = configHome,
        autostart = {{command = "uwsm-app -- demo.desktop", omarchy = "uwsm-app -- demo.desktop"}}})
    local launched = env:kinds("exec_on_start")
    assert(#launched == 1 and launched[1].command == "uwsm-app -- demo.desktop")
    assert(env.events["hyprland.start"] == nil, "Omarchy handles autostart, Hyprland must not run it twice")
end)

check("autostart falls back to Hyprland's start event elsewhere", function()
    local env = environment()
    local stateHome, configHome = roots()
    install({state_home = stateHome, config_home = configHome,
        autostart = {{command = "demo --flag"}}})
    env:emit("hyprland.start")
    local launches = env:kinds("dispatch")
    assert(#launches == 1)
    assert(launches[1].command.kind == "exec" and launches[1].command.command == "demo --flag")
end)

check("stop removes subscriptions and keybinds and is safe to repeat", function()
    local env = environment({omarchy = true})
    local stateHome, configHome = roots()
    local state = install(base({state_home = stateHome, config_home = configHome,
        startup = {{class = "Spotify", workspace = "1", within = 60}},
        autostart = {{command = "demo"}}}))
    local bind = env.rebinds[1]
    assert(bind and not bind.removed)
    state.stop()
    assert(#env.rebinds > 0)
    for _, entry in ipairs(env.rebinds) do assert(entry.removed, entry.key) end
    for _, entries in pairs(env.events) do
        for _, entry in ipairs(entries) do assert(entry.removed) end
    end
    state.stop()
    assert(state.stopped == true)
end)

check("a missing display on the saved layout is left alone", function()
    local env = environment()
    local stateHome, configHome = roots()
    install({state_home = stateHome, config_home = configHome,
        workspaces = {["1"] = {monitor = ""}},
        monitors = {[""] = {mode = "1920x1080@60.00"}}})
    assert(#env:kinds("monitor") == 0, "an empty description cannot identify a display")
    local spec = env:workspace("1")
    assert(spec and spec.monitor == nil and spec["default"] == nil)
end)

print(string.format("AppSpace runtime: %d regression scenarios passed.", passed))
