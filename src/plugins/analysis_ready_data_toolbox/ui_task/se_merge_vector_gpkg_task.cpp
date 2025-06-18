#define _HAS_STD_BYTE 0
#include "se_merge_vector_gpkg_task.h"
#include <qdir.h>
#include <filesystem>
#include "commontype/se_commontypedef.h"
#include <sqlite3.h>

#include "commontype/se_commondef.h"

/*spdlog日志库头文件*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
using namespace std;


/*构造函数*/
SE_MergeVectorGpkgTask::SE_MergeVectorGpkgTask(const QString& name,
    const vector<string>& vInputGpkgPath,
    MergePackageStrategy mergeStrategy,
	const string& strDataTime,
    const string& strOutputPath,
	int iLogLevel,
	const string& strOutputLogPath)
    : QgsTask(name), 
    m_vInputGpkgPath(vInputGpkgPath),
    m_MergeStrategy(mergeStrategy),
	m_strDataTime(strDataTime),
    m_strOutputPath(strOutputPath), 
	m_iLogLevel(iLogLevel),
	m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_MergeVectorGpkgTask::run()
{
	/*算法思路：
	（1）遍历待合包的gpkg文件列表A，根据每个gpkg文件名称及分包规则计算合包后的gpkg文件名及分析就绪数据产品列表
	，并加入到gpkg文件列表B及分析就绪产品列表
	（2）遍历列表B中的gpkg文件及每个分析就绪产品列表，按照Z-X-Y循环，加分析就绪数据产品标识，
    依次读取数据，如果要素个数为0，跳过当前图层，
	分别按照融合和替换方式进行同一分析就绪产品类型、同一Z-X-Y位置数据的处理；
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


	// 日志器名称
	string strLogFileName = "MergeVectorGpkg";

	// 日志文件全路径
	string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";

	// 日志器名称
	string strLoggerName = "MergeVectorGpkg";


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
	sprintf(szLog, "正在执行矢量分析就绪型数据包合包任务...");
	file_logger->info(szLog);
	file_logger->debug(szLog);
	file_logger->flush();

#pragma endregion






#pragma region "算法实现部分"

	// 注册所有的格式
	GDALAllRegister();

	// 输出合包时间文件
	string strMergeTime = m_strOutputPath + "/merge_vector_gpkg_time.txt";
	FILE* fpMergeTime = fopen(strMergeTime.c_str(), "w");
	if (!fpMergeTime)
	{
		file_logger->error("合包时间文件创建失败！");
		file_logger->flush();
		// 卸载日志记录器
		spdlog::drop(strLoggerName.c_str());
		setProgress(0);
		return false;
	}

	fprintf(fpMergeTime, "合包时间：%s\n", m_strDataTime.c_str());
	fflush(fpMergeTime);
	fclose(fpMergeTime);

	file_logger->info("合包时间文件创建成功！");
	file_logger->debug("合包时间文件创建成功！");
	file_logger->flush();


	file_logger->info("========================合包开始========================");
	file_logger->debug("========================合包开始========================");
	file_logger->flush();


#pragma region "（1）遍历待合包数据包路径列表，记录数据包信息"

	file_logger->info("------------------待合包数据------------------");
	file_logger->debug("------------------待合包数据------------------");
	file_logger->flush();


	// 所有gpkg文件全路径
	vector<string> vInputGpkgFilePathList;
	vInputGpkgFilePathList.clear();


	// 获取每个路径下的gpkg文件列表
	for (const auto& gpkgPath : m_vInputGpkgPath)
	{
		vector<string> vCurrentDirFilesPath;
		vCurrentDirFilesPath.clear();
		GetGpkgFilesFromFilePath(gpkgPath, vCurrentDirFilesPath);

		// 如果当前路径下没有gpkg文件，跳过
		if (vCurrentDirFilesPath.size() == 0)
		{
			continue;
		}

		// 遍历每个gpkg路径
		for (const auto& currentDirFiles : vCurrentDirFilesPath)
		{
			vInputGpkgFilePathList.push_back(currentDirFiles);
		}
	}

	// 打开所有的gpkg文件
	// 创建数据库
	vector<GDALDataset*> vInputGpkgDBPtr;
	GDALDataset* pInputGpkgDB;
	for (const auto& gpkgFile : vInputGpkgFilePathList)
	{
		// 打开gpkg文件
		pInputGpkgDB = (GDALDataset*)GDALOpenEx(gpkgFile.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);

		if (pInputGpkgDB == nullptr)
		{
			vInputGpkgDBPtr.push_back(nullptr);
			continue;
		}

		vInputGpkgDBPtr.push_back(pInputGpkgDB);
	}




#pragma endregion

#pragma region "（2）遍历每个待合包的gpkg文件，按ZXY顺序依次读取shp文件"

	// 记录当前所有gpkg的信息，目标gpkg及图层均按照此信息创建
	vector<VectorARDGpkgInfo> vInputGpkgInfos;
	vInputGpkgInfos.clear();


	/*按照输入gpkg文件的名称，根据分包规则，计算目标gpkg包名*/
	for (const auto& gpkgFilePath : vInputGpkgFilePathList)
	{
		VectorARDGpkgInfo info;
		// 读取gpkg的元数据表，获取分析就绪产品集合
		if (!GetVectorGpkgInfos(gpkgFilePath, info))
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "读取%s的分析就绪数据产品集合失败！", gpkgFilePath.c_str());
			file_logger->error(szLog);
			file_logger->flush();

			continue;
		}

		// 如果不存在
		if (!IsExistedInVector(info, vInputGpkgInfos))
		{
			vInputGpkgInfos.push_back(info);
		}
	}

