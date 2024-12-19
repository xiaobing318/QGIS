#include "se_feature_category_check_thread.h"

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
#include "se_feature_category_check_thread.h"
#endif



SE_FeatureCategoryThread::SE_FeatureCategoryThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;
}

SE_FeatureCategoryThread::~SE_FeatureCategoryThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

}
void SE_FeatureCategoryThread::SetThreadParams(QString qstrInputOdataDataPath,
	QString qstrInputShpDataPath,
	QString qstrOutputLogPath)
{
	QMutexLocker locker(&mutex);

	this->m_qstrInputShpDataPath = qstrInputShpDataPath;
	this->m_qstrInputOdataDataPath = qstrInputOdataDataPath;
	this->m_qstrOutputLogPath = qstrOutputLogPath;

	start();
}


void SE_FeatureCategoryThread::run()
{
    mutex.lock();
	QString qstrInputShpDataPath = this->m_qstrInputShpDataPath ;
	QString qstrInputOdataDataPath = this->m_qstrInputOdataDataPath;
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
		/*QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = tr("创建日志文件失败！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/
		dPercent = 0;
		qstrResult = tr("创建日志文件失败！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	fprintf(fpLog, "=============================要素分类统计记录=============================\n");
	fflush(fpLog);


	if (!FilePathIsExisted(qstrInputOdataDataPath))
	{
		/*QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = tr("输入odata数据目录不存在，请重新输入！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/
		dPercent = 0;
		qstrResult = tr("输入odata数据目录不存在，请重新输入！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	if (!FilePathIsExisted(qstrInputShpDataPath))
	{
		/*QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = tr("输入shp数据目录不存在，请重新输入！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/
		dPercent = 0;
		qstrResult = tr("输入shp数据目录不存在，请重新输入！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	// 日志文件路径
	if (!FileIsExisted(qstrOutputLogPath))
	{
		/*QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = tr("输出日志文件路径不合法，请重新输入！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/
		dPercent = 0;
		qstrResult = tr("输出日志文件路径不合法，请重新输入！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}

	// 获取odata目录下的文件夹个数
	QStringList qstrOdataSubDir = GetSubDirPathOfCurrentDir(m_qstrInputOdataDataPath);

	// 获取shp目录下的文件夹个数
	QStringList qstrShpSubDir = GetSubDirPathOfCurrentDir(m_qstrInputShpDataPath);

	// 如果输入odata目录的子文件夹数和输入shp目录的子文件夹数不同
	if (qstrOdataSubDir.size() != qstrShpSubDir.size())
	{
		/*QString qstrTitle = tr("线划图要素分类统计");
		QString qstrText = tr("odata目录下文件夹数目和shp目录下文件夹数目不一致，请重新选择输入数据！");
		QMessageBox msgBox;
		msgBox.setWindowTitle(qstrTitle);
		msgBox.setText(qstrText);
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();*/
		dPercent = 0;
		qstrResult = tr("odata目录下文件夹数目和shp目录下文件夹数目不一致，请重新选择输入数据！");
		emit resultProcess(dPercent, qstrResult);
		return;
	}


	// 如果当前输入odata目录为单个图幅
	if (qstrOdataSubDir.size() == 0
		&& qstrShpSubDir.size() == 0)
	{
		QByteArray qInputOdataPath = m_qstrInputOdataDataPath.toLocal8Bit();
		string strInputOdataDataPath = string(qInputOdataPath);

		QByteArray qInputShpPath = m_qstrInputShpDataPath.toLocal8Bit();
		string strInputShpDataPath = string(qInputShpPath);

		// 获取指定目录的最后一级目录
		// 获取odata图幅号
		string strOdataSheet;
		GetFolderNameFromPath_string(strInputOdataDataPath, strOdataSheet);

		// 获取shp图幅号
		string strShpSheet;
		GetFolderNameFromPath_string(strInputShpDataPath, strShpSheet);

		if (strOdataSheet != strShpSheet)
		{
			/*QString qstrTitle = tr("线划图要素分类统计");
			QString qstrText = tr("当前选择的odata数据图幅号与shp数据图幅号不相同，请重新选择数据！");
			QMessageBox msgBox;
			msgBox.setWindowTitle(qstrTitle);
			msgBox.setText(qstrText);
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();*/
			dPercent = 0;
			qstrResult = tr("当前选择的odata数据图幅号与shp数据图幅号不相同，请重新选择数据！");
			emit resultProcess(dPercent, qstrResult);
			return;
		}

		SE_Error err = SE_ERROR_FAILURE;

		vector<VectorLayerInfo> vLayerInfo;
		vLayerInfo.clear();

		err = CSE_VectorDataCheck::FeatureCategoryStatistics(strInputOdataDataPath.c_str(),
			strInputShpDataPath.c_str(),
			vLayerInfo);


		if (err != SE_ERROR_NONE)
		{
			//QString qstrTitle = tr("线划图要素分类统计");
			char szText[500] = { 0 };
			sprintf(szText, "线划图要素分类统计错误，错误代码：%d", err);
			QString qstrText = tr(szText);
			/*QMessageBox msgBox;
			msgBox.setWindowTitle(qstrTitle);
			msgBox.setText(qstrText);
			msgBox.setStandardButtons(QMessageBox::Yes);
			msgBox.setDefaultButton(QMessageBox::Yes);
			msgBox.exec();*/
			dPercent = 0;
			emit resultProcess(dPercent, qstrText);
			return;
		}
		else
		{
			fprintf(fpLog, "\n					当前文件夹图幅数：1个\n");
			fflush(fpLog);

			fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
			fflush(fpLog);

			fprintf(fpLog, "					第1个图幅%s统计信息\n", strOdataSheet.c_str());
			fflush(fpLog);

			fprintf(fpLog, "\n	图幅号		图层名称	几何类型	要素个数	与元数据对比情况\n");
			fflush(fpLog);

			for (int i = 0; i < vLayerInfo.size(); i++)
			{
				// 如果shp数据点要素个数与元数据一致
				if (vLayerInfo[i].iPointCount == vLayerInfo[i].iPointCount_SMS)
				{
					fprintf(fpLog, "	%s		%s		point			%d			一致\n",
						strOdataSheet.c_str(),
						vLayerInfo[i].strLayerName.c_str(),
						vLayerInfo[i].iPointCount);
					fflush(fpLog);
				}
				else
				{
					fprintf(fpLog, "	%s		%s		point			%d			不一致:元数据中为%d个\n",
						strOdataSheet.c_str(),
						vLayerInfo[i].strLayerName.c_str(),
						vLayerInfo[i].iPointCount,
						vLayerInfo[i].iPointCount_SMS);
					fflush(fpLog);
				}

				// 如果shp数据线要素个数与元数据一致
				if (vLayerInfo[i].iLineCount == vLayerInfo[i].iLineCount_SMS)
				{
					fprintf(fpLog, "	%s		%s		line			%d			一致\n",
						strOdataSheet.c_str(),
						vLayerInfo[i].strLayerName.c_str(),
						vLayerInfo[i].iLineCount);
				}
				else
				{
					fprintf(fpLog, "	%s		%s		line			%d			不一致:元数据中为%d个\n",
						strOdataSheet.c_str(),
						vLayerInfo[i].strLayerName.c_str(),
						vLayerInfo[i].iLineCount,
						vLayerInfo[i].iLineCount_SMS);
					fflush(fpLog);
				}

				// 如果shp数据面要素个数与元数据一致
				if (vLayerInfo[i].iPolygonCount == vLayerInfo[i].iPolygonCount_SMS)
				{
					fprintf(fpLog, "	%s		%s		polygon			%d			一致\n",
						strOdataSheet.c_str(),
						vLayerInfo[i].strLayerName.c_str(),
						vLayerInfo[i].iPolygonCount);
				}
				else
				{
					fprintf(fpLog, "	%s		%s		polygon			%d			不一致:元数据中为%d个\n",
						strOdataSheet.c_str(),
						vLayerInfo[i].strLayerName.c_str(),
						vLayerInfo[i].iPolygonCount,
						vLayerInfo[i].iPolygonCount_SMS);
					fflush(fpLog);
				}
			}

			fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
			fflush(fpLog);

			fprintf(fpLog, "\n==========================================================================\n");
			fflush(fpLog);
			fclose(fpLog);

			dPercent = 1.0;
			qstrResult = tr("线划图要素分类统计完成！");
			emit resultProcess(dPercent, qstrResult);
		}

	}

	// 如果当前输入odata目录为多个图幅目录
	else
	{
		fprintf(fpLog, "\n					当前文件夹图幅数：%d个\n", qstrOdataSubDir.size());
		fflush(fpLog);

		for (int iIndex = 0; iIndex < qstrOdataSubDir.size(); iIndex++)
		{
			QByteArray qInputOdataPath = qstrOdataSubDir[iIndex].toLocal8Bit();
			string strInputOdataDataPath = string(qInputOdataPath);

			QByteArray qInputShpPath = qstrShpSubDir[iIndex].toLocal8Bit();
			string strInputShpDataPath = string(qInputShpPath);

			// 获取指定目录的最后一级目录
			// 获取odata图幅号
			string strOdataSheet;
			GetFolderNameFromPath_string(strInputOdataDataPath, strOdataSheet);

			// 获取shp图幅号
			string strShpSheet;
			GetFolderNameFromPath_string(strInputShpDataPath, strShpSheet);

			if (strOdataSheet != strShpSheet)
			{
				/*QString qstrTitle = tr("线划图要素分类统计");
				QString qstrText = tr("当前选择的odata数据图幅号与shp数据图幅号不相同，请重新选择数据！");
				QMessageBox msgBox;
				msgBox.setWindowTitle(qstrTitle);
				msgBox.setText(qstrText);
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();*/
				dPercent = 0;
				qstrResult = tr("当前选择的odata数据图幅号与shp数据图幅号不相同，请重新选择数据！");
				emit resultProcess(dPercent, qstrResult);
				return;
			}

			SE_Error err = SE_ERROR_FAILURE;

			vector<VectorLayerInfo> vLayerInfo;
			vLayerInfo.clear();

			err = CSE_VectorDataCheck::FeatureCategoryStatistics(strInputOdataDataPath.c_str(),
				strInputShpDataPath.c_str(),
				vLayerInfo);


			if (err != SE_ERROR_NONE)
			{
				//QString qstrTitle = tr("线划图要素分类统计");
				char szText[500] = { 0 };
				sprintf(szText, "线划图要素分类统计错误，错误代码：%d", err);
				QString qstrText = tr(szText);
				/*QMessageBox msgBox;
				msgBox.setWindowTitle(qstrTitle);
				msgBox.setText(qstrText);
				msgBox.setStandardButtons(QMessageBox::Yes);
				msgBox.setDefaultButton(QMessageBox::Yes);
				msgBox.exec();*/
				
				dPercent = 0;
				emit resultProcess(dPercent, qstrText);
				return;
			}
			else
			{
				fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
				fflush(fpLog);

				fprintf(fpLog, "					第%d个图幅%s统计信息\n", iIndex + 1, strOdataSheet.c_str());
				fflush(fpLog);

				fprintf(fpLog, "\n	图幅号		图层名称	几何类型	要素个数	与元数据对比情况\n");
				fflush(fpLog);

				for (int i = 0; i < vLayerInfo.size(); i++)
				{
					// 如果shp数据点要素个数与元数据一致
					if (vLayerInfo[i].iPointCount == vLayerInfo[i].iPointCount_SMS)
					{
						fprintf(fpLog, "	%s		%s		point			%d			一致\n",
							strOdataSheet.c_str(),
							vLayerInfo[i].strLayerName.c_str(),
							vLayerInfo[i].iPointCount);
						fflush(fpLog);
					}
					else
					{
						fprintf(fpLog, "	%s		%s		point			%d			不一致:元数据中为%d个\n",
							strOdataSheet.c_str(),
							vLayerInfo[i].strLayerName.c_str(),
							vLayerInfo[i].iPointCount,
							vLayerInfo[i].iPointCount_SMS);
						fflush(fpLog);
					}

					// 如果shp数据线要素个数与元数据一致
					if (vLayerInfo[i].iLineCount == vLayerInfo[i].iLineCount_SMS)
					{
						fprintf(fpLog, "	%s		%s		line			%d			一致\n",
							strOdataSheet.c_str(),
							vLayerInfo[i].strLayerName.c_str(),
							vLayerInfo[i].iLineCount);
					}
					else
					{
						fprintf(fpLog, "	%s		%s		line			%d			不一致:元数据中为%d个\n",
							strOdataSheet.c_str(),
							vLayerInfo[i].strLayerName.c_str(),
							vLayerInfo[i].iLineCount,
							vLayerInfo[i].iLineCount_SMS);
						fflush(fpLog);
					}

					// 如果shp数据面要素个数与元数据一致
					if (vLayerInfo[i].iPolygonCount == vLayerInfo[i].iPolygonCount_SMS)
					{
						fprintf(fpLog, "	%s		%s		polygon			%d			一致\n",
							strOdataSheet.c_str(),
							vLayerInfo[i].strLayerName.c_str(),
							vLayerInfo[i].iPolygonCount);
					}
					else
					{
						fprintf(fpLog, "	%s		%s		polygon			%d			不一致:元数据中为%d个\n",
							strOdataSheet.c_str(),
							vLayerInfo[i].strLayerName.c_str(),
							vLayerInfo[i].iPolygonCount,
							vLayerInfo[i].iPolygonCount_SMS);
						fflush(fpLog);
					}
				}

				fprintf(fpLog, "\n--------------------------------------------------------------------------\n");
				fflush(fpLog);

				dPercent = (iIndex + 1) * 1.0 / qstrOdataSubDir.size();
				qstrResult = tr("线划图要素范围检查完成！");
				emit resultProcess(dPercent, qstrResult);

			}
		}

		fprintf(fpLog, "\n==========================================================================\n");
		fflush(fpLog);
		fclose(fpLog);

	}

	
	// ----------------------结束-------------------------//

}


// 判断目录是否存在
bool SE_FeatureCategoryThread::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool SE_FeatureCategoryThread::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


/*获取在指定目录下的目录的路径*/
QStringList SE_FeatureCategoryThread::GetSubDirPathOfCurrentDir(QString dirPath)
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
void SE_FeatureCategoryThread::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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


