// MOD (not in the original UnleashX): a demoscene-style sine scroller with
// copper bars under the main menu. Shows what owning the source allows:
// skins can place text, but not animate it per character.
#pragma once

#include "../render/Fonts.h"

// Draws the bars and the scrolling text for the time t (seconds).
void RenderDemoScroller(CXBFont *font, float t);
