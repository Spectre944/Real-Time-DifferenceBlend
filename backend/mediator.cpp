#include <backend/mediator.h>

Mediator::Mediator(QObject *parent) : QObject(parent)
{
    imgBlender = new ImageBlender(this);
    windowSelecter = new WindowSelecter(this);
    capture = new DXWindowCapture(this);
    processOutput = new ProcessOutput();

    windowSelecter->scanAvaliableWindows();
    QList<WindowSelecter::WinInfo> avaliableWindows = windowSelecter->getAvaliableList();

    capture->initCapture(avaliableWindows[1].id);
    capture->startCapture(16);

    connect(this, &Mediator::imageDataLoaded, this, [=](){
        QImage result = imgBlender->differenceBlendTrailV4Fast(imageBuffer, threshold);
        imgBlender->showResult(result);
    });

    connect(capture, &DXWindowCapture::screenshotCaptured, processOutput, &ProcessOutput::updateImageData);
    //connect(processOutput, &ProcessOutput::captureAreaChanged, capture, &DXWindowCapture::setCaptureArea);
}

Mediator::~Mediator()
{
    delete imgBlender;
    delete windowSelecter;
    delete capture;
    delete processOutput;
}

void Mediator::loadImagesToBuffer()
{
    imageBuffer.clear();

    QStringList fileNames = QFileDialog::getOpenFileNames(
        nullptr, "Select one or more images", QDir::homePath(),
        "Images (*.png *.jpg *.jpeg *.bmp *.gif)");

    foreach (const QString &fileName, fileNames) {
        QImage image(fileName);
        if (!image.isNull()) {
            imageBuffer.append(image);
        } else {
            qDebug() << "Failed to load image:" << fileName;
        }
    }

    emit imageDataLoaded();
}

void Mediator::loadImagesToBuffer(const QList<QUrl> &list)
{
    imageBuffer.clear();

    foreach (const QUrl &fileUrl, list) {
        QString filePath = fileUrl.toLocalFile(); // Преобразуем QUrl в путь
        QImage image;
        if (image.load(filePath)) { // Используем load() для загрузки
            imageBuffer.append(image);
        } else {
            qDebug() << "Failed to load image:" << filePath;
        }
    }

    emit imageDataLoaded();
}

void Mediator::processStoredImages(int mode)
{
    QImage result;
    switch (mode) {
        case 0: result = imgBlender->differenceBlendTrail(imageBuffer); break;
        case 1: result = imgBlender->differenceBlendTrailV2(imageBuffer); break;
        case 2: result = imgBlender->differenceBlendTrailV3(imageBuffer, threshold); break;
        case 3: result = imgBlender->differenceBlendTrailV4(imageBuffer, threshold); break;
        case 4: result = imgBlender->differenceBlendTrailV4Fast(imageBuffer, threshold); break;
        default:    break;
    }

    imgBlender->showResult(result);
}

void Mediator::chagneThreadhold(int value)
{
    threshold = value;
}

//------------------------------------------------------------------------------//
//                                                                              //
//------------------------------------------------------------------------------//

WindowSelecter::WindowSelecter(QObject *parent)
{

}

WindowSelecter::~WindowSelecter()
{

}

void WindowSelecter::scanAvaliableWindows()
{
    /*

    */
    winList.clear();
    EnumWindows(enumWindowCallback, reinterpret_cast<LPARAM>(&winList));
    //qDebug() << "Avaliable windows: " << winList;
}

QList<WindowSelecter::WinInfo> WindowSelecter::getAvaliableList()
{
    return winList;
}

BOOL WindowSelecter::enumWindowCallback(HWND hwnd, LPARAM lParam)
{
    /*

    */
    WinInfo current;
    auto windows = reinterpret_cast<QList<WinInfo>*>(lParam);

    if (!IsWindowVisible(hwnd))
        return TRUE;

    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);

    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);

    RECT rect;
    GetWindowRect(hwnd, &rect);

    if (wcslen(title) > 0) {

        current.id          = hwnd;
        current.title       = QString::fromWCharArray(title);
        current.titleClass  = QString::fromWCharArray(className);
        current.size        = QRect(QPoint(rect.left, rect.top), QPoint(rect.right, rect.bottom));

        windows->append(current);
    }

    return TRUE;
}


//------------------------------------------------------------------------------//
//                                                                              //
//------------------------------------------------------------------------------//


ImageBlender::ImageBlender(QObject *parent)
{

}

