/* -*- mode: c; tab-width: 4; c-basic-offset: 3; c-file-style: "linux" -*- */
//
// Copyright (c) 2009, Wei Mingzhi <whistler_wmz@users.sf.net>.
// All rights reserved.
//
// This file is part of SDLPAL.
//
// SDLPAL is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "main.h"

// Screen buffer
SDL_Surface              *gpScreen           = NULL;

// Backup screen buffer
SDL_Surface              *gpScreenBak        = NULL;

#if SDL_VERSION_ATLEAST(2,0,0)
SDL_Window               *gpWindow           = NULL;
static SDL_Renderer      *gpRenderer         = NULL;
static SDL_Texture       *gpTexture          = NULL;
# if PAL_HAS_TOUCH
static SDL_Texture       *gpTouchOverlay     = NULL;
# endif
static SDL_Rect          *gpRenderRect       = NULL;
static SDL_Rect           gRenderRect;
#endif

// The real screen surface
static SDL_Surface       *gpScreenReal       = NULL;

volatile BOOL g_bRenderPaused = FALSE;

#if (defined (__SYMBIAN32__) && !defined (__S60_5X__)) || defined (PSP) || defined (GEKKO)
   static BOOL bScaleScreen = FALSE;
#else
   static BOOL bScaleScreen = TRUE;
#endif

// Shake times and level
static WORD               g_wShakeTime       = 0;
static WORD               g_wShakeLevel      = 0;

#ifdef __CARDBOARD__

#define CARDBOARD_RATIO 3.0/4
//define padding/gap for cardboard's view; only center can be view most clear. Tweak it if you're using Oculus or other VR solution
#define CARDBOARD_PADDING 50
#define CARDBOARD_GAP 25
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
#define SDL_SoftStretch SDL_UpperBlit
#endif

VOID commitTexture()
{
   short           cbSingleEyeWidth,cbSingleEyeHeight;
   SDL_Rect        dstrect;
#ifdef __CARDBOARD__
   cbSingleEyeWidth     = (gConfig.dwScreenWidth / 2 - CARDBOARD_PADDING - CARDBOARD_GAP);
   cbSingleEyeHeight    = cbSingleEyeWidth * CARDBOARD_RATIO;
   dstrect.x=CARDBOARD_PADDING;
   dstrect.y=(gConfig.dwScreenHeight-cbSingleEyeHeight)/2;
   dstrect.w=cbSingleEyeWidth;
   dstrect.h=cbSingleEyeHeight;
   SDL_RenderCopy(gpRenderer, gpTexture, NULL, &dstrect);
   dstrect.x=gConfig.dwScreenWidth / 2 + CARDBOARD_GAP;
   SDL_RenderCopy(gpRenderer, gpTexture, NULL, &dstrect);
#else
   SDL_RenderCopy(gpRenderer, gpTexture, NULL, gpRenderRect);
#endif
}

INT
VIDEO_Startup(
   VOID
)
/*++
  Purpose:

    Initialze the video subsystem.

  Parameters:

    None.

  Return value:

    0 = success, -1 = fail to create the screen surface,
    -2 = fail to create screen buffer.

--*/
{
#if SDL_VERSION_ATLEAST(2,0,0)
# if PAL_HAS_TOUCH
   SDL_Surface *overlay;
# endif
   //
   // Before we can render anything, we need a window and a renderer.
   //
   gpWindow = SDL_CreateWindow("Pal", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	   gConfig.dwScreenWidth, gConfig.dwScreenHeight, PAL_VIDEO_INIT_FLAGS);

   if (gpWindow == NULL)
   {
      return -1;
   }

   gpRenderer = SDL_CreateRenderer(gpWindow, -1, SDL_RENDERER_ACCELERATED);

   if (gpRenderer == NULL)
   {
      return -1;
   }

#if defined (__IOS__)
   SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
   SDL_GL_SetAttribute(SDL_GL_RETAINED_BACKING, 1);
#endif

   //
   // Create the screen buffer and the backup screen buffer.
   //
   gpScreen = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 200, 8, 0, 0, 0, 0);
   gpScreenBak = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 200, 8, 0, 0, 0, 0);
   gpScreenReal = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 200, 32,
                                       0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

   //
   // Create texture for screen.
   //
   gpTexture = SDL_CreateTexture(gpRenderer, SDL_PIXELFORMAT_ARGB8888,
							  SDL_TEXTUREACCESS_STREAMING, 320, 200);

   //
   // Failed?
   //
   if (gpScreen == NULL || gpScreenBak == NULL || gpScreenReal == NULL || gpTexture == NULL)
   {
      if (gpScreen != NULL)
      {
         SDL_FreeSurface(gpScreen);
         gpScreen = NULL;
      }

      if (gpScreenBak != NULL)
      {
         SDL_FreeSurface(gpScreenBak);
         gpScreenBak = NULL;
      }

      if (gpScreenReal != NULL)
      {
         SDL_FreeSurface(gpScreenReal);
         gpScreenReal = NULL;
      }

	  if (gpTexture != NULL)
	  {
		 SDL_DestroyTexture(gpTexture);
		 gpTexture = NULL;
	  }

      SDL_DestroyRenderer(gpRenderer);
      gpRenderer = NULL;

      SDL_DestroyWindow(gpWindow);
      gpWindow = NULL;

      return -2;
   }

   //
   // Create texture for overlay.
   //
