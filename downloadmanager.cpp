#include "downloadmanager.h"
#include "ui_downloadmanager.h"

#include <unistd.h>

DownLoadManager::DownLoadManager(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DownLoadManager)
{
    Qt::WindowFlags flags=Qt::Dialog;
    flags |=Qt::WindowMinimizeButtonHint;
    flags |=Qt::WindowCloseButtonHint;
    setWindowFlags(flags);

    ui->setupUi(this);
    setFixedSize(geometry().width(),geometry().height()); // 禁止改变窗口大小。

    initDialog();

    //Get the click event
    ui->txtPath->installEventFilter(this);
}

DownLoadManager::~DownLoadManager()
{
    timerMonitor->stop();
    delete timerMonitor;

    board = NULL;

    //
    for(int i=0; i<10; i++)
    {
        cmd[i]->kill();
        delete cmd[i];
    }

    delete pMapperError;
    delete pMapperEnd;
    delete pMapperReady;
    delete lstIdleProcess;

    delete ui;
}

bool DownLoadManager::eventFilter(QObject* object, QEvent* event)
{
    if(object == ui->txtPath && event->type() == QEvent::MouseButtonRelease)
    {
        if(!ui->ckAskWhenSave->isChecked())
        {
            QString path = QFileDialog::getExistingDirectory(this, tr("Open Directory"),QDir::homePath());
            ui->txtPath->setText(path);
        }
        return false; // lets the event continue to the edit
    }
    return false;
}

/*!
 初始化
*/
void DownLoadManager::initDialog()
{
    //
    this->setWindowIcon(QIcon(":/three.ico"));
    //剪贴板
    board = QApplication::clipboard();

    //命令行终端
    lstIdleProcess = new QList<int>();
    pMapperReady = new QSignalMapper();
    pMapperEnd = new QSignalMapper();
    pMapperError = new QSignalMapper();

    for(int i=0; i<10; i++)
    {
        cmd[i] = new QProcess();
        cmd[i]->setProcessChannelMode(QProcess::MergedChannels);
        connect(cmd[i], SIGNAL(readyRead()), pMapperReady, SLOT(map()));
        connect(cmd[i], SIGNAL(finished(int)), pMapperEnd, SLOT(map()));
        connect(cmd[i], SIGNAL(errorOccurred(QProcess::ProcessError)), pMapperError, SLOT(map()));

        pMapperError->setMapping(cmd[i],i);
        pMapperEnd->setMapping(cmd[i],i);
        pMapperReady->setMapping(cmd[i],i);
        lstIdleProcess->append(i);
    }
    connect(pMapperReady, SIGNAL(mapped(int)), this, SLOT(readOutput(int)));
    connect(pMapperEnd, SIGNAL(mapped(int)), this, SLOT(Taskend(int)));
    connect(pMapperError, SIGNAL(mapped(int)), this, SLOT(Taskend(int)));

    //
    timerMonitor = new QTimer();
    timerMonitor->setInterval(1000);
    connect(timerMonitor, SIGNAL(timeout()), this, SLOT(onTimerOut()));

    //
    QSettings user_cfg("//dlMagager//","usercfg");
    if(user_cfg.isWritable())
    {
        //
        ui->ckAskWhenSave->setChecked(user_cfg.value("AskWhenSave").toBool());
        ui->ckMonClipborad->setChecked(user_cfg.value("MointorClipborad").toBool());
        ui->txtPath->setText(user_cfg.value("DownloadPath").toString());
    }
    else
    {
        QDir dir("//dlMagager//");
        dir.mkdir("//dlMagager//");
        QFile file("//dlMagager//usercfg.cfg");
        file.open( QIODevice::ReadWrite | QIODevice::Text );
        file.close();
        QSettings user_cfg("//dlMagager//","usercfg");
        user_cfg.setValue("AskWhenSave","1");
        user_cfg.setValue("MointorClipborad","1");
        user_cfg.setValue("DownloadPath","");
    }

    //
    tmpFile.setAutoRemove(true);
    if(tmpFile.open())
    {
        tmpFile.close();
    }
}

void DownLoadManager::on_pBtnSart_clicked()
{
    if(ui->ckAskWhenSave->checkState()!=Qt::Unchecked
            || ui->txtPath->text()=="")
    {
        //得到用户选择的文件夹
        QString path = QFileDialog::getExistingDirectory(this, tr("Open Directory"),QDir::homePath());
        ui->txtPath->setText(path);
    }

    QDir dir(ui->txtPath->text());
    if(!dir.exists())
    {
        //得到用户选择的文件夹
        QString path = QFileDialog::getExistingDirectory(this, tr("Open Directory"),QDir::homePath());
        ui->txtPath->setText(path);
    }

    //
    QString input("");
    if(ui->rbtnAxel->isChecked())
    {
        //--user-agent=\"Mozilla/5.0\"
        input = "axel -n5 -o "+ui->txtPath->text()+" "+ui->txtUrl->toPlainText();
    }
    if(ui->rbtnWget->isChecked())
    {
        input = "wget -c -P "+ui->txtPath->text()+" "+ui->txtUrl->toPlainText();
    }

    if(!input.isEmpty())
    {
        ui->listWidget->insertItem(0, m_queuedUrl+"@"+QString::number(lstIdleProcess->first()));
        m_queuedUrl = "";
        cmd[lstIdleProcess->first()]->start(input);
        lstIdleProcess->removeFirst();
        ui->pBtnSart->setEnabled(false);
        ui->listWidget->item(0)->setSelected(true);
        ui->listWidget->item(0)->setTextColor(QColor(Qt::darkGreen));
        ui->txtFeedback->clear();
        QApplication::processEvents();

    }
}

