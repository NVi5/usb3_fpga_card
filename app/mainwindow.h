#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "CyAPI.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btn0_clicked(bool checked);

    void on_btn1_clicked(bool checked);

    void on_btn2_clicked(bool checked);

    void on_btn3_clicked(bool checked);

private:
    void find_transport(void);
    void send_bulk(unsigned char state);
    bool read_bulk(unsigned char *state);
    unsigned char get_button_state(void);
    void set_button_state(unsigned char state);
    static DWORD WINAPI thread_read(LPVOID argument);

    bool read_enabled;
    HANDLE thread_handle;
    Ui::MainWindow *ui;
    CCyUSBDevice *USBDevice;
    CCyBulkEndPoint *BulkInEpt;
    CCyBulkEndPoint *BulkOutEpt;
};
#endif // MAINWINDOW_H
