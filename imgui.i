%module imgui_tcl
%include "typemaps.i"
%{
#include "cimgui.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
%}

/*── Ignore all the C‐API varargs helpers (the …V functions) ──*/
%ignore igTextV;
%ignore igTextDisabledV;
%ignore igTextColoredV;
%ignore igTextWrappedV;
%ignore igLabelTextV;
%ignore igBulletTextV;
%ignore igTreeNodeV_Str;
%ignore igTreeNodeV_Ptr;
%ignore igTreeNodeExV_Str;
%ignore igTreeNodeExV_Ptr;
%ignore igImFormatStringV;
%ignore igImFormatStringToTempBufferV;
%ignore igLogTextV;
%ignore igDebugLogV;
%ignore ImGuiTextBuffer_appendfv;
%ignore igSetTooltipV;
%ignore igSetItemTooltipV;
%ignore igTextAlignedV;

/* Now parse the rest of the C‐API: */
%include "cimgui.h"
