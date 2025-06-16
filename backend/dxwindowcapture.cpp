#include "dxwindowcapture.h"

DXWindowCapture::DXWindowCapture(QObject *parent)
    : QObject(parent)
    , m_targetWindow(nullptr)
    , m_timer(new QTimer(this))
    , m_useCustomArea(false)
    , m_d3dDevice(nullptr)
    , m_d3dContext(nullptr)
    , m_dxgiDuplication(nullptr)
    , m_stagingTexture(nullptr)
    , m_dxgiSupported(false)
    , m_dwmSupported(false)
{
    connect(m_timer, &QTimer::timeout, this, &DXWindowCapture::captureScreenshot);

    // Проверяем поддержку DWM
    BOOL dwmEnabled = FALSE;
    if (SUCCEEDED(DwmIsCompositionEnabled(&dwmEnabled))) {
        m_dwmSupported = dwmEnabled;
    }
}

DXWindowCapture::~DXWindowCapture()
{
    cleanup();
}

bool DXWindowCapture::initCapture(HWND targetWindow)
{
    if (!targetWindow || !IsWindow(targetWindow)) {
        emit captureError("Неверный дескриптор окна");
        return false;
    }

    m_targetWindow = targetWindow;

    // Пытаемся инициализировать DirectX
    if (initDirectX()) {
        m_dxgiSupported = true;
        qDebug() << "DXGI захват инициализирован";
    } else {
        qDebug() << "DXGI недоступен, используем альтернативные методы";
    }

    return true;
}

bool DXWindowCapture::initDirectX()
{
    cleanup();

    // Создаем D3D11 устройство
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &m_d3dDevice,
        &featureLevel,
        &m_d3dContext
        );

    if (FAILED(hr)) {
        return false;
    }

    // Получаем DXGI адаптер
    IDXGIDevice* dxgiDevice = nullptr;
    hr = m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    if (FAILED(hr)) {
        return false;
    }

    IDXGIAdapter* dxgiAdapter = nullptr;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    dxgiDevice->Release();

    if (FAILED(hr)) {
        return false;
    }

    // Получаем выход
    IDXGIOutput* dxgiOutput = nullptr;
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    dxgiAdapter->Release();

    if (FAILED(hr)) {
        return false;
    }

    // Получаем DXGI Output1
    IDXGIOutput1* dxgiOutput1 = nullptr;
    hr = dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&dxgiOutput1);
    dxgiOutput->Release();

    if (FAILED(hr)) {
        return false;
    }

    // Создаем дубликатор
    hr = dxgiOutput1->DuplicateOutput(m_d3dDevice, &m_dxgiDuplication);
    dxgiOutput1->Release();

    return SUCCEEDED(hr);
}

void DXWindowCapture::startCapture(int intervalMs)
{
    if (!m_targetWindow) {
        emit captureError("Окно не инициализировано");
        return;
    }

    m_timer->start(intervalMs);
}

void DXWindowCapture::stopCapture()
{
    m_timer->stop();
}

void DXWindowCapture::setCaptureArea(const QRect& area)
{
    m_captureArea = area;
    m_useCustomArea = true;
}

void DXWindowCapture::resetCaptureArea()
{
    m_useCustomArea = false;
    m_captureArea = QRect();
}

QRect DXWindowCapture::getWindowRect() const
{
    if (!isWindowValid()) {
        return QRect();
    }

    RECT rect;
    if (GetWindowRect(m_targetWindow, &rect)) {
        return QRect(rect.left, rect.top,
                     rect.right - rect.left,
                     rect.bottom - rect.top);
    }

    return QRect();
}

void DXWindowCapture::captureScreenshot()
{
    if (!isWindowValid()) {
        emit captureError("Окно недоступно");
        return;
    }

    QImage screenshot = captureWindow();

    if (!screenshot.isNull()) {
        m_lastScreenshot = screenshot;
        emit screenshotCaptured(screenshot);
    } else {
        emit captureError("Не удалось захватить скриншот");
    }
}

QImage DXWindowCapture::captureWindow()
{
    // Пробуем методы в порядке приоритета
    QImage result;

    // 1. DXGI (лучший для DirectX приложений)
    if (m_dxgiSupported) {
        result = captureWithDXGI();
        if (!result.isNull()) return result;
    }

    // 2. DWM (хорошо для композитных окон)
    if (m_dwmSupported) {
        result = captureWithDWM();
        if (!result.isNull()) return result;
    }

    // 3. BitBlt (универсальный метод)
    result = captureWithBitBlt();

    return result;
}

