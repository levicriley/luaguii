-- sine_ui.lua — Audio (PortAudio) + Rainbow Torus (imgui-knobs)

-- audio state
local amp     = 1.0
local freq    = 440.0
local samples = 512

-- torus state
local yaw, pitch = 0.0, 0.0
local R, r       = 0.75, 0.25
local rainbow    = 0.25  -- speed
local knob_size  = 56.0
local variants   = { "Tick","Dot","Wiper","WiperOnly","WiperDot","Stepped","Space" }
local vindex     = 1
local function vname() return variants[vindex] end

function draw_ui()
  ----------------------------------------------------------------
  -- AUDIO & SINE
  ----------------------------------------------------------------
  demo.SetNextWindowSize(420, 320)
  demo.Begin("Audio & Sine")
    -- frequency knob (50..2000 Hz)
    freq = demo.knob_float_full("Freq (Hz)", freq, 50.0, 2000.0, 1.0, "%.0f", vname(), knob_size)
    demo.audio_set_freq(freq)

    -- amplitude knob (0..5)
    amp  = demo.knob_float_full("Amplitude", amp, 0.0, 5.0, 0.01, "%.2f", vname(), knob_size)

    -- samples (keep as knob too)
    local raw = demo.knob_float_full("Samples", samples, 16, 2048, 16, "%.0f", vname(), knob_size)
    samples = math.floor(raw + 0.5)

    demo.Separator()
    demo.plot_sine(amp, freq, samples)
  demo.End()

  ----------------------------------------------------------------
  -- TORUS
  ----------------------------------------------------------------
  demo.SetNextWindowSize(520, 560)
  demo.Begin("Torus (Knobs + Rainbow)")

    -- Variant chooser row
    if demo.BeginTable("vrow", 3) then
      demo.TableNextColumn(); if demo.Button("◀ Variant") then vindex = (vindex-2)%#variants + 1 end
      demo.TableNextColumn(); demo.Button(vname()) -- inert label-like
      demo.TableNextColumn(); if demo.Button("Variant ▶") then vindex = (vindex)%#variants + 1 end
      demo.EndTable()
    end
    demo.Spacing()

    -- 2x2 grid of shape/angle knobs
    if demo.BeginTable("grid", 2) then
      demo.TableNextColumn()
      yaw   = demo.knob_float_full("Yaw",   yaw,   0.0, 2*math.pi, 0.01, "%.2f", vname(), knob_size)
      demo.TableNextColumn()
      pitch = demo.knob_float_full("Pitch", pitch, 0.0, 2*math.pi, 0.01, "%.2f", vname(), knob_size)

      demo.TableNextColumn()
      R = demo.knob_float_full("Major R", R, 0.2, 1.4, 0.001, "%.3f", vname(), knob_size)
      demo.TableNextColumn()
      r = demo.knob_float_full("Tube r",  r, 0.05, 0.6, 0.001, "%.3f", vname(), knob_size)
      demo.EndTable()
    end

    demo.Separator()
    rainbow = demo.knob_float_full("Rainbow Speed", rainbow, 0.0, 2.0, 0.01, "%.2f", vname(), knob_size)
    demo.gl_torus_rainbow_speed(rainbow)

    -- Draw torus texture auto-fit into remaining area (side = -1)
    demo.gl_torus(-1, yaw, pitch, R, r)

  demo.End()
end
