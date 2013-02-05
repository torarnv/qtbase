#include "qwinrtscreen.h"
#include "qwinrtbackingstore.h"
#include <qpa/qwindowsysteminterface.h>
#include <QtGui/QTouchDevice>
#include <QtGui/QGuiApplication>

#include <d3d11_1.h>
#include <wrl.h>
#include <Windows.ApplicationModel.core.h>
#include <windows.ui.core.h>
#include <windows.ui.input.h>
#include <windows.devices.input.h>
using namespace Microsoft::WRL;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::System;
using namespace ABI::Windows::UI::Core;
using namespace ABI::Windows::UI::Input;
using namespace ABI::Windows::Devices::Input;
typedef ITypedEventHandler<CoreWindow*, WindowActivatedEventArgs*> ActivatedHandler;
typedef ITypedEventHandler<CoreWindow*, CoreWindowEventArgs*> ClosedHandler;
typedef ITypedEventHandler<CoreWindow*, CharacterReceivedEventArgs*> CharacterReceivedHandler;
typedef ITypedEventHandler<CoreWindow*, InputEnabledEventArgs*> InputEnabledHandler;
typedef ITypedEventHandler<CoreWindow*, KeyEventArgs*> KeyHandler;
typedef ITypedEventHandler<CoreWindow*, PointerEventArgs*> PointerHandler;
typedef ITypedEventHandler<CoreWindow*, WindowSizeChangedEventArgs*> SizeChangedHandler;
typedef ITypedEventHandler<CoreWindow*, VisibilityChangedEventArgs*> VisibilityChangedHandler;
typedef QWindowSystemInterface::TouchPoint TouchPoint;

// Utilities for COM
template <typename T>
inline HRESULT queryInterface(IUnknown *source, T **destination) {
    return source->QueryInterface(__uuidof(T), reinterpret_cast<void**>(destination));
}
template <typename T>
inline HRESULT getParent(IDXGIObject *object, T **parent) {
    return object->GetParent(__uuidof(T), reinterpret_cast<void**>(parent));
}
inline void deletePtr(IUnknown *pointer) {
    if (pointer) {
        pointer->Release();
        delete pointer;
    }
}
// QColor => float[4]
struct float4 {
    union { float f[4]; struct { float f1; float f2; float f3; float f4; }; };
    float4(const QColor &c) : f1(c.redF()), f2(c.greenF()), f3(c.blueF()), f4(c.alphaF()) { }
    operator float*() { return f; }
};

// Generic touch/mouse event union
class PointerValue {
public:
    PointerValue(IPointerEventArgs *args) {
        args->get_KeyModifiers(&mods);
        args->get_CurrentPoint(&point);
        point->get_Properties(&properties);
        point->get_PointerDevice(&device);
    }
    Qt::KeyboardModifiers modifiers() const { return (mods & 1 ? Qt::ControlModifier : 0)|(mods & 4 ? Qt::ShiftModifier : 0)|(mods & 8 ? Qt::MetaModifier : 0); }
    PointerDeviceType type() const
    {
        if (device == nullptr)
            return PointerDeviceType_Touch;
        PointerDeviceType t;
        device->get_PointerDeviceType(&t);
        return t;
    }
    Qt::MouseButtons buttons() const
    {
        boolean a;
        Qt::MouseButtons b;
        properties->get_IsLeftButtonPressed(&a); b |= a ? Qt::LeftButton : Qt::NoButton;
        properties->get_IsMiddleButtonPressed(&a); b |= a ? Qt::MiddleButton : Qt::NoButton;
        properties->get_IsRightButtonPressed(&a); b |= a ? Qt::RightButton : Qt::NoButton;
        properties->get_IsXButton1Pressed(&a); b |= a ? Qt::XButton1 : Qt::NoButton;
        properties->get_IsXButton2Pressed(&a); b |= a ? Qt::XButton2 : Qt::NoButton;
        return b;
    }
    QPointF pos() const { Point pt; point->get_Position(&pt); return QPointF(pt.X, pt.Y); }
    int delta() const { int d; properties->get_MouseWheelDelta(&d); return d; }
    Qt::Orientation orientation() const { boolean b; properties->get_IsHorizontalMouseWheel(&b); return b ? Qt::Horizontal : Qt::Vertical; }
    TouchPoint touchPoint() const
    {
        TouchPoint tp;
        quint32 id; point->get_PointerId(&id); tp.id = id;

        if (device)
        {
            Rect sceneRect; device->get_ScreenRect(&sceneRect);
            QPointF pt = pos(); tp.area = QRectF(pt, QSize(1, 1)).translated(-0.5, -0.5);
            tp.normalPosition = QPointF(pt.x()/sceneRect.Width, pt.y()/sceneRect.Height);
        }
        else
        {
            QPointF pt = pos();
            tp.area = QRectF(pt, QSize(1, 1)).translated(-0.5, -0.5);
            tp.normalPosition = pt;
        }
        return tp;
    }
private:
    ComPtr<IPointerPoint> point;
    ComPtr<IPointerPointProperties> properties;
    ComPtr<IPointerDevice> device;
    VirtualKeyModifiers mods;
};

