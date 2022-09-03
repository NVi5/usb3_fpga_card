#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <QDebug>
#include <QColor>
#include <QElapsedTimer>
#include "Windows.h"

// #define DEBUG_PATTERN
#define DEBUG_TIME

#define PACKET_SIZE (16*1024)
#define RX_PACKETS_PER_TRANSFER (8)
#define TX_PACKETS_PER_TRANSFER (1)

#define RX_TRANSFER_SIZE (PACKET_SIZE*RX_PACKETS_PER_TRANSFER)
#define TX_TRANSFER_SIZE (PACKET_SIZE*TX_PACKETS_PER_TRANSFER)

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), BulkInEpt(NULL), BulkOutEpt(NULL)
{
    this->selectedDevice = new CCyUSBDevice(NULL, CYUSBDRV_GUID, true);
    this->ui->setupUi(this);
    this->communication_enabled = false;
    this->data_buffer.clear();

    this->get_devices();
    this->get_endpoint_for_device();

    QStringList items = {"fpga_counter0", "fpga_counter1", "fpga_counter2", "fpga_counter3", "fpga_counter4", "fpga_counter5", "fpga_counter6", "fpga_counter7", "disabled", "PB0", "PB1", "PB2", "PB3"};
    QRegularExpression exp("ch[0-7]");
    QList<QComboBox *> channels = ui->channelBox->findChildren<QComboBox *>(exp);

    int counter=0;
    for(QComboBox *child : channels)
    {
        child->addItems(items);
        child->setCurrentIndex(counter++);
    }

    ui->time_label->setText(QString("%1ms").arg(ui->packet_slider->value()*8*(1/200000*16384)));
    ui->packet_label->setText(QString("16kB packets: %1").arg(ui->packet_slider->value()*8));

    this->init_plot();
}

MainWindow::~MainWindow()
{
    this->communication_enabled = false;
    this->data_buffer.clear();
    delete ui;
    if (this->selectedDevice)
    {
        if (this->selectedDevice->IsOpen()) this->selectedDevice->Close();
        delete this->selectedDevice;
    }
}

void MainWindow::init_plot()
{
    QList<QColor> colorList = {
        QColorConstants::Svg::mediumblue,
        QColorConstants::Svg::darkmagenta,
        QColorConstants::Svg::darkgreen,
        QColorConstants::Svg::deeppink,
        QColorConstants::Svg::indigo,
        QColorConstants::Svg::mediumaquamarine,
        QColorConstants::Svg::orange,
        QColorConstants::Svg::maroon,
    };
    QPen pen;
    QStringList lineNames = {"CH0", "CH1", "CH2", "CH3", "CH4", "CH5", "CH6", "CH7"};

    connect(ui->qplot->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(xAxisChanged(QCPRange)));
    connect(ui->qplot->yAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(yAxisChanged(QCPRange)));

    ui->qplot->legend->setVisible(true);
    ui->qplot->legend->setFont(QFont("Helvetica", 9));

    for (int i=0; i<8; ++i)
    {
        ui->qplot->addGraph();
        pen.setColor(colorList.at(i));
        pen.setWidthF(1);
        ui->qplot->graph()->setPen(pen);
        ui->qplot->graph()->setName(lineNames.at(i));
        ui->qplot->graph()->setLineStyle(QCPGraph::lsStepLeft);

        QVector<double> x(2), y(2);
        x[0] = 0;
        x[1] = 5;
        y[0] = (7-i)*2.5;
        y[1] = y[0];

        ui->qplot->graph()->setData(x, y, TRUE);
        ui->qplot->graph()->rescaleAxes(true);
        ui->qplot->graph()->setAdaptiveSampling(TRUE);
    }

    // zoom out a bit
    ui->qplot->yAxis->scaleRange(1.1, ui->qplot->yAxis->range().center());

    // set blank axis lines
    ui->qplot->xAxis->setTicks(true);
    ui->qplot->yAxis->setTicks(true);
    ui->qplot->xAxis->setTickLabels(true);
    ui->qplot->yAxis->setTickLabels(false);
    ui->qplot->xAxis->setVisible(true);
    ui->qplot->yAxis->setVisible(true);
    ui->qplot->yAxis->ticker()->setTickCount(8);

    // make top right axes clones of bottom left axes
    ui->qplot->axisRect()->setupFullAxesBox();
    ui->qplot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    ui->qplot->axisRect()->setRangeDrag(Qt::Horizontal);
    ui->qplot->axisRect()->setRangeZoom(Qt::Horizontal);

    // add zoom limits
    ui->qplot->setProperty("xmin", ui->qplot->xAxis->range().lower);
    ui->qplot->setProperty("xmax", ui->qplot->xAxis->range().upper);
}