#if PAL_HAS_TOUCH
#ifndef __CARDBOARD__
   overlay = SDL_LoadBMP(va("%s%s", PAL_PREFIX, "overlay.bmp"));
   if (overlay != NULL)
   {
      SDL_SetColorKey(overlay, SDL_RLEACCEL, SDL_MapRGB(overlay->format, 255, 0, 255));
      gpTouchOverlay = SDL_CreateTextureFromSurface(gpRenderer, overlay);
      SDL_SetTextureAlphaMod(gpTouchOverlay, 120);
      SDL_FreeSurface(overlay);
   }
#endif
#endif

   //
   // Check whether to keep the aspect ratio
   //
   gpRenderRect = NULL;
   if (gConfig.fKeepAspectRatio)
   {
	   float ax = (float)gConfig.dwScreenWidth / 320.0f;
	   float ay = (float)gConfig.dwScreenHeight / 200.0f;
	   if (ax != ay)
	   {
		   float ratio = (ax > ay) ? ay : ax;
		   WORD w = (WORD)(ratio * 320.0f);
		   WORD h = (WORD)(ratio * 200.0f);
		   if (w % 4 != 0) w += 4 - (w % 4);
		   if (h % 4 != 0) h += 4 - (h % 4);
		   gRenderRect.x = (gConfig.dwScreenWidth - w) / 2;
		   gRenderRect.y = (gConfig.dwScreenHeight - h) / 2;
		   gRenderRect.w = w;
		   gRenderRect.h = h;
		   gpRenderRect = &gRenderRect;
# if PAL_HAS_TOUCH
		   PAL_SetTouchBounds(gConfig.dwScreenWidth, gConfig.dwScreenHeight, gRenderRect);
# endif
	   }
   }
#else

   //
   // Create the screen surface.
   //
   gpScreenReal = SDL_SetVideoMode(gConfig.dwScreenWidth, gConfig.dwScreenHeight, 8, PAL_VIDEO_INIT_FLAGS);

   if (gpScreenReal == NULL)
   {
      //
      // Fall back to 640x480 software mode.
      //
      gpScreenReal = SDL_SetVideoMode(640, 480, 8,
         SDL_SWSURFACE | (gConfig.fFullScreen ? SDL_FULLSCREEN : 0));
   }

   //
   // Still fail?
   //
   if (gpScreenReal == NULL)
   {
      return -1;
   }

   //
   // Create the screen buffer and the backup screen buffer.
   //
   gpScreen = SDL_CreateRGBSurface(gpScreenReal->flags & ~SDL_HWSURFACE, 320, 200, 8,
      gpScreenReal->format->Rmask, gpScreenReal->format->Gmask,
      gpScreenReal->format->Bmask, gpScreenReal->format->Amask);

   gpScreenBak = SDL_CreateRGBSurface(gpScreenReal->flags & ~SDL_HWSURFACE, 320, 200, 8,
      gpScreenReal->format->Rmask, gpScreenReal->format->Gmask,
      gpScreenReal->format->Bmask, gpScreenReal->format->Amask);

   //
   // Failed?
   //
   if (gpScreen == NULL || gpScreenBak == NULL)
   {
      if (gpScreen != NULL)
      {
         SDL_FreeSurface(gpScreen);
		 gpScreen = NULL;
      }

      if (gpScreenBak != NULL)
      {
         SDL_FreeSurface(gpScreenBak);
		 gpScreenBak = NULL;
      }

      SDL_FreeSurface(gpScreenReal);
	  gpScreenReal = NULL;

      return -2;
   }

   if (gConfig.fFullScreen)
   {
      SDL_ShowCursor(FALSE);
   }