#pragma region "遍历目标gpkg，在输出目录下创建gpkg名称，按照数据包的级别信息依次从输入gpkg中读取数据"


	memset(szLog, 0, 1000);
	sprintf(szLog, "矢量分析就绪型数据包合包成果共生成%d个gpkg文件", vInputGpkgInfos.size());
	file_logger->info(szLog);
	file_logger->debug(szLog);
	file_logger->flush();


	// 目标gpkg数据库对象
	vector<GDALDataset*> vTargetGpkgDB;
	vTargetGpkgDB.clear();

	// 获取GPKG驱动
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
	if (poDriver == nullptr)
	{
		return false;
	}

	// 当前进行的进度
	int iProcessCount = 0;
	for (const auto& gpkgInfo : vInputGpkgInfos)
	{
		// 输出结果gpkg数据库
		GDALDataset* pTargetGpkgDB;

		iProcessCount++;

		string strTargetGpkgFilePath = m_strOutputPath;
		strTargetGpkgFilePath += "/";
		strTargetGpkgFilePath += gpkgInfo.strGpkgName;
		strTargetGpkgFilePath += ".gpkg";

		memset(szLog, 0, 1000);
		sprintf(szLog, "正在进行第%d个名称为%s的矢量分析就绪型数据包合包...", iProcessCount + 1, strTargetGpkgFilePath.c_str());
		file_logger->info(szLog);
		file_logger->debug(szLog);
		file_logger->flush();


		// 创建GPKG对象
		pTargetGpkgDB = poDriver->Create(strTargetGpkgFilePath.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
		if (pTargetGpkgDB == nullptr)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "无法创建数据库:%s!", strTargetGpkgFilePath.c_str());
			file_logger->error(szLog);
			file_logger->flush();

			continue;
		}

		vector<GDALDataset*> vOutputGpkgDBPtr;
		vOutputGpkgDBPtr.clear();

		// 根据gpkg名称获取对应的输入gpkg数据库对象集合
		GetGpkgDBPtrByGpkgName(vInputGpkgDBPtr, vInputGpkgFilePathList, gpkgInfo.strGpkgName, vOutputGpkgDBPtr);

		// 如果不存在对应的gpkg数据库，跳过
		if (vOutputGpkgDBPtr.size() == 0)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "不存在对应的gpkg数据库!");
			file_logger->error(szLog);
			file_logger->flush();
			continue;
		}

		// 创建"VECTOR_分析就绪数据产品标识_Z_X_Y图层"
		for (const auto& ARDIdentify : gpkgInfo.vVectorARDIdentifys)
		{

#pragma region "循环每个分析就绪数据，读取ZXY对应的图层，如果至少有一个图层并且要素个数大于0，则创建结果图层"

			// 根据gpkg名称依次遍历级别、行、列及分析就绪数据层
			int iMinZ = 0;		// 包内最小级别
			int iMaxZ = 0;		// 包内最大级别
			int iBaseZ = 0;		// 基础分包级别
			int iBaseX = 0;		// 基础分包X索引
			int iBaseY = 0;		// 基础分包Y索引
			GetGpkgInfo(gpkgInfo.strGpkgName, iMinZ, iMaxZ, iBaseZ, iBaseX, iBaseY);

			// ................................
			// TODO:后面根据元数据获取
			// ................................
			// 如果是以第4级打包，则格网级别为10级
			if (iBaseZ == 4)
			{
				iMinZ = 10;
				iMaxZ = 10;
			}
			// 如果是以第8级打包，则格网级别为12级
			if (iBaseZ == 8)
			{
				iMinZ = 12;
				iMaxZ = 12;
			}

			// 循环每个级别
			for (int iZ = iMinZ; iZ <= iMaxZ; ++iZ)
			{

				memset(szLog, 0, 1000);
				sprintf(szLog, "正在进行级别%d的数据包合包...", iZ);
				file_logger->info(szLog);
				file_logger->debug(szLog);
				file_logger->flush();


				// 计算基本分包ZXY覆盖的当前级别Z对应的X、Y范围
				SE_DRect dZxyRect = CalGridExtentByZXY(iBaseZ, iBaseX, iBaseY);


				int start_x = 0;
				int start_y = 0;
				int end_x = 0;
				int end_y = 0;

				CalXRangeAndYRangeByGeoRectAndLevel(dZxyRect, iZ, start_x, end_x, start_y, end_y);

				// 循环列
				for (int index_x = start_x; index_x <= end_x; ++index_x)
				{
					// 循环行
					for (int index_y = start_y; index_y <= end_y; ++index_y)
					{
						// 读取同一ZXY的n个图层
						vector<OGRLayer*> vOgrLayerInfos;
						vOgrLayerInfos.clear();

						// 优先加入的是最顶层显示的gpkg路径下的图层
						char szGpkgLayerNameTemp[50] = { 0 };
						sprintf(szGpkgLayerNameTemp, "VECTOR_%s_%d_%d_%d",
							ARDIdentify,
							iZ,
							index_x,
							index_y);

						string strGpkgLayerNameTemp = szGpkgLayerNameTemp;

						// 读取对应分析就绪数据表中的ZXY图层
						for (int iLayerIndex = 0; iLayerIndex < vOutputGpkgDBPtr.size(); ++iLayerIndex)
						{

							OGRLayer* pLayerTemp = vOutputGpkgDBPtr[iLayerIndex]->GetLayerByName(strGpkgLayerNameTemp.c_str());
							// 如果没有对应的图层，跳过
							if (!pLayerTemp)
							{
								memset(szLog, 0, 1000);
								sprintf(szLog, "数据库中不存在图层：%s！", strGpkgLayerNameTemp.c_str());
								file_logger->error(szLog);
								file_logger->flush();

								continue;
							}
						
							vOgrLayerInfos.push_back(pLayerTemp);
						}


						// 如果所有输入gpkg文件中不存在当前ZXY图层，跳过当前循环
						if (vOgrLayerInfos.size() == 0)
						{
							continue;
						}

						// 如果只存在1个图层，则直接存入gpkg中，策略为替换
						if (vOgrLayerInfos.size() == 1)
						{
							if (!ImportOGRLayerToGeoPackage(
								strGpkgLayerNameTemp.c_str(),
								vOgrLayerInfos,
								MergePackageStrategy::REPLACE,
								pTargetGpkgDB))
							{
								memset(szLog, 0, 1000);
								sprintf(szLog, "%s图层替换入库失败！", strGpkgLayerNameTemp.c_str());
								file_logger->error(szLog);
								file_logger->flush();

								continue;
							}
						}
						// 如果存在多于1个同名像素块，才需要考虑是否融合
						else
						{
							if (!ImportOGRLayerToGeoPackage(
								strGpkgLayerNameTemp.c_str(),
								vOgrLayerInfos,
								m_MergeStrategy,
								pTargetGpkgDB))
							{
								memset(szLog, 0, 1000);
								sprintf(szLog, "%s图层融合入库失败！", strGpkgLayerNameTemp.c_str());
								file_logger->error(szLog);
								file_logger->flush();
								continue;
							}
						}
					}
				}


			}

#pragma endregion


		}
		GDALClose(pTargetGpkgDB);

		// 设置进度
		setProgress(iProcessCount * 100.0 / vInputGpkgInfos.size());
	}


