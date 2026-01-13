#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <dwmapi.h>
#include "resource.h"

#pragma comment(lib, "dwmapi.lib")

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")

// Constantes
#define ID_RICHEDIT 1001

// Variables globales
HWND hRichEdit;
HINSTANCE hInst;
HACCEL hAccelTable; // Tabla de aceleradores
BOOL bWordWrap = TRUE; // Forzamos el ajuste de línea
int zoomFactor = 100; // Porcentaje de zoom
HWND hFindDlg = NULL;
HWND hReplaceDlg = NULL;
TCHAR currentFile[MAX_PATH] = {0}; // Nombre del archivo actual

// Estructuras para búsqueda
typedef struct {
    FINDREPLACE fr;
    TCHAR szFindWhat[256];
    TCHAR szReplaceWith[256];
} FINDREPLACEEX, *PFINDREPLACEEX;

// Prototipos
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK FindDlgProc(HWND, UINT, WPARAM, LPARAM);
void ApplyDarkMode(HWND hwnd);
void SetEditorColors();
void SetWordWrap(BOOL wrap);
void SetZoom(int factor);
BOOL LoadFile(HWND hwnd, LPCTSTR pszFileName);
BOOL SaveFile(HWND hwnd, LPCTSTR pszFileName);

// --- Implementación de Búsqueda/Reemplazo ---
// Estructura para mantener el estado de la búsqueda
typedef struct {
    wchar_t szFind[256];
    wchar_t szReplace[256];
    DWORD dwFlags;
    BOOL bWrapAround; // Para controlar si ya se ha dado la vuelta
} SEARCH_STATE;

// Función auxiliar para realizar una búsqueda. Devuelve TRUE si se encuentra, FALSE si no.
static BOOL PerformFind(HWND hDlg, SEARCH_STATE* pState)
{
    // Obtener la selección actual para saber desde dónde empezar
    CHARRANGE cr;
    SendMessage(hRichEdit, EM_EXGETSEL, 0, (LPARAM)&cr);

    FINDTEXTEXW ftex;
    ftex.lpstrText = pState->szFind;

    // Configurar el rango de búsqueda basado en la dirección
    if (pState->dwFlags & FR_DOWN) {
        ftex.chrg.cpMin = cr.cpMax;
        ftex.chrg.cpMax = -1; // Hasta el final del texto
    } else { // Búsqueda hacia atrás
        ftex.chrg.cpMin = cr.cpMin;
        ftex.chrg.cpMax = 0;    // Hasta el principio del texto
    }

    // Realizar la búsqueda
    if (SendMessageW(hRichEdit, EM_FINDTEXTEXW, pState->dwFlags, (LPARAM)&ftex) != -1) {
        // Encontrado. Seleccionar el texto y enfocar.
        SendMessage(hRichEdit, EM_EXSETSEL, 0, (LPARAM)&ftex.chrgText);
        SendMessage(hRichEdit, EM_SCROLLCARET, 0, 0);
        SetFocus(hRichEdit);
        pState->bWrapAround = FALSE; // Reiniciar el contador de vuelta
        return TRUE;
    }
    
    // Si no se encontró y no hemos dado la vuelta, intentarlo desde el otro extremo
    if (!pState->bWrapAround) {
        pState->bWrapAround = TRUE;
        MessageBoxW(hDlg, L"Se llego al final del documento. Continuando desde el principio.", L"Buscar", MB_OK | MB_ICONINFORMATION);
        
        // Mover el cursor al extremo opuesto
        if (pState->dwFlags & FR_DOWN) {
            cr.cpMin = cr.cpMax = 0;
        } else {
            cr.cpMin = cr.cpMax = -1; // EM_SETSEL lo interpreta como el final
        }
        SendMessage(hRichEdit, EM_EXSETSEL, 0, (LPARAM)&cr);
        
        // Volver a llamar a la función para buscar desde el nuevo punto
        return PerformFind(hDlg, pState);
    }

    // Si llegamos aquí, no se encontró nada en todo el documento
    MessageBoxW(hDlg, L"Texto no encontrado.", L"Buscar", MB_OK | MB_ICONINFORMATION);
    pState->bWrapAround = FALSE; // Reiniciar para la próxima búsqueda
    return FALSE;
}