// Copy rectangles - TODO: use SIMD if available
inline void rectCopy(void *dst, int dstStride, const void *src, int srcStride, const QRect &rect, const QPoint &offset)
{
    int w = rect.width() * 4, h = rect.height();
    char *d = reinterpret_cast<char*>(dst) + (offset.x() + rect.x()) * 4 + (offset.y() + rect.y()) * dstStride;
    const char *s = reinterpret_cast<const char*>(src) + rect.x() * 4 + rect.y() * srcStride;
    for (int i = 0; i < h; ++i) {
        memcpy(d, s, w);
        d += dstStride; s += srcStride;
    }
}

QWinRTPageFlipper::QWinRTPageFlipper(ICoreWindow *window)
    : device(nullptr)
    , context(nullptr)
    , frontBuffer(nullptr)
    , backBuffer(nullptr)
    , swapChain(nullptr)
{
    // Select feature levels
    D3D_FEATURE_LEVEL featureLevel;
    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    // Create Device
    if (FAILED(D3D11CreateDevice(
                   nullptr,
                   D3D_DRIVER_TYPE_HARDWARE,
                   nullptr,
                   D3D11_CREATE_DEVICE_DEBUG,
                   featureLevels,
                   ARRAYSIZE(featureLevels),
                   D3D11_SDK_VERSION,
                   reinterpret_cast<ID3D11Device**>(&device),
                   &featureLevel,
                   reinterpret_cast<ID3D11DeviceContext**>(&context)))) {
        qCritical("Failed to create D3D device.");
    }

    // Ensure D3D 11.1
    if (FAILED(queryInterface(device, &device)))
        qCritical("Failed to get D3D 11.1 device");
    if (FAILED(queryInterface(context, &context)))
        qCritical("Failed to get D3D 11.1 context");

    // Obtain DXGI device and adapter
    IDXGIDevice1 *dxgi;
    if (FAILED(queryInterface(device, &dxgi))) {
        qCritical("Failed to create DXGI device.");
        return;
    }
    if (FAILED(dxgi->SetMaximumFrameLatency(1)))
        qCritical("Unable to set the maximum frame latency.");
    IDXGIAdapter *dxgiAdapter;
    if (FAILED(dxgi->GetAdapter(&dxgiAdapter))) {
        qCritical("Failed to get the DXGI adapter.");
        return;
    }
    IDXGIFactory2 *dxgiFactory;
    if (FAILED(getParent(dxgiAdapter, &dxgiFactory))) {
        qCritical("Could not get parent of DXGI factory.");
        return;
    }

    Rect rect;
    window->get_Bounds(&rect);

    // Initialize swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};
    swapChainDesc.Width = static_cast<UINT>(rect.Width); // Match the size of the window.
    swapChainDesc.Height = static_cast<UINT>(rect.Height);
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // This is the most common swap chain format.
    swapChainDesc.Stereo = false;
    swapChainDesc.SampleDesc.Count = 1; // Don't use multi-sampling.
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 1; // On phone, only single buffering is supported.
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH; // On phone, only stretch and aspect-ratio stretch scaling are allowed.
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // On phone, no swap effects are supported.
    swapChainDesc.Flags = 0;

    if (FAILED(dxgiFactory->CreateSwapChainForCoreWindow(device, window, &swapChainDesc, nullptr, &swapChain))) {
        qCritical("Could not create swap chain for core window.");
        return;
    }

    resize(QSize(rect.Width, rect.Height));
}