#endif

   return 0;
}

VOID
VIDEO_Shutdown(
   VOID
)
/*++
  Purpose:

    Shutdown the video subsystem.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   if (gpScreen != NULL)
   {
      SDL_FreeSurface(gpScreen);
   }
   gpScreen = NULL;

   if (gpScreenBak != NULL)
   {
      SDL_FreeSurface(gpScreenBak);
   }
   gpScreenBak = NULL;

#if SDL_VERSION_ATLEAST(2,0,0)

# if PAL_HAS_TOUCH
   if (gpTouchOverlay)
   {
      SDL_DestroyTexture(gpTouchOverlay);
   }
   gpTouchOverlay = NULL;
# endif

   if (gpTexture)
   {
	  SDL_DestroyTexture(gpTexture);
   }
   gpTexture = NULL;

   if (gpRenderer)
   {
      SDL_DestroyRenderer(gpRenderer);
   }
   gpRenderer = NULL;

   if (gpWindow)
   {
      SDL_DestroyWindow(gpWindow);
   }
   gpWindow = NULL;

#endif

   if (gpScreenReal != NULL)
   {
      SDL_FreeSurface(gpScreenReal);
   }
   gpScreenReal = NULL;
}

#if SDL_VERSION_ATLEAST(2,0,0)
PAL_FORCE_INLINE
VOID
VIDEO_RenderCopy(
   VOID
)
{
   SDL_UpdateTexture(gpTexture, NULL, gpScreenReal->pixels, gpScreenReal->pitch);
   commitTexture();
#if PAL_HAS_TOUCH
   if (gpTouchOverlay)
   {
      SDL_RenderCopy(gpRenderer, gpTouchOverlay, NULL, gpRenderRect);
   }
#endif
   SDL_RenderPresent(gpRenderer);
}
#endif

VOID
VIDEO_UpdateScreen(
   const SDL_Rect  *lpRect
)
/*++
  Purpose:

    Update the screen area specified by lpRect.

  Parameters:

    [IN]  lpRect - Screen area to update.

  Return value:

    None.

--*/
{
   SDL_Rect        srcrect, dstrect;
   short           offset = 240 - 200;
   short           screenRealHeight = gpScreenReal->h;
   short           screenRealY = 0;

#if SDL_VERSION_ATLEAST(2,0,0)
   if (g_bRenderPaused)
   {
	   return;
   }
   SDL_RenderClear(gpRenderer); //must clear for erase flicking
#endif

   //
   // Lock surface if needed
   //
   if (SDL_MUSTLOCK(gpScreenReal))
   {
      if (SDL_LockSurface(gpScreenReal) < 0)
         return;
   }

   if (!bScaleScreen)
   {
      screenRealHeight -= offset;
      screenRealY = offset / 2;
   }

   if (lpRect != NULL)
   {
      dstrect.x = (SHORT)((INT)(lpRect->x) * gpScreenReal->w / gpScreen->w);
      dstrect.y = (SHORT)((INT)(screenRealY + lpRect->y) * screenRealHeight / gpScreen->h);
      dstrect.w = (WORD)((DWORD)(lpRect->w) * gpScreenReal->w / gpScreen->w);
      dstrect.h = (WORD)((DWORD)(lpRect->h) * screenRealHeight / gpScreen->h);

      SDL_SoftStretch(gpScreen, (SDL_Rect *)lpRect, gpScreenReal, &dstrect);

      if (SDL_MUSTLOCK(gpScreenReal))
      {
         SDL_UnlockSurface(gpScreenReal);
      }
   }
   else if (g_wShakeTime != 0)
   {
      //
      // Shake the screen
      //
      srcrect.x = 0;
      srcrect.y = 0;
      srcrect.w = 320;
      srcrect.h = 200 - g_wShakeLevel;

      dstrect.x = 0;
      dstrect.y = screenRealY;
      dstrect.w = 320 * gpScreenReal->w / gpScreen->w;
      dstrect.h = (200 - g_wShakeLevel) * screenRealHeight / gpScreen->h;

      if (g_wShakeTime & 1)
      {
         srcrect.y = g_wShakeLevel;
      }
      else
      {
         dstrect.y = (screenRealY + g_wShakeLevel) * screenRealHeight / gpScreen->h;
      }

      SDL_SoftStretch(gpScreen, &srcrect, gpScreenReal, &dstrect);

      if (g_wShakeTime & 1)
      {
         dstrect.y = (screenRealY + screenRealHeight - g_wShakeLevel) * screenRealHeight / gpScreen->h;
      }
      else
      {
         dstrect.y = screenRealY;
      }

      dstrect.h = g_wShakeLevel * screenRealHeight / gpScreen->h;

      SDL_FillRect(gpScreenReal, &dstrect, 0);

      if (SDL_MUSTLOCK(gpScreenReal))
      {
         SDL_UnlockSurface(gpScreenReal);
      }

#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION <= 2
      dstrect.x = dstrect.y = 0;
      dstrect.w = gpScreenReal->w;
      dstrect.h = gpScreenReal->h;
#endif
      g_wShakeTime--;
   }
   else
   {
      dstrect.x = 0;
      dstrect.y = screenRealY;
      dstrect.w = gpScreenReal->w;
      dstrect.h = screenRealHeight;

      SDL_SoftStretch(gpScreen, NULL, gpScreenReal, &dstrect);

      if (SDL_MUSTLOCK(gpScreenReal))
      {
         SDL_UnlockSurface(gpScreenReal);
      }

#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION <= 2
      dstrect.x = dstrect.y = 0;
      dstrect.w = gpScreenReal->w;
      dstrect.h = gpScreenReal->h;
#endif
   }

#if SDL_VERSION_ATLEAST(2,0,0)
   VIDEO_RenderCopy();
#else
   SDL_UpdateRect(gpScreenReal, dstrect.x, dstrect.y, dstrect.w, dstrect.h);
#endif
}

