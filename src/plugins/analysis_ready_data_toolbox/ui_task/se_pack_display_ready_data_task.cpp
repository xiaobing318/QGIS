#define _HAS_STD_BYTE 0
// 使用json头文件
#include <nlohmann/json.hpp>
#include "se_pack_display_ready_data_task.h"
#include <qdir.h>

#include "commontype/se_commontypedef.h"
#include <sqlite3.h>

#include "commontype/se_commondef.h"

#include <iostream> 
#include <fstream>
#include <random>


#include "iconv.h"
using namespace std;

using ordered_json = nlohmann::ordered_json;


/*构造函数*/
SE_PackDisplayReadyDataTask::SE_PackDisplayReadyDataTask(
    const QString& name,
    const string& strInputPath,
    const vector<string>& vIdentifys,
    const vector<string>& vDataTypes,
    const vector<string>& vFeatureTypes,
    const SE_DRect& dRect,
    const string& strOutputPath,
    int iLogLevel,
    const string& strOutputLogPath)
    : QgsTask(name), 
    m_strInputPath(strInputPath),
    m_vIdentifys(vIdentifys),
    m_vDataTypes(vDataTypes),
    m_vFeatureTypes(vFeatureTypes),
    m_dRect(dRect),
    m_strOutputPath(strOutputPath), 
    m_iLogLevel(iLogLevel),
    m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_PackDisplayReadyDataTask::run()
{
    /*算法思路：
    1）读取当前输入目录下的json元数据文件，
    获取：a、数据类型（"管理信息-产品名称"字段）
          b、空间范围（"标识信息-数据四至范围"字段）
          c、要素类型通过遍历目录下所有的文件进行判断（仅当数据类型是矢量数据时，才需要判断要素类型）
    2）与输入的条件进行对比，满足条件的数据拷贝到输出目录下   
    */

#pragma region "创建日志器"
#pragma region "日志文件增加日志级别"

    // 日志文件名称示例：System_Running_Error_A_point.txt
    string strLogLevel;

    // 错误日志
    if (m_iLogLevel == SE_LOG_LEVEL_ERROR)
    {
        strLogLevel = "Error";
    }

    // 信息
    else if (m_iLogLevel == SE_LOG_LEVEL_INFO)
    {
        strLogLevel = "Info";
    }

    // 调试
    else if (m_iLogLevel == SE_LOG_LEVEL_DEBUG)
    {
        strLogLevel = "Debug";
    }

#pragma endregion



    string strUUID = generateUUID();
    
    // 日志器名称
    string strLogFileName = SE_LOG_TYPE_SYSTEM;         // 类型：系统运行
    strLogFileName += "_";
    strLogFileName += strLogLevel;
    strLogFileName += "_PackDisplayReadyData_";
    strLogFileName += strUUID;

    // 日志文件全路径
    string strLogFileFullPath = m_strOutputLogPath + "/" + strLogFileName + ".txt";

    // 日志器名称
    string strLoggerName = SE_LOG_TYPE_SYSTEM;         // 类型：系统运行
    strLoggerName += "_";
    strLoggerName += strLogLevel;
    strLoggerName += "_PackDisplayReadyData_";
    strLoggerName += strUUID;

    // 创建一个写入普通文件的日志器"
    auto file_logger = spdlog::basic_logger_mt(strLoggerName.c_str(),
        strLogFileFullPath.c_str());

    /*记录日志*/

    // 错误日志
    if (m_iLogLevel == SE_LOG_LEVEL_ERROR)
    {
        // 设置日志级别
        file_logger->set_level(spdlog::level::err);
    }

    // 信息
    else if (m_iLogLevel == SE_LOG_LEVEL_INFO)
    {
        // 设置日志级别
        file_logger->set_level(spdlog::level::info);
    }

    // 调试
    else if (m_iLogLevel == SE_LOG_LEVEL_DEBUG)
    {
        // 设置日志级别
        file_logger->set_level(spdlog::level::debug);
    }

    // 输出一条启动信息
    char szLog[1000] = { 0 };
    sprintf(szLog, "正在执行基础通用显示就绪型数据封装任务...");
    file_logger->info(szLog);
    file_logger->debug(szLog);
    file_logger->flush();

#pragma endregion


#pragma region "获取当前目录下的json元数据文件，如果没有，跳过当前目录"



    // 读取每个数据路径下的json格式元数据文件，获取属性设置表格中对应的数据项属性值
    // 获取生产日期
    vector<string> vJsonFilesPath;
    vJsonFilesPath.clear();
    GetJsonFilesFromFilePath(m_strInputPath, vJsonFilesPath);

    // 元数据项
    vector<MetaDataTemplateInfo> vMetaDataTemplateInfos;
    vMetaDataTemplateInfos.clear();

    // 如果该路径下没有json文件，则跳过
    if (vJsonFilesPath.size() == 0)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "当前目录:%s下没有json元数据文件，暂不做处理！", convertEncoding(m_strInputPath, "gbk", "UTF-8").c_str());
        file_logger->info(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());
        setProgress(100);
        return true;
    }

    // 一个或多个json元数据文件
    else if (vJsonFilesPath.size() >= 1)
    {
        if (!ReadMetaDataJsonFile(vJsonFilesPath[0], vMetaDataTemplateInfos))
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "读取元数据文件：%s失败！", convertEncoding(vJsonFilesPath[0], "gbk", "UTF-8").c_str());
            file_logger->error(szLog);
            file_logger->flush();
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());
            setProgress(100);
            return true;
        }
    }


    // 管理信息-产品名称，矢量数据(SE_DATA_TYPE_SHILIANG)、影像数据(SE_DATA_TYPE_YINGXIANG)等
    string strGLXX_CPMC = GetValueByKey("GLXX_CPMC", vMetaDataTemplateInfos);

    // 标识信息-数据四至范围
    string strBSXX_SJSZFW = GetValueByKey("BSXX_SJSZFW", vMetaDataTemplateInfos);

    // 根据四至范围四至范围用","分隔，获取经纬度范围，左、下、右、上
    vector<string> vRange;
    vRange.clear();
    string strDelimiter = ",";
    size_t pos = 0;
    string token;
    while ((pos = strBSXX_SJSZFW.find(strDelimiter)) != string::npos)
    {
        token = strBSXX_SJSZFW.substr(0, pos);
        vRange.push_back(token);
        strBSXX_SJSZFW.erase(0, pos + strDelimiter.length());
    }
    vRange.push_back(strBSXX_SJSZFW); // 将最后一个token添加到vector中

    double dleft = atof(vRange[0].c_str());
    double dbottom = atof(vRange[1].c_str());
    double dright = atof(vRange[2].c_str());
    double dtop = atof(vRange[3].c_str());

