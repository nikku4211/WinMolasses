#include <windows.h>
#include <windowsx.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include "main.h"
#include "models.h"

const char g_szClassName[] = "myWindowClass";

game_bitmap gBackBuffer;

performance_data gPerformanceData;
uint16_t cookedframecount = 0;

uint16_t scaled_game_width = game_res_width;
uint16_t scaled_game_height = game_res_height;
uint8_t scaled_game_multiplier = 1;
int border_thickness;

pixel32 bgPix = {0};
pixel32 debugBgPix = {0};

threedPoint cam;

short sin_lut[160];

//! Look-up a sine value
static inline short lu_sin(unsigned char theta)
{   return sin_lut[(theta)>>1];   }

//! Look-up a cosine value
static inline short lu_cos(unsigned char theta)
{   return sin_lut[((theta+64)>>1)]; }

twodPoint projected[8];
twodPoint oldProjected[8];

typedef struct threedAngleMat {
	unsigned char sx;
	unsigned char sy;
	unsigned char sz;
} threedAngleMat;

const short initCamX = pix_to_subpix(4);
const short initCamY = pix_to_subpix(4);
const short initCamZ = pix_to_subpix(-64);

const char initSX = 0;
const char initSY = 0;
const char initSZ = 0;

short matrixXX = 0;
short matrixXY = 0;
short matrixXZ = 0;

short matrixYX = 0;
short matrixYY = 0;
short matrixYZ = 0;

short matrixZX = 0;
short matrixZY = 0;
short matrixZZ = 0;

short matrixXX_XY = 0;
short matrixYX_YY = 0;
short matrixZX_ZY = 0;

short matrixX_m_Y = 0;
short matrixZ_XZ = 0;
short matrixZ_YZ = 0;
short matrixZ_ZZ = 0;

threedPoint matrixPoints[8];
threedAngleMat matrixSAngle;

HWND hwnd;
BOOL gGameRunning;

uint32_t time_last;
uint32_t time_accumulator = 0;
uint32_t timeStep = 16;

static inline int64_t GetTicks()
{
    LARGE_INTEGER ticks;

    if (!QueryPerformanceCounter(&ticks))
    {
        // FIXME: GetTickCount() has the 49.7 days limitation,
        // we could try to work around that somehow
        return GetTickCount();
    }

    return ticks.QuadPart;
}

uint32_t
gvm_platform_now_ms()
{
    static bool initial = true;
    static int64_t ts0;

    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency)) {
        frequency.QuadPart = 1000;
    }

    if (initial) {
        initial = false;
        ts0 = GetTicks();
    }

    int64_t ts = GetTicks();

    return 1000 * (ts - ts0) / frequency.QuadPart;
}

// Step 4: the Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
		FARPROC proc;
		WORD cmd;
    switch(msg)
    {
        case WM_CLOSE:
            DestroyWindow(hwnd);
        break;
        case WM_DESTROY:
						gGameRunning = FALSE;
            PostQuitMessage(0);
        break;
				case WM_COMMAND:
						cmd = LOWORD( wParam );
						switch( cmd ) {
							case MENU_QUIT:
								gGameRunning = FALSE;
								PostQuitMessage(0);
							break;
							case MENU_ABOUT:
								proc = MakeProcInstance( (FARPROC)WinAbout, inst_handle );
								DialogBox(NULL,"AboutBox", hwnd, (DLGPROC)WinAbout);
								FreeProcInstance( proc );
							break;
							case MENU_SCALE_BASE:
								scaled_game_multiplier = 1;
								scaled_game_width = game_res_width*scaled_game_multiplier*2;
								scaled_game_height = game_res_height*scaled_game_multiplier*4;
								SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, scaled_game_width + border_thickness, scaled_game_height + border_thickness, SWP_NOMOVE);
							break;
							case MENU_SCALE_2X:
								scaled_game_multiplier = 2;
								scaled_game_width = game_res_width*scaled_game_multiplier*2;
								scaled_game_height = game_res_height*scaled_game_multiplier*4;
								SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, scaled_game_width + border_thickness, scaled_game_height + border_thickness, SWP_NOMOVE);
							break;
							case MENU_SCALE_3X:
								scaled_game_multiplier = 3;
								scaled_game_width = game_res_width*scaled_game_multiplier*2;
								scaled_game_height = game_res_height*scaled_game_multiplier*4;
								SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, scaled_game_width + border_thickness, scaled_game_height + border_thickness, SWP_NOMOVE);
							break;
							case MENU_SCALE_4X:
								scaled_game_multiplier = 4;
								scaled_game_width = game_res_width*scaled_game_multiplier*2;
								scaled_game_height = game_res_height*scaled_game_multiplier*4;
								SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, scaled_game_width + border_thickness, scaled_game_height + border_thickness, SWP_NOMOVE);
							break;
							case MENU_FULLSCREEN:
							break;
						}
				break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

