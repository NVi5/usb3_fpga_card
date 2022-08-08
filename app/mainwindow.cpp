#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <QDebug>
#include "Windows.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), BulkInEpt(NULL), BulkOutEpt(NULL)
{
    this->selectedDevice = new CCyUSBDevice(NULL, CYUSBDRV_GUID, true);
    this->ui->setupUi(this);
    this->communication_enabled = false;

    this->get_devices();
    this->get_endpoint_for_device();

    QStringList items = {"fpga_counter0", "fpga_counter1", "fpga_counter2", "fpga_counter3", "fpga_counter4", "fpga_counter5", "fpga_counter6", "fpga_counter7"};
    QRegularExpression exp("ch[0-9]");
    QList<QComboBox *> channels = this->ui->channelBox->findChildren<QComboBox *>(exp);

    int counter=0;
    for(QComboBox *child : channels)
    {
        child->addItems(items);
        child->setCurrentIndex(counter++);
    }
}

MainWindow::~MainWindow()
{
    this->communication_enabled = false;
    delete ui;
    if (this->selectedDevice)
    {
        if (this->selectedDevice->IsOpen()) this->selectedDevice->Close();
        delete this->selectedDevice;
    }
}

void MainWindow::plot(UCHAR *buf)
{
    connect(ui->qplot->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(xAxisChanged(QCPRange)));
    connect(ui->qplot->yAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(yAxisChanged(QCPRange)));
//    ui->qplot->legend->setVisible(true);
    ui->qplot->legend->setFont(QFont("Helvetica", 9));
    QPen pen;
    QStringList lineNames;
//    lineNames << "lsNone" << "lsLine" << "lsStepLeft" << "lsStepRight" << "lsStepCenter" << "lsImpulse";

    for (int i=0; i<8; ++i)
    {
        ui->qplot->addGraph();
        pen.setColor(QColor(qSin(i*1+1.2)*80+80, qSin(i*0.3+0)*80+80, qSin(i*0.3+1.5)*80+80));
        ui->qplot->graph()->setPen(pen);
//        ui->qplot->graph()->setName(lineNames.at(i-QCPGraph::lsNone));
        ui->qplot->graph()->setLineStyle(QCPGraph::lsStepLeft);

        QVector<double> x(16384/4), y(16384/4);
        for (int j=0; j<16384/4; ++j)
        {
            x[j] = j;
            y[j] = (double)((buf[j*4] >> i) & 1) + i*2;
        }
        ui->qplot->graph()->setData(x, y);
        ui->qplot->graph()->rescaleAxes(true);
    }
//    // zoom out a bit:
    ui->qplot->yAxis->scaleRange(1.1, ui->qplot->yAxis->range().center());
//    ui->qplot->xAxis->scaleRange(1.1, ui->qplot->xAxis->range().center());
    // set blank axis lines:
    ui->qplot->xAxis->setTicks(true);
    ui->qplot->yAxis->setTicks(true);
    ui->qplot->xAxis->setTickLabels(true);
    ui->qplot->yAxis->setTickLabels(false);
    ui->qplot->xAxis->setVisible(true);
    ui->qplot->yAxis->setVisible(true);
    // make top right axes clones of bottom left axes:
    ui->qplot->axisRect()->setupFullAxesBox();
    ui->qplot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    ui->qplot->axisRect()->setRangeDrag(Qt::Horizontal);
    ui->qplot->axisRect()->setRangeZoom(Qt::Horizontal);

    ui->qplot->setProperty("xmin", ui->qplot->xAxis->range().lower);
    ui->qplot->setProperty("xmax", ui->qplot->xAxis->range().upper);
    ui->qplot->setProperty("ymin", ui->qplot->yAxis->range().lower);
    ui->qplot->setProperty("ymax", ui->qplot->yAxis->range().upper);
}

void MainWindow::xAxisChanged(const QCPRange & newRange)
{
    QCPAxis * axis = qobject_cast<QCPAxis *>(QObject::sender());
    QCustomPlot * plot = axis->parentPlot();

    QCPRange limitRange(plot->property("xmin").toDouble(), plot->property("xmax").toDouble());
    limitAxisRange(axis, newRange, limitRange);
}