void MainWindow::update_plot(QList<unsigned char> &new_data)
{
    QVector<double> *x_vect = new QVector<double>;
    QVector<double> *y_vect = new QVector<double>;
    x_vect->reserve(new_data.size());
    y_vect->reserve(new_data.size());

    for (int j=0; j<new_data.size(); ++j)
    {
        x_vect->insert(j, j);
    }

    for (int i=0; i<8; ++i)
    {
        for (int j=0; j<new_data.size(); ++j)
        {
            y_vect->insert(j,((new_data.at(j) >> i) & 1)*1.25 + i*2.5);
        }

        ui->qplot->graph(7-i)->setData(*x_vect, *y_vect, TRUE);

        y_vect->clear();
    }

    delete x_vect;
    delete y_vect;

    ui->qplot->setProperty("xmin", 0);
    ui->qplot->setProperty("xmax", new_data.size());

    ui->qplot->rescaleAxes();
    ui->qplot->yAxis->scaleRange(1.1, ui->qplot->yAxis->range().center());

    ui->qplot->replot();

    ui->lb_status->setText("Plot ready");
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

bool MainWindow::send_bulk(QList<unsigned char> &tx_buf)
{
    Q_ASSERT(BulkOutEpt);
    if (!this->communication_enabled || tx_buf.size() > TX_TRANSFER_SIZE) return FALSE;

    LONG packet_length = TX_TRANSFER_SIZE;
    UCHAR outBuf[TX_TRANSFER_SIZE];

    for (int i=0; i<tx_buf.size(); i++)
    {
        outBuf[i] = tx_buf.at(i);
    }

    BOOL status = BulkOutEpt->XferData(outBuf, packet_length);

    qDebug() << "Sent:" << tx_buf << "Result:" << status;

    return status;
}

bool MainWindow::send_and_read_bulk(QList<unsigned char> &tx_buf, QList<unsigned char> &rx_buf, unsigned char packets_to_read)
{
    Q_ASSERT(BulkInEpt);
    if (!this->communication_enabled) return FALSE;

#ifdef DEBUG_TIME
    QElapsedTimer timer;
    qint64 elapsed_time;
#endif /* DEBUG_PATTERN */

    unsigned char valid_transfers_ctr = 0;
    unsigned char queue_size = packets_to_read/RX_PACKETS_PER_TRANSFER;
    bool status = true;
    OVERLAPPED *inOvLap = new OVERLAPPED[queue_size];
    // OVERLAPPED inOvLap[255];
    PUCHAR *inContext = new PUCHAR[queue_size];
    PUCHAR *inBuf = new PUCHAR[queue_size];

    LONG packet_length = RX_TRANSFER_SIZE;

    rx_buf.clear();


    for (int i=0; i < queue_size; i++)
    {
        inBuf[i] = new UCHAR[RX_TRANSFER_SIZE];
        inOvLap[i].hEvent = CreateEvent(NULL, false, false, NULL);
    }

    for (int i=0; i < queue_size; i++)
    {
        inContext[i] = BulkInEpt->BeginDataXfer(inBuf[i], packet_length, &inOvLap[i]);
        if (BulkInEpt->NtStatus || BulkInEpt->UsbdStatus) // BeginDataXfer failed
        {
            qDebug() << "Xfer request rejected. NTSTATUS =" << BulkInEpt->NtStatus;
            status = FALSE;
        }
    }

#ifdef DEBUG_TIME
    timer.start();
#endif /* DEBUG_PATTERN */

    qDebug() << "send_bulk result : " << this->send_bulk(tx_buf);

    if (status)
    {
        for (int q_ctr=0; q_ctr < queue_size; q_ctr++)
        {
            LONG r_packet_length = packet_length; // Reset each time because FinishDataXfer may modify it

            if(!BulkInEpt->WaitForXfer(&inOvLap[q_ctr], 1500))
            {
                status = false;

                BulkInEpt->Abort();
                if (BulkInEpt->LastError == ERROR_IO_PENDING)
                {
                    qDebug() << "WaitForXfer ERROR_IO_PENDING";
                    WaitForSingleObject(inOvLap[q_ctr].hEvent, 2000);
                }
                else
                {
                    qDebug() << "WaitForXfer unknown error -" << BulkInEpt->LastError;
                    ui->lb_status->setText("Error");
                }
            }

            if (!BulkInEpt->FinishDataXfer(inBuf[q_ctr], r_packet_length, &inOvLap[q_ctr], inContext[q_ctr]))
            {
                qDebug() << "FinishDataXfer Error";
                status = false;
            }

            if (status)
            {
                valid_transfers_ctr++;
            }
        }
    }

#ifdef DEBUG_TIME
    elapsed_time = timer.nsecsElapsed();
    qDebug() << "Elapsed time:" << elapsed_time/1000 << "us Transferred bytes:" << (qint64)packets_to_read*16*1024*8;
    qDebug() << "Estimated speed:" << (qint64)packets_to_read*16*8*1000*1000*1000/elapsed_time/1024 << "Mbps";
#endif /* DEBUG_PATTERN */

    BulkInEpt->Abort();

    for (int i=0; i < valid_transfers_ctr; i++)
    {
        qDebug() << "Reading buffer:" << i;
        for (int j=0; j < RX_TRANSFER_SIZE; j++)
        {
            rx_buf.append(inBuf[i][j]);
        }
    }

    qDebug() << "send_and_read_bulk Received data:" << rx_buf.size();

    for (int i=0; i < queue_size; i++)
    {
        CloseHandle(inOvLap[i].hEvent);
        delete [] inBuf[i];
    }

    delete [] inOvLap;
    delete [] inBuf;
    delete [] inContext;

    return status;
}

void MainWindow::select_endpoints(void)
{
    QString strINData, strOutData;
    bool ok;
    int inEpAddress = 0x0, outEpAddress = 0x0;

    strINData = ui->cb_in_ept->currentText();
    strOutData = ui->cb_out_ept->currentText();

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
    if (ui->cb_device->count() > 0 ) strDevice = ui->cb_device->currentText();

    ui->cb_device->clear();

    if (USBDevice == NULL) return FALSE;

    for (int nCount = 0; nCount < USBDevice->DeviceCount(); nCount++ )
    {
        USBDevice->Open(nCount);

        QString strDeviceData;
        strDeviceData += QString("(0x%1 - 0x%2) ").arg(USBDevice->VendorID, 0 ,16).arg(USBDevice->ProductID, 0 ,16);
        strDeviceData += QString(USBDevice->FriendlyName);
        qDebug() << strDeviceData;

        ui->cb_device->insertItem(nCount, strDeviceData);

        if (nCboIndex == -1 && strDevice.isEmpty() == FALSE && strDevice == strDeviceData)
            nCboIndex = nCount;

        USBDevice->Close();
    }
    delete USBDevice;
    if (ui->cb_device->count() >= 1 )
    {
        if (nCboIndex != -1 ) ui->cb_device->setCurrentIndex(nCboIndex);
        else ui->cb_device->setCurrentIndex(0);
    }

    return TRUE;
}

bool MainWindow::get_endpoint_for_device()
{
    int nDeviceIndex = ui->cb_device->currentIndex();

    ui->cb_in_ept->clear();
    ui->cb_out_ept->clear();

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

                if (ept->bIn ) ui->cb_in_ept->addItem(strData);
                else ui->cb_out_ept->addItem(strData);
            }
        }
    }

    if (ui->cb_in_ept->count() > 0 ) ui->cb_in_ept->setCurrentIndex(0);
    if (ui->cb_out_ept->count() > 0 ) ui->cb_out_ept->setCurrentIndex(0);

    return ui->cb_in_ept->count() > 0 && ui->cb_out_ept->count() > 0;
}