void DownLoadManager::readOutput(int nIndex)
{
    //
    QString strSelected = ui->listWidget->currentItem()->text();
    strSelected.simplified();
    strSelected = strSelected.right(1);
    if(lstIdleProcess->contains((int)strSelected.toInt())
            || strSelected.toInt() != nIndex)
    {
        return;
    }
    //转码支持Unicode
    QTextCodec* pcodec = QTextCodec::codecForLocale();
    QByteArray processStdOutInfo = cmd[nIndex]->readAll();
    QString unicodeInfo = pcodec->toUnicode(processStdOutInfo);

    if(ui->txtFeedback->blockCount()>=ui->txtFeedback->maximumBlockCount()-unicodeInfo.length())
    {
        ui->txtFeedback->clear();
        QApplication::processEvents();
    }
    ui->txtFeedback->insertPlainText(unicodeInfo);
    QApplication::processEvents();

}


void DownLoadManager::Taskend(int nIndex)
{
    //
    for(int i=0; i<ui->listWidget->count(); i++)
    {
        if(ui->listWidget->item(i)->textColor() == QColor(Qt::darkGreen))
        {
            if(ui->listWidget->item(i)->text().right(1)==QString::number(nIndex))
            {
                if(cmd[nIndex]->exitCode()!=0 || cmd[nIndex]->exitStatus()!=QProcess::NormalExit)
                {
                    ui->listWidget->item(i)->setTextColor(QColor(Qt::red));
                }
                else
                {
                    ui->listWidget->item(i)->setTextColor(QColor(Qt::black));
                }
            }
        }
    }

    if(ui->listWidget->currentItem()->text().right(1)==QString::number(nIndex))
    {
        ui->pBtnStop->setEnabled(false);
        if(cmd[nIndex]->exitCode()!=0 || cmd[nIndex]->exitStatus()!=QProcess::NormalExit)
        {
            ui->listWidget->currentItem()->setTextColor(QColor(Qt::red));
        }
        else
        {
            ui->listWidget->currentItem()->setTextColor(QColor(Qt::black));
        }
        return;
    }

    //
    lstIdleProcess->append(nIndex);

}

void DownLoadManager::on_pBtnStop_clicked()
{
    //停止下载
    QString strItem = ui->listWidget->currentItem()->text();
    strItem.simplified();
    strItem = strItem.right(1);
    cmd[strItem.toInt()]->kill();
    ui->pBtnStop->setEnabled(false);
    lstIdleProcess->append(strItem.toInt());

}

void DownLoadManager::on_ckMonClipborad_toggled(bool checked)
{
    if(checked)
    {
        timerMonitor->start();
    }
    else
    {
        timerMonitor->stop();
        m_clipboardText = "";
    }

    //
    QSettings user_cfg("//dlMagager//","usercfg");
    user_cfg.setValue("MointorClipborad",QString::number(checked));
}


void DownLoadManager::onTimerOut()
{
    //
    QString pattern = "(^(https?|ftp):\\//\\//){1,}";

    QString strText = board->text();
    strText = strText.simplified();

    if(strText != m_clipboardText)
    {
        //链接地址
        QRegExp rx;
        rx.setPattern(pattern);
        rx.exactMatch(strText);
        if(rx.matchedLength()>0)
        {
            m_clipboardText = strText;
            ui->txtUrl->setText(strText);
        }
    }
}

void DownLoadManager::on_ckAskWhenSave_clicked(bool checked)
{
    QSettings user_cfg("//dlMagager//","usercfg");
    user_cfg.setValue("AskWhenSave",QString::number(checked));
}



/*!
 \brief: Get the file information from the url server

 \param: strUrl
 \param: tryTimes
*/
void DownLoadManager::getFileTotalSize(QString strUrl)
{
    // 根据输入下载url，请求获取文件信息
    tmpFile.open();
    tmpFile.close();
    cmdInfo.setProcessChannelMode(QProcess::MergedChannels);
    connect(&cmdInfo, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(readheader(int, QProcess::ExitStatus)));
    cmdInfo.start("wget -t5 -S --spider "+ strUrl +" -o "+tmpFile.fileName());
}