void MainWindow::yAxisChanged(const QCPRange & newRange)
{
    QCPAxis * axis = qobject_cast<QCPAxis *>(QObject::sender());
    QCustomPlot * plot = axis->parentPlot();

    QCPRange limitRange(plot->property("ymin").toDouble(), plot->property("ymax").toDouble());
    limitAxisRange(axis, newRange, limitRange);
}


void MainWindow::limitAxisRange(QCPAxis * axis, const QCPRange & newRange, const QCPRange & limitRange)
{
    auto lowerBound = limitRange.lower;
    auto upperBound = limitRange.upper;

    // code assumes upperBound > lowerBound
    QCPRange fixedRange(newRange);
    if (fixedRange.lower < lowerBound)
    {
        fixedRange.lower = lowerBound;
        fixedRange.upper = lowerBound + newRange.size();
        if (fixedRange.upper > upperBound || qFuzzyCompare(newRange.size(), upperBound-lowerBound))
            fixedRange.upper = upperBound;
        axis->setRange(fixedRange); // adapt this line to use your plot/axis
    } else if (fixedRange.upper > upperBound)
    {
        fixedRange.upper = upperBound;
        fixedRange.lower = upperBound - newRange.size();
        if (fixedRange.lower < lowerBound || qFuzzyCompare(newRange.size(), upperBound-lowerBound))
            fixedRange.lower = lowerBound;
        axis->setRange(fixedRange); // adapt this line to use your plot/axis
    }
}

void MainWindow::send_bulk(unsigned char state)
{
    Q_ASSERT(this->BulkOutEpt);
    if (!this->communication_enabled) return;

    LONG packet_length = 64;
    UCHAR buf[64] = {state, 0xFF, 0};
    BOOL status = this->BulkOutEpt->XferData(buf, packet_length);

    qDebug() << "Sent: " << state << "Result: " << status;
}

bool MainWindow::read_bulk(unsigned char *state)
{
    Q_ASSERT(this->BulkInEpt);
    bool status = true;
    UCHAR inBuf[16*1024];
    LONG packet_length = 16*1024;

    if (!this->communication_enabled) return FALSE;

    OVERLAPPED inOvLap;
    inOvLap.hEvent = CreateEvent(NULL, false, false, L"CYUSB_IN");

    UCHAR *inContext = this->BulkInEpt->BeginDataXfer(inBuf, packet_length, &inOvLap);
    if(!this->BulkInEpt->WaitForXfer(&inOvLap, 1500))
    {
        status = false;

        this->BulkInEpt->Abort();
        if (this->BulkInEpt->LastError == ERROR_IO_PENDING)
        {
            qDebug() << "BulkInEpt ERROR_IO_PENDING";
            WaitForSingleObject(inOvLap.hEvent, 1500);
        }
        else
        {
            qDebug() << "BulkInEpt unknown error - " << this->BulkInEpt->LastError;
            this->ui->lb_status->setText("Error");
        }
    }
    this->BulkInEpt->FinishDataXfer(inBuf, packet_length, &inOvLap, inContext);

    CloseHandle(inOvLap.hEvent);

    if(status)
    {
        qDebug("Received - %d", inBuf[0]);
    }

    if (state)
    {
        *state = inBuf[0];
    }

    this->plot(inBuf);

    return status;
}

void MainWindow::select_endpoints(void)
{
    QString strINData, strOutData;
    bool ok;
    int inEpAddress = 0x0, outEpAddress = 0x0;

    strINData = this->ui->cb_in_ept->currentText();
    strOutData = this->ui->cb_out_ept->currentText();

    strINData = strINData.right(4);
    strOutData = strOutData.right(4);

    inEpAddress = strINData.toInt(&ok, 16);
    Q_ASSERT(ok);
    outEpAddress = strOutData.toInt(&ok, 16);
    Q_ASSERT(ok);

    BulkOutEpt = this->selectedDevice->EndPointOf(outEpAddress);
    BulkInEpt = this->selectedDevice->EndPointOf(inEpAddress);
}