void MainWindow::populate_header(QList<unsigned char> &buf, unsigned int number_of_packets)
{
    buf.append(0xBA);
    buf.append(0xB0);
    buf.append(0xFE);
    buf.append(0xCA);

    buf.append((number_of_packets >>  0) & 0xFF);
    buf.append((number_of_packets >>  8) & 0xFF);
    buf.append((number_of_packets >> 16) & 0xFF);
    buf.append((number_of_packets >> 24) & 0xFF);

    buf.append(ui->ch0->currentIndex());
    buf.append(ui->ch1->currentIndex());
    buf.append(ui->ch2->currentIndex());
    buf.append(ui->ch3->currentIndex());

    buf.append(ui->ch4->currentIndex());
    buf.append(ui->ch5->currentIndex());
    buf.append(ui->ch6->currentIndex());
    buf.append(ui->ch7->currentIndex());

    buf.append( (ui->gpio3->isChecked() << 0) |
                (ui->gpio2->isChecked() << 1) |
                (ui->gpio1->isChecked() << 2) |
                (ui->gpio0->isChecked() << 3));
    buf.append(0);
    buf.append(0);
    buf.append(0);
}

void MainWindow::handle_button()
{
    QList<unsigned char> new_data;
    this->populate_header(new_data, 0);

    qDebug() << "handle_button header:" << new_data;

    for (int i=0; i<(256 - new_data.size()); ++i)
    {
        new_data.append(0);
    }

    qDebug() << "send_bulk:" << this->send_bulk(new_data);
}

