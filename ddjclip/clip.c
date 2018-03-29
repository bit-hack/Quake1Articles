/* clip.c

   Win32 program to demonstrate 3-D clipping.

   Derived from the VC++ generic sample application.
   Tested with VC++ 2.0 running on Windows NT 3.5.
   
   Note: polygon processing could be considerably more efficient
   if polygons shared common edges and edges shared common vertices.
   Also, indirection to vertices could be used to avoid having to
   copy all the vertices during every clip test. Outcode-type
   testing could be used to determine completely clipped or
   unclipped polygons ahead of time, avoiding the need to clip and
   copy entirely for such polygons. Outcode-type tests work best in
   viewspace, with the frustum normalized so that the field of view
   is 90 degrees, so simple compares, rather than dot products, can
   be used to categorize points with respect to the frustum. See
   _Computer Graphics_, by Foley & van Dam, or _Procedural Elements
   of Computer Graphics_, by Rogers, for further information.
*/

#include <windows.h>   	// required for all Windows applications
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "clip.h"  		// specific to this program

#define INITIAL_DIB_WIDTH  	320		// initial dimensions of DIB
#define INITIAL_DIB_HEIGHT	240		//  into which we'll draw
#define MAX_POLY_VERTS      8
#define MAX_SCREEN_HEIGHT   2048
#define MOVEMENT_SPEED      3.0
#define VMOVEMENT_SPEED     3.0
#define MAX_MOVEMENT_SPEED  30.0
#define PI                  3.141592
#define ROLL_SPEED          (PI/20.0)
#define PITCH_SPEED         (PI/20.0)
#define YAW_SPEED           (PI/20.0)
#define MAX_COORD           0x4000
#define NUM_FRUSTUM_PLANES  4
#define CLIP_PLANE_EPSILON  0.0001

typedef struct {
    double v[3];
} point_t;

typedef struct {
    double   x, y;
} point2D_t;

typedef struct {
    int         color;
    int         numverts;
    point_t     verts[MAX_POLY_VERTS];
} polygon_t;

typedef struct {
    int         color;
    int         numverts;
    point2D_t   verts[MAX_POLY_VERTS];
} polygon2D_t;

typedef struct convexobject_s {
    struct convexobject_s   *pnext;
    point_t                 center;
    double                  vdist;
    int                     numpolys;
    polygon_t               *ppoly;
} convexobject_t;

typedef struct {
    int xleft, xright;
} span_t;

typedef struct {
    double  distance;
    point_t normal;
} plane_t;

BITMAPINFO *pbmiDIB;		// pointer to the BITMAPINFO
char *pDIB, *pDIBBase;		// pointers to DIB section we'll draw into
HBITMAP hDIBSection;        // handle of DIB section
HINSTANCE hInst;            // current instance
char szAppName[] = "Clip";  // The name of this application
char szTitle[]   = "3D clipping demo"; // The title bar text
HPALETTE hpalold, hpalDIB;
HWND hwndOutput;
int DIBWidth, DIBHeight;
int DIBPitch;
double  roll, pitch, yaw;
double  currentspeed;
point_t currentpos;
double  fieldofview, xcenter, ycenter;
double  xscreenscale, yscreenscale, maxscale;
int     numobjects;
double  speedscale = 1.0;
plane_t frustumplanes[NUM_FRUSTUM_PLANES];