ImageBlender::~ImageBlender()
{

}

void ImageBlender::showResult(const QImage &result)
{
    QWidget *window = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(window);

    QScrollArea *scrollArea = new QScrollArea();
    QLabel *label = new QLabel();
    QPixmap pixmap = QPixmap::fromImage(result);

    label->setPixmap(pixmap);
    label->setScaledContents(true); // Позволяет динамически изменять размер изображения
    scrollArea->setWidget(label);
    scrollArea->setWidgetResizable(true); // Позволяет изменять размер изображения внутри окна

    layout->addWidget(scrollArea);
    window->setLayout(layout);
    window->resize(800, 600); // Размер по умолчанию
    window->show();

    // Сохранение изображения
    QString savePath = "result.png"; // Можно задать динамически
    //result.save(savePath);

}

QImage ImageBlender::differenceBlendTrail(const QVector<QImage> &images) {
    if (images.isEmpty()) return QImage(); // Проверка на пустой список

    QElapsedTimer timer;
    timer.start();

    QImage result(images[0].size(), QImage::Format_ARGB32);
    result.fill(Qt::black); // Заполняем черным

    for (int i = 1; i < images.size(); ++i) {
        for (int y = 0; y < images[i].height(); ++y) {
            for (int x = 0; x < images[i].width(); ++x) {
                QColor prevPixel = images[i - 1].pixelColor(x, y);
                QColor currPixel = images[i].pixelColor(x, y);
                QColor resultPixel = result.pixelColor(x, y);

                int r = qMax(resultPixel.red(), abs(currPixel.red()  - prevPixel.red()));
                int g = qMax(resultPixel.green(), abs(currPixel.green() - prevPixel.green()));
                int b = qMax(resultPixel.blue(),  abs(currPixel.blue() - prevPixel.blue()));

                result.setPixelColor(x, y, QColor(r, g, b, 255)); // Сохраняем максимум изменения
            }
        }
    }

    qDebug() << "Время выполнения differenceBlendTrail:" << timer.elapsed() << "мс";

    return result;
}

QImage ImageBlender::differenceBlendTrailV2(const QVector<QImage> &images) {
    if (images.isEmpty()) return QImage();

    QElapsedTimer timer;
    timer.start();

    const QImage &firstImage = images[0];
    QImage result(firstImage.size(), QImage::Format_ARGB32);
    result.fill(Qt::black);

    // Проверяем, что все изображения имеют одинаковый размер и формат
    for (int i = 1; i < images.size(); ++i) {
        if (images[i].size() != firstImage.size()) {
            qWarning() << "Image sizes don't match!";
            return result;
        }
    }

    const int width = firstImage.width();
    const int height = firstImage.height();

    // Получаем прямой доступ к данным пикселей
    QRgb* resultData = reinterpret_cast<QRgb*>(result.bits());

    for (int i = 1; i < images.size(); ++i) {
        // Конвертируем в нужный формат для быстрого доступа
        QImage prevImg = images[i - 1].convertToFormat(QImage::Format_ARGB32);
        QImage currImg = images[i].convertToFormat(QImage::Format_ARGB32);

        const QRgb* prevData = reinterpret_cast<const QRgb*>(prevImg.constBits());
        const QRgb* currData = reinterpret_cast<const QRgb*>(currImg.constBits());

        // Обрабатываем пиксели напрямую через указатели
        for (int pixelIndex = 0; pixelIndex < width * height; ++pixelIndex) {
            const QRgb prevPixel = prevData[pixelIndex];
            const QRgb currPixel = currData[pixelIndex];
            const QRgb resultPixel = resultData[pixelIndex];

            // Извлекаем компоненты цвета через битовые операции (быстрее)
            const int prevR = (prevPixel >> 16) & 0xFF;
            const int prevG = (prevPixel >> 8) & 0xFF;
            const int prevB = prevPixel & 0xFF;

            const int currR = (currPixel >> 16) & 0xFF;
            const int currG = (currPixel >> 8) & 0xFF;
            const int currB = currPixel & 0xFF;

            const int resultR = (resultPixel >> 16) & 0xFF;
            const int resultG = (resultPixel >> 8) & 0xFF;
            const int resultB = resultPixel & 0xFF;

            // Вычисляем разность и максимум
            const int diffR = abs(currR - prevR);
            const int diffG = abs(currG - prevG);
            const int diffB = abs(currB - prevB);

            const int newR = qMax(resultR, diffR);
            const int newG = qMax(resultG, diffG);
            const int newB = qMax(resultB, diffB);

            // Собираем новый пиксель через битовые операции
            resultData[pixelIndex] = 0xFF000000 | (newR << 16) | (newG << 8) | newB;
        }
    }

    qDebug() << "Время выполнения differenceBlendTrailV2 (оптимизировано):" << timer.elapsed() << "мс";
    return result;
}

