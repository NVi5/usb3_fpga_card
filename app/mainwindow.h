#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "CyAPI.h"
#include "qcustomplot.h"

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

    void on_btn_select_clicked();

    void on_btn_refresh_clicked();

    void on_cb_device_currentIndexChanged(int index);

    void on_cb_in_ept_currentIndexChanged(int index);

    void on_cb_out_ept_currentIndexChanged(int index);

    void xAxisChanged(const QCPRange & newRange);
    void yAxisChanged(const QCPRange & newRange);

private:
    void plot();
    void limitAxisRange(QCPAxis * axis, const QCPRange & newRange, const QCPRange & limitRange);
    void send_bulk(unsigned char state);
    bool read_bulk(unsigned char *state);
    unsigned char get_button_state(void);
    void set_button_state(unsigned char state);
    void select_endpoints(void);
    bool get_devices();
    bool get_endpoint_for_device();
    static DWORD WINAPI thread_read(LPVOID argument);

    bool communication_enabled;
    HANDLE thread_handle;
    Ui::MainWindow *ui;
    CCyUSBDevice *selectedDevice;
    CCyUSBEndPoint *BulkInEpt;
    CCyUSBEndPoint *BulkOutEpt;
};
#endif // MAINWINDOW_H