double  mroll[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
double  mpitch[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
double  myaw[3][3] =  {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
point_t vpn, vright, vup;
point_t xaxis = {1, 0, 0};
point_t zaxis = {0, 0, 1};

polygon_t polys0[] = {
{15, 4, {{-10,10,-10}, {10,10,-10}, {10,-10,-10}, {-10,-10,-10}}},
{14, 4, {{10,10,-10}, {10,10,10}, {10,-10,10}, {10,-10,-10}}},
{13, 4, {{10,10,10}, {-10,10,10}, {-10,-10,10}, {10,-10,10}}},
{12, 4, {{-10,10,10}, {-10,10,-10}, {-10,-10,-10}, {-10,-10,10}}},
{11, 4, {{-10,10,-10}, {-10,10,10}, {10,10,10}, {10,10,-10}}},
{10, 4, {{-10,-10,-10}, {10,-10,-10}, {10,-10,10}, {-10,-10,10}}},
};

polygon_t polys1[] = {
{1, 4, {{-200,9980,-200}, {-200,9980,200},
        {200,9980,200}, {200,9980,-200}}},
};

convexobject_t objects[] = {
{NULL, {-50,0,70}, 0, sizeof(polys0) / sizeof(polys0[0]), polys0},
{NULL, {0,20,70}, 0, sizeof(polys0) / sizeof(polys0[0]), polys0},
{NULL, {50,0,70}, 0, sizeof(polys0) / sizeof(polys0[0]), polys0},
{NULL, {-50,0,-70}, 0, sizeof(polys0) / sizeof(polys0[0]), polys0},
{NULL, {0,20,-70}, 0, sizeof(polys0) / sizeof(polys0[0]), polys0},
{NULL, {50,30,-70}, 0, sizeof(polys0) / sizeof(polys0[0]), polys0},
{NULL, {-50,15,0}, 0, sizeof(polys0) / sizeof(polys0[0]), polys0},
{NULL, {50,15,0}, 0, sizeof(polys0) / sizeof(polys0[0]), polys0},
{NULL, {0,50,0}, 0, sizeof(polys0) / sizeof(polys0[0]), polys0},
{NULL, {-100,100,115}, 0, sizeof(polys0) / sizeof(polys0[0]), polys0},
{NULL, {-100,150,120}, 0, sizeof(polys0) / sizeof(polys0[0]), polys0},
{NULL, {100,200,100}, 0, sizeof(polys0) / sizeof(polys0[0]), polys0},
{NULL, {0,-10000,0}, 0, sizeof(polys1) / sizeof(polys1[0]), polys1},
};

// Head and sentinel for object list
convexobject_t objecthead = {NULL, {0,0,0}, -999999.0};

void UpdateWorld(void);

/////////////////////////////////////////////////////////////////////
// WinMain
/////////////////////////////////////////////////////////////////////
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    HANDLE hAccelTable;

    if (!hPrevInstance) {       // Other instances of app running?
        if (!InitApplication(hInstance)) { // Initialize shared things
            return (FALSE);     // Exits if unable to initialize
        }
    }

    // Perform initializations that apply to a specific instance
    if (!InitInstance(hInstance, nCmdShow)) {
        return (FALSE);
    }

    hAccelTable = LoadAccelerators (hInstance, szAppName);

    // Acquire and dispatch messages until a WM_QUIT message is
    // received
    for (;;) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			do {
	    		if (msg.message == WM_QUIT) {
					goto Done;
				}
	      		if (!TranslateAccelerator (msg.hwnd, hAccelTable,
	      		        &msg)) {
            		TranslateMessage(&msg);// xlates virt keycodes
       	        	DispatchMessage(&msg); // Dispatches msg to window
	       	   	}
			} while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE));
		}

        // Update the world
		UpdateWorld();
	}

Done:
    return (msg.wParam); // Returns the value from PostQuitMessage

    lpCmdLine; // This will prevent 'unused formal parameter' warnings
}

/////////////////////////////////////////////////////////////////////
// InitApplication
/////////////////////////////////////////////////////////////////////
BOOL InitApplication(HINSTANCE hInstance)
{
        WNDCLASS  wc;

        // Fill in window class structure with parameters that
        // describe the main window.
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = (WNDPROC)WndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = hInstance;
        wc.hIcon         = LoadIcon (hInstance, szAppName);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        wc.lpszMenuName  = szAppName;
        wc.lpszClassName = szAppName;

        // Register the window class and return success/failure code.
        return (RegisterClass(&wc));
}

