-- gui.lua  – edit & save ➜ auto‑reloads
local ig   = require "ig"     -- Sol‑3 ImGui table
local demo = require "demo"   -- helpers we exported

local rot = 0.0   -- cube angle in degrees
local hz  = 440   -- test‑tone frequency

function draw_ui()
  ig.Begin("Controls")
    local changed,val = ig.SliderFloat("Cube angle (°)", rot, 0, 360)
    if changed then rot = val end

    local ch,freq = ig.SliderFloat("Tone (Hz)", hz, 50, 1000)
    if ch then hz=freq; demo.audio_set_freq(hz) end
  ig.End()

  ig.Begin("Cube + Plot")
    demo.gl_cube(300, math.rad(rot))
    ig.Separator()
    demo.plot_sine(1,5,256)
  ig.End()
end