VOID
VIDEO_SetPalette(
   SDL_Color        rgPalette[256]
)
/*++
  Purpose:

    Set the palette of the screen.

  Parameters:

    [IN]  rgPalette - array of 256 colors.

  Return value:

    None.

--*/
{
#if SDL_VERSION_ATLEAST(2,0,0)
   SDL_Palette   *palette = SDL_AllocPalette(256);

   if (palette == NULL)
   {
      return;
   }

   SDL_SetPaletteColors(palette, rgPalette, 0, 256);

   SDL_SetSurfacePalette(gpScreen, palette);
   SDL_SetSurfacePalette(gpScreenBak, palette);

   //
   // HACKHACK: need to invalidate gpScreen->map otherwise the palette
   // would not be effective during blit
   //
   SDL_SetSurfaceColorMod(gpScreen, 0, 0, 0);
   SDL_SetSurfaceColorMod(gpScreen, 0xFF, 0xFF, 0xFF);
   SDL_SetSurfaceColorMod(gpScreenBak, 0, 0, 0);
   SDL_SetSurfaceColorMod(gpScreenBak, 0xFF, 0xFF, 0xFF);

   VIDEO_UpdateScreen(NULL);

   // The palette should be freed, or memory leak occurs.
   SDL_FreePalette(palette);
#else
   SDL_SetPalette(gpScreen, SDL_LOGPAL | SDL_PHYSPAL, rgPalette, 0, 256);
   SDL_SetPalette(gpScreenBak, SDL_LOGPAL | SDL_PHYSPAL, rgPalette, 0, 256);
   SDL_SetPalette(gpScreenReal, SDL_LOGPAL | SDL_PHYSPAL, rgPalette, 0, 256);
#if (defined (__SYMBIAN32__))
   {
      static UINT32 time = 0;
      if (SDL_GetTicks() - time > 50)
      {
	      SDL_UpdateRect(gpScreenReal, 0, 0, gpScreenReal->w, gpScreenReal->h);
	      time = SDL_GetTicks();
      }
   }
#endif
#endif
}