#pragma endregion

#pragma region "遍历当前输入目录下所有的文件，判断是否符合封装要求"

    // 首先判断空间范围是否在封装空间范围内
    if (dleft >= m_dRect.dleft
        && dbottom >= m_dRect.dbottom
        && dright <= m_dRect.dright
        && dtop <= m_dRect.dtop)
    {
        fs::path sourceDir = m_strInputPath;
        fs::path destinationDir = m_strOutputPath;

        // 判断数据类型是否在任务-模型-数据的类型列表中或数据类型列表中
        if (!IsExistedInVector(strGLXX_CPMC, m_vIdentifys)
            && !IsExistedInVector(strGLXX_CPMC, m_vDataTypes))
        {
            memset(szLog, 0, 1000);
            sprintf(szLog, "元数据中数据类型：%s不在显示就绪型数据封装设置的数据类型范围内！", strGLXX_CPMC.c_str());
            file_logger->info(szLog);
            file_logger->flush();
            // 卸载日志记录器
            spdlog::drop(strLoggerName.c_str());
            setProgress(100);
            return true;
        }

        // 如果是矢量数据，判断要素类型是否在要素类型列表中
        if (strGLXX_CPMC == SE_DATA_TYPE_SHILIANG)
        {

            if (!copyDirectory_4_ShiLiang(
                sourceDir,
                destinationDir,
                file_logger))
            {
                spdlog::drop(strLoggerName.c_str());
                setProgress(100);
                return false;
            }
        }
        // 其它类型数据
        else
        {
            if (!copyDirectory_4_OtherDataType(
                sourceDir,
                destinationDir,
                file_logger))
            {
                spdlog::drop(strLoggerName.c_str());
                setProgress(100);
                return false;
            }

        }
    }
    else
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "输入数据范围不在显示就绪型数据封装设置的空间范围内！");
        file_logger->info(szLog);
        file_logger->flush();
        // 卸载日志记录器
        spdlog::drop(strLoggerName.c_str());
        setProgress(100);
        return true;
    }