QImage ImageBlender::differenceBlendTrailV3(const QVector<QImage> &images, int threshold) {
    if (images.isEmpty()) return QImage();

    QElapsedTimer timer;
    timer.start();

    const QImage &firstImage = images[0];
    QImage result(firstImage.size(), QImage::Format_ARGB32);
    result.fill(Qt::black);

    // Проверяем, что все изображения имеют одинаковый размер и формат
    for (int i = 1; i < images.size(); ++i) {
        if (images[i].size() != firstImage.size()) {
            qWarning() << "Image sizes don't match!";
            return result;
        }
    }

    const int width = firstImage.width();
    const int height = firstImage.height();
    const int totalPixels = width * height;

    // Получаем прямой доступ к данным пикселей
    QRgb* resultData = reinterpret_cast<QRgb*>(result.bits());

    for (int i = 1; i < images.size(); ++i) {
        // Конвертируем в нужный формат для быстрого доступа
        QImage prevImg = images[i - 1].convertToFormat(QImage::Format_ARGB32);
        QImage currImg = images[i].convertToFormat(QImage::Format_ARGB32);

        const QRgb* prevData = reinterpret_cast<const QRgb*>(prevImg.constBits());
        const QRgb* currData = reinterpret_cast<const QRgb*>(currImg.constBits());

        // Обрабатываем пиксели напрямую через указатели
        for (int pixelIndex = 0; pixelIndex < totalPixels; ++pixelIndex) {
            const QRgb prevPixel = prevData[pixelIndex];
            const QRgb currPixel = currData[pixelIndex];

            // Быстрая проверка на различие - если пиксели одинаковые, пропускаем
            if (prevPixel == currPixel) {
                continue;
            }

            // Извлекаем компоненты цвета через битовые операции
            const int prevR = (prevPixel >> 16) & 0xFF;
            const int prevG = (prevPixel >> 8) & 0xFF;
            const int prevB = prevPixel & 0xFF;

            const int currR = (currPixel >> 16) & 0xFF;
            const int currG = (currPixel >> 8) & 0xFF;
            const int currB = currPixel & 0xFF;

            // Вычисляем разность
            const int diffR = abs(currR - prevR);
            const int diffG = abs(currG - prevG);
            const int diffB = abs(currB - prevB);

            // Вычисляем общую разность (можно использовать разные метрики)
            const int totalDiff = qMax(qMax(diffR, diffG), diffB); // Максимальная разность по каналам
            // Альтернатива: const int totalDiff = (diffR + diffG + diffB) / 3; // Средняя разность

            // Если разность превышает порог, устанавливаем белый пиксель
            if (totalDiff > threshold) {
                resultData[pixelIndex] = 0xFFFFFFFF; // Белый цвет (ARGB: 255,255,255,255)
            }
        }
    }

    qDebug() << "Время выполнения differenceBlendTrailV3 (с проверкой различий):" << timer.elapsed() << "мс";
    return result;
}

QImage ImageBlender::differenceBlendTrailV4(const QVector<QImage> &images, int threshold) {
    if (images.isEmpty()) return QImage();

    QElapsedTimer timer;
    timer.start();

    const QImage &firstImage = images[0];
    QImage result(firstImage.size(), QImage::Format_ARGB32);
    result.fill(Qt::black);

    // Проверяем, что все изображения имеют одинаковый размер
    for (int i = 1; i < images.size(); ++i) {
        if (images[i].size() != firstImage.size()) {
            qWarning() << "Image sizes don't match!";
            return result;
        }
    }

    const int width = firstImage.width();
    const int height = firstImage.height();
    const int totalPixels = width * height;

    // Получаем прямой доступ к данным результата
    QRgb* resultData = reinterpret_cast<QRgb*>(result.bits());

    for (int i = 1; i < images.size(); ++i) {
        // Конвертируем изображения в серый формат для быстрого сравнения
        QImage prevGray = images[i - 1].convertToFormat(QImage::Format_Grayscale8);
        QImage currGray = images[i].convertToFormat(QImage::Format_Grayscale8);

        const uchar* prevData = prevGray.constBits();
        const uchar* currData = currGray.constBits();

        // Обрабатываем пиксели как байты (намного быстрее)
        for (int pixelIndex = 0; pixelIndex < totalPixels; ++pixelIndex) {
            const uchar prevGrayValue = prevData[pixelIndex];
            const uchar currGrayValue = currData[pixelIndex];

            // Быстрая проверка на различие
            if (prevGrayValue == currGrayValue) {
                continue;
            }

            // Вычисляем разность яркости
            const int diff = abs(static_cast<int>(currGrayValue) - static_cast<int>(prevGrayValue));

            // Если разность превышает порог, устанавливаем белый пиксель
            if (diff > threshold) {
                resultData[pixelIndex] = 0xFFFFFFFF; // Белый цвет
            }
        }
    }

    qDebug() << "Время выполнения differenceBlendTrailV4 (серый, оптимизированный):" << timer.elapsed() << "мс";
    return result;
}

