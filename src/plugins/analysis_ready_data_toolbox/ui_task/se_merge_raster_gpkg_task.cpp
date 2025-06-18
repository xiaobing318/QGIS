#define _HAS_STD_BYTE 0
#include "se_merge_raster_gpkg_task.h"
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
SE_MergeRasterGpkgTask::SE_MergeRasterGpkgTask(const QString& name,
    const vector<string>& vInputGpkgPath,
    MergePackageStrategy mergeStrategy,
    FusionType fusionType,
	const string& strDataTime,
    const string& strOutputPath,
	int iLogLevel,
	const string& strOutputLogPath)
    : QgsTask(name), 
    m_vInputGpkgPath(vInputGpkgPath),
    m_MergeStrategy(mergeStrategy),
    m_FusionType(fusionType),
	m_strDataTime(strDataTime),
    m_strOutputPath(strOutputPath), 
	m_iLogLevel(iLogLevel),
	m_strOutputLogPath(strOutputLogPath),
    mProgress(0)
{
    mCanceled = false;
}


bool SE_MergeRasterGpkgTask::run()
{
	/*算法思路：
	（1）遍历待合包的gpkg文件列表A，根据每个gpkg文件名称及分包规则计算合包后的gpkg文件名及分析就绪数据产品列表
	，并加入到gpkg文件列表B及分析就绪产品列表，建立对应的表
	（2）遍历列表B中的gpkg文件及每个分析就绪产品列表，按照Z-X-Y循环，根据Z-X-Y及分包规则计算原始输入的gpkg包名，然后读取数据，
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
	string strLogFileName = "MergeRasterGpkg";

	// 日志文件全路径
	string strLogFileFullPath = m_strOutputLogPath + "/System_Running_" + strLogLevel + "_" + strLogFileName + ".txt";


	// 日志器名称
	string strLoggerName = "MergeRasterGpkg";


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
	sprintf(szLog, "正在执行栅格分析就绪型数据包合包任务...");
	file_logger->info(szLog);
	file_logger->debug(szLog);
	file_logger->flush();

#pragma endregion


#pragma region "算法实现部分"

	// 输出合包时间文件
	string strMergeTime = m_strOutputPath + "/merge_raster_gpkg_time.txt";
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
	for (const auto& gpkgPath:  m_vInputGpkgPath)
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
		for ( const auto& currentDirFiles: vCurrentDirFilesPath)
		{
			vInputGpkgFilePathList.push_back(currentDirFiles);
		}
	}

	// 打开所有的gpkg文件
	// 创建数据库
	vector<sqlite3*> vInputGpkgDBPtr;
	sqlite3* pInputGpkgDB;
	for (const auto& gpkgFile : vInputGpkgFilePathList)
	{
		if (sqlite3_open(gpkgFile.c_str(), &pInputGpkgDB) != SQLITE_OK)
		{
			vInputGpkgDBPtr.push_back(nullptr);
			file_logger->info(sqlite3_errmsg(pInputGpkgDB));
			file_logger->debug(sqlite3_errmsg(pInputGpkgDB));
			file_logger->flush();
			continue;
		}

		vInputGpkgDBPtr.push_back(pInputGpkgDB);
	}




#pragma endregion

#pragma region "（2）遍历每个待合包的gpkg文件，按ZXY顺序依次读取二进制流"

	// 记录当前所有gpkg的信息，目标gpkg及表均按照此信息创建
	vector<RasterARDGpkgInfo> vInputGpkgInfos;
	vInputGpkgInfos.clear();


	/*按照输入gpkg文件的名称，根据分包规则，计算目标gpkg包名*/
	for (const auto& gpkgFilePath : vInputGpkgFilePathList)
	{
		RasterARDGpkgInfo info;
		// 读取gpkg的元数据表，获取分析就绪产品集合
		if (!GetRasterGpkgInfos(gpkgFilePath, info))
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "读取%s的元数据表失败！", gpkgFilePath.c_str());
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

#pragma region "遍历目标gpkg，在输出目录下创建gpkg名称及表，按照数据包的级别信息依次从输入gpkg中读取数据"

	memset(szLog, 0, 1000);
	sprintf(szLog, "栅格分析就绪型数据包合包成果共生成%d个gpkg文件", vInputGpkgInfos.size());
	file_logger->info(szLog);
	file_logger->debug(szLog);
	file_logger->flush();

	vector<sqlite3*> vTargetGpkgDB;
	vTargetGpkgDB.clear();



	// 当前进行的进度
	int iProcessCount = 0;
	for (const auto& gpkgInfo : vInputGpkgInfos)
	{
		// 输出结果gpkg数据库
		sqlite3* pTargetGpkgDB;

		iProcessCount++;

		string strTargetGpkgFilePath = m_strOutputPath;
		strTargetGpkgFilePath += "/";
		strTargetGpkgFilePath += gpkgInfo.strGpkgName;
		strTargetGpkgFilePath += ".gpkg";

		memset(szLog, 0, 1000);
		sprintf(szLog, "正在进行第%d个名称为%s的栅格分析就绪型数据包合包...", iProcessCount + 1, strTargetGpkgFilePath.c_str());
		file_logger->info(szLog);
		file_logger->debug(szLog);
		file_logger->flush();

		// 创建数据库
		if (sqlite3_open(strTargetGpkgFilePath.c_str(), &pTargetGpkgDB) != SQLITE_OK)
		{
			memset(szLog, 0, 1000);
			sprintf(szLog, "无法打开数据库:%s!", sqlite3_errmsg(pTargetGpkgDB));
			file_logger->error(szLog);
			file_logger->flush();
			continue;
		}

		vector<sqlite3*> vOutputGpkgDBPtr;
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

		// 创建表格
		for (const auto& tableName : gpkgInfo.vRasterARDIdentifys)
		{
			// 创建表
			createTable(pTargetGpkgDB, tableName.c_str());
		
		
#pragma region "循环每个分析就绪数据，每个ZXY对应的二进制块"

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
			for (int z = iMinZ; z <= iMaxZ; ++z)
			{

				memset(szLog, 0, 1000);
				sprintf(szLog, "正在进行级别%d的数据包合包...", z);
				file_logger->info(szLog);
				file_logger->debug(szLog);
				file_logger->flush();


				// 计算基本分包ZXY覆盖的当前级别Z对应的X、Y范围
				SE_DRect dZxyRect = CalGridExtentByZXY(iBaseZ, iBaseX, iBaseY);


				int start_x = 0;
				int start_y = 0;
				int end_x = 0;
				int end_y = 0;

				CalXRangeAndYRangeByGeoRectAndLevel(dZxyRect, z, start_x, end_x, start_y, end_y);

				// 循环列
				for (int index_x = start_x; index_x <= end_x; ++index_x)
				{
					// 循环行
					for (int index_y = start_y; index_y <= end_y; ++index_y)
					{

						// 记录所有同一表同一位置的像素块集合
						vector<GPKG_ZXY_BLOB_INFO> vBlobInfo;
						vBlobInfo.clear();

						// 读取对应分析就绪数据表中的ZXY数据块
						for (int iBlobIndex = 0; iBlobIndex < vOutputGpkgDBPtr.size(); ++iBlobIndex)
						{
							// 优先加入的是最顶层显示的gpkg路径下的数据块
              GPKG_ZXY_BLOB_INFO blobInfo;
							if (!GetBlobFromTableByZXY(
								vOutputGpkgDBPtr[iBlobIndex],
								tableName.c_str(),
								z,
								index_x,
								index_y,
								blobInfo))
							{

								memset(szLog, 0, 1000);
								sprintf(szLog, "读取表：%s的%d-%d-%d像素块失败！", 
									tableName.c_str(),
									z,
									index_x,
									index_y);
								file_logger->error(szLog);
								file_logger->flush();

								continue;
							}

							vBlobInfo.push_back(blobInfo);
						}

					
						// 如果所有输入gpkg文件中不存在当前ZXY数据块，跳过当前循环
						if (vBlobInfo.size() == 0)
						{
							continue;
						}

						// 如果只存在1个像素块，则直接存入gpkg中
						if (vBlobInfo.size() == 1)
						{
							// 插入到表格中
							insertData(
								pTargetGpkgDB, 
								tableName.c_str(), 
								z,
								index_x,
								index_y, 
								vBlobInfo[0].vValues);

							// 提交事务
							sqlite3_exec(pTargetGpkgDB, "COMMIT;", nullptr, nullptr, nullptr);
						}
						// 如果存在多于1个同名像素块，才需要考虑是否融合
						else
						{
							// 如果合包策略为融合
							if (m_MergeStrategy == MergePackageStrategy::FUSION)
							{
								// 结果值
								vector<float> vFloatValues;
								vFloatValues.clear();

								// 融合方法包括：最大值、最小值、平均值
								// 最大值
								if (m_FusionType == FusionType::MAX)
								{
									if (!CalculateMaxStats(vBlobInfo, vFloatValues))
									{
										continue;
									}
								}

								// 最小值
								else if (m_FusionType == FusionType::MIN)
								{
									if (!CalculateMinStats(vBlobInfo, vFloatValues))
									{
										continue;
									}
								}


								// 平均值
								else if (m_FusionType == FusionType::MEAN)
								{
									if (!CalculateMeanStats(vBlobInfo, vFloatValues))
									{
										continue;
									}

								}

								// 插入到表格中
								insertData(
									pTargetGpkgDB,
									tableName.c_str(),
									z,
									index_x,
									index_y,
									vFloatValues);

								// 提交事务
								sqlite3_exec(pTargetGpkgDB, "COMMIT;", nullptr, nullptr, nullptr);

							}
							// 如果瓦片合包策略为替换，则直接存入第一个加入vector的元素即可
							else if (m_MergeStrategy == MergePackageStrategy::REPLACE)
							{
								// 插入到表格中
								insertData(
									pTargetGpkgDB,
									tableName.c_str(),
									z,
									index_x,
									index_y,
									vBlobInfo[0].vValues);

								// 提交事务
								sqlite3_exec(pTargetGpkgDB, "COMMIT;", nullptr, nullptr, nullptr);
								
							}

						}
					}
				}
			}



#pragma endregion
	
		
		}

		sqlite3_close(pTargetGpkgDB);

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
		sqlite3_close(vInputGpkgDBPtr[i]);
	}

#pragma endregion


	setProgress(100);
	return true;
}

