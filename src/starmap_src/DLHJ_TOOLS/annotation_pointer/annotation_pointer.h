#include <vector>
#include <string>

#include <QString>
#include <QStringList>
#include <QDir>

#include "cse_vector_format_conversion.h"
#include "log_message.h"

class annotation_pointer
{
public:
  //  构造函数
  annotation_pointer(
  const std::string& input_data_path,
  const std::string& output_data_path);

  ~annotation_pointer();

public:
  void perform_annotation_pointer();

private:
  QStringList GetSubDirPathOfCurrentDir(const QString& dirPath);

private:
  std::string m_input_data_path;
  std::string m_output_data_path;
};

