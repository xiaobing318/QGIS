#include "se_layer_code_standardization_check_thread.h"

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



SE_LayerCodeStandardizationCheckThread::SE_LayerCodeStandardizationCheckThread(QObject *parent)
    : QThread(parent)
{
    restart = false;
    abort = false;
}

SE_LayerCodeStandardizationCheckThread::~SE_LayerCodeStandardizationCheckThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();

}
void SE_LayerCodeStandardizationCheckThread::SetThreadParams(QString qstrInputDataPath, QString qstrOutputLogPath)
{
	QMutexLocker locker(&mutex);

	this->m_qstrInputDataPath = qstrInputDataPath;
	this->m_qstrOutputLogPath = qstrOutputLogPath;

	start();
}



void SE_LayerCodeStandardizationCheckThread::run()
{
  mutex.lock();
  QString qstrInputDataPath = this->m_qstrInputDataPath;
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
    return; // 文件创建失败是严重错误，直接返回
  }

  // 输入odata目录检查
  if (!FilePathIsExisted(qstrInputDataPath))
  {
    dPercent = 0;
    qstrResult = tr("输入odata数据目录不存在，请重新输入！");
    emit resultProcess(dPercent, qstrResult);
    return; // 输入路径不存在是严重错误，直接返回
  }

  // 日志文件路径检查
  if (!FileIsExisted(qstrOutputLogPath))
  {
    dPercent = 0;
    qstrResult = tr("输出日志文件路径不合法，请重新输入！");
    emit resultProcess(dPercent, qstrResult);
    return; // 日志路径不合法是严重错误，直接返回
  }

  // 获取odata目录下的文件夹个数
  QStringList qstrSubDir = GetSubDirPathOfCurrentDir(m_qstrInputDataPath);

  // 如果当前输入odata目录为单个图幅
  if (qstrSubDir.size() == 0)
  {
    QByteArray qInputPath = m_qstrInputDataPath.toLocal8Bit();
    string strInputDataPath = string(qInputPath);

    // 获取指定目录的最后一级目录
    string strSheet;
    GetFolderNameFromPath_string(strInputDataPath, strSheet);

    SE_Error err = SE_ERROR_FAILURE;

    // 数据编码规范性检查
    err = CSE_VectorDataCheck::DataCodeStandardizationCheck_Odata(
      strInputDataPath.c_str(),
      strOutputLogPath.c_str());

    if (err != SE_ERROR_NONE)
    {
      char szText[500] = { 0 };
      sprintf(szText, "数据编码规范性检查错误，错误代码：%d", err);
      qstrResult = tr(szText);
      dPercent = 0;
    }
    else
    {
      qstrResult = tr("数据编码规范性检查完成！");
      dPercent = 1.0;
    }
    emit resultProcess(dPercent, qstrResult); // 发出最终信号
  }
  // 如果当前输入odata目录为多个图幅目录
  else
  {
    int totalSheets = qstrSubDir.size(); // 总图幅数
    int processedSheets = 0; // 已处理图幅数
    bool hasError = false; // 是否有错误

    for (int iIndex = 0; iIndex < totalSheets && !hasError; iIndex++)
    {
      QByteArray qInputPath = qstrSubDir[iIndex].toLocal8Bit();
      string strInputDataPath = string(qInputPath);

      // 获取odata图幅号
      string strSheet;
      GetFolderNameFromPath_string(strInputDataPath, strSheet);

      SE_Error err = SE_ERROR_FAILURE;

      // 数据编码规范性检查
      try {
        err = CSE_VectorDataCheck::DataCodeStandardizationCheck_Odata(
          strInputDataPath.c_str(),
          strOutputLogPath.c_str());
      }
      catch (const std::exception& e) {
        qstrResult = tr("检查过程中发生异常：%1").arg(e.what());
        dPercent = 0;
        emit resultProcess(dPercent, qstrResult);
      }

      if (err != SE_ERROR_NONE)
      {
        char szText[500] = { 0 };
        sprintf(szText, "数据编码规范性检查错误，错误代码：%d", err);
        qstrResult = tr(szText);
        dPercent = 0;
        hasError = true;
        emit resultProcess(dPercent, qstrResult); // 发出错误信号
      }
      else
      {
        processedSheets++; // 更新已处理图幅数
        dPercent = static_cast<double>(processedSheets) / totalSheets; // 计算当前进度
        qstrResult = tr("正在处理第 %1 个图幅...").arg(processedSheets);
        emit resultProcess(dPercent, qstrResult); // 发出进度更新信号
      }
    }

    // 所有图幅处理完成后发出最终信号
    if (!hasError)
    {
      dPercent = 1.0;
      qstrResult = tr("所有数据编码规范性检查完成！");
    }
    else
    {
      dPercent = 0;
      qstrResult = tr("数据编码规范性检查失败！");
    }
    emit resultProcess(dPercent, qstrResult); // 发出最终信号
  }
  // ----------------------结束-------------------------//
}
// 判断目录是否存在
bool SE_LayerCodeStandardizationCheckThread::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}

// 判断文件是否存在
bool SE_LayerCodeStandardizationCheckThread::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


/*获取在指定目录下的目录的路径*/
QStringList SE_LayerCodeStandardizationCheckThread::GetSubDirPathOfCurrentDir(QString dirPath)
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
void SE_LayerCodeStandardizationCheckThread::GetFolderNameFromPath_string(string strPath, string& strFolderName)
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