BOOL WINAPI WinAbout( HWND window_handle, UINT msg, WPARAM wparam, LPARAM)
{
    window_handle = window_handle;

    switch( msg ) {
    case WM_INITDIALOG:
        return( TRUE );
    case WM_COMMAND:
        if( LOWORD( wparam ) == IDOK ) {
            EndDialog( window_handle, TRUE );
            return( TRUE );
        }
        break;
    }
    return( FALSE );
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
	
		time_last = gvm_platform_now_ms();
	
    WNDCLASSEX wc;
    
    MSG Msg;

		border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);

    //Step 1: Registering the Window Class
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(GenericIcon));;
		wc.hIconSm       = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(GenericIcon), IMAGE_ICON, 16, 16, 0);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = MAKEINTRESOURCE(GenericMenu);
    wc.lpszClassName = g_szClassName;

    if(!RegisterClassEx(&wc))
    {
        MessageBox(NULL, "Window Registration Failed!", "Error!",
            MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Step 2: Creating the Window
    hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        g_szClassName,
        game_name,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_BORDER | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, game_res_width, game_res_height,
        NULL, NULL, hInstance, NULL);

    if(hwnd == NULL)
    {
        MessageBox(NULL, "Window creation failed!", "Error!",
            MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

		scaled_game_multiplier = 1;
		scaled_game_width = game_res_width*scaled_game_multiplier*2;
		scaled_game_height = game_res_height*scaled_game_multiplier*4;

		SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, scaled_game_width + border_thickness, scaled_game_height + border_thickness, SWP_NOMOVE);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
		
		gBackBuffer.bitmap_info.bmiHeader.biSize = sizeof(gBackBuffer.bitmap_info.bmiHeader);
		
		gBackBuffer.bitmap_info.bmiHeader.biWidth = game_res_width;
		gBackBuffer.bitmap_info.bmiHeader.biHeight = game_res_height+64;
		
		gBackBuffer.bitmap_info.bmiHeader.biBitCount = game_bpp;
		gBackBuffer.bitmap_info.bmiHeader.biCompression = BI_RGB;
		
		gBackBuffer.bitmap_info.bmiHeader.biPlanes = 1;
		
		gBackBuffer.memory = VirtualAlloc(NULL, game_drawing_area_mem_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		
		if (gBackBuffer.memory == NULL) {
			MessageBox(NULL, "Drawing area creation failed!", "Error!",
            MB_ICONEXCLAMATION | MB_OK);
			return(0);
		}
		
		initTrig();
		
		initThreed();
		
		// Step 3: The Message Loop
		gGameRunning = TRUE;
		
		while(gGameRunning == TRUE){
			
			while(PeekMessage(&Msg, hwnd, 0, 0, PM_REMOVE)){
				DispatchMessage(&Msg);	
			}
			
			//playerInput();
			
			uint32_t time_now = gvm_platform_now_ms();
			
			time_accumulator += (time_now - time_last);
			
			while (time_accumulator >= timeStep) {
				if(gPerformanceData.total_frames_rendered > 0 && \
				(gPerformanceData.total_frames_rendered % calculate_avg_fps_after_x_frames) == 0 && \
				time_accumulator > 0){
					gPerformanceData.raw_fps_average = 16.667f / ((float)time_accumulator / (float)calculate_avg_fps_after_x_frames);
				}
				playerInput();
				threedStuff();
				
                matrixSAngle.sy++;
				matrixSAngle.sz++;
				time_accumulator -= timeStep;
				cookedframecount++;
			}
      
			frameGraphics();
			
			gPerformanceData.total_frames_rendered++;
			time_last = time_now;
    }
    return Msg.wParam;
}

void initTrig(){
	for(int i=0; i<SIN_SIZE+32; i++)
	{
		sin_lut[i] = (short)(sin(i*2*M_PI/SIN_SIZE)*(1<<SIN_FP));
	}
}

void initThreed(){
	cam.x = initCamX;
	cam.y = initCamY;
	cam.z = initCamZ;
	matrixSAngle.sx = initSX;
	matrixSAngle.sy = initSY;
	matrixSAngle.sz = initSZ;
	for (unsigned short i = 0; i < 8; i++){
		oldProjected[i].x = 0;
		oldProjected[i].y = 0;
	}
}

void threedStuff(){
	matrixXX = subpix_to_pix(lu_cos(matrixSAngle.sz) * lu_cos(matrixSAngle.sy));
    matrixXY = subpix_to_pix(lu_sin(matrixSAngle.sz) * lu_cos(matrixSAngle.sy));
    matrixXZ = subpix_to_pix(lu_sin(matrixSAngle.sy));
    
    matrixYX = subpix_to_pix((lu_sin(matrixSAngle.sz) * lu_cos(matrixSAngle.sx)) + (lu_cos(matrixSAngle.sz) * lu_sin(matrixSAngle.sy) + lu_sin(matrixSAngle.sx)));
    matrixYY = subpix_to_pix((-lu_cos(matrixSAngle.sz) * lu_cos(matrixSAngle.sx)) + (lu_sin(matrixSAngle.sz) * lu_sin(matrixSAngle.sy) + lu_sin(matrixSAngle.sx)));
    matrixYZ = subpix_to_pix(-lu_cos(matrixSAngle.sy) * lu_sin(matrixSAngle.sx));
    
    matrixZX = subpix_to_pix((lu_sin(matrixSAngle.sz) * lu_sin(matrixSAngle.sx)) - (lu_cos(matrixSAngle.sz) * lu_sin(matrixSAngle.sy) + lu_cos(matrixSAngle.sx)));
    matrixZY = subpix_to_pix((-lu_cos(matrixSAngle.sz) * lu_cos(matrixSAngle.sx)) - (lu_sin(matrixSAngle.sz) * lu_sin(matrixSAngle.sy) + lu_sin(matrixSAngle.sx)));
    matrixZZ = subpix_to_pix(lu_cos(matrixSAngle.sy) * lu_cos(matrixSAngle.sx));
    
    matrixXX_XY = matrixXX*matrixXY;
    matrixYX_YY = matrixYX*matrixYY;
    matrixZX_ZY = matrixZX*matrixZY;
	
	for (unsigned short i = 0; i < 8; i++){
        matrixX_m_Y = cube.x[i]*cube.y[i];
        
        threedPoint matrixPrime[8];
        
        matrixPoints[i].x = subpix_to_pix(cube.x[i] + (cube.x[i] * matrixXX + cube.y[i] * matrixXY + cube.z[i] * matrixXZ));
        matrixPoints[i].y = subpix_to_pix(cube.y[i] + (cube.x[i] * matrixYX + cube.y[i] * matrixYY + cube.z[i] * matrixYZ));
        matrixPoints[i].z = subpix_to_pix(cube.z[i] + (cube.x[i] * matrixZX + cube.y[i] * matrixZY + cube.z[i] * matrixZZ));
	}
	
	for (unsigned short i = 0; i < 8; i++){
		if (subpix_to_pix(matrixPoints[i].z - cam.z) > 0){
				projected[i].x = (((matrixPoints[i].x) - cam.x) / subpix_to_pix(matrixPoints[i].z - cam.z)) + 64;
				projected[i].y = ((((matrixPoints[i].y) - cam.y) / subpix_to_pix(matrixPoints[i].z - cam.z)) + 56) >> 1;
		}
	}
}

void playerInput() {
	int16_t leftKeyDown = GetAsyncKeyState(VK_LEFT);
	int16_t rightKeyDown = GetAsyncKeyState(VK_RIGHT);
	int16_t forwardKeyDown = GetAsyncKeyState(VK_UP);
	int16_t backwardKeyDown = GetAsyncKeyState(VK_DOWN);
	int16_t aboveKeyDown = GetAsyncKeyState(0x51);
	int16_t belowKeyDown = GetAsyncKeyState(0x57);
	
	int16_t debugKeyDown = GetAsyncKeyState(VK_F12);
	static int16_t debugKeyWasDown;
	if (debugKeyDown && !debugKeyWasDown) {
		gPerformanceData.display_debug = !gPerformanceData.display_debug;
	}
	
	if (leftKeyDown) {
		cam.x -= 16;
	} else if (rightKeyDown) {
		cam.x += 16;
	}
	if (aboveKeyDown) {
		cam.y -= 16;
	} else if (belowKeyDown) {
		cam.y += 16;
	}
	if (backwardKeyDown) {
		cam.z -= 16;
	} else if (forwardKeyDown) {
		cam.z += 16;
	}
	debugKeyWasDown = debugKeyDown;
}

void frameGraphics() {
	HDC	DeviceContext;
	
	char debugTextBuffer[64] = {0};
	
	int32_t StartingScreenPixel;
	
	StartingScreenPixel = ((game_res_width*game_res_height) - game_res_width);
	
	for (unsigned short i = 0; i < 8; i++){
		if (oldProjected[i].x >= 0 && oldProjected[i].x < 128 && \
				oldProjected[i].x >= 0 && oldProjected[i].y < 56)
			memset(&((pixel32*)gBackBuffer.memory)[StartingScreenPixel + oldProjected[i].x - (oldProjected[i].y*game_res_width)],0x00,4);
		if (projected[i].x >= 0 && projected[i].x < 128 && \
				projected[i].x >= 0 && projected[i].y < 56)
			memset(&((pixel32*)gBackBuffer.memory)[StartingScreenPixel + projected[i].x - (projected[i].y*game_res_width)],0xff,4);
		oldProjected[i].x = projected[i].x;
		oldProjected[i].y = projected[i].y;
	}
	
	DeviceContext = GetDC(hwnd);
	
	StretchDIBits(DeviceContext, 
								0, 
								0, 
								scaled_game_width, 
								scaled_game_height, 
								0, 
								0, 
								game_res_width, 
								game_res_height, 
								gBackBuffer.memory, 
								&gBackBuffer.bitmap_info, 
								DIB_RGB_COLORS, 
								SRCCOPY);
	
	if (gPerformanceData.display_debug){
		SelectObject(DeviceContext, (HFONT)GetStockObject(OEM_FIXED_FONT));
		
		snprintf(debugTextBuffer, sizeof(debugTextBuffer), "Milliseconds accumulated: %d", time_accumulator);
		
		TextOutA(DeviceContext, 0, 0, debugTextBuffer, (int)strlen(debugTextBuffer));
		
		snprintf(debugTextBuffer, sizeof(debugTextBuffer), "FPS Raw: %.2f", gPerformanceData.raw_fps_average);
		
		TextOutA(DeviceContext, 0, 16, debugTextBuffer, (int)strlen(debugTextBuffer));
		
		snprintf(debugTextBuffer, sizeof(debugTextBuffer), "Matrix Point 0 X: %d", matrixPoints[0].x);
		
		TextOutA(DeviceContext, 0, 32, debugTextBuffer, (int)strlen(debugTextBuffer));
		
		snprintf(debugTextBuffer, sizeof(debugTextBuffer), "Cam Z: %d", cam.z);
		
		TextOutA(DeviceContext, 0, 48, debugTextBuffer, (int)strlen(debugTextBuffer));
	}
	
	ReleaseDC(hwnd, DeviceContext);
}