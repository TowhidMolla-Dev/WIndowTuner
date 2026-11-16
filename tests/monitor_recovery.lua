local install = assert(loadfile(arg[1] or "src/monitor_recovery.lua"))()
local passed = 0
local function check(name, test)
    test()
    passed = passed + 1
    print("PASS: " .. name)
end
local function setup(extraPolicies, options)
    options = options or {}
    local a = {name = "DP-1", width = 3840, height = 2160, scale = 2, x = 1920, y = 0, reserved = {top = 40}}
    local b = {name = "eDP-1", width = 1280, height = 720, scale = 1, x = 0, y = 0, reserved = {top = 40}}
    local w = {address = "0x1", pid = 42, stable_id = 7, mapped = true, hidden = false, floating = true,
        fullscreen = 0, fullscreen_client = 0, monitor = a, at = {x = 2100, y = 120},
        size = {width = 800, height = 600}, tags = {owned = true}}
    local env = {monitors = {a, b}, windows = {w}, events = {}, calls = {}, timers = {}, a = a, b = b, w = w}
    _G.hl = {
        get_monitors = function() return env.monitors end,
        get_windows = function(filter)
            local found = {}
            for _, window in ipairs(env.windows) do
                if window.tags[filter.tag] then found[#found + 1] = window end
            end
            return found
        end,
        on = function(event, callback)
            env.events[event] = callback
            return {remove = function() env.events[event] = nil end}
        end,
        timer = function(callback, options)
            -- Hyprland --verify-config has no event loop; the runtime must not
            -- require a timer there.
            if env.noTimer then error("timer API unavailable during verification") end
            assert(options.type == "oneshot")
            local timer = {enabled = true, callback = callback}
            function timer:set_timeout(timeout) assert(timeout == 250) end
            function timer:set_enabled(enabled) self.enabled = enabled end
            env.timers[#env.timers + 1] = timer
            return timer
        end,
        dsp = {window = {
            move = function(args) return {kind = "move", args = args} end,
            resize = function(args) return {kind = "resize", args = args} end
        }},
        dispatch = function(command)
            env.calls[#env.calls + 1] = command
            if env.fail and command.kind == "resize" then env.fail = false; return {ok = false} end
            assert(command.args.window == "address:0x1", "Must never dispatch to an active/unowned window")
            if command.kind == "resize" then w.size = {width = command.args.x, height = command.args.y}
            elseif command.args.monitor then
                assert(command.args.follow == false)
                for _, monitor in ipairs(env.monitors) do
                    if monitor.name == command.args.monitor then w.monitor = monitor end
                end
            else w.at = {x = command.args.x, y = command.args.y} end
            return {ok = true}
        end
    }
    local p = {monitor = "DP-1", fallback = "eDP-1", unit = "percent", width = 50, height = 50,
        anchor = "bottom-right", margin = 24}
    env.policy = {tag = "owned", placement = p}
    local policies = {env.policy}
    for _, other in ipairs(extraPolicies or {}) do policies[#policies + 1] = other end
    env.noTimer = options.noTimer
    env.state = install(policies)
    function env:flush()
        local timer = self.timers[#self.timers]
        if timer and timer.enabled then timer.enabled = false; timer.callback() end
    end
    function env:event(name) assert(self.events[name])(); self:flush() end
    function env:disconnect()
        self.monitors = {self.b}; self.w.monitor = self.b -- Hyprland's initial rehome.
        self:event("monitor.removed")
    end
    assert(#env.calls == 0) -- Installing records a baseline without moving healthy windows.
    assert(env.state.records["0x1"])
    return env
end

check("installing records a baseline without creating a timer", function()
    local e = setup()
    assert(#e.timers == 0)
    assert(e.state.records["0x1"].observed == "DP-1")
end)
check("runtime events create the debounce timer lazily", function()
    local e = setup(); e:event("monitor.layout_changed")
    assert(#e.timers == 1 and #e.calls == 0)
end)
check("timer-less runtimes reconcile immediately", function()
    local e = setup(nil, {noTimer = true}); e:disconnect()
    assert(#e.timers == 0 and e.w.monitor.name == "eDP-1" and #e.calls == 3)
end)

check("disconnect recalculates scaled percentages and uses named fallback", function()
    local e = setup(); e:disconnect()
    assert(e.w.monitor.name == "eDP-1")
    assert(e.w.size.width == 640 and e.w.size.height == 340)
    assert(e.w.at.x == 616 and e.w.at.y == 356)
    assert(#e.calls == 3)
end)
check("reconnect returns untouched window using global monitor coordinates", function()
    local e = setup(); e:disconnect(); e.monitors = {e.a, e.b}; e:event("monitor.added")
    assert(e.w.monitor == e.a and e.w.at.x == 2856 and e.w.at.y == 536)
    assert(e.w.size.width == 960 and e.w.size.height == 520)
end)
check("event storms debounce and repeated topology events are idempotent", function()
    local e = setup(); e:disconnect()
    for _ = 1, 20 do e.events["monitor.layout_changed"]() end
    e:flush()
    assert(#e.calls == 3)
end)
check("manual fallback movement prevents forced reconnect restoration", function()
    local e = setup(); e:disconnect(); e.w.at.x = 50
    e.monitors = {e.a, e.b}; e:event("monitor.added")
    assert(e.w.monitor == e.b and e.w.at.x == 50 and #e.calls == 3)
end)
check("all-output outage preserves recovery and recovers on reconnect", function()
    local e = setup(); e.monitors = {}; e:event("monitor.removed")
    assert(#e.calls == 0 and e.state.records["0x1"])
    e.monitors = {e.b}; e.w.monitor = e.b; e:event("monitor.added")
    assert(e.w.size.width == 640)
end)
check("fullscreen tiled hidden and grouped windows are not changed", function()
    for _, field in ipairs({"fullscreen", "fullscreen_client", "floating", "hidden", "group"}) do
        local e = setup()
        e.w[field] = field == "floating" and false or field == "group" and {} or
            (field == "fullscreen" or field == "fullscreen_client") and 1 or true
        -- Lua's and/or expression cannot retain false.
        if field == "floating" then e.w.floating = false end
        e:disconnect(); assert(#e.calls == 0, field)
    end
end)
check("native-tag scope excludes unrelated windows and unknown matches", function()
    local e = setup(); e.w.tags = {}; e:disconnect()
    assert(#e.calls == 0)
end)
check("multiple matching runtime policies are skipped rather than fought over", function()
    local e = setup({{tag = "owned", placement = {monitor = "DP-1"}}})
    e:disconnect(); assert(#e.calls == 0)
end)
check("address reuse cannot inherit old recovery ownership", function()
    local e = setup(); e:disconnect(); e.w.stable_id = 99
    e.monitors = {e.a, e.b}; e:event("monitor.added")
    assert(#e.calls == 3 and e.w.monitor == e.b)
end)
check("partial dispatch failure rolls back safely and retries on next event", function()
    local e = setup(); e.fail = true; e:disconnect()
    assert(e.state.last_error and e.w.monitor == e.b)
    assert(e.w.at.x >= 0 and e.w.at.x + e.w.size.width <= 1280)
    e:event("monitor.layout_changed")
    assert(e.state.last_error == nil and e.w.size.width == 640)
end)
check("missing preferred and fallback choose a deterministic usable output", function()
    local e = setup(); e.policy.placement.fallback = "not-connected"
    e:disconnect(); assert(e.w.monitor == e.b and e.w.size.width == 640)
end)
check("cleanup disables timers and removes every event subscription", function()
    local e = setup(); e.events["monitor.layout_changed"]()
    assert(#e.timers == 1 and e.timers[1].enabled)
    e.state.stop()
    assert(next(e.events) == nil and not e.timers[1].enabled)
    e.state.reconcile(); assert(#e.calls == 0)
    local bare = setup(); bare.state.stop()
    assert(next(bare.events) == nil and #bare.timers == 0)
end)
print(string.format("Monitor recovery: %d regression scenarios passed.", passed))
