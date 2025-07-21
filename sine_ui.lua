-- sine_ui.lua

-- path to this script (must match how it's loaded in your C++ main)
local script = "sine_ui.lua"

-- fetch initial modification time
local function get_mtime()
  local p = io.popen('stat -c "%Y" "' .. script .. '" 2>/dev/null')
  if not p then return 0 end
  local t = tonumber(p:read("*a")) or 0
  p:close()
  return t
end

local last_mtime = get_mtime()

--------------------------------------------------------------------------------
-- Demo state
--------------------------------------------------------------------------------

-- state for sine plot
local amp     = 1.0
local freq    = 2.0
local samples = 512

-- state for cube
local angle   = 0.0

--------------------------------------------------------------------------------
-- Hooks
--------------------------------------------------------------------------------

function pre_frame()
  -- nothing here (yet)
end

function draw_ui()
  -- ─── SINE WINDOW ────────────────────────────────────────────
  demo.Begin("Lua Sine Plot")
    amp     = demo.slider_float("Amplitude", amp,     0.0,  5.0)
    freq    = demo.slider_float("Frequency", freq,    0.1, 20.0)
    local raw = demo.slider_float("Samples", samples, 16, 1024)
    samples   = math.floor(raw + 0.5)
    demo.plot_sine(amp, freq, samples)
  demo.End()

  -- ─── CUBE WINDOW ────────────────────────────────────────────
  demo.Begin("Lua Cube")
    -- auto‑increment rotation at 1 rev/sec @~60FPS
    angle = (angle + (2*math.pi)/60) % (2*math.pi)
    demo.gl_cube(128, angle)
  demo.End()
end

function post_frame()
  -- auto‑reload when the file changes on disk
  local mtime = get_mtime()
  if mtime > last_mtime then
    last_mtime = mtime
    local ok, err = pcall(dofile, script)
    if ok then
      print("🔄 Lua reload ok")
    else
      print("⚠ Lua reload error:", err)
    end
  end
end