// Diálogo de búsqueda y reemplazo
INT_PTR CALLBACK FindDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static SEARCH_STATE searchState = {0}; // Mantiene el estado entre llamadas

    switch (message)
    {
        case WM_INITDIALOG:
            // Restaurar texto de la última búsqueda
            SetDlgItemTextW(hDlg, edt1, searchState.szFind);
            SetDlgItemTextW(hDlg, edt2, searchState.szReplace);
            searchState.bWrapAround = FALSE;
            return (INT_PTR)TRUE;

        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDOK: // Buscar siguiente
                    // Leer el estado de los controles en cada comando
                    GetDlgItemTextW(hDlg, edt1, searchState.szFind, 256);
                    GetDlgItemTextW(hDlg, edt2, searchState.szReplace, 256);

                    // Actualizar flags de búsqueda
                    searchState.dwFlags = 0;
                    if (IsDlgButtonChecked(hDlg, IDC_CHK_MATCHCASE) == BST_CHECKED) searchState.dwFlags |= FR_MATCHCASE;
                    if (IsDlgButtonChecked(hDlg, IDC_CHK_WHOLEWORDS) == BST_CHECKED) searchState.dwFlags |= FR_WHOLEWORD;
                    if (IsDlgButtonChecked(hDlg, IDC_CHK_BACKWARD) != BST_CHECKED) searchState.dwFlags |= FR_DOWN;

                    if (searchState.szFind[0] == L'\0') {
                        MessageBoxW(hDlg, L"Ingrese texto para buscar.", L"Buscar", MB_OK | MB_ICONINFORMATION);
                        break;
                    }
                    searchState.bWrapAround = FALSE;
                    PerformFind(hDlg, &searchState);
                    return (INT_PTR)TRUE;

                case IDOK_REPL: // Reemplazar
                {
                    // Leer el estado de los controles en cada comando
                    GetDlgItemTextW(hDlg, edt1, searchState.szFind, 256);
                    GetDlgItemTextW(hDlg, edt2, searchState.szReplace, 256);

                    // Actualizar flags de búsqueda
                    searchState.dwFlags = 0;
                    if (IsDlgButtonChecked(hDlg, IDC_CHK_MATCHCASE) == BST_CHECKED) searchState.dwFlags |= FR_MATCHCASE;
                    if (IsDlgButtonChecked(hDlg, IDC_CHK_WHOLEWORDS) == BST_CHECKED) searchState.dwFlags |= FR_WHOLEWORD;
                    if (IsDlgButtonChecked(hDlg, IDC_CHK_BACKWARD) != BST_CHECKED) searchState.dwFlags |= FR_DOWN;

                    if (searchState.szFind[0] == L'\0') {
                        MessageBoxW(hDlg, L"Ingrese texto para buscar.", L"Reemplazar", MB_OK | MB_ICONINFORMATION);
                        break;
                    }

                    CHARRANGE cr;
                    SendMessage(hRichEdit, EM_EXGETSEL, 0, (LPARAM)&cr);

                    if (cr.cpMin != cr.cpMax) {
                        // Hay una selección, verificar si coincide con el texto a buscar
                        size_t selLen = cr.cpMax - cr.cpMin;
                        wchar_t* pSelectedText = (wchar_t*)malloc((selLen + 1) * sizeof(wchar_t));
                        if (pSelectedText) {
                            TEXTRANGEW tr = {{cr.cpMin, cr.cpMax}, pSelectedText};
                            SendMessageW(hRichEdit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
                            
                            int compareResult;
                            if (searchState.dwFlags & FR_MATCHCASE) {
                                compareResult = wcscmp(searchState.szFind, pSelectedText);
                            } else {
                                compareResult = _wcsicmp(searchState.szFind, pSelectedText);
                            }

                            if (compareResult == 0) {
                                SendMessageW(hRichEdit, EM_REPLACESEL, TRUE, (LPARAM)searchState.szReplace);
                            }
                            free(pSelectedText);
                        }
                    }
                    
                    // Buscar la siguiente ocurrencia
                    searchState.bWrapAround = FALSE;
                    PerformFind(hDlg, &searchState);
                    return (INT_PTR)TRUE;
                }

                case IDOK_REPL_ALL: // Reemplazar todo
                {
                    // Leer el estado de los controles en cada comando
                    GetDlgItemTextW(hDlg, edt1, searchState.szFind, 256);
                    GetDlgItemTextW(hDlg, edt2, searchState.szReplace, 256);

                    // Actualizar flags de búsqueda
                    searchState.dwFlags = 0;
                    if (IsDlgButtonChecked(hDlg, IDC_CHK_MATCHCASE) == BST_CHECKED) searchState.dwFlags |= FR_MATCHCASE;
                    if (IsDlgButtonChecked(hDlg, IDC_CHK_WHOLEWORDS) == BST_CHECKED) searchState.dwFlags |= FR_WHOLEWORD;
                    if (IsDlgButtonChecked(hDlg, IDC_CHK_BACKWARD) != BST_CHECKED) searchState.dwFlags |= FR_DOWN;

                    if (searchState.szFind[0] == L'\0') {
                        MessageBoxW(hDlg, L"Ingrese texto para buscar.", L"Reemplazar todo", MB_OK | MB_ICONINFORMATION);
                        break;
                    }
                    
                    int count = 0;
                    
                    // Ocultar selección y mover al inicio
                    SendMessage(hRichEdit, EM_HIDESELECTION, TRUE, FALSE);
                    CHARRANGE cr_start = {0, 0};
                    SendMessage(hRichEdit, EM_EXSETSEL, 0, (LPARAM)&cr_start);
                    
                    // Forzar búsqueda hacia adelante para reemplazar todo
                    searchState.dwFlags |= FR_DOWN;
                    searchState.bWrapAround = TRUE; // Para buscar en todo el documento sin preguntar

                    while (PerformFind(hDlg, &searchState)) {
                        SendMessageW(hRichEdit, EM_REPLACESEL, TRUE, (LPARAM)searchState.szReplace);
                        count++;
                    }

                    SendMessage(hRichEdit, EM_HIDESELECTION, FALSE, FALSE);

                    wchar_t msg[100];
                    wsprintfW(msg, L"Se realizaron %d reemplazos.", count);
                    MessageBoxW(hDlg, msg, L"Reemplazar todo", MB_OK | MB_ICONINFORMATION);
                    return (INT_PTR)TRUE;
                }

                case IDC_CHK_MATCHCASE:
                case IDC_CHK_WHOLEWORDS:
                case IDC_CHK_BACKWARD:
                    // Manual Toggle Logic
                    UINT state = IsDlgButtonChecked(hDlg, LOWORD(wParam));
                    CheckDlgButton(hDlg, LOWORD(wParam), state == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED);
                    return (INT_PTR)TRUE;

                case IDCANCEL:
                    DestroyWindow(hDlg);
                    return (INT_PTR)TRUE;

                default: // Add this default case to handle checkbox clicks
                    return (INT_PTR)FALSE;
            }
        }

        case WM_CLOSE:
            DestroyWindow(hDlg);
            return (INT_PTR)TRUE;

        case WM_DESTROY:
            hFindDlg = NULL;
            break;
    }
    return (INT_PTR)FALSE;
}
// --- Fin de Búsqueda/Reemplazo ---