VOID
VIDEO_Resize(
   INT             w,
   INT             h
)
/*++
  Purpose:

    This function is called when user resized the window.

  Parameters:

    [IN]  w - width of the window after resizing.

    [IN]  h - height of the window after resizing.

  Return value:

    None.

--*/
{
#if SDL_VERSION_ATLEAST(2,0,0)
   // TODO
#else
   DWORD                    flags;
   PAL_LARGE SDL_Color      palette[256];
   int                      i;

   //
   // Get the original palette.
   //
   for (i = 0; i < gpScreenReal->format->palette->ncolors; i++)
   {
      palette[i] = gpScreenReal->format->palette->colors[i];
   }

   //
   // Create the screen surface.
   //
   flags = gpScreenReal->flags;

   SDL_FreeSurface(gpScreenReal);
   gpScreenReal = SDL_SetVideoMode(w, h, 8, flags);

   if (gpScreenReal == NULL)
   {
#ifdef __SYMBIAN32__
#ifdef __S60_5X__
      gpScreenReal = SDL_SetVideoMode(640, 360, 8, SDL_SWSURFACE);
#else
      gpScreenReal = SDL_SetVideoMode(320, 240, 8, SDL_SWSURFACE);
#endif
#else
      //
      // Fall back to 640x480 software windowed mode.
      //
      gpScreenReal = SDL_SetVideoMode(640, 480, 8, SDL_SWSURFACE);
#endif
   }

   SDL_SetPalette(gpScreenReal, SDL_PHYSPAL | SDL_LOGPAL, palette, 0, i);
   VIDEO_UpdateScreen(NULL);
#endif
}

SDL_Color *
VIDEO_GetPalette(
   VOID
)
/*++
  Purpose:

    Get the current palette of the screen.

  Parameters:

    None.

  Return value:

    Pointer to the current palette.

--*/
{
#if SDL_VERSION_ATLEAST(2,0,0)
   return gpScreen->format->palette->colors;
#else
   return gpScreenReal->format->palette->colors;
#endif
}

VOID
VIDEO_ToggleScaleScreen(
   VOID
)
/*++
  Purpose:

    Toggle scalescreen mode.

  Parameters:

    None.

  Return value:

    None.

--*/
{
#ifdef __SYMBIAN32__
   bScaleScreen = !bScaleScreen;
   VIDEO_Resize(320, 240);
   VIDEO_UpdateScreen(NULL);
#endif
}

VOID
VIDEO_ToggleFullscreen(
   VOID
)
/*++
  Purpose:

    Toggle fullscreen mode.

  Parameters:

    None.

  Return value:

    None.

--*/
{
#if SDL_VERSION_ATLEAST(2,0,0)
   // TODO
#else
   DWORD                    flags;
   PAL_LARGE SDL_Color      palette[256];
   int                      i;

   //
   // Get the original palette.
   //
   for (i = 0; i < gpScreenReal->format->palette->ncolors; i++)
   {
      palette[i] = gpScreenReal->format->palette->colors[i];
   }

   //
   // Get the flags of the original screen surface
   //
   flags = gpScreenReal->flags;

   if (flags & SDL_FULLSCREEN)
   {
      //
      // Already in fullscreen mode. Remove the fullscreen flag.
      //
      flags &= ~SDL_FULLSCREEN;
      flags |= SDL_RESIZABLE;
      SDL_ShowCursor(TRUE);
   }
   else
   {
      //
      // Not in fullscreen mode. Set the fullscreen flag.
      //
      flags |= SDL_FULLSCREEN;
      SDL_ShowCursor(FALSE);
   }

   //
   // Free the original screen surface
   //
   SDL_FreeSurface(gpScreenReal);

   //
   // ... and create a new one
   //
   if (gConfig.dwScreenWidth == 640 && gConfig.dwScreenHeight == 400 && (flags & SDL_FULLSCREEN))
   {
      gpScreenReal = SDL_SetVideoMode(640, 480, 8, flags);
   }
   else if (gConfig.dwScreenWidth == 640 && gConfig.dwScreenHeight == 480 && !(flags & SDL_FULLSCREEN))
   {
      gpScreenReal = SDL_SetVideoMode(640, 400, 8, flags);
   }
   else
   {
      gpScreenReal = SDL_SetVideoMode(gConfig.dwScreenWidth, gConfig.dwScreenHeight, 8, flags);
   }

   VIDEO_SetPalette(palette);

   //
   // Update the screen
   //
   VIDEO_UpdateScreen(NULL);
#endif
}