bool SE_MergeRasterGpkgTask::isCanceled()
{
    // 返回当前取消状态
    return mCanceled;           
}

void SE_MergeRasterGpkgTask::cancel()
{
    // 设置取消状态为 true
    mCanceled = true;           
}

int SE_MergeRasterGpkgTask::progress() const
{
    // 返回当前进度
    return mProgress;           
}

void SE_MergeRasterGpkgTask::finished(bool result)
{
    emit taskFinished(result);
}

QStringList SE_MergeRasterGpkgTask::GetFileNames(const QString& path)
{
    QDir dir(path);
    QStringList nameFilters;
    nameFilters << "*.gpkg";
    QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
    return files;
}

// 判断目录是否存在
bool SE_MergeRasterGpkgTask::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}


/*获取当前目录下所有子目录*/
void SE_MergeRasterGpkgTask::GetSubDirFromFilePath(const string& strFilePath,
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
void SE_MergeRasterGpkgTask::GetTifFilesFromFilePath(
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
bool SE_MergeRasterGpkgTask::extractZXY(
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
double SE_MergeRasterGpkgTask::GetGridResByGridLevel(int iLevel)
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
SE_DRect SE_MergeRasterGpkgTask::CalGridExtentByZXY(int iLevel, int iX, int iY)
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
void SE_MergeRasterGpkgTask::CalXRangeAndYRangeByGeoRectAndLevel(
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
void SE_MergeRasterGpkgTask::GetPackLevelByZ(vector<ARD_PACKAGE_INFO>& vInfo, int iZ, int& iPackLevel)
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
void SE_MergeRasterGpkgTask::CalXAndYByPointAndLevel(
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
void SE_MergeRasterGpkgTask::GetIndexFromGpkgDB(vector<ZXY_INDEX>& vIndex, int iZ, int iX, int iY, int& iIndex)
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




// 根据ZXY获取对应的sqlite*对象索引
void SE_MergeRasterGpkgTask::GetIndexFromSQLiteDB(vector<ZXY_INDEX>& vIndex, int iZ, int iX, int iY, int& iIndex)
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


void SE_MergeRasterGpkgTask::createTable(sqlite3* db, const char* szTableName)
{
    char szSQL[500] = { 0 };
    sprintf(szSQL, "CREATE TABLE IF NOT EXISTS %s (GRID_LEVEL INTEGER, GRID_X INTEGER, GRID_Y INTEGER, GRID_DATA BLOB);", szTableName);

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, szSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        std::cerr << "SQL错误: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }

    /* 修改时间：20250120
    修改内容：增加字段索引  
    */
    memset(szSQL, 0, 500);
    sprintf(szSQL, "CREATE INDEX idx_Z_X_Y ON %s (GRID_LEVEL, GRID_X, GRID_Y);", szTableName);

    rc = sqlite3_exec(db, szSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        std::cerr << "SQL错误: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}



void SE_MergeRasterGpkgTask::insertData(
    sqlite3* db,
    const char* szTableName,
    const int z,
    const int x,
    const int y,
    const std::vector<float>& data)
{
    sqlite3_stmt* stmt;
    char szSQL[500] = { 0 };
    sprintf(szSQL, "INSERT INTO %s (GRID_LEVEL, GRID_X, GRID_Y, GRID_DATA) VALUES (?, ?, ?, ?);", szTableName);
 
    int rc = sqlite3_prepare_v2(db, szSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        //std::cerr << "无法预编译SQL语句: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_bind_int(stmt, 1, z);
    sqlite3_bind_int(stmt, 2, x);
    sqlite3_bind_int(stmt, 3, y);
    //	使用SQLITE_TRANSIENT，SQLite会在内部拷贝数据
    sqlite3_bind_blob(stmt, 4, data.data(), data.size() * sizeof(float), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {      
        //std::cerr << "执行SQL语句失败: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_finalize(stmt);
}


// 获取当前路径下所有gpkg文件路径
void SE_MergeRasterGpkgTask::GetGpkgFilesFromFilePath(
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


void SE_MergeRasterGpkgTask::GetGpkgInfo(string strGpkgFileName,
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


bool SE_MergeRasterGpkgTask::GetRasterGpkgInfos(const string& strGpkgFilePath, RasterARDGpkgInfo& info)
{
	info.strGpkgName = CPLGetBasename(strGpkgFilePath.c_str());

	sqlite3* db;
	char* errMsg = nullptr;

	// 打开数据库
	if (sqlite3_open(strGpkgFilePath.c_str(), &db) != SQLITE_OK) 
	{
		//std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
		return false;
	}

	// SQL 查询获取所有表名
	const char* sql = "SELECT name FROM sqlite_master WHERE type='table';";
	sqlite3_stmt* stmt;

	// 准备 SQL 语句
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) 
	{
		// std::cerr << "准备 SQL 语句失败: " << sqlite3_errmsg(db) << std::endl;
		sqlite3_close(db);
		return false;
	}

	// 执行查询并获取结果
	//std::cout << "数据库中的表:\n";
	while (sqlite3_step(stmt) == SQLITE_ROW) 
	{
		const char* tableName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
		info.vRasterARDIdentifys.push_back(tableName);
	}

	// 清理
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return true;
}

bool SE_MergeRasterGpkgTask::IsExistedInVector(RasterARDGpkgInfo& info, vector<RasterARDGpkgInfo>& vValue)
{
	for (int i = 0; i < vValue.size(); ++i)
	{
		if (info.strGpkgName == vValue[i].strGpkgName
			&& info.vRasterARDIdentifys == vValue[i].vRasterARDIdentifys)
		{		
			return true;
		}
	}

	return false;
}


// 根据名称获取对应的gpkg数据库对象
void SE_MergeRasterGpkgTask::GetGpkgDBPtrByGpkgName(
	vector<sqlite3*>& vGpkgDBPtr,
	vector<string>& vGpkgFilePath,
	const string& strGpkgName,
	vector<sqlite3*>& vOutputGpkgDBPtr)
{
	vOutputGpkgDBPtr.clear();

	for (int i = 0; i < vGpkgFilePath.size(); ++i)
	{
		// 如果找到了对应名称的gpkg
		if (strstr(vGpkgFilePath[i].c_str(), strGpkgName.c_str()) != nullptr)
		{
			vOutputGpkgDBPtr.push_back(vGpkgDBPtr[i]);
		}
	}
}


/*根据查询条件从分析就绪数据集中获取BLOB*/
bool SE_MergeRasterGpkgTask::GetBlobFromTableByZXY(
	sqlite3* db,
	const char* szTableName,
	int iZ,
	int iX,
	int iY,
  GPKG_ZXY_BLOB_INFO& info)
{
	info.iX = iX;
	info.iY = iY;
	info.iZ = iZ;
	info.dBlobRect = CalGridExtentByZXY(iZ, iX, iY);

	// SQL查询语句
	char szSQL[500] = { 0 };

	sprintf(szSQL, "select GRID_DATA from %s where GRID_LEVEL = %d and GRID_X = %d and GRID_Y = %d", szTableName, iZ, iX, iY);

	sqlite3_stmt* stmt;

	// 准备SQL语句
	if (sqlite3_prepare_v2(db, szSQL, -1, &stmt, nullptr) != SQLITE_OK)
	{
		return false;
	}
	const float* floatData;
	// 执行查询并处理结果
	int rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW)
	{
		// 获取BLOB列
		const void* pBlobData = sqlite3_column_blob(stmt, 0);

		// 获取BLOB大小
		int iBlobSize = sqlite3_column_bytes(stmt, 0);

		// 计算浮点数的数量
		int numFloats = iBlobSize / sizeof(float);

		// 如果为空，返回
		if (iBlobSize == 0)
		{
			return false;
		}

		if (iBlobSize > 0)
		{
			for (int i = 0; i < numFloats; i++)
			{
				info.vValues.push_back(static_cast<const float*>(pBlobData)[i]);
			}
		}

		// 清理
		sqlite3_finalize(stmt);
		return true;
	}

	// 清理
	sqlite3_finalize(stmt);
	return false;
}


bool SE_MergeRasterGpkgTask::CalculateMaxStats(vector<GPKG_ZXY_BLOB_INFO>& vBlobInfo,
	std::vector<float>& maxValues) 
{
	if (vBlobInfo.empty())
	{
		return false;
	}
		
	// blobInfo的个数
	size_t numVectors = vBlobInfo.size();

	// 每个blobInfo中浮点数的个数
	size_t vectorSize = vBlobInfo[0].vValues.size();

	// 初始化 maxValues 和 minValues
	maxValues.resize(vectorSize, std::numeric_limits<float>::lowest());
	

	// 计算最大值、最小值和平均值
	for (size_t i = 0; i < vectorSize; ++i) 
	{
		for (size_t j = 0; j < numVectors; ++j) 
		{
			if (i < vBlobInfo[j].vValues.size())
			{
				float value = vBlobInfo[j].vValues[i];
	
				// 更新最大值
				if (value > maxValues[i]) 
				{
					maxValues[i] = value;
				}
			}
		}
	}

	return true;
}

bool SE_MergeRasterGpkgTask::CalculateMinStats(vector<GPKG_ZXY_BLOB_INFO>& vBlobInfo,
	std::vector<float>& minValues)
{
	if (vBlobInfo.empty())
	{
		return false;
	}

	// blobInfo的个数
	size_t numVectors = vBlobInfo.size();

	// 每个blobInfo中浮点数的个数
	size_t vectorSize = vBlobInfo[0].vValues.size();

	minValues.resize(vectorSize, std::numeric_limits<float>::max());

	// 计算最大值、最小值和平均值
	for (size_t i = 0; i < vectorSize; ++i)
	{
		for (size_t j = 0; j < numVectors; ++j)
		{
			if (i < vBlobInfo[j].vValues.size())
			{
				float value = vBlobInfo[j].vValues[i];

				// 更新最小值
				if (value < minValues[i])
				{
					minValues[i] = value;
				}
			}
		}
	}

	return true;
}


bool SE_MergeRasterGpkgTask::CalculateMeanStats(
	vector<GPKG_ZXY_BLOB_INFO>& vBlobInfo,
	std::vector<float>& avgValues)
{
	if (vBlobInfo.empty())
	{
		return false;
	}

	// blobInfo的个数
	size_t numVectors = vBlobInfo.size();

	// 每个blobInfo中浮点数的个数
	size_t vectorSize = vBlobInfo[0].vValues.size();

	avgValues.resize(vectorSize, 0.0f);

	// 计算最大值、最小值和平均值
	for (size_t i = 0; i < vectorSize; ++i)
	{
		float sum = 0.0f;

		for (size_t j = 0; j < numVectors; ++j)
		{
			if (i < vBlobInfo[j].vValues.size())
			{
				float value = vBlobInfo[j].vValues[i];
				sum += value;
			}
		}

		// 计算平均值
		avgValues[i] = sum / numVectors;
	}

	return true;
}