// Función principal
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    hInst = hInstance;

    // Inicializar Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
    
    // Registrar clase de ventana
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = TEXT("MicronotesClass");
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    
    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, TEXT("Error al registrar la clase de ventana"), TEXT("Error"), MB_OK | MB_ICONERROR);
        return FALSE;
    }
    
    // Crear ventana principal
    HWND hwnd = CreateWindow(
        TEXT("MicronotesClass"),
        TEXT("Micronotes - Editor de Texto"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd)
    {
        MessageBox(NULL, TEXT("Error al crear la ventana principal"), TEXT("Error"), MB_OK | MB_ICONERROR);
        return FALSE;
    }
    
    // Habilitar modo oscuro en la barra de título
    ApplyDarkMode(hwnd);
    
    // Cargar Msftedit.dll antes de crear el control RichEdit
    HMODULE hRichEditModule = LoadLibrary(TEXT("Msftedit.dll"));
    if (!hRichEditModule)
    {
        MessageBox(hwnd, TEXT("Error: No se pudo cargar Msftedit.dll"), TEXT("Error"), MB_OK | MB_ICONERROR);
        return FALSE;
    }

    hRichEdit = CreateWindow(
        TEXT("RICHEDIT50W"),
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        hwnd, (HMENU)ID_RICHEDIT, hInstance, NULL
    );

    if (!hRichEdit)
    {
        MessageBox(hwnd, TEXT("Error: No se pudo crear el control RichEdit"), TEXT("Error"), MB_OK | MB_ICONERROR);
        FreeLibrary(hRichEditModule);
        return FALSE;
    }

    // Configurar colores del editor
    SetEditorColors();

    // Configurar ajuste de línea por defecto
    SetWordWrap(bWordWrap);
    
    // Mostrar ventana
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Cargar tabla de aceleradores
    hAccelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_ACCELERATOR1));

    // Mensajes de DPI awareness
    SetProcessDPIAware();
    
    // Bucle de mensajes
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        // Dar al diálogo de buscar/reemplazar la primera oportunidad de procesar el mensaje
        if (hFindDlg && IsWindow(hFindDlg) && IsDialogMessage(hFindDlg, &msg))
        {
            continue;
        }

        // Intentar procesar aceleradores
        if (hAccelTable && TranslateAccelerator(hwnd, hAccelTable, &msg))
        {
            continue; // El mensaje ya fue procesado por los aceleradores
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CREATE:
            break;
            
        case WM_SIZE:
            // Redimensionar el control RichEdit cuando cambia el tamaño de la ventana
            MoveWindow(hRichEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
            break;
            
        case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);
            
            switch (wmId)
            {
                case IDM_NEW:
                    // Limpiar el contenido del editor
                    SendMessage(hRichEdit, WM_SETTEXT, 0, (LPARAM)TEXT(""));
                    break;
                    
                case IDM_OPEN:
                {
                    OPENFILENAMEW ofn;
                    WCHAR szFile[MAX_PATH] = {0};
                    
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.lpstrFilter = L"All Supported Files\0*.txt;*.md;*.log;*.ini;*.cfg;*.conf;*.json;*.xml;*.yaml;*.toml;*.html;*.css;*.js;*.ts;*.py;*.c;*.h;*.cpp;*.hpp;*.bat;*.cmd;*.ps1;*.sh;*.ahk;*.lua;*.sql;*.env\0"
                      L"Web Files (*.html;*.css;*.js)\0*.html;*.css;*.js;*.ts;*.php\0"
                      L"Config Files (*.ini;*.json;*.xml)\0*.ini;*.cfg;*.conf;*.json;*.xml;*.yaml;*.toml;*.env\0"
                      L"Scripts (*.bat;*.py;*.ps1)\0*.bat;*.cmd;*.ps1;*.sh;*.py;*.ahk;*.lua\0"
                      L"C/C++ Source (*.c;*.h)\0*.c;*.h;*.cpp;*.hpp\0"
                      L"All Files (*.*)\0*.*\0\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    
                    if (GetOpenFileNameW(&ofn))
                    {
                        LoadFile(hwnd, ofn.lpstrFile);
                    }
                    break;
                }
                
                case IDM_SAVE:
                {
                    if (currentFile[0] == '\0')
                    {
                        // Si no hay archivo actual, mostrar diálogo de guardar como
                        goto save_as;
                    }
                    else
                    {
                        SaveFile(hwnd, currentFile);
                    }
                    break;
                }

                case IDM_SAVEAS:
                {
                    save_as:
                    OPENFILENAMEW ofn;
                    WCHAR szFile[MAX_PATH] = {0};

                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.lpstrFilter = L"All Supported Files\0*.txt;*.md;*.log;*.ini;*.cfg;*.conf;*.json;*.xml;*.yaml;*.toml;*.html;*.css;*.js;*.ts;*.py;*.c;*.h;*.cpp;*.hpp;*.bat;*.cmd;*.ps1;*.sh;*.ahk;*.lua;*.sql;*.env\0"
                      L"Web Files (*.html;*.css;*.js)\0*.html;*.css;*.js;*.ts;*.php\0"
                      L"Config Files (*.ini;*.json;*.xml)\0*.ini;*.cfg;*.conf;*.json;*.xml;*.yaml;*.toml;*.env\0"
                      L"Scripts (*.bat;*.py;*.ps1)\0*.bat;*.cmd;*.ps1;*.sh;*.py;*.ahk;*.lua\0"
                      L"C/C++ Source (*.c;*.h)\0*.c;*.h;*.cpp;*.hpp\0"
                      L"All Files (*.*)\0*.*\0\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_OVERWRITEPROMPT;

                    if (GetSaveFileNameW(&ofn))
                    {
                        SaveFile(hwnd, ofn.lpstrFile);
                        // Copiar el nombre del archivo para futuras operaciones de guardado
                        #ifdef UNICODE
                            wcscpy_s(currentFile, MAX_PATH, ofn.lpstrFile);
                        #else
                            strcpy_s(currentFile, MAX_PATH, ofn.lpstrFile);
                        #endif
                    }
                    break;
                }
                
                case IDM_EXIT:
                    PostQuitMessage(0);
                    break;
                    
                case IDM_UNDO:
                    SendMessage(hRichEdit, EM_UNDO, 0, 0);
                    break;
                    
                case IDM_CUT:
                    SendMessage(hRichEdit, WM_CUT, 0, 0);
                    break;
                    
                case IDM_COPY:
                    SendMessage(hRichEdit, WM_COPY, 0, 0);
                    break;
                    
                case IDM_PASTE:
                    SendMessage(hRichEdit, WM_PASTE, 0, 0);
                    break;
                    
                case IDM_DELETE:
                    SendMessage(hRichEdit, WM_CLEAR, 0, 0);
                    break;
                    
                case IDM_SELECTALL:
                    SendMessage(hRichEdit, EM_SETSEL, 0, -1);
                    break;
                    
                case IDM_FIND:
                case IDM_REPLACE:
                    if (!hFindDlg || !IsWindow(hFindDlg))
                    {
                        hFindDlg = CreateDialog(hInst, MAKEINTRESOURCE(IDD_FIND_REPLACE), hwnd, FindDlgProc);
                        ShowWindow(hFindDlg, SW_SHOW);
                    }
                    else
                    {
                        SetForegroundWindow(hFindDlg);
                    }
                    break;
                    
                case IDM_WORDWRAP:
                    bWordWrap = !bWordWrap;
                    SetWordWrap(bWordWrap);
                    break;
                    
                case IDM_ZOOMIN:
                    SetZoom(zoomFactor + 10);
                    break;
                    
                case IDM_ZOOMOUT:
                    SetZoom(zoomFactor - 10);
                    break;
                    
                case IDM_ZOOMRESET:
                    SetZoom(100);
                    break;
            }
            break;
        }
        
        case WM_NOTIFY:
        {
            LPNMHDR pnmhdr = (LPNMHDR)lParam;
            if (pnmhdr->idFrom == ID_RICHEDIT)
            {
                switch (pnmhdr->code)
                {
                    case EN_MSGFILTER:
                    {
                        MSGFILTER *pMsgFilter = (MSGFILTER *)lParam;
                        if (pMsgFilter->msg == WM_KEYDOWN)
                        {
                            // Manejar combinaciones de teclas especiales
                            if (GetKeyState(VK_CONTROL) & 0x8000)
                            {
                                switch (pMsgFilter->wParam)
                                {
                                    case 'N': // Ctrl+N
                                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_NEW, 0), 0);
                                        return TRUE;

                                    case 'O': // Ctrl+O
                                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_OPEN, 0), 0);
                                        return TRUE;

                                    case 'S': // Ctrl+S
                                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_SAVE, 0), 0);
                                        return TRUE;

                                    case 'F': // Ctrl+F
                                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_FIND, 0), 0);
                                        return TRUE;

                                    case VK_ADD: // Ctrl++
                                    case '=': // Ctrl+=
                                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_ZOOMIN, 0), 0);
                                        return TRUE;

                                    case VK_SUBTRACT: // Ctrl+-
                                    case '-': // Ctrl+-
                                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_ZOOMOUT, 0), 0);
                                        return TRUE;

                                    case '0': // Ctrl+0
                                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_ZOOMRESET, 0), 0);
                                        return TRUE;
                                }
                            }

                            // Manejar Ctrl+Shift+S para guardar como
                            if ((GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000))
                            {
                                if (pMsgFilter->wParam == 'S')
                                {
                                    SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_SAVEAS, 0), 0);
                                    return TRUE;
                                }
                            }
                        }
                        break;
                    }
                }
            }
            break;
        }

        case WM_KEYDOWN:
        {
            // Manejar combinaciones de teclas especiales
            if (GetKeyState(VK_CONTROL) & 0x8000)
            {
                switch (wParam)
                {
                    case 'O': // Ctrl+O
                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_OPEN, 0), 0);
                        return 0;

                    case 'S': // Ctrl+S
                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_SAVE, 0), 0);
                        return 0;

                    case 'F': // Ctrl+F
                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_FIND, 0), 0);
                        return 0;

                    case 'N': // Ctrl+N
                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_NEW, 0), 0);
                        return 0;

                    case 'Z': // Ctrl+Z
                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_UNDO, 0), 0);
                        return 0;

                    case 'X': // Ctrl+X
                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_CUT, 0), 0);
                        return 0;

                    case 'C': // Ctrl+C
                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_COPY, 0), 0);
                        return 0;

                    case 'V': // Ctrl+V
                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_PASTE, 0), 0);
                        return 0;

                    case 'A': // Ctrl+A
                        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_SELECTALL, 0), 0);
                        return 0;
                }
            }
            break;
        }

        case WM_MOUSEWHEEL:
        {
            // Manejar zoom con rueda del mouse + Ctrl
            if (GetKeyState(VK_CONTROL) & 0x8000)
            {
                int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (zDelta > 0)
                {
                    SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_ZOOMIN, 0), 0);
                }
                else
                {
                    SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDM_ZOOMOUT, 0), 0);
                }
                return 0;
            }
            break;
        }
        
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