#pragma endregion


#pragma endregion




	memset(szLog, 0, 1000);
	sprintf(szLog, "========================合包完成！========================");
	file_logger->info(szLog);
	file_logger->debug(szLog);
	file_logger->flush();
	// 卸载日志记录器
	spdlog::drop(strLoggerName.c_str());

	// 关闭数据库
	for (int i = 0; i < vInputGpkgDBPtr.size(); ++i)
	{
		GDALClose(vInputGpkgDBPtr[i]);
	}



#pragma endregion

    setProgress(100);
    return true;

}

bool SE_MergeVectorGpkgTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_MergeVectorGpkgTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_MergeVectorGpkgTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_MergeVectorGpkgTask::finished(bool result)
{
    emit taskFinished(result);
}

QStringList SE_MergeVectorGpkgTask::GetFileNames(const QString& path)
{
    QDir dir(path);
    QStringList nameFilters;
    nameFilters << "*.gpkg";
    QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
    return files;
}

// 判断目录是否存在
bool SE_MergeVectorGpkgTask::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}


/*获取当前目录下所有子目录*/
void SE_MergeVectorGpkgTask::GetSubDirFromFilePath(const string& strFilePath,
    vector<string>& vSubDir)
{
    // 获取当前工作目录
    std::filesystem::path currentPath = strFilePath;
    std::cout << "Current Path: " << currentPath << std::endl;

    // 遍历当前目录下的所有条目
    for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
        // 检查条目是否是目录
        if (std::filesystem::is_directory(entry.status())) {
            vSubDir.push_back(entry.path().filename().string());
            //std::cout << "Subdirectory: " << entry.path().filename() << std::endl;
        }
    }
}