/////////////////////////////////////////////////////////////////////
// InitInstance
/////////////////////////////////////////////////////////////////////
BOOL InitInstance(
        HINSTANCE       hInstance,
        int             nCmdShow)
{
        HWND            hwnd; // Main window handle.
		HDC				hdc;
		INT				i, j, k;
		LOGPALETTE *	plogpal;
		PUSHORT 		pusTemp;
		HPALETTE		hpal;
		RECT			rctmp;
		int				screenx, screeny;

        // Save the instance handle in static variable, which will be
        // used in many subsequent calls from this application to
        // Windows
        hInst = hInstance; // Store inst handle in our global variable

        // Create a main window for this application instance
		DIBWidth = INITIAL_DIB_WIDTH;
		DIBHeight = INITIAL_DIB_HEIGHT;
	   	rctmp.left = 0;
		rctmp.top = 0;
		rctmp.right = DIBWidth;
		rctmp.bottom = DIBHeight;
   
	  	AdjustWindowRect(&rctmp, WS_OVERLAPPEDWINDOW, 1);

		screenx = GetSystemMetrics(SM_CXSCREEN);
		screeny = GetSystemMetrics(SM_CYSCREEN);

        hwnd = CreateWindow(
                szAppName,           // See RegisterClass() call.
                szTitle,             // Text for window title bar.
                WS_OVERLAPPEDWINDOW,// Window style.
                screenx - (rctmp.right - rctmp.left),
                screeny - (rctmp.bottom - rctmp.top),
				rctmp.right - rctmp.left,
				rctmp.bottom - rctmp.top,
                NULL,
                NULL,
                hInstance,
                NULL
        );

        // If window could not be created, return "failure"
        if (!hwnd) {
        	return (FALSE);
        }

		// Make the window visible and draw it
        ShowWindow(hwnd, nCmdShow); // Show the window
        UpdateWindow(hwnd);         // Sends WM_PAINT message

        hdc = GetDC(hwnd);

        // For 256-color mode, set up the palette for maximum speed
        // in copying to the screen. If not a 256-color mode, the
        // adapter isn't palettized, so we'll just use the default
        // palette. However, we will run a lot slower in this case
        // due to translation while copying
        if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE) {
            // This is a 256-color palettized mode.
    		// Set up and realize our palette and the identity color
    		// table for the DIB (identity means that DIB color
            // indices and screen palette indices match exactly, so
            // GDI doesn't have to do any translation when copying
            // from DIB to screen. This helps performance a lot)
		    plogpal = malloc(sizeof(LOGPALETTE) +
	    	        256 * sizeof(PALETTEENTRY));

    		if (plogpal == NULL) {
		    	return(FALSE);
    		}

	    	// Take up all physical palette entries, to flush out
	    	// anything that's currently in the palette
	    	plogpal->palVersion = 0x300;
		    plogpal->palNumEntries = 236;
	    	for (i=0; i<236; i++) {
		    	plogpal->palPalEntry[i].peRed = i;
			    plogpal->palPalEntry[i].peGreen = 0;
    			plogpal->palPalEntry[i].peBlue = 0;
	    		plogpal->palPalEntry[i].peFlags =
		    	        PC_RESERVED | PC_NOCOLLAPSE;
    		}

	    	hpal = CreatePalette(plogpal);

    		if (hpal == 0) {
		    	return(FALSE);
    		}

		    hpalold = SelectPalette(hdc, hpal, FALSE);

    		if (hpalold == 0) {
		    	return(FALSE);
    		}

    		if (RealizePalette(hdc) != plogpal->palNumEntries) {
			    return(FALSE);
		    }

    		if (SelectPalette(hdc, hpalold, FALSE) == 0) {
		    	return(FALSE);
    		}

	    	if (!DeleteObject(hpal)) {
			    return(FALSE);
    		}

		    // Now set up the 6value-6value-6value RGB palette,
		    // followed by 20 gray levels, that we want to work with
		    // into the physical palette
	    	for (i=0; i<6; i++) {
			    for (j=0; j<6; j++) {
	    			for (k=0; k<6; k++) {
			    		plogpal->palPalEntry[i*36+j*6+k].peRed =
                                i*255/6;
					    plogpal->palPalEntry[i*36+j*6+k].peGreen =
					            j*255/6;
    					plogpal->palPalEntry[i*36+j*6+k].peBlue =
	   				            k*255/6;
	    				plogpal->palPalEntry[i*36+j*6+k].peFlags =
					        PC_RESERVED | PC_NOCOLLAPSE;
		    		}
    			}
	    	}

		    for (i=0; i<20; i++) {
    			plogpal->palPalEntry[i+216].peRed = i*255/20;
	    		plogpal->palPalEntry[i+216].peGreen = i*255/20;
			    plogpal->palPalEntry[i+216].peBlue = i*255/20;
    			plogpal->palPalEntry[i+216].peFlags =
	    		        PC_RESERVED | PC_NOCOLLAPSE;
		    }

    		hpal = CreatePalette(plogpal);

	    	if (hpal == 0) {
			    return(FALSE);
    		}

	    	if (SelectPalette(hdc, hpal, FALSE) == 0) {
			    return(FALSE);
    		}

	    	if (RealizePalette(hdc) != plogpal->palNumEntries) {
			    return(FALSE);
    		}

		    // Get back the 256 colors now in the physical palette,
		    // which are the 236 we just selected, plus the 20 static
		    // colors
    		if (GetSystemPaletteEntries(hdc, 0, 256,
    		        plogpal->palPalEntry) != 256) {
	    		return(FALSE);
		    }

    		for (i=10; i<246; i++) {
		    	plogpal->palPalEntry[i].peFlags =
			            PC_RESERVED | PC_NOCOLLAPSE;
    		}

	    	// Now create a logical palette that exactly matches the
		    // physical palette, and realize it. This is the palette
    		// into which the DIB pixel values will be indices
    		plogpal->palNumEntries = 256;

	    	hpalDIB = CreatePalette(plogpal);

    		if (hpalDIB == 0)
		    	return(FALSE);

	    	if (SelectPalette(hdc, hpalDIB, FALSE) == 0)
			    return(FALSE);

	    	DeleteObject(hpal);

    		if (RealizePalette(hdc) != plogpal->palNumEntries)
		    	return(FALSE);

	    	if (SelectPalette(hdc, hpalold, FALSE) == FALSE)
			    return(FALSE);

       		free(plogpal);
        }

		// Finally, set up the DIB section
		pbmiDIB = malloc(sizeof(BITMAPINFO) - 4 + 256*sizeof(USHORT));

		if (pbmiDIB == NULL)
			return(FALSE);


		pbmiDIB->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		pbmiDIB->bmiHeader.biWidth = DIBWidth;
		pbmiDIB->bmiHeader.biHeight = DIBHeight;
		pbmiDIB->bmiHeader.biPlanes = 1;
		pbmiDIB->bmiHeader.biBitCount = 8;
		pbmiDIB->bmiHeader.biCompression = BI_RGB;
		pbmiDIB->bmiHeader.biSizeImage = 0;
		pbmiDIB->bmiHeader.biXPelsPerMeter = 0;
		pbmiDIB->bmiHeader.biYPelsPerMeter = 0;
		pbmiDIB->bmiHeader.biClrUsed = 256;
		pbmiDIB->bmiHeader.biClrImportant = 256;

		pusTemp = (PUSHORT) pbmiDIB->bmiColors;

		for (i=0; i<256; i++) {
			*pusTemp++ = i;
		}

        hDIBSection = CreateDIBSection (hdc, pbmiDIB, DIB_PAL_COLORS,
                          &pDIBBase, NULL, 0);

        if (!hDIBSection) {
            free(pbmiDIB);
            return(FALSE);
        }

        if (pbmiDIB->bmiHeader.biHeight > 0)
        {
            pDIB = (pDIBBase + (DIBHeight - 1) * DIBWidth);
            DIBPitch = -DIBWidth;   // bottom-up
        }
        else
        {
            pDIB = pDIBBase;
            DIBPitch = DIBWidth;    // top-down
        }

		// Clear the DIB
		memset(pDIBBase, 0, DIBWidth*DIBHeight);

		ReleaseDC(hwnd, hdc);

		hwndOutput = hwnd;

        // Set the initial location, direction, and speed
        roll = 0.0;
        pitch = 0.0;
        yaw = 0.0;
        currentspeed = 0.0;
        currentpos.v[0] = 0.0;
        currentpos.v[1] = 0.0;
        currentpos.v[2] = 0.0;
        fieldofview = 2.0;
        xscreenscale = DIBWidth / fieldofview;
        yscreenscale = DIBHeight / fieldofview;
        maxscale = max(xscreenscale, yscreenscale);
        xcenter = DIBWidth / 2.0 - 0.5;
        ycenter = DIBHeight / 2.0 + 0.5;

        numobjects = sizeof(objects) / sizeof(objects[0]);

        return (TRUE);              // We succeeded...
}