// Aplicar modo oscuro a la ventana
void ApplyDarkMode(HWND hwnd)
{
    // Verificar si la función DwmSetWindowAttribute está disponible
    HMODULE hDwmApi = LoadLibrary(TEXT("dwmapi.dll"));
    if (hDwmApi)
    {
        typedef HRESULT(WINAPI* DwmSetWindowAttributeFunc)(HWND, DWORD, LPCVOID, DWORD);
        DwmSetWindowAttributeFunc pDwmSetWindowAttribute =
            (DwmSetWindowAttributeFunc)GetProcAddress(hDwmApi, "DwmSetWindowAttribute");

        if (pDwmSetWindowAttribute)
        {
            BOOL darkMode = TRUE;
            pDwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(BOOL)); // DWMWA_USE_IMMERSIVE_DARK_MODE
        }

        FreeLibrary(hDwmApi);
    }
}

// Configurar colores del editor
void SetEditorColors()
{
    // Establecer color de fondo oscuro
    SendMessage(hRichEdit, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
    
    // Configurar color de texto por defecto
    CHARFORMAT cf;
    ZeroMemory(&cf, sizeof(CHARFORMAT));
    cf.cbSize = sizeof(CHARFORMAT);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = RGB(220, 220, 220); // Gris claro
    
    SendMessage(hRichEdit, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
}

// Configurar ajuste de línea
void SetWordWrap(BOOL wrap)
{
    bWordWrap = wrap;

    if (wrap)
    {
        // Ajuste de línea habilitado
        SendMessage(hRichEdit, EM_SETTARGETDEVICE, 0, 0);
    }
    else
    {
        // Ajuste de línea deshabilitado
        SendMessage(hRichEdit, EM_SETTARGETDEVICE, 0, 1);
    }
}

// Configurar zoom
void SetZoom(int factor)
{
    if (factor < 10) factor = 10;
    if (factor > 500) factor = 500;
    
    zoomFactor = factor;
    
    // Enviar mensaje para cambiar el zoom
    SendMessage(hRichEdit, EM_SETZOOM, factor, 100);
}

// Cargar archivo
BOOL LoadFile(HWND hwnd, LPCTSTR pszFileName)
{
    HANDLE hFile;
    DWORD dwFileSize, dwBytesRead;
    char* pBuffer;
    BOOL bSuccess = FALSE;
    LPWSTR pwzWide = NULL;

    hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize == INVALID_FILE_SIZE)
        goto cleanup;

    pBuffer = (char*)HeapAlloc(GetProcessHeap(), 0, dwFileSize);
    if (!pBuffer)
        goto cleanup;

    if (!ReadFile(hFile, pBuffer, dwFileSize, &dwBytesRead, NULL) || dwFileSize != dwBytesRead)
        goto cleanup_heap;

    // Inicia la lógica de "Cascade Conversion"
    char* textStart = pBuffer;
    DWORD textSize = dwFileSize;

    // 1. Detección de BOM UTF-8
    if (dwFileSize >= 3 && memcmp(pBuffer, "\xEF\xBB\xBF", 3) == 0)
    {
        textStart = pBuffer + 3;
        textSize = dwFileSize - 3;
    }

    // 2. Intentar convertir de UTF-8 a WideChar
    int cchWide = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, textStart, textSize, NULL, 0);
    if (cchWide > 0)
    {
        pwzWide = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (cchWide + 1) * sizeof(WCHAR));
        if (pwzWide)
        {
            MultiByteToWideChar(CP_UTF8, 0, textStart, textSize, pwzWide, cchWide);
            pwzWide[cchWide] = L'\0'; // Asegurar terminador nulo
        }
    }
    else // 3. Si UTF-8 falla, intentar con ANSI (CP_ACP)
    {
        cchWide = MultiByteToWideChar(CP_ACP, 0, textStart, textSize, NULL, 0);
        if (cchWide > 0)
        {
            pwzWide = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (cchWide + 1) * sizeof(WCHAR));
            if (pwzWide)
            {
                MultiByteToWideChar(CP_ACP, 0, textStart, textSize, pwzWide, cchWide);
                pwzWide[cchWide] = L'\0'; // Asegurar terminador nulo
            }
        }
    }

    if (pwzWide)
    {
        // 4. Pasar el WCHAR* resultante a SetWindowTextW
        if (SetWindowTextW(hRichEdit, pwzWide))
        {
             bSuccess = TRUE;
            // Actualizar el archivo actual
            #ifdef UNICODE
                wcscpy_s(currentFile, MAX_PATH, pszFileName);
            #else
                strcpy_s(currentFile, MAX_PATH, pszFileName);
            #endif
        }
        HeapFree(GetProcessHeap(), 0, pwzWide);
    }
    else
    {
        // Si ambas conversiones fallan, mostrar un error o limpiar.
        SetWindowTextW(hRichEdit, L""); // Limpiar editor
        MessageBoxW(hwnd, L"No se pudo decodificar el archivo. Formato no compatible.", L"Error de Lectura", MB_OK | MB_ICONERROR);
    }