#pragma endregion   

    memset(szLog, 0, 1000);
    sprintf(szLog, "基础通用显示就绪型数据封装任务完成！");
    file_logger->info(szLog);
    file_logger->flush();
    // 卸载日志记录器
    spdlog::drop(strLoggerName.c_str());

#pragma endregion


#pragma endregion


    mProgress = 100;
    setProgress(mProgress);
    
    if (isCanceled())
    {
        return false; // 如果任务被取消，返回 false
    }

    // 返回 true 表示任务成功完成
    return true; 
}

bool SE_PackDisplayReadyDataTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_PackDisplayReadyDataTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_PackDisplayReadyDataTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_PackDisplayReadyDataTask::finished(bool result)
{
    emit taskFinished(result);
}

QStringList SE_PackDisplayReadyDataTask::GetFileNames(const QString& path)
{
    QDir dir(path);
    QStringList nameFilters;
    nameFilters << "*.geojson" << "*.GEOJSON";
    QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
    return files;
}



bool SE_PackDisplayReadyDataTask::IsExistedInVector(string strValue, vector<string>& vValue)
{
    for (int i = 0; i < vValue.size(); ++i)
    {
        if (strValue == vValue[i])
        {
            return true;
        }
    }

    return false;
}


// 获取当前路径下所有json元数据文件路径
void SE_PackDisplayReadyDataTask::GetJsonFilesFromFilePath(
    const string& strFilePath,
    vector<string>& vFilesPath)
{
    vFilesPath.clear();

    try
    {
        std::filesystem::path folderPath = strFilePath;

        if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
        {
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
        {
            if (entry.is_regular_file() && (entry.path().extension() == ".json" || entry.path().extension() == ".JSON"))
            {
                vFilesPath.push_back(entry.path().string());
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        return;
    }
}


bool SE_PackDisplayReadyDataTask::ReadMetaDataJsonFile(const string& strFile,
    vector<MetaDataTemplateInfo>& vMetadatInfo)
{
    vMetadatInfo.clear();

    // 读取JSON文件
    std::ifstream file(strFile.c_str());
    if (!file.is_open())
    {
        // 记录日志
        return false;
    }

    // 解析JSON文件
    ordered_json j;
    file >> j;

    // 数据源元数据
    // 访问data_source_list数组中的第一个元素
    auto& dsl = j["data_source_list"][0];
    // 管理信息
    MetaDataTemplateInfo metaInfo;

    // 管理信息-产品名称
    metaInfo.strShortFormItem = "GLXX_CPMC";
    metaInfo.strValue = convertEncoding(dsl["GLXX_CPMC"], "utf-8", "gbk");
    vMetadatInfo.push_back(metaInfo);

    // 标识信息-数据四至范围
    metaInfo.strShortFormItem = "BSXX_SJSZFW";
    metaInfo.strValue = convertEncoding(dsl["BSXX_SJSZFW"], "utf-8", "gbk");
    vMetadatInfo.push_back(metaInfo);

    return true;
}


// 根据元数据标识获取属性值
string SE_PackDisplayReadyDataTask::GetValueByKey(const string& strKey,
    vector<MetaDataTemplateInfo>& vMetadatInfo)
{
    for (int i = 0; i < vMetadatInfo.size(); ++i)
    {
        if (vMetadatInfo[i].strShortFormItem == strKey)
        {
            // 找到匹配的元素，输出其值
            return vMetadatInfo[i].strValue;
        }
    }
    // 如果没有找到匹配的元素，输出一个默认值或进行其他处理
    return "";
}

string SE_PackDisplayReadyDataTask::convertEncoding(const string& input,
    const string& fromEncoding,
    const string& toEncoding)
{
    iconv_t conv = iconv_open(toEncoding.c_str(), fromEncoding.c_str());
    if (conv == (iconv_t)(-1)) {
        perror("iconv_open");
        return "";
    }

    size_t inBytesLeft = input.size();
    size_t outBytesLeft = inBytesLeft * 4; // 预留足够的空间
    std::vector<char> output(outBytesLeft);
    char* inBuf = const_cast<char*>(input.data());
    char* outBuf = output.data();

    size_t result = iconv(conv, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft);
    if (result == (size_t)(-1)) {
        perror("iconv");
        iconv_close(conv);
        return "";
    }

    iconv_close(conv);
    return std::string(output.data(), output.size() - outBytesLeft);
}


// 封装文件拷贝函数
bool SE_PackDisplayReadyDataTask::copyFile(const fs::path& sourceFilePath, 
    const fs::path& destinationFilePath,
    std::shared_ptr<spdlog::logger> file_logger)
{
    char szLog[1000] = { 0 };
    // 以二进制模式打开源文件
    std::ifstream sourceFile(sourceFilePath, std::ios::binary);
    if (!sourceFile) 
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "无法打开源文件: %s", convertEncoding(sourceFilePath.string(), "gbk", "UTF-8").c_str() );
        file_logger->error(szLog);
        file_logger->flush();

        return false;
    }

    // 以二进制模式打开目标文件
    std::ofstream destinationFile(destinationFilePath, std::ios::binary);
    if (!destinationFile) 
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "无法打开目标文件: %s", convertEncoding(destinationFilePath.string(), "gbk", "UTF-8").c_str());
        file_logger->error(szLog);
        file_logger->flush();
        sourceFile.close();
        return false;
    }

    // 读取源文件内容并写入目标文件
    std::vector<char> buffer(4096);
    while (sourceFile.read(buffer.data(), buffer.size())) {
        destinationFile.write(buffer.data(), buffer.size());
    }
    destinationFile.write(buffer.data(), sourceFile.gcount());

    // 关闭文件流
    sourceFile.close();
    destinationFile.close();

    memset(szLog, 0, 1000);
    sprintf(szLog, "文件拷贝成功: %s->%s", 
        convertEncoding(sourceFilePath.string(), "gbk", "UTF-8").c_str(), 
        convertEncoding(destinationFilePath.string(), "gbk", "UTF-8").c_str());
    file_logger->info(szLog);
    file_logger->flush();
    
    return true;
}