/////////////////////////////////////////////////////////////////////
// WndProc
/////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndProc(
                HWND hwnd,         // window handle
                UINT message,      // type of message
                WPARAM uParam,     // additional information
                LPARAM lParam)     // additional information
{
    int wmId, wmEvent;
	UINT fwSizeType;
	int oldDIBWidth, oldDIBHeight;
    HBITMAP holdDIBSection;
    HDC hdc;

    switch (message) {

    case WM_COMMAND:  // message: command from application menu

    	wmId    = LOWORD(uParam);
        wmEvent = HIWORD(uParam);

        switch (wmId) {

     	case IDM_EXIT:
        	DestroyWindow (hwnd);
            break;

        default:
        	return (DefWindowProc(hwnd, message, uParam, lParam));
        }
        break;

	case WM_KEYDOWN:
		switch (uParam) {

        case VK_DOWN:
            currentspeed -= MOVEMENT_SPEED * speedscale;
            if (currentspeed < -(MAX_MOVEMENT_SPEED * speedscale))
                currentspeed = -(MAX_MOVEMENT_SPEED * speedscale);
			break;

        case VK_UP:
            currentspeed += MOVEMENT_SPEED * speedscale;
            if (currentspeed > (MAX_MOVEMENT_SPEED * speedscale))
                currentspeed = (MAX_MOVEMENT_SPEED * speedscale);
			break;

        case 'N':
            roll += ROLL_SPEED * speedscale;
            if (roll >= (PI * 2))
                roll -= PI * 2;
            break;

        case 'M':
            roll -= ROLL_SPEED * speedscale;
            if (roll < 0)
                roll += PI * 2;
            break;

        case 'A':
            pitch -= PITCH_SPEED * speedscale;
            if (pitch < 0)
                pitch += PI * 2;
            break;

        case 'Z':
            pitch += PITCH_SPEED * speedscale;
            if (pitch >= (PI * 2))
                pitch -= PI * 2;
            break;

        case 'D':
            currentpos.v[1] += VMOVEMENT_SPEED;
            break;

        case 'C':
            currentpos.v[1] -= VMOVEMENT_SPEED;
            break;

        case VK_LEFT:
            yaw -= YAW_SPEED * speedscale;
            if (yaw < 0)
                yaw += PI * 2;
			break;

        case VK_RIGHT:
            yaw += YAW_SPEED * speedscale;
            if (yaw >= (PI * 2))
                yaw -= PI * 2;
			break;

		default:
			break;
		}
		return(0);

	case WM_KEYUP:
		switch (uParam) {

		case VK_SUBTRACT:
            fieldofview *= 0.9;
            xscreenscale = DIBWidth / fieldofview;
            yscreenscale = DIBHeight / fieldofview;
            maxscale = max(xscreenscale, yscreenscale);
			break;

		case VK_ADD:
            fieldofview *= 1.1;
            xscreenscale = DIBWidth / fieldofview;
            yscreenscale = DIBHeight / fieldofview;
            maxscale = max(xscreenscale, yscreenscale);
			break;

        case 'F':
            speedscale *= 1.1;
            break;

        case 'S':
            speedscale *= 0.9;
            break;

		default:
			break;
		}
		return(0);

	case WM_SIZE:	// window size changed
		fwSizeType = uParam;
		if (fwSizeType != SIZE_MINIMIZED) {
            // Skip when this is called before the first DIB
            // section is created
            if (hDIBSection == 0)
                break;

			oldDIBWidth = DIBWidth;
			oldDIBHeight = DIBHeight;

			DIBWidth = LOWORD(lParam);
			DIBWidth = (DIBWidth + 3) & ~3;
			DIBHeight = HIWORD(lParam);

            if ((DIBHeight < 10) || (DIBWidth < 10))
            {
            // Keep the DIB section big enough so we don't start
            // drawing outside the DIB (the window can get smaller,
            // but we don't really care, and GDI will clip the
            // blts for us)
    			DIBWidth = oldDIBWidth;
    			DIBHeight = oldDIBHeight;
            }

			// Resize the DIB section to the new size
            holdDIBSection = hDIBSection;
			pbmiDIB->bmiHeader.biWidth = DIBWidth;
			pbmiDIB->bmiHeader.biHeight = DIBHeight;

            hdc = GetDC(hwnd);
            hDIBSection = CreateDIBSection (hdc, pbmiDIB,
                    DIB_PAL_COLORS, &pDIBBase, NULL, 0);

            if (hDIBSection) {
                // Success
                DeleteObject(holdDIBSection);

                if (pbmiDIB->bmiHeader.biHeight > 0)
                {
                    pDIB = (pDIBBase + (DIBHeight - 1) * DIBWidth);
                    DIBPitch = -DIBWidth;   // bottom-up
                }
                else
                {
                    pDIB = pDIBBase;
                    DIBPitch = DIBWidth;    // top-down
                }

                xscreenscale = DIBWidth / fieldofview;
                yscreenscale = DIBHeight / fieldofview;
                maxscale = max(xscreenscale, yscreenscale);
                xcenter = DIBWidth / 2.0 - 0.5;
                ycenter = DIBHeight / 2.0 + 0.5;
            } else {
                // Failed, just use old size
    			pbmiDIB->bmiHeader.biWidth = oldDIBWidth;
	    		pbmiDIB->bmiHeader.biHeight = oldDIBHeight;
    			DIBWidth = oldDIBWidth;
    			DIBHeight = oldDIBHeight;
            }

			// Clear the DIB
			memset(pDIBBase, 0, DIBWidth*DIBHeight);
		}
		break;

	case WM_DESTROY:  // message: window being destroyed
		free(pbmiDIB);
        DeleteObject(hDIBSection);
		DeleteObject(hpalold);
                        
        PostQuitMessage(0);
        break;

    default:          // Passes it on if unproccessed
		return (DefWindowProc(hwnd, message, uParam, lParam));
    }
    return (0);
}

