##############################################################################
# sine_ui.tcl  – demo UI (auto‑reloads on save)                              #
##############################################################################

namespace eval ::demo {
    variable amp     1.0
    variable freq    1.0
    variable tone    440.0          ;# audible test tone
    variable rotDeg  0.0            ;# rotation (°)
    variable script  [file normalize [info script]]
    variable last_mtime [file mtime $script]
}

# Helper: safe ImGui window --------------------------------------------------
proc win {title bodyScript} {
    igBegin $title
    set ok 0
    try {
        set ok 1
        uplevel 1 $bodyScript
    } finally {
        if {$ok} { igEnd }
    }
}

# Per‑frame hooks ------------------------------------------------------------
proc pre_frame  {} {}
proc post_frame {} {
    variable ::demo::script
    variable ::demo::last_mtime
    set m [file mtime $script]
    if {$m > $last_mtime} {
        set last_mtime $m
        if {[catch { uplevel #0 [list source $script] } err]} {
            puts "⚠ reload error: $err"
        } else {
            puts "🔄  reload ok"
        }
    }
}

# Main UI --------------------------------------------------------------------
proc draw_ui {} {
    win "Controls" {
        slider_float "Cube angle (deg)###rot" ::demo::rotDeg 0 360 1
        set radians [expr {$::demo::rotDeg * acos(-1) / 180.0}]

        slider_float "Tone (Hz)###tone" ::demo::tone 50 1000 1
        audio_set_freq $::demo::tone

        # expose current rotation (radians) for C side
        set ::demo::current_rot_rad $radians
    }

    win "Cube + Sine" {
        set side 300
        gl_cube $side $::demo::current_rot_rad
        igSeparator
        plot_sine $::demo::amp  $::demo::tone 5120;# ← plot follows audio
    }
}
