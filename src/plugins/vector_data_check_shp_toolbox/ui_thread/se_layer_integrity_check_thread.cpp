#include "se_layer_integrity_check_thread.h"

#include <QtWidgets>
#include <cmath>

/*----------------GDAL--------------*/
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "gdal_utils.h"
/*-----------------------------------*/


#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#endif



SE_LayerIntegrityCheckThread::SE_LayerIntegrityCheckThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;
}

SE_LayerIntegrityCheckThread::~SE_LayerIntegrityCheckThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

}
void SE_LayerIntegrityCheckThread::SetThreadParams(QString qstrInputShpDataPath, QString qstrOutputLogPath)
{
	QMutexLocker locker(&mutex);

	this->m_qstrInputShpDataPath = qstrInputShpDataPath;
	this->m_qstrOutputLogPath = qstrOutputLogPath;

	start();
}



void SE_LayerIntegrityCheckThread::run()
{
    mutex.lock();
	QString qstrInputShpDataPath = this->m_qstrInputShpDataPath ;
	QString qstrOutputLogPath = this->m_qstrOutputLogPath;
    mutex.unlock();

	double dPercent = 0;
	QString qstrResult;
	// ----------------------开始-------------------------//
	
	QByteArray qOutputLogPath = qstrOutputLogPath.toLocal8Bit();
	string strOutputLogPath = string(qOutputLogPath);
	FILE* fpLog = fopen(strOutputLogPath.c_str(), "w");
	if (!fpLog)
	{
		dPercent = 0;
		qstrResult = tr("创建日志文件失败！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	fprintf(fpLog, "============================================================图层文件完整性检查记录============================================================\n");
	fflush(fpLog);


	// 输入shp目录
	if (!FilePathIsExisted(qstrInputShpDataPath))
	{
		dPercent = 0;
		qstrResult = tr("输入shp数据目录不存在，请重新输入！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	// 日志文件路径
	if (!FileIsExisted(qstrOutputLogPath))
	{
		dPercent = 0;
		qstrResult = tr("输出日志文件路径不合法，请重新输入！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	// 获取shp目录下的文件夹个数
	QStringList qstrShpSubDir = GetSubDirPathOfCurrentDir(m_qstrInputShpDataPath);

	// 如果当前输入shp目录为单个图幅
	if (qstrShpSubDir.size() == 0)
	{
		QByteArray qInputShpPath = m_qstrInputShpDataPath.toLocal8Bit();
		string strInputShpDataPath = string(qInputShpPath);

		// 获取指定目录的最后一级目录

		// 获取shp图幅号
		string strShpSheet;
		GetFolderNameFromPath_string(strInputShpDataPath, strShpSheet);

		SE_Error err = SE_ERROR_FAILURE;

		vector<LayerIntegrityCheckInfo> vCheckInfos;
		vCheckInfos.clear();

		// 图层完整性检查
		err = CSE_VectorDataCheck::LayerIntegrityCheck(
			strInputShpDataPath.c_str(),
			vCheckInfos);


		if (err != SE_ERROR_NONE)
		{
			char szText[500] = { 0 };
			sprintf(szText, "图层文件完整性检查错误，错误代码：%d", err);
			QString qstrText = tr(szText);

			dPercent = 0;
			emit resultProcess(dPercent, qstrText);
			return;
		}
		else
		{
			fprintf(fpLog, "\n														  当前文件夹图幅数：1个\n");
			fflush(fpLog);

			fprintf(fpLog, "\n----------------------------------------------------------------------------------------------------------------------------------------\n");
			fflush(fpLog);

			fprintf(fpLog, "													第1个图幅%s检查信息\n", strShpSheet.c_str());
			fflush(fpLog);

			fprintf(fpLog, "\n	图幅号		图层名称	几何类型	图层文件完整性	   shx文件	   dbf文件	   prj文件\n");
			fflush(fpLog);

			for (int iLayerIndex = 0; iLayerIndex < vCheckInfos.size(); ++iLayerIndex)
			{
				// 将是否完整标志改为“完整”、“不完整”
				string strIntegrityFlag;

				if (vCheckInfos[iLayerIndex].strIntegrityFlag == "TRUE")
				{
					strIntegrityFlag = "完整";
				}
				else
				{
					strIntegrityFlag = "不完整";
				}

				/*shx文件*/
				string strShxFlag;

				if (vCheckInfos[iLayerIndex].strShxFileFlag == "TRUE")
				{
					strShxFlag = "完整";
				}
				else
				{
					strShxFlag = "不完整";
				}

				/*dbf文件*/
				string strDbfFlag;

				if (vCheckInfos[iLayerIndex].strDbfFileFlag == "TRUE")
				{
					strDbfFlag = "完整";
				}
				else
				{
					strDbfFlag = "不完整";
				}

				/*prj文件*/
				string strPrjFlag;

				if (vCheckInfos[iLayerIndex].strPrjFileFlag == "TRUE")
				{
					strPrjFlag = "完整";
				}
				else
				{
					strPrjFlag = "不完整";
				}

				fprintf(fpLog, "	%s		%s		%s			%s			%s	    %s	     %s\n",
					strShpSheet.c_str(),
					vCheckInfos[iLayerIndex].strLayerName.c_str(),
					vCheckInfos[iLayerIndex].strGeoType.c_str(),
					strIntegrityFlag.c_str(),
					strShxFlag.c_str(),
					strDbfFlag.c_str(),
					strPrjFlag.c_str());

				fflush(fpLog);
			}



			fprintf(fpLog, "\n----------------------------------------------------------------------------------------------------------------------------------------\n");
			fflush(fpLog);

			fprintf(fpLog, "\n========================================================================================================================================\n");
			fflush(fpLog);
			fclose(fpLog);

			dPercent = 1.0;
			qstrResult = tr("图层文件完整性检查完成！");
			emit resultProcess(dPercent, qstrResult);
		}
	}

	// 如果当前输入shp目录为多个图幅目录
	else
	{
		fprintf(fpLog, "\n														  当前文件夹图幅数：%d个\n", qstrShpSubDir.size());
		fflush(fpLog);

		for (int iIndex = 0; iIndex < qstrShpSubDir.size(); iIndex++)
		{

			QByteArray qInputShpPath = qstrShpSubDir[iIndex].toLocal8Bit();
			string strInputShpDataPath = string(qInputShpPath);

			// 获取指定目录的最后一级目录

			// 获取shp图幅号
			string strShpSheet;
			GetFolderNameFromPath_string(strInputShpDataPath, strShpSheet);


			SE_Error err = SE_ERROR_FAILURE;

			vector<LayerIntegrityCheckInfo> vCheckInfos;
			vCheckInfos.clear();

			// 图层完整性检查
			err = CSE_VectorDataCheck::LayerIntegrityCheck(
				strInputShpDataPath.c_str(),
				vCheckInfos);


			if (err != SE_ERROR_NONE)
			{
				char szText[500] = { 0 };
				sprintf(szText, "图层文件完整性检查错误，错误代码：%d", err);
				QString qstrText = tr(szText);

				dPercent = 0;
				emit resultProcess(dPercent, qstrText);
				return;
			}
			else
			{

				fprintf(fpLog, "\n----------------------------------------------------------------------------------------------------------------------------------------\n");
				fflush(fpLog);

				fprintf(fpLog, "													第%d个图幅%s检查信息\n", iIndex + 1, strShpSheet.c_str());
				fflush(fpLog);

				fprintf(fpLog, "\n	图幅号		图层名称	几何类型	图层文件完整性	   shx文件	   dbf文件	   prj文件\n");
				fflush(fpLog);

				for (int iLayerIndex = 0; iLayerIndex < vCheckInfos.size(); ++iLayerIndex)
				{
					// 将是否完整标志改为“完整”、“不完整”
					string strIntegrityFlag;

					if (vCheckInfos[iLayerIndex].strIntegrityFlag == "TRUE")
					{
						strIntegrityFlag = "完整";
					}
					else
					{
						strIntegrityFlag = "不完整";
					}

					/*shx文件*/
					string strShxFlag;

					if (vCheckInfos[iLayerIndex].strShxFileFlag == "TRUE")
					{
						strShxFlag = "完整";
					}
					else
					{
						strShxFlag = "不完整";
					}

					/*dbf文件*/
					string strDbfFlag;

					if (vCheckInfos[iLayerIndex].strDbfFileFlag == "TRUE")
					{
						strDbfFlag = "完整";
					}
					else
					{
						strDbfFlag = "不完整";
					}

					/*prj文件*/
					string strPrjFlag;

					if (vCheckInfos[iLayerIndex].strPrjFileFlag == "TRUE")
					{
						strPrjFlag = "完整";
					}
					else
					{
						strPrjFlag = "不完整";
					}

					fprintf(fpLog, "	%s		%s		%s			%s			%s	    %s	     %s\n",
						strShpSheet.c_str(),
						vCheckInfos[iLayerIndex].strLayerName.c_str(),
						vCheckInfos[iLayerIndex].strGeoType.c_str(),
						strIntegrityFlag.c_str(),
						strShxFlag.c_str(),
						strDbfFlag.c_str(),
						strPrjFlag.c_str());

					fflush(fpLog);
				}



				fprintf(fpLog, "\n----------------------------------------------------------------------------------------------------------------------------------------\n");
				fflush(fpLog);

				fprintf(fpLog, "\n========================================================================================================================================\n");
				fflush(fpLog);
				fclose(fpLog);

				dPercent = 1.0;
				qstrResult = tr("图层文件完整性检查完成！");
				emit resultProcess(dPercent, qstrResult);
			}
		}

		fprintf(fpLog, "\n========================================================================================================================================\n");
		fflush(fpLog);
		fclose(fpLog);

	}
	// ----------------------结束-------------------------//

}


// 判断目录是否存在
bool SE_LayerIntegrityCheckThread::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool SE_LayerIntegrityCheckThread::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


/*获取在指定目录下的目录的路径*/
QStringList SE_LayerIntegrityCheckThread::GetSubDirPathOfCurrentDir(QString dirPath)
{
	QStringList dirPaths;
	QDir splDir(dirPath);
	QFileInfoList fileInfoListInSplDir = splDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	QFileInfo tempFileInfo;
	foreach(tempFileInfo, fileInfoListInSplDir) {
		dirPaths << tempFileInfo.absoluteFilePath();
	}
	return dirPaths;
}

/*获取指定目录的最后一级目录*/
void SE_LayerIntegrityCheckThread::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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