void DownLoadManager::readheader(int exitCode, QProcess::ExitStatus exitStatus)
{
    //
    if(exitStatus == QProcess::NormalExit && 0==exitCode)
    {
        QString strline;
        if (!tmpFile.open())
            return;

        QTextStream in(&tmpFile);
        qint64 fp = tmpFile.size();
        QString szChar;
        qint64 ln = -1;
        qint64 fileSize = -1;
        QString fileName = "";
        while (in.seek(fp))
        {
            szChar = in.read(1);
            if(szChar == "\n")
            {
                ln++;
                if(ln==1)
                {
                    if(strline.isEmpty())
                    {
                        ln--;
                    }
                    else
                    {
                        if(strline.indexOf(FILE_EXIST)<0)
                        {
                            tmpFile.close();
                            break;
                        }

                    }
                }
                else
                {
                    int nPos = strline.indexOf(FILE_LENGTH,0, Qt::CaseInsensitive);
                    if( nPos >= 0 && fileSize < 0)
                    {
                        fileSize = strline.mid(nPos+15).simplified().toLongLong();
                    }
                    nPos = strline.indexOf(FILE_LOCATION,0, Qt::CaseInsensitive);
                    if( nPos >= 0 && fileName.isEmpty())
                    {
                        int len = strline.indexOf(FOLLOW_CHAR,0, Qt::CaseInsensitive);
                        if(len>0) len--;
                        QUrl url(strline.mid(nPos,len));
                        fileName = url.fileName();
                        break;
                    }

                    if(!fileName.isEmpty() && fileSize>=0)
                    {
                        tmpFile.close();
                        break;
                    }
                }
                strline = "";

            }
            else
            {
                strline.insert(0,szChar);
            }

            fp--;
        }


        //
        if (!fileName.isEmpty())
        {
            m_queuedUrl =fileName+":"+transformUnit(fileSize);
        }
        else
        {
            m_queuedUrl = "Unknown File";
        }
    }
    else
    {
        m_queuedUrl = "";
    }

    if(tmpFile.exists())
    {
        if(tmpFile.isOpen())
            tmpFile.close();
        //tmpFile.remove();
    }
    //
    tmpLoop.quit();
}

/*!
 \brief: Convert unit from byte

 \param: bytes -- size of file with unit byte
 \return: QString -- size of file that with the comfortable unit
*/
QString DownLoadManager::transformUnit(double bytes)
{
    // 转换单位;
    QString strUnit = " B";
    if (bytes <= 0)
    {
        bytes = 0;
    }
    else if (bytes < UNIT_KB)
    {
    }
    else if (bytes < UNIT_MB)
    {
        bytes /= UNIT_KB;
        strUnit = " KB";
    }
    else if (bytes < UNIT_GB)
    {
        bytes /= UNIT_MB;
        strUnit = " MB";
    }
    else if (bytes > UNIT_GB)
    {
        bytes /= UNIT_GB;
        strUnit = " GB";
    }

    // 保留小数点后两位;
    return QString("%1%2").arg(QString::number(bytes, 'f', 2)).arg(strUnit);
}

void DownLoadManager::on_txtUrl_textChanged()
{
    QString pattern = "(^(https?|ftp):\\//\\//){1,}";
    QString strText = ui->txtUrl->toPlainText();
    strText = strText.simplified();
    if(!strText.isEmpty())
    {
        //链接地址
        QRegExp rx;
        rx.setPattern(pattern);
        rx.exactMatch(strText);
        if(rx.matchedLength()<0)
        {
            ui->txtUrl->setText("");
            return;
        }

        //
        getFileTotalSize(strText);
        tmpLoop.exec();
        if(!m_queuedUrl.isEmpty())
        {
            ui->pBtnSart->setEnabled(true);
        }
    }
}

void DownLoadManager::on_listWidget_itemSelectionChanged()
{
    QString strItem = ui->listWidget->currentItem()->text();;
    strItem.simplified();
    strItem = strItem.right(1);

    //查出非空闲的QProcess
    if(!lstIdleProcess->contains(strItem.toInt()))
    {
        ui->pBtnStop->setEnabled(true);
        if(ui->txtFeedback->documentTitle() != strItem)
        {
            ui->txtFeedback->setDocumentTitle(strItem);
            ui->txtFeedback->clear();
        }
    }
    else
    {
        ui->pBtnStop->setEnabled(false);
        if(ui->txtFeedback->documentTitle() != strItem)
        {
            ui->txtFeedback->setDocumentTitle(strItem);
            ui->txtFeedback->setPlainText("Not Active!!!");
        }
    }
    QApplication::processEvents();
}

void DownLoadManager::on_txtPath_textChanged(const QString &arg1)
{
    QDir dir(arg1);
    if ( dir.exists() )
    {
        if(!ui->ckAskWhenSave->isChecked())
        {
            QSettings user_cfg("//dlMagager//","usercfg");
            user_cfg.setValue("DownloadPath",ui->txtPath->text());
        }
    }

}