/////////////////////////////////////////////////////////////////////
// 3-D dot product.
/////////////////////////////////////////////////////////////////////
double DotProduct(point_t *vec1, point_t *vec2)
{
    return vec1->v[0] * vec2->v[0] +
           vec1->v[1] * vec2->v[1] +
           vec1->v[2] * vec2->v[2];
}

/////////////////////////////////////////////////////////////////////
// 3-D cross product.
/////////////////////////////////////////////////////////////////////
void CrossProduct(point_t *in1, point_t *in2, point_t *out)
{
    out->v[0] = in1->v[1] * in2->v[2] - in1->v[2] * in2->v[1];
    out->v[1] = in1->v[2] * in2->v[0] - in1->v[0] * in2->v[2];
    out->v[2] = in1->v[0] * in2->v[1] - in1->v[1] * in2->v[0];
}

/////////////////////////////////////////////////////////////////////
// Concatenate two 3x3 matrices.
/////////////////////////////////////////////////////////////////////
void MConcat(double in1[3][3], double in2[3][3], double out[3][3])
{
    int     i, j;

    for (i=0 ; i<3 ; i++)
    {
        for (j=0 ; j<3 ; j++)
        {
            out[i][j] = in1[i][0] * in2[0][j] +
                        in1[i][1] * in2[1][j] +
                        in1[i][2] * in2[2][j];
        }
    }
}

