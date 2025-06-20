#pragma once

#pragma comment(lib, "Msimg32.lib")

#include "backend/dxwindowcapture.h"

#include <QVBoxLayout>
#include <QScrollArea>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QLabel>
#include <QList>
#include <QRect>
#include <QDebug>

#include <vector>

#include <windows.h>
#include <winuser.h>

class Mediator;
class WindowSelecter;
class ImageBlender;
class ProcessOutput;

class Mediator : public QObject
{
    Q_OBJECT

public:

    explicit Mediator(QObject *parent = nullptr);
    ~Mediator();

public slots:
    void loadImagesToBuffer();
    void loadImagesToBuffer(const QList<QUrl> &list);
    void processStoredImages(int);
    void chagneThreadhold(int);

signals:
    void imageDataLoaded();

private:
    ImageBlender *imgBlender;
    WindowSelecter *windowSelecter;
    DXWindowCapture *capture;
    ProcessOutput *processOutput;

    QTimer *captureTimer;
    const int bufferSize = 100;
    int threshold = 30;
    int diffusionShift = 1;

    QVector<HBITMAP> frameBuffer;
    QVector<QImage> imageBuffer;


};


//------------------------------------------------------------------------------//
//                                                                              //
//------------------------------------------------------------------------------//


class WindowSelecter : public QObject
{
    Q_OBJECT

public:
    struct WinInfo{
        HWND id;
        QString title;
        QString titleClass;
        QRect size;
    };

public:

    explicit WindowSelecter(QObject *parent = nullptr);
    ~WindowSelecter();

    void scanAvaliableWindows();
    QList<WinInfo> getAvaliableList();

    //---------STATIC---------//
    static BOOL CALLBACK enumWindowCallback(HWND hwnd, LPARAM lParam);

private:
    QList<WinInfo> winList;
    WinInfo customFrame;

};


//------------------------------------------------------------------------------//
//                                                                              //
//------------------------------------------------------------------------------//


class ImageBlender : public QObject
{
    Q_OBJECT

public:
    explicit ImageBlender(QObject *parent = nullptr);
    ~ImageBlender();

    void showResult(const QImage& );

public slots:
    QImage differenceBlendTrail(const QVector<QImage> &images);
    QImage differenceBlendTrailV2(const QVector<QImage> &images);
    QImage differenceBlendTrailV3(const QVector<QImage> &images, int threshold = 60);
    QImage differenceBlendTrailV4(const QVector<QImage> &images, int threshold = 15);
    QImage differenceBlendTrailV4Fast(const QVector<QImage> &images, int threshold = 15);
    //threshold = 5 - более чувствительный к изменениям
    //threshold = 15-20 - менее чувствительный, игнорирует больше шума
    //threshold = 30+ - только значительные изменения

private:

};


//------------------------------------------------------------------------------//
//                                                                              //
//------------------------------------------------------------------------------//


class ProcessOutput : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessOutput(QWidget *parent = nullptr);
    ~ProcessOutput();

    void showResult(const QImage& );

signals:
    void captureAreaChanged(const QRect&);

public slots:
    void updateImageData(const QImage&);



private:
    QVBoxLayout *layout = nullptr;
    QScrollArea *scrollArea = nullptr;
    QLabel *label = nullptr;
    QPixmap *pixmap = nullptr;

};