// 递归拷贝目录函数（矢量数据类型使用）
bool SE_PackDisplayReadyDataTask::copyDirectory_4_ShiLiang(
    const fs::path& sourceDir, 
    const fs::path& destinationDir,
    std::shared_ptr<spdlog::logger> file_logger)
{
    char szLog[1000] = { 0 };
    try {
        // 创建目标目录
        if (!fs::exists(destinationDir)) 
        {
            fs::create_directories(destinationDir);
        }

        // 遍历源目录中的所有条目
        for (const auto& entry : fs::recursive_directory_iterator(sourceDir)) 
        {
            const fs::path& sourcePath = entry.path();
            fs::path relativePath = fs::relative(sourcePath, sourceDir);
            fs::path destinationPath = destinationDir / relativePath;

            if (fs::is_directory(entry)) {
                // 如果是目录，创建对应的目标目录
                if (!fs::exists(destinationPath)) {
                    fs::create_directories(destinationPath);
                }
            }
            else if (fs::is_regular_file(entry)) 
            {
                // 如果是文件，调用文件拷贝函数
                // 与要素类型列表判断
                string strFileName = CPLGetBasename(sourcePath.string().c_str());
                for (const auto& name : m_vFeatureTypes)
                {
                    // 如果文件名包含要素类型名称，则进行拷贝
                    if (strFileName.find(name) != string::npos)
                    {
                        // 拷贝
                        if (!copyFile(sourcePath, destinationPath, file_logger)) 
                        {
                            memset(szLog, 0, 1000);
                            sprintf(szLog, "文件拷贝失败: %s->%s", 
                                convertEncoding(sourcePath.string(), "gbk", "UTF-8").c_str(),
                                convertEncoding(destinationPath.string(), "gbk", "UTF-8").c_str());
                            file_logger->error(szLog);
                            file_logger->flush();
                            return false;
                        }
                        break;
                    }
                }
            }
        }

        memset(szLog, 0, 1000);
        sprintf(szLog, "目录拷贝成功: %s->%s", 
            convertEncoding(sourceDir.string(), "gbk", "UTF-8").c_str(),
            convertEncoding(destinationDir.string(), "gbk", "UTF-8").c_str());
        file_logger->info(szLog);
        file_logger->flush();
        
        return true;
    }
    catch (const fs::filesystem_error& e) 
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "目录拷贝失败: %s", e.what());
        file_logger->error(szLog);
        file_logger->flush();
        return false;
    }
}