/////////////////////////////////////////////////////////////////////
// Fills a screen polygon.
// Polygon is assumed to contain only valid, on-screen coordinates.
// Uses fixed-point, but definitely not very optimized.
/////////////////////////////////////////////////////////////////////
void FillPolygon2D(polygon2D_t *ppoly)
{
    int     i, j, topvert, bottomvert, leftvert, rightvert, nextvert;
    int     itopy, ibottomy, islope, spantopy, spanbottomy, x, count;
    double  topy, bottomy, slope, height, width, prestep;
    span_t  spans[MAX_SCREEN_HEIGHT], *pspan;

    topy = 999999.0;
    bottomy = -999999.0;
    for (i=0 ; i<ppoly->numverts ; i++)
    {
        if (ppoly->verts[i].y < topy)
        {
            topy = ppoly->verts[i].y;
            topvert = i;
        }
        if (ppoly->verts[i].y > bottomy)
        {
            bottomy = ppoly->verts[i].y;
            bottomvert = i;
        }
    }

    itopy = (int)ceil(topy);
    ibottomy = (int)ceil(bottomy);

    if (ibottomy == itopy)
        return;     // reject polygons that don't cross a scan

    // Scan out the left edge
    pspan = spans;
    leftvert = topvert;

    do
    {
        nextvert = leftvert - 1;
        if (nextvert < 0)
            nextvert = ppoly->numverts - 1;
        spantopy = (int)ceil(ppoly->verts[leftvert].y);
        spanbottomy = (int)ceil(ppoly->verts[nextvert].y);
        if (spantopy < spanbottomy)
        {
            height = ppoly->verts[nextvert].y -
                    ppoly->verts[leftvert].y;
            width = ppoly->verts[nextvert].x -
                    ppoly->verts[leftvert].x;
            slope = width / height;
            prestep = spantopy - ppoly->verts[leftvert].y;
            x = (int)((ppoly->verts[leftvert].x +
                    (slope * prestep)) * 65536.0) + ((1 << 16) - 1);
            islope = (int)(slope * 65536.0);

            for (j=spantopy ; j<spanbottomy ; j++)
            {
                pspan->xleft = x >> 16;
                x += islope;
                pspan++;
            }
        }

        leftvert--;
        if (leftvert < 0)
            leftvert = ppoly->numverts - 1;
    } while (leftvert != bottomvert);

    // Scan out the right edge
    pspan = spans;
    rightvert = topvert;

    do
    {
        nextvert = (rightvert + 1) % ppoly->numverts;
        spantopy = (int)ceil(ppoly->verts[rightvert].y);
        spanbottomy = (int)ceil(ppoly->verts[nextvert].y);
        if (spantopy < spanbottomy)
        {
            height = ppoly->verts[nextvert].y -
                    ppoly->verts[rightvert].y;
            width = ppoly->verts[nextvert].x -
                    ppoly->verts[rightvert].x;
            slope = width / height;
            prestep = spantopy - ppoly->verts[rightvert].y;
            x = (int)((ppoly->verts[rightvert].x +
                    (slope * prestep)) * 65536.0) + ((1 << 16) - 1);
            islope = (int)(slope * 65536.0);

            for (j=spantopy ; j<spanbottomy ; j++)
            {
                pspan->xright = x >> 16;
                x += islope;
                pspan++;
            }
        }

        rightvert = (rightvert + 1) % ppoly->numverts;
    } while (rightvert != bottomvert);

    // Draw the spans
    pspan = spans;

    for (i=itopy ; i<ibottomy ; i++)
    {
        count = pspan->xright - pspan->xleft;
        if (count > 0)
        {
            memset (pDIB + (DIBPitch * i) + pspan->xleft,
                    ppoly->color,
                    count);
        }
        pspan++;
    }
}

/////////////////////////////////////////////////////////////////////
// Project viewspace polygon vertices into screen coordinates.
// Note that the y axis goes up in worldspace and viewspace, but
// goes down in screenspace.
/////////////////////////////////////////////////////////////////////
void ProjectPolygon (polygon_t *ppoly, polygon2D_t *ppoly2D)
{
    int     i;
    double  zrecip;

    for (i=0 ; i<ppoly->numverts ; i++)
    {
        zrecip = 1.0 / ppoly->verts[i].v[2];
        ppoly2D->verts[i].x =
               ppoly->verts[i].v[0] * zrecip * maxscale + xcenter;
        ppoly2D->verts[i].y = DIBHeight -
             (ppoly->verts[i].v[1] * zrecip * maxscale + ycenter);
    }

    ppoly2D->color = ppoly->color;
    ppoly2D->numverts = ppoly->numverts;
}

/////////////////////////////////////////////////////////////////////
// Sort the objects according to z distance from viewpoint.
/////////////////////////////////////////////////////////////////////
void ZSortObjects(void)
{
    int             i, j;
    double          vdist;
    convexobject_t  *pobject;
    point_t         dist;

    objecthead.pnext = &objecthead;

    for (i=0 ; i<numobjects ; i++)
    {
        for (j=0 ; j<3 ; j++)
        {
            dist.v[j] = objects[i].center.v[j] - currentpos.v[j];
        }

        objects[i].vdist = sqrt(dist.v[0] * dist.v[0] +
                                dist.v[1] * dist.v[1] +
                                dist.v[2] * dist.v[2]);

        pobject = &objecthead;
        vdist = objects[i].vdist;

        // Viewspace-distance-sort this object into the others.
        // Guaranteed to terminate because of sentinel
        while (vdist < pobject->pnext->vdist)
        {
            pobject = pobject->pnext;
        }

        objects[i].pnext = pobject->pnext;
        pobject->pnext = &objects[i];
    }
}


