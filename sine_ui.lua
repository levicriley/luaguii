-- sine_ui.lua — dark theme; compact Torus window; centered FBO image

-- audio/sine state
local amp  = 1.0
local freq = 440.0

-- torus state
local yaw, pitch, R, r = 0.0, 0.0, 0.75, 0.25

function draw_ui()
  -- ─── AUDIO & SINE ───────────────────────────────────────────
  demo.Begin("Audio & Sine")
    if demo.BeginTable("audio_grid", 2) then
      demo.TableNextColumn()
      freq = demo.knob_float("Freq (Hz)", freq, 50.0, 2000.0, 1.0, 52.0)
      demo.TableNextColumn()
      amp  = demo.knob_float("Amplitude", amp, 0.0, 5.0, 0.01, 48.0)
      demo.EndTable()
    end
    demo.Spacing()
    demo.plot_sine(amp, freq, 512)
    demo.audio_set_freq(freq)
  demo.End()

  -- ─── TORUS (KNOBS + IMAGE) ─────────────────────────────────
  -- small, fixed first size so the window doesn't explode
  demo.SetNextWindowSize(380, 440) -- ImGuiCond_FirstUseEver by default in binding
  demo.Begin("Torus (Knobs)")
    if demo.BeginTable("torus_grid", 2) then
      demo.TableNextColumn(); yaw   = demo.knob_float("Yaw",   yaw,   0.0, 2*math.pi, 0.01, 56.0)
      demo.TableNextColumn(); pitch = demo.knob_float("Pitch", pitch, 0.0, 2*math.pi, 0.01, 56.0)
      demo.TableNextColumn(); R     = demo.knob_float("Major R", R, 0.2, 1.4, 0.001, 48.0)
      demo.TableNextColumn(); r     = demo.knob_float("Tube r",  r, 0.05, 0.6, 0.001, 48.0)
      demo.EndTable()
    end
    demo.Separator()

    -- auto-fit image to remaining region (C++ clamps to 64..512)
    demo.gl_torus(-1, yaw, pitch, R, r)
  demo.End()
end