// 递归拷贝目录函数（除矢量数据类型之外使用）
bool SE_PackDisplayReadyDataTask::copyDirectory_4_OtherDataType(
    const fs::path& sourceDir,
    const fs::path& destinationDir,
    std::shared_ptr<spdlog::logger> file_logger)
{
    char szLog[1000] = { 0 };
    try {
        // 创建目标目录
        if (!fs::exists(destinationDir))
        {
            fs::create_directories(destinationDir);
        }

        // 遍历源目录中的所有条目
        for (const auto& entry : fs::recursive_directory_iterator(sourceDir))
        {
            const fs::path& sourcePath = entry.path();
            fs::path relativePath = fs::relative(sourcePath, sourceDir);
            fs::path destinationPath = destinationDir / relativePath;

            if (fs::is_directory(entry)) {
                // 如果是目录，创建对应的目标目录
                if (!fs::exists(destinationPath)) {
                    fs::create_directories(destinationPath);
                }
            }
            else if (fs::is_regular_file(entry))
            {
                // 如果是文件，调用文件拷贝函数
                if (!copyFile(sourcePath, destinationPath, file_logger))
                {
                    memset(szLog, 0, 1000);
                    sprintf(szLog, "文件拷贝失败: %s->%s", 
                        convertEncoding(sourcePath.string(), "gbk", "UTF-8").c_str(),
                        convertEncoding(destinationPath.string(), "gbk", "UTF-8").c_str());
                    file_logger->error(szLog);
                    file_logger->flush();
                    return false;
                }
            }
        }

        memset(szLog, 0, 1000);
        sprintf(szLog, "目录拷贝成功: %s->%s", 
            convertEncoding(sourceDir.string(), "gbk", "UTF-8").c_str(),
            convertEncoding(destinationDir.string(), "gbk", "UTF-8").c_str());
        file_logger->info(szLog);
        file_logger->flush();

        return true;
    }
    catch (const fs::filesystem_error& e)
    {
        memset(szLog, 0, 1000);
        sprintf(szLog, "目录拷贝失败: %s", e.what());
        file_logger->error(szLog);
        file_logger->flush();
        return false;
    }
}


std::string SE_PackDisplayReadyDataTask::generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    const char* hex_chars = "0123456789abcdef";
    std::stringstream uuid;
    for (int i = 0; i < 8; ++i) {
        uuid << hex_chars[dis(gen)];
    }
    uuid << '-';
    for (int i = 0; i < 4; ++i) {
        uuid << hex_chars[dis(gen)];
    }
    uuid << '-';
    // 版本 4 的 UUID，第 13 位固定为 4
    uuid << '4';
    for (int i = 0; i < 3; ++i) {
        uuid << hex_chars[dis(gen)];
    }
    uuid << '-';
    // 变体部分
    uuid << hex_chars[dis2(gen)];
    for (int i = 0; i < 3; ++i) {
        uuid << hex_chars[dis(gen)];
    }
    uuid << '-';
    for (int i = 0; i < 12; ++i) {
        uuid << hex_chars[dis(gen)];
    }

    return uuid.str();
}