VOID
VIDEO_SaveScreenshot(
   VOID
)
/*++
  Purpose:

    Save the screenshot of current screen to a BMP file.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   int      iNumBMP = 0;
   FILE    *fp;

   //
   // Find a usable BMP filename.
   //
   for (iNumBMP = 0; iNumBMP <= 9999; iNumBMP++)
   {
      fp = fopen(va("%sscrn%.4d.bmp", PAL_PREFIX, iNumBMP), "rb");
      if (fp == NULL)
      {
         break;
      }
      fclose(fp);
   }

   if (iNumBMP > 9999)
   {
      return;
   }

   //
   // Save the screenshot.
   //
#if SDL_VERSION_ATLEAST(2,0,0)
   SDL_SaveBMP(gpScreen, va("%sscrn%.4d.bmp", PAL_PREFIX, iNumBMP));
#else
   SDL_SaveBMP(gpScreenReal, va("%sscrn%.4d.bmp", PAL_PREFIX, iNumBMP));
#endif
}

VOID
VIDEO_BackupScreen(
   VOID
)
/*++
  Purpose:

    Backup the screen buffer.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   SDL_BlitSurface(gpScreen, NULL, gpScreenBak, NULL);
}

VOID
VIDEO_RestoreScreen(
   VOID
)
/*++
  Purpose:

    Restore the screen buffer which has been saved with VIDEO_BackupScreen().

  Parameters:

    None.

  Return value:

    None.

--*/
{
   SDL_BlitSurface(gpScreenBak, NULL, gpScreen, NULL);
}

VOID
VIDEO_ShakeScreen(
   WORD           wShakeTime,
   WORD           wShakeLevel
)
/*++
  Purpose:

    Set the screen shake time and level.

  Parameters:

    [IN]  wShakeTime - how many times should we shake the screen.

    [IN]  wShakeLevel - level of shaking.

  Return value:

    None.

--*/
{
   g_wShakeTime = wShakeTime;
   g_wShakeLevel = wShakeLevel;
}

VOID
VIDEO_SwitchScreen(
   WORD           wSpeed
)
/*++
  Purpose:

    Switch the screen from the backup screen buffer to the current screen buffer.
    NOTE: This will destroy the backup buffer.

  Parameters:

    [IN]  wSpeed - speed of fading (the larger value, the slower).

  Return value:

    None.

--*/
{
   int               i, j;
   const int         rgIndex[6] = {0, 3, 1, 5, 2, 4};
   SDL_Rect          dstrect;

   short             offset = 240 - 200;
   short             screenRealHeight = gpScreenReal->h;
   short             screenRealY = 0;

   if (!bScaleScreen)
   {
      screenRealHeight -= offset;
      screenRealY = offset / 2;
   }

   wSpeed++;
   wSpeed *= 10;

   for (i = 0; i < 6; i++)
   {
      for (j = rgIndex[i]; j < gpScreen->pitch * gpScreen->h; j += 6)
      {
         ((LPBYTE)(gpScreenBak->pixels))[j] = ((LPBYTE)(gpScreen->pixels))[j];
      }

      //
      // Draw the backup buffer to the screen
      //
      dstrect.x = 0;
      dstrect.y = screenRealY;
      dstrect.w = gpScreenReal->w;
      dstrect.h = screenRealHeight;

      SDL_SoftStretch(gpScreenBak, NULL, gpScreenReal, &dstrect);
#if SDL_VERSION_ATLEAST(2, 0, 0)
      VIDEO_RenderCopy();
#else
      SDL_UpdateRect(gpScreenReal, 0, 0, gpScreenReal->w, gpScreenReal->h);
#endif
      UTIL_Delay(wSpeed);
   }
}

