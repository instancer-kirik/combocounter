#ifndef CLAY_TYPES_H
#define CLAY_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "clay.h"

// Only declare functions we need
Clay_ElementId Clay__GetClickedElement(Clay_RenderCommandArray commands, Clay_PointerData pointer);
void Clay_RenderCommandArray_RenderRaylib(Clay_RenderCommandArray commands);
void Clay__ResetArena(void);

#endif // CLAY_TYPES_H 