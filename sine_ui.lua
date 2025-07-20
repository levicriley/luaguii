-- sine_ui.lua

-- initial parameters (you can tweak these)
local amp     = 1.0
local freq    = 2.0
local samples = 512

-- called before ImGui frames (optional)
function pre_frame()
  -- no-op for now
end

-- your Lua‑driven UI
function draw_ui()
  -- open a new window called "Lua Sine Plot"
  demo.Begin("Lua Sine Plot")
    -- sliders (demo.slider_float = our C++ wrapper)
    amp     = demo.slider_float("Amplitude", amp,     0.0,  5.0, 0.01)
    freq    = demo.slider_float("Frequency", freq,    0.1, 20.0, 0.10)
    samples = math.tointeger(demo.slider_float("Samples", samples, 16, 1024, 16))

    -- plot the sine wave
    demo.plot_sine(amp, freq, samples)
  demo.End()
end

-- called after ImGui frames (optional)
function post_frame()
  -- no-op for now
end