cleanup_heap:
    HeapFree(GetProcessHeap(), 0, pBuffer);

cleanup:
    CloseHandle(hFile);
    return bSuccess;
}

// Guardar archivo
BOOL SaveFile(HWND hwnd, LPCTSTR pszFileName)
{
    HANDLE hFile;
    DWORD dwTextLen, dwBytesWritten;
    LPWSTR pszText;
    BOOL bSuccess = FALSE;

    dwTextLen = GetWindowTextLength(hRichEdit);
    pszText = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (dwTextLen + 1) * sizeof(WCHAR));

    if (!pszText)
        return FALSE;

    GetWindowText(hRichEdit, (LPTSTR)pszText, dwTextLen + 1);

    hFile = CreateFile(pszFileName, GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        goto cleanup;

    // Convertir de Unicode a UTF-8
    int cbMulti = WideCharToMultiByte(CP_UTF8, 0, pszText, -1, NULL, 0, NULL, NULL);
    LPSTR pszMulti = (LPSTR)HeapAlloc(GetProcessHeap(), 0, cbMulti);
    if (!pszMulti)
        goto cleanup_file;

    WideCharToMultiByte(CP_UTF8, 0, pszText, -1, pszMulti, cbMulti, NULL, NULL);

    if (!WriteFile(hFile, pszMulti, cbMulti - 1, &dwBytesWritten, NULL)) // -1 para excluir el terminador nulo
        goto cleanup_multi;

    bSuccess = TRUE;

cleanup_multi:
    HeapFree(GetProcessHeap(), 0, pszMulti);

cleanup_file:
    CloseHandle(hFile);

cleanup:
    HeapFree(GetProcessHeap(), 0, pszText);
    return bSuccess;
}