// Альтернативная версия с ручной конвертацией в серый (еще быстрее)
QImage ImageBlender::differenceBlendTrailV4Fast(const QVector<QImage> &images, int threshold) {
    if (images.isEmpty()) return QImage();

    QElapsedTimer timer;
    timer.start();

    const QImage &firstImage = images[0];
    QImage result(firstImage.size(), QImage::Format_ARGB32);
    result.fill(Qt::black);

    const int width = firstImage.width();
    const int height = firstImage.height();
    const int totalPixels = width * height;

    QRgb* resultData = reinterpret_cast<QRgb*>(result.bits());

    for (int i = 1; i < images.size(); ++i) {
        // Конвертируем в ARGB32 для прямого доступа
        QImage prevImg = images[i - 1].convertToFormat(QImage::Format_ARGB32);
        QImage currImg = images[i].convertToFormat(QImage::Format_ARGB32);

        const QRgb* prevData = reinterpret_cast<const QRgb*>(prevImg.constBits());
        const QRgb* currData = reinterpret_cast<const QRgb*>(currImg.constBits());

        for (int pixelIndex = 0; pixelIndex < totalPixels; ++pixelIndex) {
            const QRgb prevPixel = prevData[pixelIndex];
            const QRgb currPixel = currData[pixelIndex];

            // Быстрая проверка на полное совпадение
            if (prevPixel == currPixel) {
                continue;
            }

            // Быстрое вычисление яркости через битовые операции
            // Используем приближенную формулу: Gray = (R + G + B) / 3
            const int prevGray = (((prevPixel >> 16) & 0xFF) + ((prevPixel >> 8) & 0xFF) + (prevPixel & 0xFF)) / 3;
            const int currGray = (((currPixel >> 16) & 0xFF) + ((currPixel >> 8) & 0xFF) + (currPixel & 0xFF)) / 3;

            const int diff = abs(currGray - prevGray);

            if (diff > threshold) {
                //resultData[pixelIndex] = 0xFFFFFFFF;  // Заполнение белым
                resultData[pixelIndex] = currPixel;     // Заполнение цветным
            }
        }
    }

    qDebug() << "Время выполнения differenceBlendTrailV4Fast (быстрая серая):" << timer.elapsed() << "мс";
    return result;
}

//------------------------------------------------------------------------------//
//                                                                              //
//------------------------------------------------------------------------------//




ProcessOutput::ProcessOutput(QWidget *parent)
{
    setWindowFlags(Qt::Window);
    setWindowFlags(Qt::WindowStaysOnTopHint);

    QVBoxLayout *layout = new QVBoxLayout(this);

    scrollArea = new QScrollArea();
    label = new QLabel();
    pixmap = new QPixmap();

    label->setPixmap(*pixmap);
    label->setScaledContents(true); // Позволяет динамически изменять размер изображения
    scrollArea->setWidget(label);
    scrollArea->setWidgetResizable(true); // Позволяет изменять размер изображения внутри окна

    layout->addWidget(scrollArea);
    setLayout(layout);
    resize(800, 600); // Размер по умолчанию
    show();
}

ProcessOutput::~ProcessOutput()
{

}

void ProcessOutput::showResult(const QImage &result)
{

}

void ProcessOutput::updateImageData(const QImage &imageNew)
{
    *pixmap = QPixmap::fromImage(imageNew);
    label->setPixmap(*pixmap);
}

