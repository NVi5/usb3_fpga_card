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

    void on_spinBox_valueChanged(int arg1);

    void on_gpio0_clicked();

    void on_gpio1_clicked();

    void on_gpio2_clicked();

    void on_gpio3_clicked();

    void on_clr_btn_clicked();

private:
    void init_plot();
    void update_plot(QList<unsigned char> &new_data);
    void limitAxisRange(QCPAxis * axis, const QCPRange & newRange, const QCPRange & limitRange);
    bool send_bulk(QList<unsigned char> &tx_buf);
    bool read_bulk(QList<unsigned char> &rx_buf, unsigned char packets_to_read);
    void select_endpoints(void);
    bool get_devices();
    bool get_endpoint_for_device();
    void populate_header(QList<unsigned char> &buf, unsigned int number_of_packets);
    void handle_button();

    bool communication_enabled;
    Ui::MainWindow *ui;
    CCyUSBDevice *selectedDevice;
    CCyUSBEndPoint *BulkInEpt;
    CCyUSBEndPoint *BulkOutEpt;
    QList<unsigned char> data_buffer;
};
#endif // MAINWINDOW_H
