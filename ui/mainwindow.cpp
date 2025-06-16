#include "ui/mainwindow.h"
#include "./ui_mainwindow.h"




MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    Mediator *md = new Mediator(this);



    connect(ui->actionLoad, &QAction::triggered, md, QOverload<>::of(&Mediator::loadImagesToBuffer));
    connect(ui->labelDropArea, &DropArea::dropAreaFileReviced, md, QOverload<const QList<QUrl>&>::of(&Mediator::loadImagesToBuffer));
    connect(ui->spinBoxThreadhold, &QSpinBox::valueChanged, md, &Mediator::chagneThreadhold);
    connect(ui->comboBoxMethod, &QComboBox::activated, md, &Mediator::processStoredImages);
}

MainWindow::~MainWindow()
{
    delete ui;
}
