#ifndef SE_PACK_DISPLAY_READY_DATA_TASK_H
#define SE_PACK_DISPLAY_READY_DATA_TASK_H

#include <QgsTaskManager.h>
#include <QgsMessageLog.h>
#include <string>
#include <qstring.h>
#include <vector>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include "commontype/se_commontypedef.h"

/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#include <filesystem>
using namespace std;
namespace fs = std::filesystem;

/*显示就绪型数据封装任务类*/
class SE_PackDisplayReadyDataTask : public QgsTask
{
    Q_OBJECT

public:

    SE_PackDisplayReadyDataTask(const QString& name,
        const string& strInputPath,
        const vector<string>& vIdentifys,
        const vector<string>& vDataTypes,
        const vector<string>& vFeatureTypes,
        const SE_DRect& dRect,
        const string& strOutputPath,
        int iLogLevel,
        const string& strOutputLogPath);

    // 运行任务的主逻辑
    bool run() override;
 
    // 是否取消
    bool isCanceled() ;

    // 取消任务
    void cancel();

    // 进度
    int progress() const;

    void finished(bool result) override;


signals:
    void taskFinished(bool result);

private:

    string m_strInputPath;              // 输入分析就绪数据路径
    string m_strOutputPath;             // 结果保存路径
    vector<string> m_vIdentifys;        // 任务模型需求的显示就绪数据类型列表
    vector<string> m_vDataTypes;        // 显示就绪数据类型列表（交互☑的）
    vector<string> m_vFeatureTypes;     // 显示就绪数据要素类型列表
    SE_DRect m_dRect;                   // 空间范围
    int mProgress;                      // 进度值
    bool mCanceled;                     // 新增的成员变量

    int             m_iLogLevel;                // 日志级别
    string          m_strOutputLogPath;         // 日志输出路径

private:

    // 获取geojson数据文件列表
    QStringList GetFileNames(const QString& path);

    // 是否在vector容器中
    bool IsExistedInVector(string strValue, vector<string>& vValue);

    // 获取文件夹下的所有文件路径
    void GetJsonFilesFromFilePath(const string& strFilePath, vector<string>& vFilesPath);

    // 读取元数据文件
    bool ReadMetaDataJsonFile(const string& strFile, vector<MetaDataTemplateInfo>& vMetadatInfo);

    // 获取元数据信息
    string GetValueByKey(const string& strKey, vector<MetaDataTemplateInfo>& vMetadatInfo);

    // 字符编码转换
    string convertEncoding(const string& input, const string& fromEncoding, const string& toEncoding);

    // 复制文件
    bool copyFile(const fs::path& sourceFilePath, 
        const fs::path& destinationFilePath,
        std::shared_ptr<spdlog::logger> file_logger);

    // 复制文件夹
    bool copyDirectory_4_ShiLiang(
        const fs::path& sourceDir, 
        const fs::path& destinationDir,
        std::shared_ptr<spdlog::logger> file_logger);

    // 递归拷贝目录函数（除矢量数据类型之外使用）
    bool copyDirectory_4_OtherDataType(const fs::path& sourceDir, const fs::path& destinationDir, std::shared_ptr<spdlog::logger> file_logger);

    // 生成UUID
    string generateUUID();
    

};



#endif // SE_PACK_DISPLAY_READY_DATA_TASK_H