QWinRTPageFlipper::~QWinRTPageFlipper()
{
    deletePtr(device);
    deletePtr(context);
    deletePtr(frontBuffer);
    deletePtr(backBuffer);
    deletePtr(swapChain);
}

void QWinRTPageFlipper::update(const QRegion &region, const QPoint &offset, const void *handle, int stride)
{
    D3D11_MAPPED_SUBRESOURCE target;
    context->Map(backBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &target);
    m_dirtyRects += region.translated(offset).rects();
    foreach (const QRect &rect, region.rects())
        rectCopy(target.pData, target.RowPitch, handle, stride, rect, offset);
    context->Unmap(backBuffer, 0);
    displayBuffer(0);
}

bool QWinRTPageFlipper::displayBuffer(QPlatformScreenBuffer *buffer)
{
    Q_UNUSED(buffer)    // Using internal buffer - consider if this could be encapsulated into separate class
    if (m_dirtyRects.isEmpty())
        return false;
    if (swapChain) {
        // Copy back buffer to front buffer
        // context->CopySubresourceRegion(frontBuffer, 0, 0, 0, 0, backBuffer, 0, NULL);
        context->CopyResource(frontBuffer, backBuffer);
        emit bufferReleased(buffer);
        // Note: this cast works because RECT and QRect have the same memory layout
        DXGI_PRESENT_PARAMETERS params = { m_dirtyRects.size(), reinterpret_cast<RECT*>(m_dirtyRects.data()) };
        if (SUCCEEDED(swapChain->Present(1, 0))) {
            emit bufferDisplayed(buffer);
            m_dirtyRects.clear();
            return true;
        }
    }
    return false;
}

void QWinRTPageFlipper::resize(const QSize &size)
{
    if (m_size != size) {
        if (frontBuffer)
            frontBuffer->Release();

        if (swapChain) {
            swapChain->ResizeBuffers(2, size.width(), size.height(), DXGI_FORMAT_UNKNOWN, 0);
            if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&frontBuffer)))) {
                qCritical("Could not recapture front buffer");
                m_size = QSize();
                return;
            }
        }
        m_size = size;

        if (backBuffer)
            backBuffer->Release();

        if (device)
        {
            D3D11_TEXTURE2D_DESC desc = {
                m_size.width(), m_size.height(),
                1, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                {1, 0}, D3D11_USAGE_DYNAMIC,
                D3D11_BIND_SHADER_RESOURCE, D3D11_CPU_ACCESS_WRITE, 0
            };
            if (FAILED(device->CreateTexture2D(&desc, nullptr, &backBuffer)))
                qCritical("Could not resize back buffer.");
        }

        m_dirtyRects.clear();
        if (swapChain)
            swapChain->Present(1, 0);
    }
}

