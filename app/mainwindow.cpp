#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <QDebug>
#include "Windows.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), BulkInEpt(NULL), BulkOutEpt(NULL)
{
    this->USBDevice = new CCyUSBDevice(NULL);
    this->find_transport();

    ui->setupUi(this);
    ui->btn0->setChecked(false);
    ui->btn1->setChecked(false);
    ui->btn2->setChecked(false);
    ui->btn3->setChecked(false);
    this->send_bulk(0);
    this->read_bulk(NULL);
    this->read_enabled = true;

    this->thread_handle = CreateThread(NULL, 1024, thread_read, this, 0, NULL);
}

MainWindow::~MainWindow()
{
    this->read_enabled = false;
    delete ui;
    delete this->USBDevice;
    CloseHandle(this->thread_handle);
}

void MainWindow::find_transport(void)
{
    int eptCount = this->USBDevice->EndPointCount();
    for (int i=1; i<eptCount; i++) {
        bool bIn = ((this->USBDevice->EndPoints[i]->Address & 0x80) == 0x80);
        bool bBulk = (this->USBDevice->EndPoints[i]->Attributes == 2);
        if (bBulk && bIn) {
            this->BulkInEpt = (CCyBulkEndPoint *) this->USBDevice->EndPoints[i];
            qDebug("BulkInEpt EpAddr - 0x%02X", this->BulkInEpt->Address);
        }
        if (bBulk && !bIn) {
            this->BulkOutEpt = (CCyBulkEndPoint *) this->USBDevice->EndPoints[i];
            qDebug("BulkOutEpt EpAddr - 0x%02X", this->BulkOutEpt->Address);
        }
    }
}

void MainWindow::send_bulk(unsigned char state)
{
    LONG packet_length = 1;
    this->BulkOutEpt->XferData(&state, packet_length);

    qDebug("Sent - %d", state);
}

bool MainWindow::read_bulk(unsigned char *state)
{
    bool status = true;
    unsigned char inBuf = 0;
    LONG packet_length = 1;

    OVERLAPPED inOvLap;
    inOvLap.hEvent = CreateEvent(NULL, false, false, L"CYUSB_IN");

    UCHAR *inContext = this->BulkInEpt->BeginDataXfer(&inBuf, packet_length, &inOvLap);
    if(!this->BulkInEpt->WaitForXfer(&inOvLap, 200))
    {
        status = false;

        this->BulkInEpt->Abort();
        if (this->BulkInEpt->LastError == ERROR_IO_PENDING)
        {
            qDebug() << "BulkInEpt ERROR_IO_PENDING";
            WaitForSingleObject(inOvLap.hEvent, 200);
        }
        else
        {
            qDebug() << "BulkInEpt unknown error - " << this->BulkInEpt->LastError;
        }
    }
    this->BulkInEpt->FinishDataXfer(&inBuf, packet_length, &inOvLap, inContext);

    CloseHandle(inOvLap.hEvent);

    if(status)
    {
        qDebug("Received - %d", inBuf);
    }

    if (state)
    {
        *state = inBuf;
    }

    return status;
}

unsigned char MainWindow::get_button_state(void)
{
    unsigned char state =   (unsigned char)ui->btn0->isChecked() << 0 |
                            (unsigned char)ui->btn1->isChecked() << 1 |
                            (unsigned char)ui->btn2->isChecked() << 2 |
                            (unsigned char)ui->btn3->isChecked() << 3;
    qDebug() << "get_button_state " << state;
    return state;
}

void MainWindow::set_button_state(unsigned char state)
{
    qDebug() << "set_button_state " << state;
    ui->btn0->setChecked(state & (1<<0));
    ui->btn1->setChecked(state & (1<<1));
    ui->btn2->setChecked(state & (1<<2));
    ui->btn3->setChecked(state & (1<<3));
}

DWORD WINAPI MainWindow::thread_read(LPVOID argument) {
    MainWindow* threadObject = reinterpret_cast<MainWindow*>(argument);
    if (threadObject) {
        while(threadObject->read_enabled)
        {
            unsigned char state;
            if(threadObject->read_bulk(&state))
            {
                threadObject->set_button_state(state);
            }
        }
    }
    return 0;
}

void MainWindow::on_btn0_clicked(bool checked)
{
    this->send_bulk(get_button_state());
    qDebug("btn0 - %d", checked);
}

void MainWindow::on_btn1_clicked(bool checked)
{
    this->send_bulk(get_button_state());
    qDebug("btn1 - %d", checked);
}

void MainWindow::on_btn2_clicked(bool checked)
{
    this->send_bulk(get_button_state());
    qDebug("btn2 - %d", checked);
}

void MainWindow::on_btn3_clicked(bool checked)
{
    this->send_bulk(get_button_state());
    qDebug("btn3 - %d", checked);
}