VOID
VIDEO_FadeScreen(
   WORD           wSpeed
)
/*++
  Purpose:

    Fade from the backup screen buffer to the current screen buffer.
    NOTE: This will destroy the backup buffer.

  Parameters:

    [IN]  wSpeed - speed of fading (the larger value, the slower).

  Return value:

    None.

--*/
{
   int               i, j, k;
   DWORD             time;
   BYTE              a, b;
   const int         rgIndex[6] = {0, 3, 1, 5, 2, 4};
   SDL_Rect          dstrect;
   short             offset = 240 - 200;
   short             screenRealHeight = gpScreenReal->h;
   short             screenRealY = 0;

   //
   // Lock surface if needed
   //
   if (SDL_MUSTLOCK(gpScreenReal))
   {
      if (SDL_LockSurface(gpScreenReal) < 0)
         return;
   }

   if (!bScaleScreen)
   {
      screenRealHeight -= offset;
      screenRealY = offset / 2;
   }

   time = SDL_GetTicks();

   wSpeed++;
   wSpeed *= 10;

   for (i = 0; i < 12; i++)
   {
      for (j = 0; j < 6; j++)
      {
         PAL_ProcessEvent();
         while (!SDL_TICKS_PASSED(SDL_GetTicks(), time))
         {
            PAL_ProcessEvent();
            SDL_Delay(5);
         }
         time = SDL_GetTicks() + wSpeed;

         //
         // Blend the pixels in the 2 buffers, and put the result into the
         // backup buffer
         //
         for (k = rgIndex[j]; k < gpScreen->pitch * gpScreen->h; k += 6)
         {
            a = ((LPBYTE)(gpScreen->pixels))[k];
            b = ((LPBYTE)(gpScreenBak->pixels))[k];

            if (i > 0)
            {
               if ((a & 0x0F) > (b & 0x0F))
               {
                  b++;
               }
               else if ((a & 0x0F) < (b & 0x0F))
               {
                  b--;
               }
            }

            ((LPBYTE)(gpScreenBak->pixels))[k] = ((a & 0xF0) | (b & 0x0F));
         }

         //
         // Draw the backup buffer to the screen
         //
         if (g_wShakeTime != 0)
         {
            //
            // Shake the screen
            //
            SDL_Rect srcrect, dstrect;

            srcrect.x = 0;
            srcrect.y = 0;
            srcrect.w = 320;
            srcrect.h = 200 - g_wShakeLevel;

            dstrect.x = 0;
            dstrect.y = screenRealY;
            dstrect.w = 320 * gpScreenReal->w / gpScreen->w;
            dstrect.h = (200 - g_wShakeLevel) * screenRealHeight / gpScreen->h;

            if (g_wShakeTime & 1)
            {
               srcrect.y = g_wShakeLevel;
            }
            else
            {
               dstrect.y = (screenRealY + g_wShakeLevel) * screenRealHeight / gpScreen->h;
            }

            SDL_SoftStretch(gpScreenBak, &srcrect, gpScreenReal, &dstrect);

            if (g_wShakeTime & 1)
            {
               dstrect.y = (screenRealY + screenRealHeight - g_wShakeLevel) * screenRealHeight / gpScreen->h;
            }
            else
            {
               dstrect.y = screenRealY;
            }

            dstrect.h = g_wShakeLevel * screenRealHeight / gpScreen->h;

            SDL_FillRect(gpScreenReal, &dstrect, 0);
#if SDL_VERSION_ATLEAST(2, 0, 0)
            VIDEO_RenderCopy();
#else
			SDL_UpdateRect(gpScreenReal, 0, 0, gpScreenReal->w, gpScreenReal->h);
#endif
            g_wShakeTime--;
         }
         else
         {
            dstrect.x = 0;
            dstrect.y = screenRealY;
            dstrect.w = gpScreenReal->w;
            dstrect.h = screenRealHeight;

            SDL_SoftStretch(gpScreenBak, NULL, gpScreenReal, &dstrect);
#if SDL_VERSION_ATLEAST(2, 0, 0)
            VIDEO_RenderCopy();
#else
            SDL_UpdateRect(gpScreenReal, 0, 0, gpScreenReal->w, gpScreenReal->h);
#endif
         }
      }
   }

   if (SDL_MUSTLOCK(gpScreenReal))
   {
      SDL_UnlockSurface(gpScreenReal);
   }

   //
   // Draw the result buffer to the screen as the final step
   //
   VIDEO_UpdateScreen(NULL);
}

#if SDL_VERSION_ATLEAST(2,0,0)

/*++
  Purpose:

    Set the caption of the window. For compatibility with SDL2 only.

  Parameters:

    [IN]  lpszCaption - the new caption of the window.

    [IN]  lpReserved - not used, for compatibility only.

  Return value:

    None.

--*/
VOID
SDL_WM_SetCaption(
   LPCSTR         lpszCaption,
   LPVOID         lpReserved
)
{
   if (gpWindow != NULL)
   {
      SDL_SetWindowTitle(gpWindow, lpszCaption);
   }
}

#endif