// 获取当前路径下所有tif文件路径
void SE_MergeVectorGpkgTask::GetTifFilesFromFilePath(
    const string& strFilePath,
    vector<string>& vFilePath)
{
    vFilePath.clear();

    try
    {
        std::filesystem::path folderPath = strFilePath;

        if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
        {
            std::cerr << "指定的路径不存在或不是一个文件夹。" << std::endl;
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".tif")
            {
                vFilePath.push_back(entry.path().string());
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::cerr << "文件系统错误: " << e.what() << std::endl;
    }
}

// 根据文件名提取Z、X、Y
bool SE_MergeVectorGpkgTask::extractZXY(
    const std::string& tifFilePath,
    int& Z,
    int& X,
    int& Y)
{
    std::filesystem::path path(tifFilePath);

    // 获取文件名（不带扩展名）"Y"
    std::string fileName = path.stem().string();

    // 获取文件所在的目录 "G:/tif_storage_data/input_data/Z/X"
    std::filesystem::path parentPath = path.parent_path();

    // 提取 Z、X、Y
    std::string zPath = parentPath.parent_path().filename().string(); // "Z"
    std::string xPath = parentPath.filename().string();               // "X"
    std::string yPath = fileName;                                     // "Y"

    try {
        Z = std::stoi(zPath);
        X = std::stoi(xPath);
        Y = std::stoi(yPath);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "无法从 " << tifFilePath << " 解析 Z、X、Y 值: " << e.what() << std::endl;
        return false;
    }
}

/*根据JK级别获取分辨率（地理坐标系，单位：度）*/
double SE_MergeVectorGpkgTask::GetGridResByGridLevel(int iLevel)
{
    switch (iLevel)
    {
    case 0:
        return 512.0;

    case 1:
        return 256.0;

    case 2:
        return 128.0;

    case 3:
        return 64.0;

    case 4:
        return 32.0;

    case 5:
        return 16.0;

    case 6:
        return 8.0;

    case 7:
        return 4.0;

    case 8:
        return 2.0;

    case 9:
        return 1.0;

    case 10:
        return 0.5;

    case 11:
        return 0.25;

    case 12:
        return 1.0 / 12;

    case 13:
        return 1.0 / 60;

    case 14:
        return 1.0 / 120;

    case 15:
        return 1.0 / 240;

    case 16:
        return 1.0 / 720;

    case 17:
        return 1.0 / 3600;

    case 18:
        return 1.0 / 7200;

    case 19:
        return 1.0 / 14400;

    case 20:
        return 1.0 / 28800;

    case 21:
        return 1.0 / 28800;

    case 22:
        return 1.0 / 57600;

    case 23:
        return 1.0 / 115200;

    case 24:
        return 1.0 / 230400;

    case 25:
        return 1.0 / 460800;

    default:
        break;
    }
    return 1.0;
}

/*根据级别、行、列索引计算格网的经纬度范围*/
SE_DRect SE_MergeVectorGpkgTask::CalGridExtentByZXY(int iLevel, int iX, int iY)
{
    // 根据格网获取对应的分辨率
    double dGridRes = GetGridResByGridLevel(iLevel);

    SE_DRect dGridRect;
    dGridRect.dleft = -256 + iX * dGridRes;
    dGridRect.dright = -256 + (iX + 1) * dGridRes;
    dGridRect.dtop = 256 - iY * dGridRes;
    dGridRect.dbottom = 256 - (iY + 1) * dGridRes;
    return dGridRect;
}


/*根据经纬度范围、打包级别计算横向X取值范围、纵向Y取值范围*/
void SE_MergeVectorGpkgTask::CalXRangeAndYRangeByGeoRectAndLevel(
    SE_DRect dRect,
    int iGridLevel,
    int& iMinX,
    int& iMaxX,
    int& iMinY,
    int& iMaxY)
{
    // 根据格网获取对应的分辨率
    double dGridRes = GetGridResByGridLevel(iGridLevel);

    // 计算最小X
    iMinX = int(fabs(dRect.dleft + 256) / dGridRes);

    // 计算最大X
    iMaxX = int(fabs(dRect.dright + 256) / dGridRes);

    // 计算最小Y
    iMinY = int(fabs(256 - dRect.dtop) / dGridRes);

    // 计算最大Y
    iMaxY = int(fabs(256 - dRect.dbottom) / dGridRes);
}



/*根据Z计算分包级别*/
void SE_MergeVectorGpkgTask::GetPackLevelByZ(vector<ARD_PACKAGE_INFO>& vInfo, int iZ, int& iPackLevel)
{
    for (int i = 0; i < vInfo.size(); ++i)
    {
        if (iZ >= vInfo[i].iMinZ && iZ <= vInfo[i].iMaxZ)
        {
            iPackLevel = vInfo[i].iBaseZ;
            break;
        }
    }
}


/*根据经纬度、打包级别计算行列索引*/
void SE_MergeVectorGpkgTask::CalXAndYByPointAndLevel(
    SE_DPoint dPoint,
    int iPackageLevel,
    int& iX,
    int& iY)
{
    // 根据格网获取对应的分辨率
    double dGridRes = GetGridResByGridLevel(iPackageLevel);

    // 计算X
    iX = int(fabs(dPoint.dx + 256) / dGridRes);

    // 计算Y
    iY = int(fabs(256 - dPoint.dy) / dGridRes);

}


// 根据ZXY获取对应的GDALDataset*对象索引
void SE_MergeVectorGpkgTask::GetIndexFromGpkgDB(vector<ZXY_INDEX>& vIndex, int iZ, int iX, int iY, int& iIndex)
{
    for (int i = 0; i < vIndex.size(); ++i)
    {
        if (vIndex[i].iZ == iZ
            && vIndex[i].iX == iX
            && vIndex[i].iY == iY)
        {
            iIndex = i;
            break;
        }
    }
}







// 获取当前路径下所有gpkg文件路径
void SE_MergeVectorGpkgTask::GetGpkgFilesFromFilePath(
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
			if (entry.is_regular_file() && (entry.path().extension() == ".gpkg" || entry.path().extension() == ".GPKG"))
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


void SE_MergeVectorGpkgTask::GetGpkgInfo(string strGpkgFileName,
	int& minz,
	int& maxz,
	int& basez,
	int& x,
	int& y)
{
	QString qstrGpkgFileName = tr(strGpkgFileName.c_str());

	QStringList qstrInfoList = qstrGpkgFileName.split("-");

	minz = qstrInfoList[0].toInt();
	maxz = qstrInfoList[1].toInt();
	basez = qstrInfoList[2].toInt();
	x = qstrInfoList[3].toInt();
	y = qstrInfoList[4].toInt();

}

// 获取gpkg数据库中所有的分析就绪产品名称
bool SE_MergeVectorGpkgTask::GetVectorGpkgInfos(const string& strGpkgFilePath, VectorARDGpkgInfo& info)
{
	info.strGpkgName = CPLGetBasename(strGpkgFilePath.c_str());

	// 矢量分析就绪数据标识
	vector<string> vVectorARDIdentifys;
	vVectorARDIdentifys.clear();


	// 打开gpkg文件
	GDALDataset* poGpkgDataset = (GDALDataset*)GDALOpenEx(strGpkgFilePath.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
	if (poGpkgDataset == nullptr) 
	{
		return false;
	}

	// 获取gpkg数据库矢量图层个数
	int iLayerCount = poGpkgDataset->GetLayerCount();
	for (int i = 0; i < iLayerCount; ++i)
	{
		OGRLayer* pLayer = poGpkgDataset->GetLayer(i);
		// 如果图层为空，跳过
		if (!pLayer) 
		{
			continue;
		}

		// 获取图层名
		string strLayerName = pLayer->GetName();

		// 从strLayerName（VECTOR_02_120000_01_11_1506_935）提取从索引第7个字符，长度为12的字符串，即分析就绪数据标识
		string strARDIdentify = strLayerName.substr(7, 12);

		// 使用 std::find 查找目标字符串
		auto it = std::find(vVectorARDIdentifys.begin(), vVectorARDIdentifys.end(), strARDIdentify);

		// 如果不存在，则加到vector中
		if (it == vVectorARDIdentifys.end()) 
		{
			vVectorARDIdentifys.push_back(strARDIdentify);
		}
	}

	info.vVectorARDIdentifys = vVectorARDIdentifys;

	GDALClose(poGpkgDataset);

	return true;
}

bool SE_MergeVectorGpkgTask::IsExistedInVector(VectorARDGpkgInfo& info, vector<VectorARDGpkgInfo>& vValue)
{
	for (int i = 0; i < vValue.size(); ++i)
	{
		if (info.strGpkgName == vValue[i].strGpkgName
			&& info.vVectorARDIdentifys == vValue[i].vVectorARDIdentifys)
		{		
			return true;
		}
	}

	return false;
}


// 根据名称获取对应的gpkg数据库对象
void SE_MergeVectorGpkgTask::GetGpkgDBPtrByGpkgName(
	vector<GDALDataset*>& vGpkgDBPtr,
	vector<string>& vGpkgFilePath,
	const string& strGpkgName,
	vector<GDALDataset*>& vOutputGpkgDBPtr)
{
	vOutputGpkgDBPtr.clear();

	for (int i = 0; i < vGpkgFilePath.size(); ++i)
	{
		// 如果找到了对应名称的gpkg，并且不为空
		if (strstr(vGpkgFilePath[i].c_str(), strGpkgName.c_str()) != nullptr
			&& vGpkgDBPtr[i])
		{
			vOutputGpkgDBPtr.push_back(vGpkgDBPtr[i]);
		}
	}
}



/*导入单个shp文件到gpkg中*/
// strARDLable:分析就绪数据标识，每个矢量入库后名称为："VECTOR_分析就绪产品标识_Z_X_Y"
bool SE_MergeVectorGpkgTask::ImportOGRLayerToGeoPackage(
	const string& strARDLable, 
	vector<OGRLayer*> vLayers, 
	MergePackageStrategy mergeStrategy,
	GDALDataset* pGpkgDB)
{
	// 如果是融合
	if (mergeStrategy == MergePackageStrategy::FUSION)
	{
		// 融合思路
		// 逐个从所有图层中取出所有要素写入gpkg数据库中
		// 创建目标图层
		string strTargetLayerName = strARDLable;

		OGRwkbGeometryType targetGeoType = wkbUnknown;

		// 图层的几何类型
		OGRwkbGeometryType srcGeoType = vLayers[0]->GetGeomType();

		// 图层的空间参考
		OGRSpatialReference* srcSR = vLayers[0]->GetSpatialRef();
		// 如果源图层是点类型
		if (srcGeoType == wkbPoint)
		{
			targetGeoType = wkbPoint;
		}

		// 如果是线
		else if (srcGeoType == wkbLineString)
		{
			targetGeoType = wkbLineString;
		}

		// 如果是多线
		else if (srcGeoType == wkbMultiLineString)
		{
			targetGeoType = wkbMultiLineString;
		}


		// 如果是面
		else if (srcGeoType == wkbPolygon)
		{
			targetGeoType = wkbPolygon;
		}

		// 如果是多面
		else if (srcGeoType == wkbMultiPolygon)
		{
			targetGeoType = wkbMultiPolygon;
		}


		// 创建结果图层
		OGRLayer* poDstLayer = pGpkgDB->CreateLayer(
			strTargetLayerName.c_str(),
			srcSR,
			targetGeoType,
			nullptr);

		if (poDstLayer == nullptr)
		{
			return false;
		}

		// 复制字段
		OGRFeatureDefn* poSrcFDefn = vLayers[0]->GetLayerDefn();
		for (int i = 0; i < poSrcFDefn->GetFieldCount(); i++)
		{
			OGRFieldDefn* poFieldDefn = poSrcFDefn->GetFieldDefn(i);
			poDstLayer->CreateField(poFieldDefn);
		}

		// 遍历图层
		for (const auto& oLayer : vLayers)
		{
			// 复制要素
			OGRFeature* poFeature;
			oLayer->ResetReading();
			while ((poFeature = oLayer->GetNextFeature()) != nullptr)
			{
				OGRFeature* poNewFeature = OGRFeature::CreateFeature(poDstLayer->GetLayerDefn());
				poNewFeature->SetFrom(poFeature);
				if (poDstLayer->CreateFeature(poNewFeature) != OGRERR_NONE)
				{
					continue;
				}
				OGRFeature::DestroyFeature(poNewFeature);
				OGRFeature::DestroyFeature(poFeature);
			}
		}
	}
	// 如果是替换
	else if (mergeStrategy == MergePackageStrategy::REPLACE)
	{
		// 创建目标图层
		string strTargetLayerName = strARDLable;

		OGRwkbGeometryType targetGeoType = wkbUnknown;

		// 图层的几何类型
		OGRwkbGeometryType srcGeoType = vLayers[0]->GetGeomType();

		// 图层的空间参考
		OGRSpatialReference *srcSR = vLayers[0]->GetSpatialRef();
		// 如果源图层是点类型
		if (srcGeoType == wkbPoint)
		{
			targetGeoType = wkbPoint;
		}

		// 如果是线
		else if (srcGeoType == wkbLineString)
		{
			targetGeoType = wkbLineString;
		}

		// 如果是多线
		else if (srcGeoType == wkbMultiLineString)
		{
			targetGeoType = wkbMultiLineString;
		}


		// 如果是面
		else if (srcGeoType == wkbPolygon)
		{
			targetGeoType = wkbPolygon;
		}

		// 如果是多面
		else if (srcGeoType == wkbMultiPolygon)
		{
			targetGeoType = wkbMultiPolygon;
		}


		// 创建结果图层
		OGRLayer* poDstLayer = pGpkgDB->CreateLayer(
			strTargetLayerName.c_str(),
			srcSR,
			targetGeoType,
			nullptr);

		if (poDstLayer == nullptr)
		{
			return false;
		}

		// 复制字段
		OGRFeatureDefn* poSrcFDefn = vLayers[0]->GetLayerDefn();
		for (int i = 0; i < poSrcFDefn->GetFieldCount(); i++)
		{
			OGRFieldDefn* poFieldDefn = poSrcFDefn->GetFieldDefn(i);
			poDstLayer->CreateField(poFieldDefn);
		}

		// 复制要素
		OGRFeature* poFeature;
		vLayers[0]->ResetReading();
		while ((poFeature = vLayers[0]->GetNextFeature()) != nullptr)
		{
			OGRFeature* poNewFeature = OGRFeature::CreateFeature(poDstLayer->GetLayerDefn());
			poNewFeature->SetFrom(poFeature);
			if (poDstLayer->CreateFeature(poNewFeature) != OGRERR_NONE)
			{
				continue;
			}
			OGRFeature::DestroyFeature(poNewFeature);
			OGRFeature::DestroyFeature(poFeature);
		}
	}


	return true;
}