/////////////////////////////////////////////////////////////////////
// Move the view position and set the world->view transform.
/////////////////////////////////////////////////////////////////////
void UpdateViewPos()
{
    int     i;
    point_t motionvec;
    double  s, c, mtemp1[3][3], mtemp2[3][3];

    // Move in the view direction, across the x-y plane, as if
    // walking. This approach moves slower when looking up or
    // down at more of an angle
    motionvec.v[0] = DotProduct(&vpn, &xaxis);
    motionvec.v[1] = 0.0;
    motionvec.v[2] = DotProduct(&vpn, &zaxis);

    for (i=0 ; i<3 ; i++)
    {
        currentpos.v[i] += motionvec.v[i] * currentspeed;
        if (currentpos.v[i] > MAX_COORD)
            currentpos.v[i] = MAX_COORD;
        if (currentpos.v[i] < -MAX_COORD)
            currentpos.v[i] = -MAX_COORD;
    }

    // Set up the world-to-view rotation.
    // Note: much of the work done in concatenating these matrices
    // can be factored out, since it contributes nothing to the
    // final result; multiply the three matrices together on paper
    // to generate a minimum equation for each of the 9 final elements
    s = sin(roll);
    c = cos(roll);
    mroll[0][0] = c;
    mroll[0][1] = s;
    mroll[1][0] = -s;
    mroll[1][1] = c;

    s = sin(pitch);
    c = cos(pitch);
    mpitch[1][1] = c;
    mpitch[1][2] = s;
    mpitch[2][1] = -s;
    mpitch[2][2] = c;

    s = sin(yaw);
    c = cos(yaw);
    myaw[0][0] = c;
    myaw[0][2] = -s;
    myaw[2][0] = s;
    myaw[2][2] = c;

    MConcat(mroll, myaw, mtemp1);
    MConcat(mpitch, mtemp1, mtemp2);

    // Break out the rotation matrix into vright, vup, and vpn.
    // We could work directly with the matrix; breaking it out
    // into three vectors is just to make things clearer
    for (i=0 ; i<3 ; i++)
    {
        vright.v[i] = mtemp2[0][i];
        vup.v[i] = mtemp2[1][i];
        vpn.v[i] = mtemp2[2][i];
    }

    // Simulate crude friction
    if (currentspeed > (MOVEMENT_SPEED * speedscale / 2.0))
        currentspeed -= MOVEMENT_SPEED * speedscale / 2.0;
    else if (currentspeed < -(MOVEMENT_SPEED * speedscale / 2.0))
        currentspeed += MOVEMENT_SPEED * speedscale / 2.0;
    else
        currentspeed = 0.0;
}

/////////////////////////////////////////////////////////////////////
// Rotate a vector from viewspace to worldspace.
/////////////////////////////////////////////////////////////////////
void BackRotateVector(point_t *pin, point_t *pout)
{
    int     i;

    // Rotate into the world orientation
    for (i=0 ; i<3 ; i++)
    {
        pout->v[i] = pin->v[0] * vright.v[i] +
                     pin->v[1] * vup.v[i] +
                     pin->v[2] * vpn.v[i];
    }
}

/////////////////////////////////////////////////////////////////////
// Transform a point from worldspace to viewspace.
/////////////////////////////////////////////////////////////////////
void TransformPoint(point_t *pin, point_t *pout)
{
    int     i;
    point_t tvert;

    // Translate into a viewpoint-relative coordinate
    for (i=0 ; i<3 ; i++)
    {
        tvert.v[i] = pin->v[i] - currentpos.v[i];
    }

    // Rotate into the view orientation
    pout->v[0] = DotProduct(&tvert, &vright);
    pout->v[1] = DotProduct(&tvert, &vup);
    pout->v[2] = DotProduct(&tvert, &vpn);
}

/////////////////////////////////////////////////////////////////////
// Transform a polygon from worldspace to viewspace.
/////////////////////////////////////////////////////////////////////
void TransformPolygon(polygon_t *pinpoly, polygon_t *poutpoly)
{
    int         i;

    for (i=0 ; i<pinpoly->numverts ; i++)
    {
        TransformPoint(&pinpoly->verts[i], &poutpoly->verts[i]);
    }

    poutpoly->color = pinpoly->color;
    poutpoly->numverts = pinpoly->numverts;
}

/////////////////////////////////////////////////////////////////////
// Returns true if polygon faces the viewpoint, assuming a clockwise
// winding of vertices as seen from the front.
/////////////////////////////////////////////////////////////////////
int PolyFacesViewer(polygon_t *ppoly)
{
    int     i;
    point_t viewvec, edge1, edge2, normal;

    for (i=0 ; i<3 ; i++)
    {
        viewvec.v[i] = ppoly->verts[0].v[i] - currentpos.v[i];
        edge1.v[i] = ppoly->verts[0].v[i] - ppoly->verts[1].v[i];
        edge2.v[i] = ppoly->verts[2].v[i] - ppoly->verts[1].v[i];
    }

    CrossProduct(&edge1, &edge2, &normal);

    if (DotProduct(&viewvec, &normal) > 0)
        return 1;
    else
        return 0;
}

/////////////////////////////////////////////////////////////////////
// Set up a clip plane with the specified normal.
/////////////////////////////////////////////////////////////////////
void SetWorldspaceClipPlane(point_t *normal, plane_t *plane)
{

    // Rotate the plane normal into worldspace
    BackRotateVector(normal, &plane->normal);

    plane->distance = DotProduct(&currentpos, &plane->normal) +
            CLIP_PLANE_EPSILON;
}

/////////////////////////////////////////////////////////////////////
// Set up the planes of the frustum, in worldspace coordinates.
/////////////////////////////////////////////////////////////////////
void SetUpFrustum(void)
{
    double  angle, s, c;
    point_t normal;

    angle = atan(2.0 / fieldofview * maxscale / xscreenscale);
    s = sin(angle);
    c = cos(angle);

    // Left clip plane
    normal.v[0] = s;
    normal.v[1] = 0;
    normal.v[2] = c;
    SetWorldspaceClipPlane(&normal, &frustumplanes[0]);

    // Right clip plane
    normal.v[0] = -s;
    SetWorldspaceClipPlane(&normal, &frustumplanes[1]);

    angle = atan(2.0 / fieldofview * maxscale / yscreenscale);
    s = sin(angle);
    c = cos(angle);

    // Bottom clip plane
    normal.v[0] = 0;
    normal.v[1] = s;
    normal.v[2] = c;
    SetWorldspaceClipPlane(&normal, &frustumplanes[2]);

    // Top clip plane
    normal.v[1] = -s;
    SetWorldspaceClipPlane(&normal, &frustumplanes[3]);
}

