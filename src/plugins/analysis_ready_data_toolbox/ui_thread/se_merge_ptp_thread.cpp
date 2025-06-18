#define _HAS_STD_BYTE 0
#pragma warning(disable: 4005)  // MAX_PATH 重定义
#pragma warning(disable: 4267)  // size_t 到 uint32
#include "se_merge_ptp_thread.h"

#include <QtWidgets>
#include <cmath>

/*----------------GDAL--------------*/
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "gdal_utils.h"
/*-----------------------------------*/

extern "C"
{
#include <ptp/common.h>
#include <ptp/typedef.h>
#include <ptp/mtp_packet_reader.h>
#include <ptp/mtp_packet_write.h>

}


#include <io.h>
#include <stdio.h>
#include <windows.h>
#include <direct.h>



SE_MergePtpThread::SE_MergePtpThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;
}

SE_MergePtpThread::~SE_MergePtpThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

}
void SE_MergePtpThread::SetThreadParams(QStringList qstrPtpPathList,
	int iMergeType,
	vector<PTP_Package_Rule> vOutputPtpPackageRule,
	QString qstrDataTime,
	QString qstrOutputPath)
{
    QMutexLocker locker(&mutex);

    this->m_qstrPtpPathList = qstrPtpPathList;
    this->m_iMergeType = iMergeType;
    this->m_vOutputPtpPackageRule = vOutputPtpPackageRule;
	this->m_qstrDataTime = qstrDataTime;
	this->m_qstrOutputPath = qstrOutputPath;

	start();

}


