#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#endif

namespace fs = std::filesystem;

// 获取程序所在目录并添加到PATH
void setupProgramPath(const char* argv0) {
  fs::path programPath;

#ifdef _WIN32
  // Windows: 获取程序完整路径
  char buffer[MAX_PATH];
  GetModuleFileNameA(NULL, buffer, MAX_PATH);
  programPath = fs::path(buffer).parent_path();
#else
  // Linux: 通过/proc/self/exe获取程序路径
  char buffer[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (len != -1) {
    buffer[len] = '\0';
    programPath = fs::path(buffer).parent_path();
  }
  else {
    // 备用方案：使用argv[0]
    programPath = fs::absolute(fs::path(argv0)).parent_path();
  }
#endif

  // 获取当前PATH环境变量
  std::string currentPath;
#ifdef _WIN32
  char* pathEnv = std::getenv("PATH");
  if (pathEnv) {
    currentPath = pathEnv;
  }

  // 将程序目录添加到PATH（如果还未添加）
  std::string progDir = programPath.string();
  if (currentPath.find(progDir) == std::string::npos) {
    std::string newPath = progDir + ";" + currentPath;
    _putenv_s("PATH", newPath.c_str());
  }
#else
  char* pathEnv = std::getenv("PATH");
  if (pathEnv) {
    currentPath = pathEnv;
  }

  // 将程序目录添加到PATH（如果还未添加）
  std::string progDir = programPath.string();
  if (currentPath.find(progDir) == std::string::npos) {
    std::string newPath = progDir + ":" + currentPath;
    setenv("PATH", newPath.c_str(), 1);
  }
#endif
}

class ShapefileConverter {
private:
  std::string sourceEncoding;
  std::string targetEncoding;
  bool verbose;

  // 执行系统命令
  int executeCommand(const std::string& command) {
    if (verbose) {
      std::cout << "执行命令: " << command << std::endl;
    }

#ifdef _WIN32
    // Windows平台
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // 创建进程
    if (!CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
      std::cerr << "创建进程失败: " << GetLastError() << std::endl;
      return -1;
    }

    // 等待进程结束
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    // 关闭句柄
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode;
#else
    // Linux/Unix平台
    int result = system(command.c_str());
    return WEXITSTATUS(result);
#endif
  }

  // 检查是否为shapefile
  bool isShapefile(const fs::path& filePath) {
    return filePath.extension() == ".shp" || filePath.extension() == ".SHP";
  }

  // 生成输出路径
  fs::path generateOutputPath(const fs::path& inputPath, const fs::path& outputBase) {
    if (outputBase.empty()) {
      // 如果没有指定输出路径，在原地创建_utf8后缀的文件
      fs::path outputPath = inputPath.parent_path() / (inputPath.stem().string() + "_utf8.shp");
      return outputPath;
    }
    else {
      // 保持相对路径结构
      fs::path relativePath = fs::relative(inputPath, inputPath.parent_path());
      return outputBase / relativePath;
    }
  }

  // 转换单个shapefile
  bool convertSingleShapefile(const fs::path& inputPath, const fs::path& outputPath) {
    std::cout << "转换: " << inputPath << " -> " << outputPath << std::endl;

    // 确保输出目录存在
    fs::create_directories(outputPath.parent_path());

    // 构建ogr2ogr命令
    std::stringstream cmd;
    cmd << "ogr2ogr";
    cmd << " -f \"ESRI Shapefile\"";
    cmd << " -oo \"ENCODING=" << sourceEncoding << "\"";
    cmd << " -lco \"ENCODING=" << targetEncoding << "\"";
    cmd << " \"" << outputPath.string() << "\"";
    cmd << " \"" << inputPath.string() << "\"";

    int result = executeCommand(cmd.str());

    if (result == 0) {
      std::cout << "成功转换: " << inputPath.filename() << std::endl;

      // 复制相关文件（.prj, .shx等）
      std::vector<std::string> extensions = { ".prj", ".shx", ".sbn", ".sbx", ".cpg" };
      for (const auto& ext : extensions) {
        fs::path srcFile = inputPath.parent_path() / (inputPath.stem().string() + ext);
        fs::path dstFile = outputPath.parent_path() / (outputPath.stem().string() + ext);

        if (fs::exists(srcFile)) {
          try {
            fs::copy_file(srcFile, dstFile, fs::copy_options::overwrite_existing);
          }
          catch (const std::exception& e) {
            std::cerr << "复制文件失败 " << srcFile << ": " << e.what() << std::endl;
          }
        }

        // 也检查大写扩展名
        std::string upperExt = ext;
        std::transform(upperExt.begin(), upperExt.end(), upperExt.begin(), ::toupper);
        srcFile = inputPath.parent_path() / (inputPath.stem().string() + upperExt);

        if (fs::exists(srcFile) && srcFile != inputPath.parent_path() / (inputPath.stem().string() + ext)) {
          try {
            fs::copy_file(srcFile, dstFile, fs::copy_options::overwrite_existing);
          }
          catch (const std::exception& e) {
            std::cerr << "复制文件失败 " << srcFile << ": " << e.what() << std::endl;
          }
        }
      }

      // 创建新的.cpg文件指定UTF-8编码
      fs::path cpgFile = outputPath.parent_path() / (outputPath.stem().string() + ".cpg");
      std::ofstream cpg(cpgFile);
      if (cpg.is_open()) {
        cpg << "UTF-8" << std::endl;
        cpg.close();
      }

      return true;
    }
    else {
      std::cerr << "转换失败: " << inputPath.filename() << std::endl;
      return false;
    }
  }

public:
  ShapefileConverter(const std::string& srcEnc = "GBK",
    const std::string& tgtEnc = "UTF-8",
    bool verb = false)
    : sourceEncoding(srcEnc), targetEncoding(tgtEnc), verbose(verb) {}

  // 转换单个文件或目录（非递归）
  int convertPath(const fs::path& inputPath, const fs::path& outputPath = "") {
    if (!fs::exists(inputPath)) {
      std::cerr << "错误: 输入路径不存在: " << inputPath << std::endl;
      return 1;
    }

    int successCount = 0;
    int failCount = 0;

    if (fs::is_regular_file(inputPath)) {
      // 单个文件
      if (isShapefile(inputPath)) {
        fs::path outPath = outputPath.empty() ?
          generateOutputPath(inputPath, "") : outputPath;

        if (convertSingleShapefile(inputPath, outPath)) {
          successCount++;
        }
        else {
          failCount++;
        }
      }
      else {
        std::cerr << "错误: 输入文件不是shapefile: " << inputPath << std::endl;
        return 1;
      }
    }
    else if (fs::is_directory(inputPath)) {
      // 目录（非递归）
      for (const auto& entry : fs::directory_iterator(inputPath)) {
        if (entry.is_regular_file() && isShapefile(entry.path())) {
          fs::path outPath = generateOutputPath(entry.path(), outputPath);
          if (convertSingleShapefile(entry.path(), outPath)) {
            successCount++;
          }
          else {
            failCount++;
          }
        }
      }
    }

    std::cout << "\n转换完成！成功: " << successCount << " 个文件, 失败: " << failCount << " 个文件" << std::endl;
    return failCount > 0 ? 1 : 0;
  }

  // 递归转换目录
  int convertRecursive(const fs::path& inputPath, const fs::path& outputPath = "") {
    if (!fs::exists(inputPath)) {
      std::cerr << "错误: 输入路径不存在: " << inputPath << std::endl;
      return 1;
    }

    if (!fs::is_directory(inputPath)) {
      std::cerr << "错误: 递归模式需要输入目录: " << inputPath << std::endl;
      return 1;
    }

    int successCount = 0;
    int failCount = 0;

    // 递归遍历目录
    for (const auto& entry : fs::recursive_directory_iterator(inputPath)) {
      if (entry.is_regular_file() && isShapefile(entry.path())) {
        fs::path relativePath = fs::relative(entry.path(), inputPath);
        fs::path outPath;

        if (outputPath.empty()) {
          // 在原地创建带_utf8后缀的文件
          outPath = entry.path().parent_path() /
            (entry.path().stem().string() + "_utf8.shp");
        }
        else {
          // 在输出目录中保持相对路径结构
          outPath = outputPath / relativePath.parent_path() /
            (entry.path().stem().string() + ".shp");
        }

        if (convertSingleShapefile(entry.path(), outPath)) {
          successCount++;
        }
        else {
          failCount++;
        }
      }
    }

    std::cout << "\n递归转换完成！成功: " << successCount << " 个文件, 失败: " << failCount << " 个文件" << std::endl;
    return failCount > 0 ? 1 : 0;
  }
};

void printUsage(const char* programName) {
  std::cout << "使用方法: " << programName << " [选项] <输入路径> [输出路径]\n\n";
  std::cout << "选项:\n";
  std::cout << "  -r, --recursive     递归处理子目录\n";
  std::cout << "  -s, --source-enc    源编码 (默认: GBK)\n";
  std::cout << "  -t, --target-enc    目标编码 (默认: UTF-8)\n";
  std::cout << "  -v, --verbose       显示详细信息\n";
  std::cout << "  -h, --help          显示帮助信息\n\n";
  std::cout << "示例:\n";
  std::cout << "  " << programName << " input.shp output.shp\n";
  std::cout << "  " << programName << " -r input_folder output_folder\n";
  std::cout << "  " << programName << " -r -s GB2312 input_folder\n";
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
  // 设置Windows控制台为UTF-8编码
  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);
  // 启用ANSI转义序列（Windows 10+）
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut != INVALID_HANDLE_VALUE) {
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
      dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(hOut, dwMode);
    }
  }