/////////////////////////////////////////////////////////////////////
// Clip a polygon to a plane.
/////////////////////////////////////////////////////////////////////
int ClipToPlane(polygon_t *pin, plane_t *pplane, polygon_t *pout)
{
    int     i, j, nextvert, curin, nextin;
    double  curdot, nextdot, scale;
    point_t *pinvert, *poutvert;

    pinvert = pin->verts;
    poutvert = pout->verts;

    curdot = DotProduct(pinvert, &pplane->normal);
    curin = (curdot >= pplane->distance);

    for (i=0 ; i<pin->numverts ; i++)
    {
        nextvert = (i + 1) % pin->numverts;

        // Keep the current vertex if it's inside the plane
        if (curin)
            *poutvert++ = *pinvert;

        nextdot = DotProduct(&pin->verts[nextvert], &pplane->normal);
        nextin = (nextdot >= pplane->distance);

        // Add a clipped vertex if one end of the current edge is
        // inside the plane and the other is outside
        if (curin != nextin)
        {
            scale = (pplane->distance - curdot) /
                    (nextdot - curdot);
            for (j=0 ; j<3 ; j++)
            {
                poutvert->v[j] = pinvert->v[j] +
                    ((pin->verts[nextvert].v[j] - pinvert->v[j]) *
                     scale);
            }
            poutvert++;
        }

        curdot = nextdot;
        curin = nextin;
        pinvert++;
    }

    pout->numverts = poutvert - pout->verts;
    if (pout->numverts < 3)
        return 0;

    pout->color = pin->color;
    return 1;
}

/////////////////////////////////////////////////////////////////////
// Clip a polygon to the frustum.
/////////////////////////////////////////////////////////////////////
int ClipToFrustum(polygon_t *pin, polygon_t *pout)
{
    int         i, curpoly;
    polygon_t   tpoly[2], *ppoly;

    curpoly = 0;
    ppoly = pin;

    for (i=0 ; i<(NUM_FRUSTUM_PLANES-1); i++)
    {
        if (!ClipToPlane(ppoly,
                         &frustumplanes[i],
                         &tpoly[curpoly]))
        {
            return 0;
        }
        ppoly = &tpoly[curpoly];
        curpoly ^= 1;
    }

    return ClipToPlane(ppoly,
                       &frustumplanes[NUM_FRUSTUM_PLANES-1],
                       pout);
}

/////////////////////////////////////////////////////////////////////
// Render the current state of the world to the screen.
/////////////////////////////////////////////////////////////////////
void UpdateWorld()
{
	HPALETTE        holdpal;
    HDC             hdcScreen, hdcDIBSection;
    HBITMAP         holdbitmap;
    polygon2D_t     screenpoly;
    polygon_t       *ppoly, tpoly0, tpoly1, tpoly2;
    convexobject_t  *pobject;
    int             i, j, k;

    UpdateViewPos();
    memset(pDIBBase, 0, DIBWidth*DIBHeight);    // clear frame
    SetUpFrustum();
    ZSortObjects();

    // Draw all visible faces in all objects
    pobject = objecthead.pnext;

    while (pobject != &objecthead)
    {
        ppoly = pobject->ppoly;

        for (i=0 ; i<pobject->numpolys ; i++)
        {
            // Move the polygon relative to the object center
            tpoly0.color = ppoly->color;
            tpoly0.numverts = ppoly->numverts;
            for (j=0 ; j<tpoly0.numverts ; j++)
            {
                for (k=0 ; k<3 ; k++)
                    tpoly0.verts[j].v[k] = ppoly->verts[j].v[k] +
                            pobject->center.v[k];
            }

            if (PolyFacesViewer(&tpoly0))
            {
                if (ClipToFrustum(&tpoly0, &tpoly1))
                {
                    TransformPolygon (&tpoly1, &tpoly2);
                    ProjectPolygon (&tpoly2, &screenpoly);
                    FillPolygon2D (&screenpoly);
                }
            }

            ppoly++;
        }

        pobject = pobject->pnext;
    }

	// We've drawn the frame; copy it to the screen
	hdcScreen = GetDC(hwndOutput);
	holdpal = SelectPalette(hdcScreen, hpalDIB, FALSE);
	RealizePalette(hdcScreen);

    hdcDIBSection = CreateCompatibleDC(hdcScreen);
    holdbitmap = SelectObject(hdcDIBSection, hDIBSection);

    BitBlt(hdcScreen, 0, 0, DIBWidth, DIBHeight, hdcDIBSection,
           0, 0, SRCCOPY);

	SelectPalette(hdcScreen, holdpal, FALSE);
	ReleaseDC(hwndOutput, hdcScreen);
    SelectObject(hdcDIBSection, holdbitmap);
    DeleteDC(hdcDIBSection);
}