bool MainWindow::get_devices()
{
    CCyUSBDevice *USBDevice;
    USBDevice = new CCyUSBDevice(NULL, CYUSBDRV_GUID, true);
    QString strDevice;
    int nCboIndex = -1;
    if (this->ui->cb_device->count() > 0 ) strDevice = this->ui->cb_device->currentText();

    this->ui->cb_device->clear();

    if (USBDevice == NULL) return FALSE;

    for (int nCount = 0; nCount < USBDevice->DeviceCount(); nCount++ )
    {
        USBDevice->Open(nCount);

        QString strDeviceData;
        strDeviceData += QString("(0x%1 - 0x%2) ").arg(USBDevice->VendorID, 0 ,16).arg(USBDevice->ProductID, 0 ,16);
        strDeviceData += QString(USBDevice->FriendlyName);
        qDebug() << strDeviceData;

        this->ui->cb_device->insertItem(nCount, strDeviceData);

        if (nCboIndex == -1 && strDevice.isEmpty() == FALSE && strDevice == strDeviceData)
            nCboIndex = nCount;

        USBDevice->Close();
    }
    delete USBDevice;
    if (this->ui->cb_device->count() >= 1 )
    {
        if (nCboIndex != -1 ) this->ui->cb_device->setCurrentIndex(nCboIndex);
        else this->ui->cb_device->setCurrentIndex(0);
    }

    return TRUE;
}

bool MainWindow::get_endpoint_for_device()
{
    int nDeviceIndex = this->ui->cb_device->currentIndex();

    this->ui->cb_in_ept->clear();
    this->ui->cb_out_ept->clear();

    // Is there any FX device connected to system?
    if (nDeviceIndex == -1 || this->selectedDevice == NULL )
    {
        qDebug() << "get_endpoint_for_device - No devices selected";
        return FALSE;
    }

    this->selectedDevice->Open(nDeviceIndex);
    int interfaces = this->selectedDevice->AltIntfcCount()+1;

    for (int nDeviceInterfaces = 0; nDeviceInterfaces < interfaces; nDeviceInterfaces++ )
    {
        this->selectedDevice->SetAltIntfc(nDeviceInterfaces);
        int eptCnt = this->selectedDevice->EndPointCount();

        for (int endPoint = 1; endPoint < eptCnt; endPoint++)
        {
            CCyUSBEndPoint *ept = this->selectedDevice->EndPoints[endPoint];

            if (ept->Attributes == 2)
            {
                QString strData;
                strData += ((ept->Attributes == 1) ? L"ISOC " : ((ept->Attributes == 2) ? L"BULK " : L"INTR "));
                strData += (ept->bIn ? L"IN, " : L"OUT, ");
                strData += QString("AltInt - %1 and EpAddr - 0x%2").arg(nDeviceInterfaces).arg(ept->Address, 0 ,16);

                qDebug() << strData;

                if (ept->bIn ) this->ui->cb_in_ept->addItem(strData);
                else this->ui->cb_out_ept->addItem(strData);
            }
        }
    }

    if (this->ui->cb_in_ept->count() > 0 ) this->ui->cb_in_ept->setCurrentIndex(0);
    if (this->ui->cb_out_ept->count() > 0 ) this->ui->cb_out_ept->setCurrentIndex(0);

    return this->ui->cb_in_ept->count() > 0 && this->ui->cb_out_ept->count() > 0;
}

void MainWindow::on_btn_select_clicked()
{
    qDebug() << "on_btn_select_clicked";

    this->select_endpoints();
    this->send_bulk(0);
    this->communication_enabled = true;
    this->ui->lb_status->setText("Selected");
}


void MainWindow::on_btn_refresh_clicked()
{
    qDebug() << "on_btn_refresh_clicked";

    this->get_devices();
}


void MainWindow::on_cb_device_currentIndexChanged(int index)
{
    qDebug() << "on_cb_device " << index;

    this->get_endpoint_for_device();
}


void MainWindow::on_cb_in_ept_currentIndexChanged(int index)
{
    qDebug() << "on_cb_in_ept " << index;
}


void MainWindow::on_cb_out_ept_currentIndexChanged(int index)
{
    qDebug() << "on_cb_out_ept " << index;
}

void MainWindow::on_start_btn_clicked()
{
    qDebug() << "on_start_btn_clicked";

    this->send_bulk(0);
    this->read_bulk(NULL);
}
