#include "se_layer_validity_check_thread.h"

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



SE_LayerValidityCheckThread::SE_LayerValidityCheckThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;
}

SE_LayerValidityCheckThread::~SE_LayerValidityCheckThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

}
void SE_LayerValidityCheckThread::SetThreadParams(QString qstrInputShpDataPath, QString qstrOutputLogPath)
{
	QMutexLocker locker(&mutex);

	this->m_qstrInputShpDataPath = qstrInputShpDataPath;
	this->m_qstrOutputLogPath = qstrOutputLogPath;

	start();
}



void SE_LayerValidityCheckThread::run()
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

	fprintf(fpLog, "============================================================图层文件可用性检查记录============================================================\n");
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

		vector<LayerValidityCheckInfo> vCheckInfos;
		vCheckInfos.clear();

		// 要素范围检查
		err = CSE_VectorDataCheck::LayerValidityCheck(
			strInputShpDataPath.c_str(),
			vCheckInfos);


		if (err != SE_ERROR_NONE)
		{
			char szText[500] = { 0 };
			sprintf(szText, "图层文件可用性检查错误，错误代码：%d", err);
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

			fprintf(fpLog, "\n	图幅号		图层名称	几何类型	图层文件可用性\n");
			fflush(fpLog);

			for (int iLayerIndex = 0; iLayerIndex < vCheckInfos.size(); ++iLayerIndex)
			{
				// 将是否一致标志改为“一致”、“不一致”
				string strLayerFlag;

				if (vCheckInfos[iLayerIndex].strValidityFlag == "TRUE")
				{
					strLayerFlag = "可用";
				}
				else
				{
					strLayerFlag = "不可用";
				}


				fprintf(fpLog, "	%s		%s		%s			%s\n",
					strShpSheet.c_str(),
					vCheckInfos[iLayerIndex].strLayerName.c_str(),
					vCheckInfos[iLayerIndex].strGeoType.c_str(),
					strLayerFlag.c_str());
				fflush(fpLog);
			}



			fprintf(fpLog, "\n----------------------------------------------------------------------------------------------------------------------------------------\n");
			fflush(fpLog);

			fprintf(fpLog, "\n========================================================================================================================================\n");
			fflush(fpLog);
			fclose(fpLog);

			dPercent = 1.0;
			qstrResult = tr("图层文件可用性检查完成！");
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

			vector<LayerValidityCheckInfo> vCheckInfos;
			vCheckInfos.clear();

			// 图层可用性检查
			err = CSE_VectorDataCheck::LayerValidityCheck(
				strInputShpDataPath.c_str(),
				vCheckInfos);


			if (err != SE_ERROR_NONE)
			{
				char szText[500] = { 0 };
				sprintf(szText, "图层文件可用性检查错误，错误代码：%d", err);
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

				fprintf(fpLog, "\n	图幅号		图层名称	几何类型	图层文件可用性\n");
				fflush(fpLog);

				for (int iLayerIndex = 0; iLayerIndex < vCheckInfos.size(); ++iLayerIndex)
				{
					// 将是否一致标志改为“一致”、“不一致”
					string strLayerFlag;

					if (vCheckInfos[iLayerIndex].strValidityFlag == "TRUE")
					{
						strLayerFlag = "可用";
					}
					else
					{
						strLayerFlag = "不可用";
					}


					fprintf(fpLog, "	%s		%s		%s			%s\n",
						strShpSheet.c_str(),
						vCheckInfos[iLayerIndex].strLayerName.c_str(),
						vCheckInfos[iLayerIndex].strGeoType.c_str(),
						strLayerFlag.c_str());
					fflush(fpLog);
				}



				fprintf(fpLog, "\n----------------------------------------------------------------------------------------------------------------------------------------\n");
				fflush(fpLog);

				fprintf(fpLog, "\n========================================================================================================================================\n");
				fflush(fpLog);
				fclose(fpLog);

				dPercent = 1.0;
				qstrResult = tr("图层文件可用性检查完成！");
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
bool SE_LayerValidityCheckThread::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool SE_LayerValidityCheckThread::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


/*获取在指定目录下的目录的路径*/
QStringList SE_LayerValidityCheckThread::GetSubDirPathOfCurrentDir(QString dirPath)
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
void SE_LayerValidityCheckThread::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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