#endif

  // 自动设置程序所在目录到PATH
  setupProgramPath(argv[0]);

  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  // 解析命令行参数
  bool recursive = false;
  bool verbose = false;
  std::string sourceEncoding = "GBK";
  std::string targetEncoding = "UTF-8";
  std::string inputPath;
  std::string outputPath;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-r" || arg == "--recursive") {
      recursive = true;
    }
    else if (arg == "-v" || arg == "--verbose") {
      verbose = true;
    }
    else if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    }
    else if ((arg == "-s" || arg == "--source-enc") && i + 1 < argc) {
      sourceEncoding = argv[++i];
    }
    else if ((arg == "-t" || arg == "--target-enc") && i + 1 < argc) {
      targetEncoding = argv[++i];
    }
    else if (inputPath.empty()) {
      inputPath = arg;
    }
    else if (outputPath.empty()) {
      outputPath = arg;
    }
  }

  if (inputPath.empty()) {
    std::cerr << "错误: 未指定输入路径\n";
    printUsage(argv[0]);
    return 1;
  }

  // 检查ogr2ogr是否可用
#ifdef _WIN32
  int checkResult = std::system("ogr2ogr --version > nul 2>&1");
#else
  int checkResult = std::system("ogr2ogr --version > /dev/null 2>&1");
#endif
  if (checkResult != 0) {
    std::cerr << "错误: 未找到ogr2ogr命令。" << std::endl;

    // 检查程序目录中是否存在ogr2ogr
    fs::path programDir;
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    programDir = fs::path(buffer).parent_path();
    fs::path ogr2ogrPath = programDir / "ogr2ogr.exe";
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
      buffer[len] = '\0';
      programDir = fs::path(buffer).parent_path();
    }
    fs::path ogr2ogrPath = programDir / "ogr2ogr";
#endif

    if (fs::exists(ogr2ogrPath)) {
      std::cerr << "注意: 在程序目录找到ogr2ogr: " << ogr2ogrPath << std::endl;
      std::cerr << "但无法执行，请检查文件权限或GDAL依赖库。" << std::endl;
    }
    else {
      std::cerr << "在程序目录未找到ogr2ogr，请确保ogr2ogr.exe与本程序在同一目录。" << std::endl;
    }
    return 1;
  }

  // 创建转换器并执行
  ShapefileConverter converter(sourceEncoding, targetEncoding, verbose);

  if (recursive) {
    return converter.convertRecursive(inputPath, outputPath);
  }
  else {
    return converter.convertPath(inputPath, outputPath);
  }
}
