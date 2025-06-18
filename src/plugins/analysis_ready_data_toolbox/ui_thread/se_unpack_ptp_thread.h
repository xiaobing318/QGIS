#ifndef SE_UNPACK_PTP_THREAD_H
#define SE_UNPACK_PTP_THREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>

#include <string>

using namespace std;


class SE_UnpackPtpThread : public QThread
{
    Q_OBJECT

public:
    SE_UnpackPtpThread(QObject *parent = 0);
    ~SE_UnpackPtpThread();

    // 设置线程运行输入参数
    // qstrInputDataPath：输入数据路径
    // strOutputFormat：输出数据格式
    // qstrOutputPath：输出数据路径
    // iMinLevel：最小级别
    // iMaxLevel：最大级别
    void SetThreadParams(QString qstrInputDataPath,
                         string strOutputFormat,
                         QString qstrOutputPath,
                         int iMinLevel,
                         int iMaxLevel);

signals:
    
    // 返回百分比进度
    void resultProcess(const double &dPercent, const QString& s);

    void resultProcessString(const QString& s);
   

protected:
    void run() override;

    bool FilePathIsExisted(QString qstrPath);

    bool FileIsExisted(QString qstrFilePath);

    void GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName);

    void GetFolderNameFromPath_string(string strPath, string& strFolderName);

    bool LoadPtpKeyConfigFile(string& strPrivateKey, string& strDevKey);

    QStringList GetPtpFileNames(const QString& path);

    void GetPtpInfo(string strPtpFileName, int& minz, int& maxz, int& basez, int& x, int& y);

    bool isDirExist(QString fullPath);

    bool CheckFileOrDirExist(const QString qstrFileDirOrPath);

    bool ClearEmptyFolder(const QString& qstrDirPath);

private:


    QMutex mutex;
    QWaitCondition condition;
    
    // 保存路径
    QString m_qstrSavePath;

    // 输入路径
    QString m_qstrInputDataPath;

    // 输出数据格式
    string m_strOutputFormat;

    // 最小级别
    int m_iMinLevel;

    // 最大级别
    int m_iMaxLevel;

    bool restart;
    bool abort;

};


#endif // SE_UNPACK_PTP_THREAD_H
