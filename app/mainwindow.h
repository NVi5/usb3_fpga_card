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
    void on_btn_select_clicked();

    void on_btn_refresh_clicked();

    void on_cb_device_currentIndexChanged(int index);

    void on_cb_in_ept_currentIndexChanged(int index);

    void on_cb_out_ept_currentIndexChanged(int index);

    void on_start_btn_clicked();

    void xAxisChanged(const QCPRange & newRange);

    void yAxisChanged(const QCPRange & newRange);

private:
    void plot(UCHAR *buf);
    void limitAxisRange(QCPAxis * axis, const QCPRange & newRange, const QCPRange & limitRange);
    void send_bulk(unsigned char state);
    bool read_bulk(unsigned char *state);
    void select_endpoints(void);
    bool get_devices();
    bool get_endpoint_for_device();
    static DWORD WINAPI thread_read(LPVOID argument);

    bool communication_enabled;
    Ui::MainWindow *ui;
    CCyUSBDevice *selectedDevice;
    CCyUSBEndPoint *BulkInEpt;
    CCyUSBEndPoint *BulkOutEpt;
};
#endif // MAINWINDOW_H
