# sine_ui.tcl — Tcl logic for the sine-wave ImGui demo
#
# Edit this file while the program is running and hit “Save”;
# post_frame will auto-reload it when the mtime changes.

namespace eval ::demo {
    variable amp   1.0
    variable freq  1.0
    variable scriptfile  [file normalize [info script]]
    variable last_reload [file mtime $scriptfile]
}

proc pre_frame {} {}

proc draw_ui {} {
    igBegin "Sine-Wave Demo"         ;# (one-arg wrapper)

    slider_float "Amplitude###amp"  ::demo::amp  0.0 2.0
    slider_float "Frequency###freq" ::demo::freq 0.1 10.0 0.05

    plot_sine  $::demo::amp $::demo::freq 512
    igEnd
}

proc post_frame {} {
    variable ::demo::scriptfile
    variable ::demo::last_reload
    set mtime [file mtime $scriptfile]
    if {$mtime > $last_reload} {
        puts "🔄  Reloading $scriptfile …"
        set ::demo::last_reload $mtime
        uplevel #0 [list source $scriptfile]
    }
}
