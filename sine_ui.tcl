##############################################################################
# sine_ui.tcl  –  hot‑reloadable ImGui UI (robust “safe_window” version)
##############################################################################

# ---------------------------------------------------------------------------
# 1.  State variables
# ---------------------------------------------------------------------------
namespace eval ::demo {
    variable amp   1.0
    variable freq  1.0
    variable tone  440.0

    variable script [file normalize [info script]]
    variable last   [file mtime $script]
}

# ---------------------------------------------------------------------------
# 2.  Safety wrapper: guarantees igEnd() even on error
# ---------------------------------------------------------------------------
proc safe_window {title bodyScript} {
    set opened 0
    if {[catch {
        igBegin $title
        set opened 1
        uplevel 1 $bodyScript       ;# run caller‑supplied UI code
    } msg opts]} {
        puts stderr "⚠️  $title error: $msg"
    }
    if {$opened} { igEnd }
}

# ---------------------------------------------------------------------------
# 3.  Per‑frame callbacks called from C++
# ---------------------------------------------------------------------------
proc pre_frame  {} {}
proc post_frame {} {
    variable ::demo::script
    variable ::demo::last
    if {[file mtime $script] > $last && [file size $script] > 0} {
        set ::demo::last [file mtime $script]
        if {[catch { uplevel #0 [list source $script] } e]} {
            puts stderr "⚠️  Reload failed: $e"
        } else {
            puts "🔄  Reloaded $script"
        }
    }
}

# ---------------------------------------------------------------------------
# 4.  Main UI
# ---------------------------------------------------------------------------
proc draw_ui {} {
    # ---------- Sine‑wave plot window --------------------------------------
    safe_window "Sine‑Wave" {
        slider_float "Amplitude###amp" ::demo::amp  0 2
        slider_float "Frequency###freq" ::demo::freq 0.1 10 0.05
        plot_sine   $::demo::amp        $::demo::freq
    }

    # ---------- Audio / MIDI tools -----------------------------------------
    safe_window "Audio + MIDI Tools" {
        slider_float "Tone (Hz)###tone" ::demo::tone 50 1000 1
        audio_set_freq $::demo::tone

        if {[imgui_button "Click me"]} { puts "clicked!" }

        igSeparator
        igText "Available MIDI inputs:"
        set ports [midi_ports]
        if {[llength $ports]==0} {
            igTextDisabled "(none)"
        } else {
            foreach p $ports { igBulletText $p }
        }
    }

    # ---------- Core‑OpenGL rendered into an ImGui window ------------------
    safe_window "Raw‑GL inside ImGui" {
        gl_triangle 300                ;# renders a 300×300 textured quad
    }
}
