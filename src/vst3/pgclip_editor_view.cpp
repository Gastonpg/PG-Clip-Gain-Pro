#include "pgclip_editor_view.h"
#include "pgclip_cids.h"

#if defined(_WIN32)
#include <windows.h>
#include "pluginterfaces/vst/ivsteditcontroller.h"

namespace {
constexpr wchar_t kWindowClass[] = L"PGClipGainProEditorWindow";
constexpr int kCaptureButtonId = 1001;

LRESULT CALLBACK editorWindowProc(HWND window, UINT message,
                                  WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return DefWindowProcW(window, message, wParam, lParam);
    }

    if (message == WM_CREATE) {
        CreateWindowExW(0, L"BUTTON", L"START CAPTURE",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        28, 150, 190, 42, window,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCaptureButtonId)),
                        GetModuleHandleW(nullptr), nullptr);
        return 0;
    }

    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT bounds{};
        GetClientRect(window, &bounds);
        HBRUSH background = CreateSolidBrush(RGB(16, 18, 22));
        FillRect(dc, &bounds, background);
        DeleteObject(background);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(238, 241, 246));
        HFONT titleFont = CreateFontW(30, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                                      FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                      CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(dc, titleFont);
        TextOutW(dc, 28, 28, L"PG CLIP GAIN PRO", 16);
        SelectObject(dc, oldFont);
        DeleteObject(titleFont);

        SetTextColor(dc, RGB(90, 166, 255));
        TextOutW(dc, 30, 82, L"INTELLIGENT CLIP GAIN", 21);
        SetTextColor(dc, RGB(180, 187, 198));
        TextOutW(dc, 30, 112, L"Capture a vocal passage to analyze its gain curve.", 50);
        TextOutW(dc, 30, 220, L"CLIP GAIN  +  CLIP PEAK", 23);
        TextOutW(dc, 30, 250, L"Offline analysis  |  Natural dynamics", 37);
        EndPaint(window, &paint);
        return 0;
    }

    if (message == WM_COMMAND && LOWORD(wParam) == kCaptureButtonId &&
        HIWORD(wParam) == BN_CLICKED) {
        auto* controller = reinterpret_cast<Steinberg::Vst::IEditController*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (controller != nullptr) {
            const auto id = Steinberg::Vst::kCaptureEnableParameterId;
            const double next = controller->getParamNormalized(id) >= 0.5 ? 0.0 : 1.0;
            // The controller interface exposes parameter state changes, while
            // edit gestures belong to IComponentHandler (the host callback).
            // Keep this native view independent of a host-specific handler.
            controller->setParamNormalized(id, next);
            SetWindowTextW(GetDlgItem(window, kCaptureButtonId),
                           next >= 0.5 ? L"CAPTURE ARMED" : L"START CAPTURE");
        }
        return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace
#endif

namespace Steinberg {
namespace Vst {

PGClipGainEditorView::PGClipGainEditorView(IEditController* controller)
: CPluginView(new ViewRect(0, 0, 720, 360)), controller_(controller) {}

PGClipGainEditorView::~PGClipGainEditorView() {
    removed();
}

void PGClipGainEditorView::attachedToParent() {
#if defined(_WIN32)
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = editorWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClass;
    RegisterClassExW(&windowClass);

    HWND parent = static_cast<HWND>(systemWindow);
    if (parent != nullptr) {
        HWND child = CreateWindowExW(
            0, kWindowClass, L"PG Clip Gain Pro",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, 720, 360, parent, nullptr,
            GetModuleHandleW(nullptr), controller_);
        editorWindow_ = child;
    }
#endif
}

void PGClipGainEditorView::removedFromParent() {
#if defined(_WIN32)
    HWND child = static_cast<HWND>(editorWindow_);
    if (child != nullptr && IsWindow(child)) {
        DestroyWindow(child);
    }
#endif
    editorWindow_ = nullptr;
}

} // namespace Vst
} // namespace Steinberg
