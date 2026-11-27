#include "pgclip_editor_view.h"
#include "pgclip_cids.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#include "pluginterfaces/vst/ivsteditcontroller.h"

namespace {
constexpr wchar_t kWindowClass[] = L"PGClipGainProEditorWindow";
constexpr int kCaptureButtonId = 1001;
constexpr UINT_PTR kMeterTimerId = 1;
constexpr UINT kMeterRefreshMs = 33;

struct EditorState {
    Steinberg::Vst::IEditController* controller{nullptr};
    std::array<float, 96> history{};
    std::size_t writeIndex{0};
};

void drawText(HDC dc, int x, int y, const wchar_t* value, int length,
              COLORREF color, int height = 18, int weight = FW_NORMAL) {
    SetTextColor(dc, color);
    HFONT font = CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ old = SelectObject(dc, font);
    TextOutW(dc, x, y, value, length);
    SelectObject(dc, old);
    DeleteObject(font);
}

LRESULT CALLBACK editorWindowProc(HWND window, UINT message,
                                  WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<EditorState*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* initial = new EditorState{};
        initial->controller = reinterpret_cast<Steinberg::Vst::IEditController*>(
            create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(initial));
        return DefWindowProcW(window, message, wParam, lParam);
    }

    if (message == WM_CREATE) {
        SetTimer(window, kMeterTimerId, kMeterRefreshMs, nullptr);
        CreateWindowExW(0, L"BUTTON", L"START CAPTURE",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            30, 150, 190, 40, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCaptureButtonId)),
            GetModuleHandleW(nullptr), nullptr);
        return 0;
    }

    if (message == WM_TIMER && wParam == kMeterTimerId && state != nullptr) {
        const double level = state->controller != nullptr
            ? state->controller->getParamNormalized(
                Steinberg::Vst::kInputLevelParameterId) : 0.0;
        state->history[state->writeIndex] =
            static_cast<float>(std::clamp(level, 0.0, 1.0));
        state->writeIndex = (state->writeIndex + 1) % state->history.size();
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }

    if (message == WM_COMMAND && LOWORD(wParam) == kCaptureButtonId &&
        HIWORD(wParam) == BN_CLICKED) {
        if (state != nullptr && state->controller != nullptr) {
            const auto id = Steinberg::Vst::kCaptureEnableParameterId;
            const double next = state->controller->getParamNormalized(id) >= 0.5
                ? 0.0 : 1.0;
            state->controller->setParamNormalized(id, next);
            SetWindowTextW(GetDlgItem(window, kCaptureButtonId),
                next >= 0.5 ? L"CAPTURE ARMED" : L"START CAPTURE");
        }
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

        drawText(dc, 30, 28, L"PG CLIP GAIN PRO", 16,
                 RGB(238, 241, 246), 30, FW_BOLD);
        drawText(dc, 32, 82, L"INTELLIGENT CLIP GAIN", 21,
                 RGB(90, 166, 255), 18, FW_SEMIBOLD);
        drawText(dc, 32, 112, L"Capture a vocal passage to analyze its gain curve.",
                 50, RGB(180, 187, 198));

        RECT graph{30, 210, bounds.right - 30, 300};
        HBRUSH graphBg = CreateSolidBrush(RGB(10, 12, 16));
        FillRect(dc, &graph, graphBg);
        DeleteObject(graphBg);
        HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(39, 45, 55));
        HGDIOBJ oldPen = SelectObject(dc, gridPen);
        for (int i = 1; i < 4; ++i) {
            const int y = graph.top + (graph.bottom - graph.top) * i / 4;
            MoveToEx(dc, graph.left, y, nullptr);
            LineTo(dc, graph.right, y);
        }
        for (int i = 1; i < 12; ++i) {
            const int x = graph.left + (graph.right - graph.left) * i / 12;
            MoveToEx(dc, x, graph.top, nullptr);
            LineTo(dc, x, graph.bottom);
        }
        SelectObject(dc, oldPen);
        DeleteObject(gridPen);

        if (state != nullptr) {
            HPEN wavePen = CreatePen(PS_SOLID, 2, RGB(73, 170, 255));
            oldPen = SelectObject(dc, wavePen);
            const int count = static_cast<int>(state->history.size());
            for (int i = 0; i < count; ++i) {
                const int index = static_cast<int>(
                    (state->writeIndex + static_cast<std::size_t>(i)) %
                    state->history.size());
                const float sample = state->history[static_cast<std::size_t>(index)];
                const int x = graph.left + i * (graph.right - graph.left - 1) /
                    (count - 1);
                const int y = graph.bottom - 4 - static_cast<int>(
                    sample * (graph.bottom - graph.top - 8));
                if (i == 0) MoveToEx(dc, x, y, nullptr);
                else LineTo(dc, x, y);
            }
            SelectObject(dc, oldPen);
            DeleteObject(wavePen);
        }

        drawText(dc, 32, 188, L"INPUT MONITOR", 13, RGB(180, 187, 198), 14,
                 FW_SEMIBOLD);
        drawText(dc, 32, 316, L"CLIP GAIN  +  CLIP PEAK", 23,
                 RGB(210, 216, 226));
        drawText(dc, 32, 342, L"Offline analysis  |  Natural dynamics", 37,
                 RGB(150, 158, 171));
        EndPaint(window, &paint);
        return 0;
    }

    if (message == WM_NCDESTROY) {
        KillTimer(window, kMeterTimerId);
        delete state;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace
#endif

namespace Steinberg {
namespace Vst {

PGClipGainEditorView::PGClipGainEditorView(IEditController* controller)
: CPluginView(new ViewRect(0, 0, 720, 390)), controller_(controller) {}

PGClipGainEditorView::~PGClipGainEditorView() {
    removed();
}

tresult PLUGIN_API PGClipGainEditorView::isPlatformTypeSupported(FIDString type) {
#if defined(_WIN32)
    return type != nullptr && std::strcmp(type, kPlatformTypeHWND) == 0
        ? kResultTrue : kResultFalse;
#else
    (void)type;
    return kResultFalse;
#endif
}

tresult PLUGIN_API PGClipGainEditorView::attached(void* parent, FIDString type) {
#if defined(_WIN32)
    if (parent == nullptr || isPlatformTypeSupported(type) != kResultTrue)
        return kResultFalse;
    systemWindow = parent;
    attachedToParent();
    return editorWindow_ != nullptr ? kResultOk : kResultFalse;
#else
    (void)parent;
    (void)type;
    return kResultFalse;
#endif
}

tresult PLUGIN_API PGClipGainEditorView::removed() {
    removedFromParent();
    systemWindow = nullptr;
    return kResultOk;
}

void PGClipGainEditorView::attachedToParent() {
#if defined(_WIN32)
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = editorWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClass;
    if (RegisterClassExW(&windowClass) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        editorWindow_ = nullptr;
        return;
    }

    HWND parent = static_cast<HWND>(systemWindow);
    if (parent != nullptr) {
        editorWindow_ = CreateWindowExW(
            0, kWindowClass, L"PG Clip Gain Pro",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, 720, 390, parent, nullptr,
            GetModuleHandleW(nullptr), controller_);
    }
#endif
}

void PGClipGainEditorView::removedFromParent() {
#if defined(_WIN32)
    HWND child = static_cast<HWND>(editorWindow_);
    if (child != nullptr && IsWindow(child)) DestroyWindow(child);
#endif
    editorWindow_ = nullptr;
}

} // namespace Vst
} // namespace Steinberg
