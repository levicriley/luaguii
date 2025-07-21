-- sine_ui.lua

-- state for sine plot
local amp     = 1.0
local freq    = 2.0
local samples = 512

-- state for cube
local angle   = 0.0

function pre_frame()
  -- (no‑op)
end

function draw_ui()
  -- ─── SINE WINDOW ────────────────────────────────────────────
  demo.Begin("Lua Sine Plot")
    -- amplitude/frequency sliders
    amp     = demo.slider_float("Amplitude", amp,     0.0,  5.0)
    freq    = demo.slider_float("Frequency", freq,    0.1, 20.0)
    -- integer samples slider
    local raw = demo.slider_float("Samples", samples, 16, 1024)
    samples   = math.floor(raw + 0.5)
    -- plot it
    demo.plot_sine(amp, freq, samples)
  demo.End()

  -- ─── CUBE WINDOW ────────────────────────────────────────────
  demo.Begin("Lua Cube")
    -- bump angle by a fixed amount each frame
    angle = angle + (2*math.pi) * (1/60)  -- 1 rev/sec @~60fps
    if angle >= 2*math.pi then angle = angle - 2*math.pi end

    -- draw the cube at 128×128 px, rotated by ‘angle’ radians
    demo.gl_cube(128, angle)
  demo.End()
end

function post_frame()
  -- (no‑op)
end
