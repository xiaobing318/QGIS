#ifndef SE_MERGE_PTP_THREAD_H
#define SE_MERGE_PTP_THREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>

#include <string>

using namespace std;


/*瓦片包分包规则结构体*/
struct PTP_Package_Rule
{
    int			min_z;		// 最小级别
    int			max_z;		// 最大级别
    int			base_z;		// 基础级别

    PTP_Package_Rule()
    {
        min_z = 0;
        max_z = 0;
        base_z = 0;
    }

    PTP_Package_Rule(int _min_z, int _max_z, int _base_z)
    {
        min_z = _min_z;
        max_z = _max_z;
        base_z = _base_z;
    }
};


/*PTP瓦片包信息结构体*/
struct PTP_Package_Info
{
    QString								qstrPath;			// 瓦片包所在路径，该路径下包括1个或多个ptp文件
    vector<QString>						vPtpFileName;		// ptp文件名称，如：0-7-0-0-0.ptp
    vector<QString>						vPtpFilePath;		// 当前路径下各瓦片的全路径：如：D:/0-7-0-0-0.ptp

    PTP_Package_Info()
    {
        qstrPath = "";
        vPtpFileName.clear();
        vPtpFilePath.clear();
    }

};


/*PTP文件路径及读取句柄结构体*/
struct PTP_Path_ReaderHandle
{
    string							strPtpPath;			// 瓦片包全路径
    vector<PTP_Package_Rule>		vRule;				// 瓦片包分包规则，如：[[0,7,0],[8,11,3],[12,15,7],[16,19,11],[20,22,15]]

    PTP_Path_ReaderHandle()
    {
        strPtpPath = "";
        vRule.clear();
    }
};

class SE_MergePtpThread : public QThread
{
    Q_OBJECT

public:
    SE_MergePtpThread(QObject *parent = 0);
    ~SE_MergePtpThread();



    // 设置线程运行输入参数
    // qstrInputDataPath：       ptp路径列表
    // iMergeType：              融合策略
    // vOutputPtpPackageRule：   瓦片包合包规则
    // qstrDataTime：            合包时间
    // qstrOutputPath：          输出瓦片数据路径
    void SetThreadParams(QStringList qstrPtpPathList, 
        int iMergeType, 
        vector<PTP_Package_Rule> vOutputPtpPackageRule,
        QString qstrDataTime, 
        QString qstrOutputPath);


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

    string GetPtpPackageNameByZXY(int x, int y, int z, vector<PTP_Package_Rule>& vPTP_Rule);

    bool IsExistedInVector(string strValue, vector<string>& vValue);

    QImage CreateImageWithOverlay(const QImage& baseImage, const QImage& overlayImage);

    bool ClearEmptyFolder(const QString& qstrDirPath);

private:


    QMutex mutex;
    QWaitCondition condition;
    
    // 瓦片包路径列表
    QStringList m_qstrPtpPathList;
    
    // 合包策略
    int m_iMergeType;

    // 合包策略
    vector<PTP_Package_Rule> m_vOutputPtpPackageRule;

    // 合包时间
    QString m_qstrDataTime;

    // 输出瓦片数据路径
    QString m_qstrOutputPath;

    bool restart;
    bool abort;

};


#endif // SE_MERGE_PTP_THREAD_H