QWinRTScreen::QWinRTScreen(ICoreWindow *window)
    : m_window(window)
    , m_depth(32)
    , m_format(QImage::Format_ARGB32_Premultiplied)
    , m_pageFlipper(new QWinRTPageFlipper(window))
{
    // TODO: query touch device capabilities
    m_touchDevice.setCapabilities(QTouchDevice::Position);
    m_touchDevice.setType(QTouchDevice::TouchScreen);
    m_touchDevice.setName("WinRTTouchDevice1");

    Rect rect;
    window->get_Bounds(&rect);
    m_geometry = QRect(0, 0, rect.Width, rect.Height);
    m_pageFlipper->resize(m_geometry.size());
    QWindowSystemInterface::handleScreenGeometryChange(screen(), m_geometry);
    QWindowSystemInterface::handleScreenAvailableGeometryChange(screen(), m_geometry);

    // Event handlers mapped to QEvents
    m_window->add_KeyDown(Callback<KeyHandler>(this, &QWinRTScreen::onKeyDown).Get(), &m_tokens[QEvent::KeyPress]);
    m_window->add_KeyUp(Callback<KeyHandler>(this, &QWinRTScreen::onKeyUp).Get(), &m_tokens[QEvent::KeyRelease]);
    m_window->add_PointerEntered(Callback<PointerHandler>(this, &QWinRTScreen::onPointerEntered).Get(), &m_tokens[QEvent::Enter]);
    m_window->add_PointerExited(Callback<PointerHandler>(this, &QWinRTScreen::onPointerExited).Get(), &m_tokens[QEvent::Leave]);
    m_window->add_PointerMoved(Callback<PointerHandler>(this, &QWinRTScreen::onPointerMoved).Get(), &m_tokens[QEvent::MouseMove]);
    m_window->add_PointerPressed(Callback<PointerHandler>(this, &QWinRTScreen::onPointerPressed).Get(), &m_tokens[QEvent::MouseButtonPress]);
    m_window->add_PointerReleased(Callback<PointerHandler>(this, &QWinRTScreen::onPointerReleased).Get(), &m_tokens[QEvent::MouseButtonRelease]);
    m_window->add_PointerWheelChanged(Callback<PointerHandler>(this, &QWinRTScreen::onPointerWheelChanged).Get(), &m_tokens[QEvent::Wheel]);
    m_window->add_SizeChanged(Callback<SizeChangedHandler>(this, &QWinRTScreen::onSizeChanged).Get(), &m_tokens[QEvent::Resize]);

    // Unused/Unimplemented handlers
    //m_window->add_Activated(Callback<ActivatedHandler>(this, &QWinRTScreen::onActivated).Get(), &m_tokens[QEvent::WindowActivate]);
    //m_window->add_Closed(Callback<ClosedHandler>(this, &QWinRTScreen::onClosed).Get(), &m_tokens[QEvent::WindowDeactivate]);
    //m_window->add_InputEnabled(Callback<InputEnabledHandler>(this, &QWinRTScreen::onInputEnabled).Get(), &m_tokens[QEvent::InputMethod]);
    //m_window->add_VisibilityChanged(Callback<VisibilityChangedHandler>(this, &QWinRTScreen::onVisibilityChanged).Get(), &m_tokens[QEvent::Show]);
    //m_window->add_TouchHitTesting();
    //m_window->add_PointerCaptureLost();
    //m_window->add_CharacterReceived(Callback<CharacterReceivedHandler>(this, &QWinRTScreen::onCharacterReceived), m_tokens[QEvent::???]);
}

QRect QWinRTScreen::geometry() const
{
    return m_geometry;
}

int QWinRTScreen::depth() const
{
    return m_depth;
}

QImage::Format QWinRTScreen::format() const
{
    return m_format;
}

QPlatformScreenPageFlipper *QWinRTScreen::pageFlipper() const
{
    return m_pageFlipper;
}

HRESULT QWinRTScreen::onKeyDown(ICoreWindow *, IKeyEventArgs *args)
{
    CorePhysicalKeyStatus status;
    args->get_KeyStatus(&status);
    QWindowSystemInterface::handleKeyEvent(0, QEvent::KeyPress, status.ScanCode, Qt::NoModifier); // TODO: a way to get modifiers?
    return S_OK;
}

HRESULT QWinRTScreen::onKeyUp(ICoreWindow *, IKeyEventArgs *args)
{
    CorePhysicalKeyStatus status;
    args->get_KeyStatus(&status);
    QWindowSystemInterface::handleKeyEvent(0, QEvent::KeyRelease, status.ScanCode, Qt::NoModifier); // TODO: a way to get modifiers?
    return S_OK;
}

