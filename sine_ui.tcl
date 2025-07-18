###############################################################################
# sine_ui.tcl  –  hot-reloadable Tcl UI for the ImGui demo
###############################################################################

# ---------- helper: ImGui button --------------------------------------------
proc button {label {w 0} {h 0}} {
    igButton $label [list $w $h]
}

# ---------- namespace for persistent state ----------------------------------
namespace eval ::demo {
    variable amp   1.0
    variable freq  1.0
    variable tone  440.0

    variable script [file normalize [info script]]
    variable last   [file mtime $script]
}

# ---------- tiny hook (can stay empty) --------------------------------------
proc pre_frame {} {}

# ---------- main UI ---------------------------------------------------------
proc draw_ui {} {
    # ---------- Window 1 ----------
    igBegin "Sine‑Wave"
    try {
        slider_float "Amp###amp"  ::demo::amp  0 2
        slider_float "Freq###freq" ::demo::freq 0.1 10 0.05
        plot_sine  $::demo::amp $::demo::freq
    } finally { igEnd }

    # ---------- Window 2 ----------
    igBegin "Audio + MIDI Tools"
    try {
        slider_float "Tone (Hz)###tone" ::demo::tone 50 1000 1
        audio_set_freq $::demo::tone

        if {[igButton "Click me" NULL]} { puts "clicked" }
    } finally { igEnd }
}


# ---------- hot-reload ------------------------------------------------------
proc post_frame {} {
    variable ::demo::script
    variable ::demo::last

    set now [file mtime $script]
    if {$now > $last} {
        puts "🔄  Reloading $script"
        set last $now
        if {[catch { uplevel #0 [list source $script] } emsg]} {
            puts "⚠️  Reload failed: $emsg"
        }
    }
}
