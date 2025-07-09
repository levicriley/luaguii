# sine_ui.tcl

namespace eval ::demo {
    variable amp  1.0
    variable freq 1.0
}

proc pre_frame {} { }

proc draw_ui {} {
    igBegin "Sine-Wave Demo"
    slider_float "Amplitude###amp" ::demo::amp  0.0 2.0
    slider_float "Frequency###freq" ::demo::freq 0.1 10.0 0.05
    plot_sine  $::demo::amp $::demo::freq 512
    igEnd
}

proc post_frame {} { }
