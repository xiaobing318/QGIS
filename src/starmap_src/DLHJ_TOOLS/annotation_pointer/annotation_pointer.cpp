#include "annotation_pointer.h"

annotation_pointer::annotation_pointer(
  const std::string& input_data_path,
  const std::string& output_data_path)
{
  this->m_input_data_path = input_data_path;
  this->m_output_data_path = output_data_path;
}

annotation_pointer::~annotation_pointer()
{

}

void annotation_pointer::perform_annotation_pointer()
{
  QString qstrInputDataPath = QString::fromStdString(this->m_input_data_path);
  QString qstrOutputDataPath = QString::fromStdString(this->m_output_data_path);

  // ----------------------开始-------------------------//

  // 获取odata目录下的文件夹个数
  QStringList qstrSubDir = GetSubDirPathOfCurrentDir(qstrInputDataPath);

  // 如果当前输入目录为单个图幅
  if (qstrSubDir.size() == 0)
  {
    SE_Error err = SE_ERROR_FAILURE;

    // 图层完整性检查
    err = CSE_VectorFormatConversion::AnnotationPointerReverseExtraction(
      this->m_input_data_path.c_str(),
      this->m_output_data_path.c_str());

    if (err != SE_ERROR_NONE)
    {
      log_message::show_log_message(4);
      return;
    }
  }

  // 如果当前输入odata目录为多个图幅目录
  else
  {
    for (int iIndex = 0; iIndex < qstrSubDir.size(); iIndex++)
    {

      QString qstr_single_frame_input_data_path = qstrSubDir[iIndex];

      SE_Error err = SE_ERROR_FAILURE;

      // 注记指针反向挂接
      err = CSE_VectorFormatConversion::AnnotationPointerReverseExtraction(
        qstr_single_frame_input_data_path.toStdString().c_str(),
        this->m_output_data_path.c_str());

      if (err != SE_ERROR_NONE)
      {
        log_message::show_log_message(4);
        continue;
      }
    }
  }

}

/*获取在指定目录下的目录的路径*/
QStringList annotation_pointer::GetSubDirPathOfCurrentDir(const QString& dirPath)
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
