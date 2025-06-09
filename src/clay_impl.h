#ifndef CLAY_IMPL_H
#define CLAY_IMPL_H

#include "clay.h"

// Frame management functions
void Clay_BeginFrame(void);
void Clay_EndFrame(void);

// Error handler
void Clay_SetErrorHandler(Clay_ErrorHandler handler);

// Application initialization
void Initialize_ComboCounter(Clay_Arena arena, Clay_Dimensions layoutDimensions, Clay_ErrorHandler errorHandler);

#endif // CLAY_IMPL_H 