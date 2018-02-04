#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QDialog>
#include <QTimer>
#include <QClipboard>
#include <QtGui>
#include <QFileDialog>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#define UNIT_KB 1024            //KB
#define UNIT_MB 1024*1024       //MB
#define UNIT_GB 1024*1024*1024  //GB
#define FILE_EXIST "Remote file exists."
#define FILE_LENGTH "Content-Length:"
#define FILE_LOCATION "Location:"
#define FOLLOW_CHAR "[following]"

namespace Ui {
class DownLoadManager;
}

class DownLoadManager : public QDialog
{
    Q_OBJECT

public:
    explicit DownLoadManager(QWidget *parent = 0);
    ~DownLoadManager();

private slots:
    void on_pBtnSart_clicked();

    void on_pBtnStop_clicked();

    void on_ckMonClipborad_toggled(bool checked);
    void readheader(int exitCode, QProcess::ExitStatus exitStatus);

    void onTimerOut();

    void readOutput(int nIndex);
    void Taskend(int nIndex);

    void on_ckAskWhenSave_clicked(bool checked);

    void on_txtUrl_textChanged();

    void on_listWidget_itemSelectionChanged();

    void on_txtPath_textChanged(const QString &arg1);



private:
    Ui::DownLoadManager *ui;
    QTimer *timerMonitor;
    QClipboard *board;
    QProcess   *cmd[10];
    QProcess   cmdInfo;
    QSignalMapper *pMapperReady;
    QSignalMapper *pMapperEnd;
    QSignalMapper *pMapperError;
    QList<int>    *lstIdleProcess;
    QString       m_clipboardText;
    QString       m_queuedUrl;
    QTemporaryFile tmpFile;
    QEventLoop     tmpLoop;

    void initDialog();
    QString transformUnit(double bytes);
    void getFileTotalSize(QString strUrl);
    bool eventFilter(QObject* object, QEvent* event);
};

#endif // DOWNLOADMANAGER_H
