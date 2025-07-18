##############################################################################
# sine_ui.tcl – ImGui GUI (auto‑reloads when you hit “Save”)              ####
##############################################################################

# ---------- state -----------------------------------------------------------
namespace eval ::demo {
    variable amp   1.0
    variable freq  1.0
    variable tone  440.0            ;# audible test tone (Hz)
    variable rotDeg 0.0             ;# rotation in degrees   <-- slider drives this
    variable script    [file normalize [info script]]
    variable last_mtime [file mtime $script]
}

# ---------- helper: safe ImGui window ---------------------------------------
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

# ---------- per‑frame hooks --------------------------------------------------
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
            puts "🔄  reloaded $script"
        }
    }
}

# ---------- main UI ----------------------------------------------------------
proc draw_ui {} {
    # --- control window ------------------------------------------------------
    win "Controls" {
        slider_float "Cube angle (deg)###rot" ::demo::rotDeg  0 360 1
        set radians [expr {$::demo::rotDeg * acos(-1) / 180.0}]

        slider_float "Tone (Hz)###tone" ::demo::tone  50 1000 1
        audio_set_freq $::demo::tone

        # (send rotation value to the C command right away)
        set ::demo::current_rot_rad $radians
    }

    # --- visualisation window -----------------------------------------------
    win "Cube + Sine" {
        set side 300
        gl_cube   $side  $::demo::current_rot_rad   ;# draw cube texture
        igSeparator
        plot_sine 1 50 ::demo::tone                           ;# arbitrary demo sine
    }
}
