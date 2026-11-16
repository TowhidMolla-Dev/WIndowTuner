-- Installed inside the generated module, not a desktop-wide background service.
-- Native rule tags delegate ALL matching (including regex/state) to Hyprland.
return function(policies)
    local state = {subscriptions = {}, records = {}, stopped = false, last_error = nil}
    local function number(value, fallback)
        local n = tonumber(value)
        if not n or n ~= n or math.abs(n) == math.huge then return fallback end
        return n
    end
    local function clamp(n, low, high) return math.max(low, math.min(high, n)) end
    local function round(n) return math.floor(n + 0.5) end
    local function geometry(p, m)
        local scale = number(m.scale, 1)
        if scale <= 0 then return nil end
        local width, height = number(m.width, 0), number(m.height, 0)
        if number(m.transform, 0) % 2 == 1 then width, height = height, width end
        local r = m.reserved or {}
        local left, top = number(r.left, 0), number(r.top, 0)
        local uw = math.floor(width / scale) - left - number(r.right, 0)
        local uh = math.floor(height / scale) - top - number(r.bottom, 0)
        if uw <= 0 or uh <= 0 then return nil end
        local margin = clamp(number(p.margin, 24), 0, math.max(0, math.min(uw, uh) / 2 - 1))
        local aw, ah = math.max(1, uw - margin * 2), math.max(1, uh - margin * 2)
        local percent = (p.unit or "percent") == "percent"
        local rw, rh = number(p.width, percent and 60 or 600), number(p.height, percent and 60 or 420)
        if rw <= 0 or rh <= 0 or (percent and (rw > 100 or rh > 100)) then return nil end
        local w = clamp(round(percent and uw * rw / 100 or rw), 1, aw)
        local h = clamp(round(percent and uh * rh / 100 or rh), 1, ah)
        if p.aspectLock then
            local aspect = number(p.aspect, 1.5)
            if aspect <= 0 then return nil end
            h = math.max(1, round(w / aspect))
            if h > ah then h = ah; w = math.max(1, round(h * aspect)) end
        end
        local anchor = p.anchor or "center"
        local x, y = left + math.floor((uw - w) / 2), top + math.floor((uh - h) / 2)
        if anchor:find("left", 1, true) then x = left + margin end
        if anchor:find("right", 1, true) then x = left + uw - w - margin end
        if anchor:find("top", 1, true) then y = top + margin end
        if anchor:find("bottom", 1, true) then y = top + uh - h - margin end
        if anchor == "custom" then
            x = clamp(number(p.x, left), left, left + uw - w)
            y = clamp(number(p.y, top), top, top + uh - h)
        end
        return {monitor = m.name, x = number(m.x, 0) + x, y = number(m.y, 0) + y, width = w, height = h}
    end
    local function snapshot(w)
        return {monitor = w.monitor and w.monitor.name, x = w.at.x, y = w.at.y,
                width = w.size.width, height = w.size.height, pid = w.pid, stable = w.stable_id}
    end
    local function untouched(w, g)
        return w.monitor and w.monitor.name == g.monitor and
            math.abs(w.at.x - g.x) <= 3 and math.abs(w.at.y - g.y) <= 3 and
            math.abs(w.size.width - g.width) <= 3 and math.abs(w.size.height - g.height) <= 3
    end
    local function dispatch(command)
        local result = hl.dispatch(command)
        assert(type(result) == "table" and result.ok == true, "placement dispatcher rejected recovery")
    end
    local function place(w, g)
        -- Absolute dispatcher coordinates are GLOBAL; rule move coordinates are
        -- monitor-local. Never use active-window dispatchers or follow focus.
        local selector = "address:" .. w.address
        dispatch(hl.dsp.window.move({window = selector, monitor = g.monitor, follow = false}))
        dispatch(hl.dsp.window.resize({window = selector, x = g.width, y = g.height, relative = false}))
        dispatch(hl.dsp.window.move({window = selector, x = g.x, y = g.y, relative = false}))
    end
    local first = true
    function state.reconcile()
        if state.stopped then return end
        local monitors, connected = {}, {}
        for _, m in ipairs(hl.get_monitors()) do
            if not m.is_mirror and m.width > 0 and m.height > 0 then
                monitors[#monitors + 1] = m; connected[m.name] = m
            end
        end
        table.sort(monitors, function(a, b) return a.name < b.name end)
        if #monitors == 0 then return end -- Keep recovery records across all-output outages.
        local candidates, seen = {}, {}
        for _, policy in ipairs(policies) do
            for _, w in ipairs(hl.get_windows({tag = policy.tag, mapped = true})) do
                local a = w.address
                if candidates[a] then candidates[a].conflict = true
                else candidates[a] = {window = w, policy = policy} end
                seen[a] = true
            end
        end
        for address, candidate in pairs(candidates) do
            local w, p = candidate.window, candidate.policy.placement
            local entry = state.records[address]
            if entry and (entry.pid ~= w.pid or entry.stable ~= w.stable_id) then entry = nil end
            if not entry then entry = {pid = w.pid, stable = w.stable_id, observed = w.monitor and w.monitor.name} end
            local eligible = not candidate.conflict and w.mapped and not w.hidden and w.floating and
                w.fullscreen == 0 and w.fullscreen_client == 0 and not w.group
            local destination = connected[p.monitor] or connected[p.fallback] or monitors[1]
            local lost = entry.observed and not connected[entry.observed]
            if entry.applied and not untouched(w, entry.applied) and not lost then entry.manual = true end
            local shouldMove = not connected[p.monitor] or lost or
                (entry.applied and not entry.manual) or
                (not first and not state.records[address])
            if eligible and shouldMove and (not entry.manual or lost) then
                local g = geometry(p, destination)
                local old = snapshot(w)
                if g and (not entry.applied or not untouched(w, g)) then
                    local ok = pcall(place, w, g)
                    if ok then entry.applied = g; entry.manual = false; state.last_error = nil
                    else
                        -- Recover to the pre-attempt monitor only if still connected;
                        -- clamp rollback geometry so a failed move cannot strand it.
                        local oldMonitor = connected[old.monitor]
                        if oldMonitor then
                            local rollback = geometry({unit = "pixels", width = old.width, height = old.height,
                                anchor = "custom", margin = 0, x = old.x - oldMonitor.x, y = old.y - oldMonitor.y}, oldMonitor)
                            if rollback then pcall(place, w, rollback) end
                        end
                        state.last_error = "WindowTuner monitor recovery failed; will retry on the next monitor event."
                        print(state.last_error)
                    end
                end
            end
            entry.observed = w.monitor and w.monitor.name
            state.records[address] = entry
        end
        for address in pairs(state.records) do if not seen[address] then state.records[address] = nil end end
        first = false
    end
    local function reconcileNow()
        local ok = pcall(state.reconcile)
        if not ok then
            state.last_error = "WindowTuner monitor recovery could not inspect compositor state."
            print(state.last_error)
        end
    end
    -- Hyprland --verify-config parses configs without an event loop, and creating
    -- a timer there can crash the parser. Create the debounce timer only when a
    -- runtime event arrives, so validation stays safe; fall back to a direct
    -- reconcile if the timer API is unavailable.
    local function debounce()
        if state.timer then return state.timer end
        local ok, timer = pcall(hl.timer, reconcileNow, {timeout = 250, type = "oneshot"})
        if ok and timer then state.timer = timer end
        return state.timer
    end
    local function schedule()
        if state.stopped then return end
        local timer = debounce()
        if timer then
            timer:set_timeout(250)
            timer:set_enabled(true)
        else
            reconcileNow()
        end
    end
    for _, event in ipairs({"monitor.added", "monitor.removed", "monitor.layout_changed", "window.open"}) do
        state.subscriptions[#state.subscriptions + 1] = hl.on(event, schedule)
    end
    function state.stop()
        if state.stopped then return end
        state.stopped = true
        if state.timer then state.timer:set_enabled(false) end
        for _, subscription in ipairs(state.subscriptions) do subscription:remove() end
    end
    -- Record a baseline synchronously so a later disconnect can tell which
    -- monitor each window came from. The first pass never rearranges healthy
    -- windows; without monitors (e.g. verify mode) it just returns.
    reconcileNow()
    return state
end