QImage DXWindowCapture::captureWithDXGI()
{
    if (!m_dxgiDuplication) {
        return QImage();
    }

    IDXGIResource* dxgiResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    HRESULT hr = m_dxgiDuplication->AcquireNextFrame(1000, &frameInfo, &dxgiResource);
    if (FAILED(hr)) {
        return QImage();
    }

    ID3D11Texture2D* texture = nullptr;
    hr = dxgiResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texture);
    dxgiResource->Release();

    if (FAILED(hr)) {
        m_dxgiDuplication->ReleaseFrame();
        return QImage();
    }

    // Создаем staging текстуру для чтения
    D3D11_TEXTURE2D_DESC textureDesc;
    texture->GetDesc(&textureDesc);

    textureDesc.Usage = D3D11_USAGE_STAGING;
    textureDesc.BindFlags = 0;
    textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    textureDesc.MiscFlags = 0;

    if (m_stagingTexture) {
        m_stagingTexture->Release();
    }

    hr = m_d3dDevice->CreateTexture2D(&textureDesc, nullptr, &m_stagingTexture);
    if (FAILED(hr)) {
        texture->Release();
        m_dxgiDuplication->ReleaseFrame();
        return QImage();
    }

    // Копируем данные
    m_d3dContext->CopyResource(m_stagingTexture, texture);

    // Читаем данные
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = m_d3dContext->Map(m_stagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource);

    QImage result;
    if (SUCCEEDED(hr)) {
        // Преобразуем в QPixmap
        QImage image((uchar*)mappedResource.pData,
                     textureDesc.Width, textureDesc.Height,
                     mappedResource.RowPitch, QImage::Format_ARGB32);

        result = image;

        m_d3dContext->Unmap(m_stagingTexture, 0);
    }

    texture->Release();
    m_dxgiDuplication->ReleaseFrame();

    return result;
}

QImage DXWindowCapture::captureWithDWM()
{
    RECT windowRect;
    if (!GetWindowRect(m_targetWindow, &windowRect)) {
        return QImage();
    }

    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;

    HDC hdcWindow = GetDC(m_targetWindow);
    HDC hdcMemDC = CreateCompatibleDC(hdcWindow);
    HBITMAP hbmScreen = CreateCompatibleBitmap(hdcWindow, width, height);

    SelectObject(hdcMemDC, hbmScreen);

    // Используем PrintWindow для захвата
    BOOL result = PrintWindow(m_targetWindow, hdcMemDC, PW_RENDERFULLCONTENT);

    QImage resultImage;
    if (result) {
        resultImage = convertToQImage(hbmScreen, width, height);
    }

    DeleteObject(hbmScreen);
    DeleteDC(hdcMemDC);
    ReleaseDC(m_targetWindow, hdcWindow);

    return resultImage;
}

QImage DXWindowCapture::captureWithBitBlt()
{
    HDC hdcWindow = GetDC(m_targetWindow);
    HDC hdcMemDC = CreateCompatibleDC(hdcWindow);

    RECT windowRect;
    GetClientRect(m_targetWindow, &windowRect);

    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;

    HBITMAP hbmScreen = CreateCompatibleBitmap(hdcWindow, width, height);
    SelectObject(hdcMemDC, hbmScreen);

    BitBlt(hdcMemDC, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);

    QImage imgae = convertToQImage(hbmScreen, width, height);

    DeleteObject(hbmScreen);
    DeleteDC(hdcMemDC);
    ReleaseDC(m_targetWindow, hdcWindow);

    return imgae;
}

QPixmap DXWindowCapture::convertToQPixmap(HBITMAP hBitmap, int width, int height)
{
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Отрицательная высота для правильной ориентации
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    QImage image(width, height, QImage::Format_ARGB32);

    HDC hdc = CreateCompatibleDC(nullptr);
    int result = GetDIBits(hdc, hBitmap, 0, height, image.bits(), &bmi, DIB_RGB_COLORS);
    DeleteDC(hdc);

    if (result == 0) {
        return QPixmap();
    }

    QPixmap pixmap = QPixmap::fromImage(image);

    // Применяем область захвата если установлена
    if (m_useCustomArea && !m_captureArea.isNull()) {
        pixmap = pixmap.copy(m_captureArea);
    }

    return pixmap;
}

QImage DXWindowCapture::convertToQImage(HBITMAP hBitmap, int width, int height)
{
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Отрицательная высота для правильной ориентации
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    QImage image(width, height, QImage::Format_ARGB32);

    HDC hdc = CreateCompatibleDC(nullptr);
    int result = GetDIBits(hdc, hBitmap, 0, height, image.bits(), &bmi, DIB_RGB_COLORS);
    DeleteDC(hdc);

    if (result == 0) {
        return QImage();
    }

    // Применяем область захвата если установлена
    if (m_useCustomArea && !m_captureArea.isNull()) {
        image = image.copy(m_captureArea);
    }

    return image;
}

bool DXWindowCapture::isWindowValid() const
{
    return m_targetWindow && IsWindow(m_targetWindow);
}

void DXWindowCapture::cleanup()
{
    if (m_stagingTexture) {
        m_stagingTexture->Release();
        m_stagingTexture = nullptr;
    }

    if (m_dxgiDuplication) {
        m_dxgiDuplication->Release();
        m_dxgiDuplication = nullptr;
    }

    if (m_d3dContext) {
        m_d3dContext->Release();
        m_d3dContext = nullptr;
    }

    if (m_d3dDevice) {
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
    }

    m_dxgiSupported = false;
}