void SE_MergePtpThread::run()
{
    mutex.lock();

	QStringList qstrPtpPathList = this->m_qstrPtpPathList;
	int iMergeType = this->m_iMergeType;
	vector<PTP_Package_Rule> vOutputPtpPackageRule = this->m_vOutputPtpPackageRule;
	QString qstrDataTime = this->m_qstrDataTime;
	QString qstrOutputPath = this->m_qstrOutputPath;

    mutex.unlock();

	double dPercent = 0;
	QString qstrResult;

#pragma region "算法实现部分"

	if (!FilePathIsExisted(qstrOutputPath))
	{
		/*QString qstrTitle = tr("线划图数据格式转换");
		QString qstrText = tr("输出数据目录不存在，请重新输入！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/

		dPercent = 0;
		qstrResult = tr("输出瓦片数据目录不存在，请重新输入！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}


	// 输出瓦片包路径
	string strOutputPath = qstrOutputPath.toLocal8Bit();


	// 输出合包时间文件
	string strMergePTPTime = strOutputPath + "/merge_ptp_time.txt";
	FILE* fpMergePTPTime = fopen(strMergePTPTime.c_str(), "w");
	if (!fpMergePTPTime)
	{
		/*QString qstrTitle = tr("瓦片包合包");
		QString qstrText = tr("创建合包时间文件失败！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/
		dPercent = 0;
		qstrResult = tr("创建合包时间文件失败！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	string strDataTime = qstrDataTime.toLocal8Bit();
	fprintf(fpMergePTPTime, "合包时间：%s\n", strDataTime.c_str());
	fflush(fpMergePTPTime);
	fclose(fpMergePTPTime);

	// 输出日志文件路径
	string strLogFilePath = strOutputPath + "/log.txt";
	FILE* fpLog = fopen(strLogFilePath.c_str(), "w");
	if (!fpLog)
	{
		/*QString qstrTitle = tr("瓦片包合包");
		QString qstrText = tr("创建合包日志文件失败！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/
		dPercent = 0;
		qstrResult = tr("创建合包日志文件失败！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	fprintf(fpLog, "\n========================合包开始========================\n");
	fflush(fpLog);

	// 读取key配置文件
	string strPrivateKey;
	string strDevKey;
	bool bResult = LoadPtpKeyConfigFile(strPrivateKey, strDevKey);
	if (!bResult)
	{
		/*QString qstrTitle = QString::fromLocal8Bit("瓦片包合包工具");
		QString qstrText = QString::fromLocal8Bit("请配置ptp读写key！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/
		dPercent = 0;
		qstrResult = tr("请配置ptp读写key！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}



#pragma region "（1）遍历待合包瓦片包路径列表，记录瓦片包信息"

	fprintf(fpLog, "\n------------------待合包数据------------------\n");
	fflush(fpLog);


	// 所有ptp文件全路径
	vector<string> vPtpFilePathList;
	vPtpFilePathList.clear();


	// 获取每个路径下的ptp文件列表
	for (int i = 0; i < qstrPtpPathList.size(); i++)
	{
		QStringList qstrPtpFileList = GetPtpFileNames(qstrPtpPathList[i]);

		// 如果当前路径下没有ptp文件，跳过
		if (qstrPtpFileList.size() == 0)
		{
			continue;
		}

		for (int j = 0; j < qstrPtpFileList.size(); j++)
		{
			QString qstrPtpFilePath = qstrPtpPathList[i] + "/" + qstrPtpFileList[j];
			string strPtpFilePath = qstrPtpFilePath.toLocal8Bit();
			vPtpFilePathList.push_back(strPtpFilePath);
		}
	}

#pragma endregion

#pragma region "（2）遍历每个待合包的ptp文件，按zxy顺序依次读取二进制流"

	// 目标ptp包名称
	vector<string> vTargetPtpFileNameList;
	vTargetPtpFileNameList.clear();

	// 目标ptp包全路径
	vector<string> vTargetPtpFilePathList;
	vTargetPtpFilePathList.clear();

	/*算法调整后的思路：
	（1）遍历待合包的ptp文件列表A，根据每个ptp文件名称及分包规则计算合包后的ptp文件名，并加入到ptp文件列表B
	（2）遍历列表B中的ptp文件，按照Z-X-Y循环，根据Z-X-Y及分包规则计算所属的ptp包名，然后读取数据，
	分别按照融合和替换方式进行同一位置瓦片数据的处理；
	*/

	/*按照输入ptp文件的名称，根据分包规则，计算目标ptp包名*/
	for (int iPtpIndex = 0; iPtpIndex < vPtpFilePathList.size(); ++iPtpIndex)
	{
		// 当前处理的ptp文件路径
		string strCurrentPtpFilePath = vPtpFilePathList[iPtpIndex];

		// 获取当前ptp文件的名称
		QString qstrPtpFilePath = QString::fromLocal8Bit(strCurrentPtpFilePath.c_str());
		QFileInfo fileInfo(qstrPtpFilePath);
		string strPtpFileName = fileInfo.baseName().toLocal8Bit();

		// 获取ptp文件的级别信息
		int minz = 0;
		int maxz = 0;
		int basez = 0;
		int x = 0;
		int y = 0;
		GetPtpInfo(strPtpFileName, minz, maxz, basez, x, y);

		// 循环每个级别
		for (int z = minz; z <= maxz; ++z)
		{
			int start_x = pow(2, z - basez) * x;
			int start_y = pow(2, z - basez) * y;
			int end_x = start_x + pow(2, z - basez) - 1;
			int end_y = start_y + pow(2, z - basez) - 1;

			for (int index_x = start_x; index_x <= end_x; ++index_x)
			{
				for (int index_y = start_y; index_y <= end_y; ++index_y)
				{
					// 根据zxy及分包规则计算目标ptp名称
					string strTargetPtpName = GetPtpPackageNameByZXY(index_x, index_y, z, vOutputPtpPackageRule);

					if (!IsExistedInVector(strTargetPtpName, vTargetPtpFileNameList))
					{
						vTargetPtpFileNameList.push_back(strTargetPtpName);

						string strTargetPtpFilePath = strOutputPath + "/" + strTargetPtpName + ".ptp";
						vTargetPtpFilePathList.push_back(strTargetPtpFilePath);
					}
				}
			}
		}
	}

	// 创建读取句柄
	HANDLE64_PTP* pReaderHandle = new HANDLE64_PTP[vPtpFilePathList.size()];
	if (!pReaderHandle)
	{
		dPercent = 0;
		qstrResult = tr("创建读取句柄失败！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	// 打开待读取的ptp文件
	for (int i = 0; i < vPtpFilePathList.size(); ++i)
	{
		uint32 status = 0;
		pReaderHandle[i] = mtp_open(
			vPtpFilePathList[i].c_str(),
			strPrivateKey.c_str(),
			strlen(strPrivateKey.c_str()),
			strDevKey.c_str(),
			strlen(strDevKey.c_str()),
			&status);

		// 如果打开失败，跳过当前循环
		if (pReaderHandle[i] == 0)
		{
			continue;
		}
	}

	fprintf(fpLog, "\n瓦片合包成果共生成%d个ptp文件\n", vTargetPtpFileNameList.size());
	fflush(fpLog);

	// 创建写ptp句柄
	HANDLE64_PTP* pWriterHandle = new HANDLE64_PTP[vTargetPtpFileNameList.size()];
	if (!pWriterHandle)
	{
		dPercent = 0;
		qstrResult = tr("创建写ptp句柄失败！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	// 遍历每个待合包的ptp文件，分别创建ptp文件并设置范围
	for (int i = 0; i < vTargetPtpFilePathList.size(); ++i)
	{
		fprintf(fpLog, "\n正在进行第%d个名称为%s的ptp文件合包...\n", i + 1, vTargetPtpFileNameList[i].c_str());
		fflush(fpLog);


		uint32 status = 0;
		string strMetaData = vTargetPtpFileNameList[i];

		// 创建ptp文件
		pWriterHandle[i] = mtp_create(vTargetPtpFilePathList[i].c_str(),
			256,
			"PTP",
			strMetaData.c_str(),
			strPrivateKey.c_str(),
			strlen(strPrivateKey.c_str()),
			strDevKey.c_str(),
			strlen(strDevKey.c_str()),
			FALSE,
			&status);


		if (pWriterHandle[i] == 0)
		{
			continue;
		}

		// 获取ptp文件的级别信息
		int minz = 0;
		int maxz = 0;
		int basez = 0;
		int x = 0;
		int y = 0;
		GetPtpInfo(vTargetPtpFileNameList[i], minz, maxz, basez, x, y);

		// 添加瓦片区域
		for (int index_z = minz; index_z <= maxz; ++index_z)
		{
			int start_x = pow(2, index_z - basez) * x;
			int start_y = pow(2, index_z - basez) * y;
			int end_x = start_x + pow(2, index_z - basez) - 1;
			int end_y = start_y + pow(2, index_z - basez) - 1;

			// 加一个瓦片区域
			add_tile_ranges(pWriterHandle[i], index_z, start_x, end_x, start_y, end_y, &status);
			if (status != 0)
			{
				continue;
			}
		}

		// 循环每个级别
		for (int z = minz; z <= maxz; ++z)
		{
			fprintf(fpLog, "\n正在进行级别%d的瓦片合包...\n", z);
			fflush(fpLog);
			int start_x = pow(2, z - basez) * x;
			int start_y = pow(2, z - basez) * y;
			int end_x = start_x + pow(2, z - basez) - 1;
			int end_y = start_y + pow(2, z - basez) - 1;

			for (int index_x = start_x; index_x <= end_x; ++index_x)
			{
				for (int index_y = start_y; index_y <= end_y; ++index_y)
				{
					// ZXY瓦片二进制流
					vector<byte*> vOriData;
					vOriData.clear();

					// ZXY瓦片二进制流长度
					vector<uint32> vOriDataLength;
					vOriDataLength.clear();

					for (int iReaderIndex = 0; iReaderIndex < vPtpFilePathList.size(); ++iReaderIndex)
					{
						// 原始zxy瓦片字节流
						byte* ori_data;

						// 原始zxy瓦片字节流长度
						uint32 ori_data_len;

						// 从输入ptp读取句柄中依次读取zxy瓦片，如果不存在，则跳过
						ori_data = mtp_get_tile(pReaderHandle[iReaderIndex], z, index_x, index_y, &ori_data_len, TRUE, &status);

						// 优先加入的是最顶层显示的ptp
						if (ori_data)
						{
							vOriData.push_back(ori_data);
							vOriDataLength.push_back(ori_data_len);
						}
					}

					// 如果所有输入ptp文件中不存在当前zxy瓦片，跳过当前循环
					if (vOriData.size() == 0)
					{
						continue;
					}

					// 如果只存在1个瓦片，则直接存入ptp中
					if (vOriData.size() == 1)
					{
						add_tile_with_data(pWriterHandle[i],
							(char*)vOriData[0],
							vOriDataLength[0],
							z,
							index_x,
							index_y,
							FALSE,
							&status);

						if (status != 0)
						{
							continue;
						}
					}
					// 如果存在多于1个同名瓦片，才需要考虑是否融合
					else
					{
						// 如果瓦片合包策略为融合
						if (iMergeType == 1)
						{
							// 已有瓦片
							QImage existImage;
							existImage.loadFromData(vOriData[0], vOriDataLength[0]);

							for (int iDataIndex = 1; iDataIndex < vOriData.size(); ++iDataIndex)
							{
								// 当前瓦片
								QImage currentImage;
								currentImage.loadFromData(vOriData[iDataIndex], vOriDataLength[iDataIndex]);

								QImage compositeImage = CreateImageWithOverlay(currentImage, existImage);
							
								QByteArray ba;
								QBuffer buffer(&ba);
								buffer.open(QIODevice::WriteOnly);
								bool bRet = compositeImage.save(&buffer, "png", 0);
								if (!bRet)
								{
									continue;
								}


								add_tile_with_data(pWriterHandle[i],
									ba.data(),
									ba.length(),
									z,
									index_x,
									index_y,
									FALSE,
									&status);

								if (status != 0)
								{
									continue;
								}

							}

						}
						// 如果瓦片合包策略为替换，则直接存入第一个加入vector的元素即可
						else if(iMergeType == 2)
						{
							add_tile_with_data(pWriterHandle[i],
								(char*)vOriData[0],
								vOriDataLength[0],
								z,
								index_x,
								index_y,
								FALSE,
								&status);

							if (status != 0)
							{
								internal_free(vOriData[0]);
								continue;
							}
						}

					}

					// 释放瓦片资源
					for (int iTileIndex = 0; iTileIndex < vOriData.size(); ++iTileIndex)
					{
						internal_free(vOriData[iTileIndex]);
					}
				}
			}

		}

		qstrResult = tr("瓦片包合包完成！");
		dPercent = (i + 1) * 1.0 / vTargetPtpFilePathList.size();
		emit resultProcess(dPercent, qstrResult);
	}


	// 关闭读取句柄
	for (int i = 0; i < vPtpFilePathList.size(); ++i)
	{
		mtp_close(pReaderHandle[i]);
	}

	// 关闭写句柄
	for (int i = 0; i < vTargetPtpFileNameList.size(); ++i)
	{
		uint32 status = 0;
		mtp_write_close(pWriterHandle[i], &status);
		if (status != 0)
		{
			continue;
		}
	}

#pragma endregion

	fprintf(fpLog, "\n========================合包完成！========================\n");
	fflush(fpLog);
	fclose(fpLog);


#pragma endregion

}


// 判断目录是否存在
bool SE_MergePtpThread::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool SE_MergePtpThread::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}






// 获取指定目录的最后一级目录
void SE_MergePtpThread::GetFolderNameFromPath(QString qstrPath, QString& qstrFolderName)
{
	int iLength = qstrPath.length();
	int i = qstrPath.lastIndexOf("/");
	qstrFolderName = qstrPath.right(iLength - i - 1);
}

/*获取指定目录的最后一级目录*/
void SE_MergePtpThread::GetFolderNameFromPath_string(string strPath, string& strFolderName)
{
	// 获取图幅文件夹名称
	int iIndex = strPath.find_last_of("/");
	if (iIndex != string::npos)
	{
		strFolderName = strPath.substr(iIndex + 1, strPath.length() - 1);
	}
	else
	{
		strFolderName = "";
	}
}


bool SE_MergePtpThread::LoadPtpKeyConfigFile(string& strPrivateKey, string& strDevKey)
{
	// 程序运行目录
	QString qstrCurExePath = QCoreApplication::applicationDirPath();
	QString qstrPtpKeyCfgFilePath = qstrCurExePath + "/config/ptp/key.cfg";
	QByteArray qKeyPath = qstrPtpKeyCfgFilePath.toLocal8Bit();
	string strKeyPath = string(qKeyPath);

	FILE* fp = fopen(strKeyPath.c_str(), "r");
	if (!fp)
	{
		return false;
	}
	char szPrivateKeyTag[100] = { 0 };
	char szPrivateKey[1000] = { 0 };
	char szDevKeyTag[100] = { 0 };
	char szDevKey[1000] = { 0 };

	// 读取key
	fscanf(fp, "%s", szPrivateKeyTag);
	fscanf(fp, "%s", szPrivateKey);
	fscanf(fp, "%s", szDevKeyTag);
	fscanf(fp, "%s", szDevKey);

	strPrivateKey = szPrivateKey;
	strDevKey = szDevKey;

	fclose(fp);

	return true;
}


QStringList SE_MergePtpThread::GetPtpFileNames(const QString& path)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << "*.ptp" << "*.PTP";
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
	return files;
}

void SE_MergePtpThread::GetPtpInfo(string strPtpFileName,
	int& minz,
	int& maxz,
	int& basez,
	int& x,
	int& y)
{
	QString qstrPtpFileName = tr(strPtpFileName.c_str());

	QStringList qstrInfoList = qstrPtpFileName.split("-");

	minz = qstrInfoList[0].toInt();
	maxz = qstrInfoList[1].toInt();
	basez = qstrInfoList[2].toInt();
	x = qstrInfoList[3].toInt();
	y = qstrInfoList[4].toInt();

}


bool SE_MergePtpThread::isDirExist(QString fullPath)
{
	QDir dir(fullPath);
	if (dir.exists())
	{
		return true;
	}
	else
	{
		bool ok = dir.mkpath(fullPath);//创建多级目录
		return ok;
	}
}


bool SE_MergePtpThread::CheckFileOrDirExist(const QString qstrFileDirOrPath)
{
	bool bRet = false;
	QFileInfo objFileInfo(qstrFileDirOrPath);
	if (objFileInfo.isFile())
	{
		bRet = objFileInfo.exists();
	}
	else if (objFileInfo.isDir())
	{
		bRet = objFileInfo.exists();
	}
	else
	{
		bRet = false;
	}

	return bRet;
}

bool SE_MergePtpThread::ClearEmptyFolder(const QString& qstrDirPath)
{
	bool bRet = true;

	do
	{
		if (!CheckFileOrDirExist(qstrDirPath))
		{
			bRet = true;
			break;
		}

		QDir qdrPath(qstrDirPath);
		qdrPath.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);      //set filter
		QFileInfoList lstFileInfo = qdrPath.entryInfoList();             //get all file info
		foreach(QFileInfo objFileInfo, lstFileInfo)
		{
			if (objFileInfo.isDir())
			{
				QString qstrSubFilePath = objFileInfo.absoluteFilePath();
				ClearEmptyFolder(qstrSubFilePath);

				QDir qdrSubPath(qstrSubFilePath);
				qdrSubPath.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
				QFileInfoList qlstFileInfo = qdrSubPath.entryInfoList();
				if (qlstFileInfo.count() <= 0)
				{
					qdrSubPath.rmdir(qstrSubFilePath);
				}
			}
		}

	} while (0);

	return bRet;
}

string SE_MergePtpThread::GetPtpPackageNameByZXY(int x, int y, int z, vector<PTP_Package_Rule>& vPTP_Rule)
{
	string strPackageName;

	for (int i = 0; i < vPTP_Rule.size(); ++i)
	{
		char szPackageName[50] = { 0 };
		if (z >= vPTP_Rule[i].min_z
			&& z <= vPTP_Rule[i].max_z)
		{
			int iBaseX = floor(x / pow(2, z - vPTP_Rule[i].base_z));
			int iBaseY = floor(y / pow(2, z - vPTP_Rule[i].base_z));
			sprintf(szPackageName, "%d-%d-%d-%d-%d",
				vPTP_Rule[i].min_z,
				vPTP_Rule[i].max_z,
				vPTP_Rule[i].base_z,
				iBaseX,
				iBaseY);

			strPackageName = szPackageName;
			break;
		}
	}

	return strPackageName;
}


bool SE_MergePtpThread::IsExistedInVector(string strValue, vector<string>& vValue)
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

// 图片融合
QImage SE_MergePtpThread::CreateImageWithOverlay(const QImage& baseImage, const QImage& overlayImage)
{
	QImage imageWithOverlay = QImage(baseImage.size(), QImage::Format_ARGB32_Premultiplied);
	QPainter painter(&imageWithOverlay);

	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.fillRect(imageWithOverlay.rect(), Qt::transparent);

	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.drawImage(0, 0, baseImage);

	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.drawImage(0, 0, overlayImage);

	painter.end();

	return imageWithOverlay;
}