unsigned char MainWindow::reverse(unsigned char b)
{
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

void MainWindow::on_btn_select_clicked()
{
    qDebug() << "on_btn_select_clicked";

    this->select_endpoints();

    if (BulkOutEpt && BulkInEpt)
    {
        this->communication_enabled = true;
        ui->lb_status->setText("Selected");
    }
}


void MainWindow::on_btn_refresh_clicked()
{
    qDebug() << "on_btn_refresh_clicked";

    this->get_devices();
}


void MainWindow::on_cb_device_currentIndexChanged(int index)
{
    qDebug() << "on_cb_device" << index;

    this->get_endpoint_for_device();
}


void MainWindow::on_cb_in_ept_currentIndexChanged(int index)
{
    qDebug() << "on_cb_in_ept" << index;
}


void MainWindow::on_cb_out_ept_currentIndexChanged(int index)
{
    qDebug() << "on_cb_out_ept" << index;
}

void MainWindow::on_start_btn_clicked()
{
    ui->lb_status->setText("Reading data");
    qDebug() << "on_start_btn_clicked";

    if (!this->communication_enabled) return;

    QList<unsigned char> new_data;
    this->populate_header(new_data, ui->packet_slider->value()*8);

    qDebug() << "on_start_btn_clicked header:" << new_data;

    for (int i=0; i<(256 - new_data.size()); ++i)
    {
        new_data.append(0);
    }

    qDebug() << "send_and_read_bulk:" << this->send_and_read_bulk(new_data, this->data_buffer, ui->packet_slider->value()*8);

#ifdef DEBUG_PATTERN
    ui->lb_status->setText("Veryfying data");
    int old_byte = -1;
    int new_byte;
    for (int i = 0; i < this->data_buffer.size(); i++)
    {
        new_byte = reverse(this->data_buffer.at(i));
        if ((new_byte - old_byte) != 1 && new_byte != 0 && old_byte != 255 && old_byte != -1)
        {
            qDebug() << "Not continuous data new:" << new_byte << "old:" << old_byte;
        }
        old_byte = new_byte;
    }
#endif /* DEBUG_PATTERN */

    this->update_plot(this->data_buffer);
}

void MainWindow::on_gpio0_clicked()
{
    this->handle_button();
}


void MainWindow::on_gpio1_clicked()
{
    this->handle_button();
}


void MainWindow::on_gpio2_clicked()
{
    this->handle_button();
}


void MainWindow::on_gpio3_clicked()
{
    this->handle_button();
}

void MainWindow::on_clr_btn_clicked()
{
    QList<unsigned char> data;
    data.append(0);
    data.append(0);
    data.append(0);
    data.append(0);
    data.append(0);
    data.append(0);
    this->update_plot(data);
}

void MainWindow::on_packet_slider_valueChanged(int value)
{
    ui->time_label->setText(QString("%1ms").arg(value*8*0.32768));
    ui->packet_label->setText(QString("16kB packets: %1").arg(value*8));
}
