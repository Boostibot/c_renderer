#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>
#include <stdbool.h>

#include <gl/glext.h>
#include <gl/wglext.h>
//#include <gl/glcorearb.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")

//X-macro to specify all extension functions along with their type
//See https://en.wikipedia.org/wiki/X_macro for info about this pattern
#define GL_EXTENSIONS_LIST() \
	GL_EXT(PFNWGLGETEXTENSIONSSTRINGARBPROC, wglGetExtensionsStringARB) \
	GL_EXT(PFNWGLCREATECONTEXTATTRIBSARBPROC, wglCreateContextAttribsARB) \
	GL_EXT(PFNWGLSWAPINTERVALEXTPROC, wglSwapIntervalEXT) \
	GL_EXT(PFNWGLCHOOSEPIXELFORMATARBPROC, wglChoosePixelFormatARB) \
	GL_EXT(PFNGLCREATESHADERPROC, glCreateShader) \
	GL_EXT(PFNGLSHADERSOURCEPROC, glShaderSource) \
	GL_EXT(PFNGLCOMPILESHADERPROC, glCompileShader) \
	GL_EXT(PFNGLGETSHADERIVPROC, glGetShaderiv) \
	GL_EXT(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
	GL_EXT(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
	GL_EXT(PFNGLATTACHSHADERPROC, glAttachShader) \
	GL_EXT(PFNGLLINKPROGRAMPROC, glLinkProgram) \
	GL_EXT(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
	GL_EXT(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
	GL_EXT(PFNGLDETACHSHADERPROC, glDetachShader) \
	GL_EXT(PFNGLDELETESHADERPROC, glDeleteShader) \
	GL_EXT(PFNGLUSEPROGRAMPROC, glUseProgram) \
	GL_EXT(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
	GL_EXT(PFNGLGENBUFFERSPROC, glGenBuffers) \
	GL_EXT(PFNGLBINDBUFFERPROC, glBindBuffer) \
	GL_EXT(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
	GL_EXT(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
	GL_EXT(PFNGLBUFFERDATAPROC, glBufferData) \
	GL_EXT(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
	GL_EXT(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback) \
	GL_EXT(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
	GL_EXT(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) \
	GL_EXT(PFNGLUNIFORM4FVPROC, glUniform4fv) \
	GL_EXT(PFNGLUNIFORM1IPROC, glUniform1i) \
	GL_EXT(PFNGLUNIFORM1FPROC, glUniform1f) \
	GL_EXT(PFNGLUNIFORM3FVPROC, glUniform3fv) \
	GL_EXT(PFNGLMAPBUFFERPROC, glMapBuffer) \
	GL_EXT(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) \

//user exposed stuff
typedef struct {
	int Width, Height;
	int MouseX, MouseY;
	float DeltaTime;
	int MouseDownLeft;
	int MouseDownMiddle;
	int MouseDownRight;
	float ScrollDelta;
	int is_Running;
} Controls;

//potentially hidden\opaque type for windows specific stuff
typedef struct {
	Controls controls;

	HWND handle;
	WNDCLASSA window_class;
	
	HDC device_context;
	HGLRC render_context;
	
	DWORD pixel_format_flags;
	PIXELFORMATDESCRIPTOR pixel_format;
} Window;


//create a list of static function pointers using the X-macro 
#define GL_EXT(type, name) static type name;
GL_EXTENSIONS_LIST()
#undef GL_EXT

static void gl_debug_callback(GLenum Source, GLenum Type, GLuint Id, GLenum Severity, GLsizei Length, const GLchar *Message, const void *UserParam);
static void debug_print(const char* format, ...);
static void error_print(const char* msg, const char* file, int line);

LRESULT Wndproc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
Window* create_window(const char* window_title, int window_width, int window_height)
{
	bool has_error = false;
	char class_name[] = "opengl_window_class";
	HINSTANCE instance = GetModuleHandleA(0);
	WNDCLASSA window_class = {
		CS_OWNDC,	// UINT      style;
		Wndproc,	// WNDPROC   lpfnWndProc;
		0,			// int       cbClsExtra;
		0,			// int       cbWndExtra;
		instance,	// HINSTANCE hInstance;
		0,			// HICON     hIcon;
		LoadCursor(0, IDC_ARROW), // HCURSOR   hCursor;
		0,			// HBRUSH    hbrBackground;
		0,			// LPCSTR    lpszMenuName;
		class_name	// LPCSTR    lpszClassName;
	};
	
	RegisterClassA(&window_class);
	
	//We allocate window before doing anything so that we can save
	// the pointer to later query it from within Wndproc.
	//We also use this struct only at the very end because then
	// all the used types are visible which increases copy-paste-ability <3
	Window* window_struct_ptr = calloc(1, sizeof *window_struct_ptr);
	if(window_struct_ptr == NULL)
	{
		error_print("window allocation failed", __FILE__, __LINE__);
		return NULL;
	}

	//DWORD window_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	DWORD window_style = WS_OVERLAPPEDWINDOW;
	HWND dummy_window = CreateWindowExA(
		WS_EX_CLIENTEDGE,	// DWORD dwExStyle
		class_name,			// LPCTSTR lpClassName
		window_title,		// LPCTSTR lpWindowName
		window_style,		// DWORD dwStyle
		CW_USEDEFAULT,		// int x
		CW_USEDEFAULT,		// int y
		window_width,		// int nWidth
		window_height,		// int nHeight
		0,					// HWND hWndParent
		0,					// HMENU hMenu
		instance,			// HINSTANCE hInstance
		window_struct_ptr 	// LPVOID lpParam
	);

	HDC dummy_device_context = GetDC(dummy_window);
	
	DWORD pixel_format_flags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	PIXELFORMATDESCRIPTOR PixelFormat = {
		sizeof(PIXELFORMATDESCRIPTOR),	// WORD  nSize;
		1,								// WORD  nVersion;
		pixel_format_flags,				// DWORD dwFlags;
		PFD_TYPE_RGBA,					// BYTE  iPixelType;
		32,								// BYTE  cColorBits;
		0,								// BYTE  cRedBits;
		0,								// BYTE  cRedShift;
		0,								// BYTE  cGreenBits;
		0,								// BYTE  cGreenShift;
		0,								// BYTE  cBlueBits;
		0,								// BYTE  cBlueShift;
		0,								// BYTE  cAlphaBits;
		0,								// BYTE  cAlphaShift;
		0,								// BYTE  cAccumBits;
		0,								// BYTE  cAccumRedBits;
		0,								// BYTE  cAccumGreenBits;
		0,								// BYTE  cAccumBlueBits;
		0,								// BYTE  cAccumAlphaBits;
		24,								// BYTE  cDepthBits;
		8,								// BYTE  cStencilBits;
		0,								// BYTE  cAuxBuffers;
		PFD_MAIN_PLANE,					// BYTE  iLayerType;
		0,								// BYTE  bReserved;
		0,								// DWORD dwLayerMask;
		0,								// DWORD dwVisibleMask;
		0								// DWORD dwDamageMask;
	};
	
	//Chose and set the right pixel format
	INT pixel_format_index = ChoosePixelFormat(dummy_device_context, &PixelFormat);
	bool set_pixel_format_result = SetPixelFormat(dummy_device_context, pixel_format_index, &PixelFormat);
	HGLRC dummy_render_contex = wglCreateContext(dummy_device_context);
	bool use_render_context_result = wglMakeCurrent(dummy_device_context, dummy_render_contex);

	if(set_pixel_format_result == false || use_render_context_result == false)
	{
		//sorry not sorry for using goto. 
		//if someone shows me better (less code) way of doing
		// this ill use it
		debug_print("ChoosePixelFormat/SetPixelFormat/wglCreateContext/wglMakeCurrent failed!\n");
		has_error = true;
		goto dummy_deninit; 
	}

	// Load all desired extensions using the X-macro
	// https://www.opengl.org/archives/resources/features/OGLextensions/
	#define GL_EXT(Type, name)										\
		name = (Type) wglGetProcAddress(#name);						\
		if(name == NULL)											\
		{															\
			debug_print("failed to load extension: %s\n", #name);	\
			return NULL;											\
		}															\

	GL_EXTENSIONS_LIST()
	#undef GL_EXT

	dummy_deninit:
	wglMakeCurrent(0, 0);
	wglDeleteContext(dummy_render_contex);
	ReleaseDC(dummy_window, dummy_device_context);
	DestroyWindow(dummy_window);

	if(has_error)
	{
		free(window_struct_ptr);
		return NULL;
	}

	HWND window = CreateWindowExA(
		WS_EX_CLIENTEDGE,	// DWORD dwExStyle
		class_name,			// LPCTSTR lpClassName
		window_title,		// LPCTSTR lpWindowName
		window_style | WS_VISIBLE,	// DWORD dwStyle
		CW_USEDEFAULT,	// int x
		CW_USEDEFAULT,	// int y
		window_width,	// int nWidth
		window_height,	// int nHeight
		0,				// HWND hWndParent
		0,				// HMENU hMenu
		instance,		// HINSTANCE hInstance
		window_struct_ptr 	// LPVOID lpParam
	);
	HDC device_context = GetDC(window);

	UINT numFormats = 0;
	int pixel_format_attribs[] = {
		WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
		WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
		WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
		WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
		WGL_COLOR_BITS_ARB, 32,
		WGL_DEPTH_BITS_ARB, 24,
		WGL_STENCIL_BITS_ARB, 8,
		WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
		WGL_SAMPLES_ARB, 8,
		//WGL_COLORSPACE_EXT, WGL_COLORSPACE_SRGB_EXT,
		0
	};
	bool choose_format_result = wglChoosePixelFormatARB(device_context, pixel_format_attribs, 0, 1, &pixel_format_index, &numFormats);

	debug_print("FormatIndex: %d\n", pixel_format_index);

	if(choose_format_result == false) {
		error_print("wglChoosePixelFormatARB: FALSE", __FILE__, __LINE__); 
		return NULL;
	}

	PIXELFORMATDESCRIPTOR pixel_format = {
		sizeof(PIXELFORMATDESCRIPTOR),	// WORD  nSize;
		1,								// WORD  nVersion;
		pixel_format_flags,				// DWORD dwFlags;
		PFD_TYPE_RGBA,					// BYTE  iPixelType;
		32,								// BYTE  cColorBits;
		0,								// BYTE  cRedBits;
		0,								// BYTE  cRedShift;
		0,								// BYTE  cGreenBits;
		0,								// BYTE  cGreenShift;
		0,								// BYTE  cBlueBits;
		0,								// BYTE  cBlueShift;
		0,								// BYTE  cAlphaBits;
		0,								// BYTE  cAlphaShift;
		0,								// BYTE  cAccumBits;
		0,								// BYTE  cAccumRedBits;
		0,								// BYTE  cAccumGreenBits;
		0,								// BYTE  cAccumBlueBits;
		0,								// BYTE  cAccumAlphaBits;
		24,								// BYTE  cDepthBits;
		8,								// BYTE  cStencilBits;
		0,								// BYTE  cAuxBuffers;
		PFD_MAIN_PLANE,					// BYTE  iLayerType;
		0,								// BYTE  bReserved;
		0,								// DWORD dwLayerMask;
		0,								// DWORD dwVisibleMask;
		0								// DWORD dwDamageMask;
	};
	bool set_pixel_format_state = SetPixelFormat(device_context, pixel_format_index, &pixel_format);
	int render_context_attribs[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
		WGL_CONTEXT_MINOR_VERSION_ARB, 0,
		//WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
		0
	};
	HGLRC render_context = wglCreateContextAttribsARB(device_context, 0, render_context_attribs);
	
	BOOL make_current_result = wglMakeCurrent(device_context, render_context);
	
	if(make_current_result == false || set_pixel_format_state == false)
	{
		debug_print("SetPixelFormat/wglCreateContextAttribsARB/wglMakeCurrent failed!\n");
		has_error = true;
		goto deninit; 
	}

	debug_print(wglGetExtensionsStringARB(device_context));
	debug_print((char*) glGetString(GL_VERSION));

	wglSwapIntervalEXT(1);
	
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(gl_debug_callback, 0);

	//save the needed info into the window ptr
	//Feel free to add if you need something more
	window_struct_ptr->handle = window;
	window_struct_ptr->window_class = window_class;
	window_struct_ptr->device_context = device_context;
	window_struct_ptr->render_context = render_context;
	window_struct_ptr->pixel_format_flags = pixel_format_flags;
	window_struct_ptr->pixel_format = pixel_format;

	window_struct_ptr->controls.Width = window_width;
	window_struct_ptr->controls.Height = window_height;

	return window_struct_ptr;
	
	deninit:
	wglMakeCurrent(0, 0);
	wglDeleteContext(render_context);
	ReleaseDC(window, device_context);
	DestroyWindow(window);
	free(window_struct_ptr);

	return NULL;
}

void destroy_window(Window* window)
{
	if(!window)
		return;
	
	wglMakeCurrent(0, 0);
	wglDeleteContext(window->render_context);
	ReleaseDC(window->handle, window->device_context);
	DestroyWindow(window->handle);
	free(window);
}

LRESULT Wndproc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
    if (msg == WM_NCCREATE) 
    {
        CREATESTRUCTW* create_struct = (CREATESTRUCTW*) l_param;
        void* user_data = create_struct->lpCreateParams;
        // store window instance pointer in window user data
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR) user_data);
    }
    Window* window = (Window*) GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!window) 
        return DefWindowProcW(hwnd, msg, w_param, l_param);

	switch(msg) {
		case WM_CLOSE:
		PostQuitMessage(0);
		window->controls.is_Running = FALSE;
		break;
		case WM_SIZE:
		window->controls.Width = LOWORD(l_param);
		window->controls.Height = HIWORD(l_param);
		break;
		case WM_MOUSEMOVE:
		window->controls.MouseX = GET_X_LPARAM(l_param);
		window->controls.MouseY = GET_Y_LPARAM(l_param);
		break;
		case WM_LBUTTONDOWN:
		window->controls.MouseDownLeft = 1;
		break;
		case WM_LBUTTONUP:
		window->controls.MouseDownLeft = 0;
		break;
		case WM_MBUTTONDOWN:
		window->controls.MouseDownMiddle = 1;
		break;
		case WM_MBUTTONUP:
		window->controls.MouseDownMiddle = 0;
		break;
		case WM_RBUTTONDOWN:
		window->controls.MouseDownRight = 1;
		break;
		case WM_RBUTTONUP:
		window->controls.MouseDownRight = 0;
		break;
		case WM_MOUSEWHEEL:
		window->controls.ScrollDelta -= GET_WHEEL_DELTA_WPARAM(w_param) / 120.0f;
		break;
		default:
		break;
	}
	return DefWindowProcA(hwnd, msg, w_param, l_param);
}


static void gl_debug_callback(GLenum Source, GLenum Type, GLuint Id, GLenum Severity, GLsizei Length, const GLchar *Message, const void *UserParam) 
{
	OutputDebugStringA(Message);
	OutputDebugStringA("\n");
}

static void debug_print(const char* format, ...)
{
	//#define NO_CONSOLE
	
    va_list args;
    va_start(args, format);

	#ifdef NO_CONSOLE
	char buffer[1024] = {0};

	vsnprintf(buffer, sizeof buffer, format, args);
	OutputDebugStringA(buffer);

	OutputDebugStringA(buffer);
	#else
	vprintf(format, args);	
	#endif

    va_end(args);
}

static void error_print(const char* msg, const char* file, int line)
{
	debug_print("[ERROR]: %s (%s : %d)\n", msg, file, line);
}

int main()
{
	Window* window = create_window("window title", 500, 600);
	if(window == NULL)
		return false;
	
	LARGE_INTEGER Freq;
	LARGE_INTEGER LastTime;
	LARGE_INTEGER Time;
	QueryPerformanceFrequency(&Freq);
	QueryPerformanceCounter(&LastTime);

	debug_print("window is %d x %d\n", window->controls.Width, window->controls.Height);

	glViewport(0, 0, window->controls.Width, window->controls.Height);
	MSG Message;
	bool Running = true;

	bool first_run = true;
	while(Running) 
	{
		while(PeekMessageA(&Message, window->handle, 0, 0, PM_REMOVE)) {
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}

		QueryPerformanceCounter(&Time);
		float DeltaTime = (Time.QuadPart - LastTime.QuadPart)/(float)Freq.QuadPart;
		LastTime = Time;

		
		debug_print("mouse %-5d %-5d delta %-6f \n", window->controls.MouseX, window->controls.MouseY, DeltaTime);

		//char Buf[128] = {0};
		//sprintf(Buf, "DeltaTime: %f\n", Context.DeltaTime);
		//OutputDebugString(Buf);
		//Draw(&Context);
		
		//Context.ScrollDelta = 0.0f;
		
		#if 0
		GLenum Error = glGetError();
		if(Error != GL_NO_ERROR)
			OutputDebugString("Error");
		#endif

		SwapBuffers(window->device_context);

		first_run = false;
	}
	
	destroy_window(window);
	return 0;
}