HRESULT QWinRTScreen::onPointerEntered(ICoreWindow *, IPointerEventArgs *args)
{
    PointerValue point(args);
    QWindowSystemInterface::handleEnterEvent(0);
    return S_OK;
}

HRESULT QWinRTScreen::onPointerExited(ICoreWindow *, IPointerEventArgs *args)
{
    PointerValue point(args);
    QWindowSystemInterface::handleLeaveEvent(0);
    return S_OK;
}

HRESULT QWinRTScreen::onPointerMoved(ICoreWindow *, IPointerEventArgs *args)
{
    PointerValue point(args); QPointF pos = point.pos();
    if (point.type() == PointerDeviceType_Mouse) {
        QWindowSystemInterface::handleMouseEvent(0, pos, pos, point.buttons(), point.modifiers());
    } else {
        TouchPoint tp = point.touchPoint(); tp.state = Qt::TouchPointMoved;
        QList<TouchPoint> points; points << tp;
        QWindowSystemInterface::handleTouchEvent(0, &m_touchDevice, points, point.modifiers());
    }
    return S_OK;
}

HRESULT QWinRTScreen::onPointerPressed(ICoreWindow *, IPointerEventArgs *args)
{
    PointerValue point(args); QPointF pos = point.pos();
    if (point.type() == PointerDeviceType_Mouse) {
        QWindowSystemInterface::handleMouseEvent(0, pos, pos, point.buttons(), point.modifiers());
    } else {
        TouchPoint tp = point.touchPoint(); tp.state = Qt::TouchPointPressed;
        QList<TouchPoint> points; points << tp;
        QWindowSystemInterface::handleTouchEvent(0, &m_touchDevice, points, point.modifiers());
    }
    return S_OK;
}

HRESULT QWinRTScreen::onPointerReleased(ICoreWindow *, IPointerEventArgs *args)
{
    PointerValue point(args); QPointF pos = point.pos();
    if (point.type() == PointerDeviceType_Mouse) {
        QWindowSystemInterface::handleMouseEvent(0, pos, pos, point.buttons(), point.modifiers());
    } else {
        TouchPoint tp = point.touchPoint(); tp.state = Qt::TouchPointReleased;
        QList<TouchPoint> points; points << tp;
        QWindowSystemInterface::handleTouchEvent(0, &m_touchDevice, points, point.modifiers());
    }
    return S_OK;
}

HRESULT QWinRTScreen::onPointerWheelChanged(ICoreWindow *, IPointerEventArgs *args)
{
    PointerValue point(args); QPointF pos = point.pos();
    QWindowSystemInterface::handleWheelEvent(0, pos, pos, point.delta(), point.orientation(), point.modifiers());
    return S_OK;
}

HRESULT QWinRTScreen::onSizeChanged(ICoreWindow *, IWindowSizeChangedEventArgs *args)
{
    Size size;
    args->get_Size(&size);
    m_geometry.setSize(QSize(size.Width, size.Height));
    m_pageFlipper->resize(m_geometry.size());
    resizeMaximizedWindows();
    QWindowSystemInterface::handleScreenGeometryChange(screen(), m_geometry);
    QWindowSystemInterface::handleScreenAvailableGeometryChange(screen(), m_geometry);
    foreach (QWindow *w, qGuiApp->topLevelWindows())
        QWindowSystemInterface::handleExposeEvent(w, QRect(QPoint(), w->size()));
    return S_OK;
}


/*HRESULT QWinRTScreen::onActivated(ICoreWindow *, IWindowActivatedEventArgs *args)
{
    return S_OK;
}

HRESULT QWinRTScreen::onClosed(ICoreWindow *, ICoreWindowEventArgs *args)
{
    QWindowSystemInterface::handleCloseEvent(0);
    return S_OK;
}

HRESULT QWinRTScreen::onInputEnabled(ICoreWindow *, IInputEnabledEventArgs *args)
{
    // Can we use this for InputMethod interaction?
    return S_OK;
}

HRESULT QWinRTScreen::onVisibilityChanged(ICoreWindow *, IVisibilityChangedEventArgs *args)
{
    return S_OK;
}
*/
