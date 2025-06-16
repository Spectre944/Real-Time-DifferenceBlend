#ifndef DXWINDOWCAPTURE_H
#define DXWINDOWCAPTURE_H

#include <QObject>
#include <QLabel>
#include <QTimer>
#include <QPixmap>
#include <QRect>
#include <QDebug>
#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <memory>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

class DXWindowCapture : public QObject
{
    Q_OBJECT

public:
    explicit DXWindowCapture(QObject *parent = nullptr);
    ~DXWindowCapture();

    // Основные методы
    bool initCapture(HWND targetWindow);
    void startCapture(int intervalMs = 1000);
    void stopCapture();

    void resetCaptureArea();

    // Получение информации
    QRect getWindowRect() const;
    bool isCapturing() const { return m_timer->isActive(); }
    QImage getLastScreenshot() const { return m_lastScreenshot; }

signals:
    void screenshotCaptured(const QImage& screenshot);
    void imageReady(const QImage& image);
    void captureError(const QString& error);

public slots:
    void setCaptureArea(const QRect& area);

private slots:
    void captureScreenshot();


private:
    // Инициализация DirectX
    bool initDirectX();
    void cleanup();

    // Методы захвата
    QImage captureWindow();
    QImage captureWithDWM();
    QImage captureWithBitBlt();
    QImage captureWithDXGI();

    // Утилиты
    QPixmap convertToQPixmap(HBITMAP hBitmap, int width, int height);
    QImage convertToQImage(HBITMAP hBitmap, int width, int height);
    bool isWindowValid() const;

private:
    HWND m_targetWindow;
    QTimer* m_timer;
    QRect m_captureArea;
    bool m_useCustomArea;
    QImage m_lastScreenshot;

    // DirectX объекты
    ID3D11Device* m_d3dDevice;
    ID3D11DeviceContext* m_d3dContext;
    IDXGIOutputDuplication* m_dxgiDuplication;
    ID3D11Texture2D* m_stagingTexture;

    // Флаги возможностей
    bool m_dxgiSupported;
    bool m_dwmSupported;
};


#endif // DXWINDOWCAPTURE_H
