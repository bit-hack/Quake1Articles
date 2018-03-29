/* maze.h

   Header file for maze.c. */

#define MAX_INT		       0x7FFFFFFF

#define IDM_EXIT           106

#define CURRENT_VERSION		1		// version of program

BOOL InitApplication(HANDLE);
BOOL InitInstance(HANDLE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
