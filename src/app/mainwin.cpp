/***************************************************************************
    mainwin.cpp
    ---------------------
    begin                : February 2017
    copyright            : (C) 2017 by Juergen E. Fischer
    email                : jef at norbit dot de
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#pragma region "头文件"
/*
* 杨小兵-2024-03-04
1. **`<windows.h>`**：
   - 这是Windows平台的一个头文件，提供了访问Windows API的函数、宏定义和类型定义。通过它，程序可以进行窗口管理、系统调用和与操作系统交互的其他任务。
例如，创建窗口、处理消息、访问系统资源等。

2. **`<io.h>`**：
   - 这是C语言标准库的扩展，主要用于处理低级的文件操作。它在Windows环境下提供了对文件的创建、打开、关闭、读写等操作的支持。包含了诸如`_open`、
`_read`、`_write`、`_close`等函数，以及对文件的属性操作（如检查文件是否存在）的函数。

3. **`<sstream>`**：
   - 这个头文件定义了字符串流类，包括`istringstream`（输入字符串流）、`ostringstream`（输出字符串流）和`stringstream`（输入输出字符串流）。
这些类用于字符串的读写操作，可以将变量转换为字符串或从字符串解析出变量，常用于格式化和解析数据。

4. **`<iostream>`**：
   - 定义了输入输出流的基础，包括`cin`、`cout`、`cerr`和`clog`对象。分别用于标准输入、标准输出、未缓冲的标准错误输出和缓冲的标准错误输出。它是处理
控制台输入输出的基础。

5. **`<fstream>`**：
   - 提供了对文件操作的输入输出流类，包括`ifstream`（用于从文件读取数据）、`ofstream`（用于向文件写入数据）和`fstream`（可以同时进行文件的读写）。
这些类支持对文件的打开、读写和关闭操作。

6. **`<list>`**：
   - 定义了双向链表容器类`list`。`list`是一个模板类，可以存储任意类型的元素。支持快速的元素插入和删除操作。由于其底层是链表实现，它不支持随机访问。

7. **`<memory>`**：
   - 提供了智能指针的定义，如`unique_ptr`、`shared_ptr`和`weak_ptr`。这些智能指针用于自动管理动态分配的内存，帮助避免内存泄漏和指针悬挂问题。
`unique_ptr`是独占所有权的智能指针，`shared_ptr`是共享所有权的智能指针，而`weak_ptr`是一种不控制对象生命周期的智能指针，用来解决`shared_ptr`间的循环引用问题。

*/
#include <windows.h>
#include <io.h>

#include <sstream>
#include <iostream>
#include <fstream>
#include <list>
#include <memory>
#include <ctime>
#include <cstdlib>

//  TJ目前设置为50，DLHJ设置为50
#define START_COUNTER 60
#pragma endregion

/*
* 杨小兵-2024-02-20
一、解释
  showError这个函数用来输出（在Windows上是通过一个对话框的形式）错误的信息，根据其中的字符串信息可以知道这部分内容是说明加载QGIS的时候出现了问题

二、总结
1、MessageBox函数是Windows.h中的一个函数，显示对话框的形式
2、其中MB_ICONERROR是在显示的对话框中加上一个“错误”图标

*/
void showError( std::string message, std::string title )
{
  std::string newmessage = "Oops, looks like an error loading QGIS \n\n Details: \n\n" + message;
  MessageBox(
    NULL,
    newmessage.c_str(),
    title.c_str(),
    MB_ICONERROR | MB_OK
  );
  std::cerr << message << std::endl;
}

/*
* 杨小兵-2024-03-25
一、解释
  这个名为`moduleExeBaseName`的函数，其目的是获取当前执行模块（通常是可执行文件）的完整路径，并从中提取基本名称（即文件名，不包括路径）。
它专为Windows平台编写，使用了Windows API函数`GetModuleFileName`。
  `GetModuleFileName`是Windows API的一部分，用于获取一个模块（可执行文件或DLL动态链接库）的完整路径名。这个函数能够返回包含驱动器号、目录、
文件名和扩展名的字符串，对于调试、日志记录、或是需要基于当前执行模块路径构造其他文件路径时非常有用。

二、`GetModuleFileName`函数的参数解释
- **hModule**: 这是一个`HMODULE`类型的句柄，指向想要获取全路径的模块。如果这个参数是`NULL`或`nullptr`，函数将返回当前进程的可执行文件的路径。
对于动态链接库（DLL）模块，可以通过`LoadLibrary`函数获取其句柄，并传递给`GetModuleFileName`。

- **lpFilename**: 指向一个缓冲区的指针，用于接收模块的完整路径字符串。这个缓冲区应该足够大，能够容纳`MAX_PATH`字符，以确保有足够的空间存放路径名。
`MAX_PATH`通常定义为260个字符，但在某些情况下路径可能会超过这个长度。

- **nSize**: 这个参数指定了`lpFilename`缓冲区的大小（以字符为单位）。这个大小包括了字符串的终止null字符。如果路径长度超过了这个值，函数调用将不会成功，
需要一个更大的缓冲区来存放完整路径。
  函数的返回值是复制到`lpFilename`缓冲区的字符串长度（不包括终止null字符）。如果函数成功，返回值是写入缓冲区的字符串长度；如果函数失败，返回值是0。
调用`GetLastError`函数可以获取更多错误信息

二、总结
1、moduleExeBaseName这个函数的作用就是获取当前可执行文件的完整路径，从中获取到文件名
2、GetModuleFileName是一个函数
3、GetModuleFileName属于windows.h中的内容
4、filepath.get()`返回的C风格字符串
5、如果GetModuleFileName函数成功，返回值是写入缓冲区的字符串长度；如果GetModuleFileName函数失败，返回值是0
*/
std::string moduleExeBaseName( void )
{
  DWORD l = MAX_PATH;
  //  通过分配一个char类型的智能指针filepath来保存文件（可执行文件）的路径信息
  std::unique_ptr<char> filepath;
  for ( ;; )
  {
    //  通过`filepath.reset(new char[l])`分配足够长的字符数组来尝试存放模块文件名
    filepath.reset( new char[l] );
    //  如果`GetModuleFileName`返回的长度小于`l`，说明文件名已成功获取，不需要再次循环；否则，增加`l`的值（`l += MAX_PATH;`）以提供更大的缓冲区并再次尝试
    if ( GetModuleFileName( nullptr, filepath.get(), l ) < l )
      break;
    l += MAX_PATH;
  }

  std::string basename( filepath.get() );
  return basename;
}

/*
* 杨小兵-2024-02-29
  UTF-8 字符串转换为UTF-16 并显示消息框（本文件是Windows中运行main函数的包装函数）
*/
void ShowUtf8MessageBox(const char* utf8Title, const char* utf8Message)
{
  int messageLen = MultiByteToWideChar(CP_UTF8, 0, utf8Message, -1, nullptr, 0);
  int titleLen = MultiByteToWideChar(CP_UTF8, 0, utf8Title, -1, nullptr, 0);

  wchar_t* wideMessage = new wchar_t[messageLen];
  wchar_t* wideTitle = new wchar_t[titleLen];

  MultiByteToWideChar(CP_UTF8, 0, utf8Message, -1, wideMessage, messageLen);
  MultiByteToWideChar(CP_UTF8, 0, utf8Title, -1, wideTitle, titleLen);

  MessageBoxW(nullptr, wideMessage, wideTitle, MB_ICONERROR | MB_OK);

  delete[] wideMessage;
  delete[] wideTitle;
}

/*
* 杨小兵-2024-02-29
  替换一个字符串中出现的多个特定子字符串
*/
void replaceAll(std::string& source, const std::string& from, const std::string& to)
{
  size_t startPos = 0;
  while ((startPos = source.find(from, startPos)) != std::string::npos)
  {
    source.replace(startPos, from.length(), to);
    // 由于替换的新字符串可能会影响后续位置，更新 startPos 以防止替换相同位置
    startPos += to.length();
  }
}

/*
1、作用：获取用户可写的系统相关目录路径（使用ProgramData目录）
*/
std::string getSystemDirectory()
{
  char programDataDir[MAX_PATH];
  // 使用ProgramData目录，所有用户都可以读写
  if (GetEnvironmentVariable("ProgramData", programDataDir, MAX_PATH) == 0)
  {
    //  获取ProgramData目录失败，使用备用方案
    if (GetTempPath(MAX_PATH, programDataDir) == 0)
    {
      //  获取临时目录也失败
      return "";
    }
  }
  
  // 创建DLHJ子目录
  std::string dlhjDir = std::string(programDataDir) + "\\DLHJ";
  CreateDirectory(dlhjDir.c_str(), NULL);  // 如果目录已存在会自动忽略
  
  return dlhjDir;
}

/*
1、作用：创建2048字节的gdal308-1.dll文件，前四字节为0，其余字节为随机数据
*/
bool createGdalDllFile(const std::string& filepath)
{
  std::ofstream file(filepath, std::ios::out | std::ios::binary);

  if (!file)
  {
    //  无法创建文件
    return false;
  }

  //  写入前四个字节（初始计数器为0）
  uint32_t initialCounter = 0;
  file.write(reinterpret_cast<const char*>(&initialCounter), sizeof(initialCounter));

  if (!file)
  {
    //  写入前四个字节失败
    return false;
  }

  //  生成并写入剩余的2044个随机字节
  srand(static_cast<unsigned int>(time(nullptr)));
  for (int i = 4; i < 2048; ++i)
  {
    char randomByte = static_cast<char>(rand() % 256);
    file.write(&randomByte, 1);
    if (!file)
    {
      //  写入随机字节失败
      return false;
    }
  }

  //  文件自动关闭，当 ofstream 对象销毁时
  return true;
}

/*
Note:杨小兵-2025-09-01

1、作用：读取系统目录下gdal308-1.dll文件前四个字节的函数
*/
bool readFirstFourBytesFromSystem(uint32_t& ui32)
{
  //  获取系统目录路径
  std::string systemDir = getSystemDirectory();
  if (systemDir.empty())
  {
    //  获取系统目录失败
    return false;
  }

  //  构建完整文件路径
  std::string filepath = systemDir + "\\gdal308-1.dll";

  //  检查文件是否存在
  std::ifstream checkFile(filepath);
  if (!checkFile)
  {
    //  文件不存在，创建新文件
    checkFile.close();
    if (!createGdalDllFile(filepath))
    {
      //  创建文件失败
      return false;
    }
  }
  else
  {
    checkFile.close();
  }

  //  以二进制读模式打开文件
  std::ifstream file(filepath, std::ios::in | std::ios::binary);

  if (!file)
  {
    //  无法打开文件
    return false;
  }

  //  读取前四个字节
  uint32_t number = 0;
  file.read(reinterpret_cast<char*>(&number), sizeof(number));

  if (!file)
  {
    //  读取前四个字节失败
    return false;
  }

  //  获取读取到的数值
  ui32 = number;

  //  文件自动关闭，当 ifstream 对象销毁时
  return true;
}

/*
1、作用：修改系统目录下gdal308-1.dll文件前四个字节的函数
*/
bool writeFirstFourBytesToSystem(const uint32_t& ui32)
{
  //  获取系统目录路径
  std::string systemDir = getSystemDirectory();
  if (systemDir.empty())
  {
    //  获取系统目录失败
    return false;
  }

  //  构建完整文件路径
  std::string filepath = systemDir + "\\gdal308-1.dll";

  //  以二进制读写模式打开文件
  std::fstream file(filepath, std::ios::in | std::ios::out | std::ios::binary);

  if (!file)
  {
    //  无法打开文件
    return false;
  }

  //  读取前四个字节
  uint32_t number = 0;
  file.read(reinterpret_cast<char*>(&number), sizeof(number));

  if (!file)
  {
    //  读取前四个字节失败
    return false;
  }

  //  将读取到的数值与 ui32 相加
  uint32_t sum = number + ui32;

  //  将文件指针移动到文件开头
  file.seekp(0, std::ios::beg);
  if (!file)
  {
    //  无法定位到文件开头
    return false;
  }

  //  写回更新后的数字
  file.write(reinterpret_cast<const char*>(&sum), sizeof(sum));
  if (!file)
  {
    //  写入更新后的数字失败
    return false;
  }

  //  文件自动关闭，当 fstream 对象销毁时
  return true;
}

/*
* 杨小兵-2024-02-20
一、解释
  int CALLBACK WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
上述函数签名是Windows应用程序的标准入口点函数`WinMain`的声明。这是一个特定于Windows平台的函数签名，用于启动基于
Windows GUI（图形用户界面）的应用程序。`WinMain`函数是Windows程序的主入口点，相当于控制台应用程序中的`main`函数。

  - `int CALLBACK`：`CALLBACK`是一个定义在Windows API中的宏，用于指定函数的调用约定。在Windows头文件中，
`CALLBACK`被定义为`__stdcall`调用约定，这种调用约定主要用于API函数和回调函数，确保函数参数从右到左入栈，
调用者清理栈空间。返回类型为`int`，表示函数的返回值是一个整数，通常用来指示程序的退出代码，`0`表示成功，
非`0`值表示错误或特定的退出原因。

二、参数说明
1、- `HINSTANCE hInstance`：这个参数是当前实例的句柄。在Windows程序中，每个运行的程序或动态链接库（DLL）都有一个实例句柄，
用于标识程序的一个加载实例。这个句柄可以在程序内部用来加载资源和执行其他与实例相关的操作。

2、- `HINSTANCE hPrevInstance`：在早期的Windows版本中，这个参数用于指示程序的上一个实例的句柄。但在32位和64位的Windows程序中，
这个参数总是为`NULL`，因为现代Windows操作系统不再使用这种方式来跟踪程序的实例。

3、`LPSTR lpCmdLine`：这个参数是一个指向命令行字符串的指针（不包括程序名）。在Windows GUI程序中，虽然不像控制台应用程序那样常见，
但仍然可以接受命令行参数，通过这个参数传递。

4、- `int nCmdShow`：这个参数指示程序窗口应该如何被显示。这个参数可以是一系列预定义的命令，用来指定窗口显示的状态，如最大化、最小化、
正常显示等。这个值通常由程序的启动命令或快捷方式指定。

三、总结
1、对比：控制台应用程序的入口是main函数；Windows GUI（图形用户界面）的应用程序的入口是WinMain函数
2、WinMain函数入口是可以调用main函数的
3、在编写Windows GUI应用程序时，`WinMain`函数是必须实现的入口点。它是程序与操作系统交互的起点，
负责初始化应用程序窗口、处理消息循环以及清理在程序退出时释放资源。

*/
int CALLBACK WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
#pragma region "获得qgis.exe程序在文件系统中的位置、获得qgis.exe所在的目录"
  std::string exename( moduleExeBaseName() );
  std::string basename( exename.substr( 0, exename.size() - 4 ) );
#pragma endregion

#pragma region "如果设置了OSGEO4W_ROOT环境变量并且是通过命令行参数的形式启动QGIS"
  /*
  * 杨小兵-2024-02-20
  一、解释
    `getenv`是一个标准C函数，用于获取环境变量的值。此处，它尝试获取名为`"OSGEO4W_ROOT"`的环境变量的值。如果该环境变量存在（即设置了值），
  `getenv`将返回一个指向该值的指针；如果不存在，返回`NULL`。`OSGEO4W_ROOT`通常用于指定OSGeo4W安装的根目录，OSGeo4W是一个Windows上的开源地理空间软件包集合，
  包括QGIS。这个检查用于确认当前环境是否配置了OSGeo4W的根目录。
    `__argc`是一个全局变量，表示程序启动时传递给`main`函数的参数数量（包括程序本身的名称）。如果`__argc`等于2，这意味着用户在程序名称之外提供了一个（且仅一个）
  命令行参数。这个检查用于确认程序是否正好接收到一个命令行参数。
    `__argv`是一个包含所有命令行参数的字符串数组的全局指针，`__argv[1]`表示传递给程序的第一个参数（索引从0开始，0位置是程序的名称）。
  `strcmp`是一个标准C函数，用于比较两个字符串是否相等。如果`__argv[1]`的值正好是`"--postinstall"`，`strcmp`函数返回0。
  这个检查用于确认传递给程序的命令行参数是否是`"--postinstall"`
  二、总结
  1、getenv是一个stdlib.h中的一个标准C函数，这个函数用来获取当前系统的环境变量的值
  2、如果该环境变量存在（即设置了值），`getenv`将返回一个指向该值的指针；如果不存在，返回`NULL`。
  3、__argc和__argv都是全局变量，用来存储程序启动之后传给main函数的参数信息
  4、strcmp是string.h中的一个标准C函数，当比较的两个字符串是相同的时候返回值为0
  */
  //  1、如果设置了OSGEO4W_ROOT环境变量并且是通过命令行参数的形式启动QGIS,那么需要读取*.vars文件中的内容并且将相应的环境变量写入到*.env中（就是设置各种环境变量）
  if ( getenv( "OSGEO4W_ROOT" ) && __argc == 2 && strcmp( __argv[1], "--postinstall" ) == 0 )
  {
    std::string envfile( basename + ".env" );
    /*
    * 杨小兵-2024-02-20
    一、解释
      `_access`函数是Windows平台特有的一个函数，用于检查程序对一个文件或目录的访问权限。它是`access`函数在Windows平台的等价，来自于C语言的标准库。
    这个函数允许程序查询文件系统中特定文件的访问权限，而不实际打开文件进行读写操作。

    二、_access函数参数解释
    1. **`const char *path`**: 第一个参数是一个指向字符数组的指针，代表要检查访问权限的文件或目录的路径。这个字符串应该是一个以null结尾的C风格字符串。

    2. **`int amode`**: 第二个参数是一个整数，指定要检查的访问权限类型。这个参数可以是以下值之一，或者是这些值的组合：
       - `0` (`F_OK`): 检查文件是否存在。
       - `2` (`W_OK`): 检查文件是否可写。
       - `4` (`R_OK`): 检查文件是否可读。
       - `6` (`R_OK | W_OK`): 检查文件是否同时可读可写
    3、返回值
      - 如果指定的访问权限检查通过（即当前用户对文件具有指定的访问权限），`_access`函数返回`0`。
      - 如果检查失败（即当前用户对文件不具有指定的访问权限，或文件不存在），函数返回`-1`。

    二、总结
    1、_access函数是Windows平台特有的一个函数，同access函数是等价的
    2、_access函数允许程序查询文件系统中特定文件的访问权限，而不实际打开文件进行读写操作
    3、_access函数检查当前运行的程序是否有权限访问指定的文件，以及具有什么样的访问权限
    4、下列条件想要表达的意思是，如果指定的文件不存在或者存在且可写，则执行后续的代码逻辑。这种检查通常用于确定是否可以创建新文件或覆写现有文件
    */
    // write or update environment file
    if ( _access( envfile.c_str(), 0 ) < 0 || _access( envfile.c_str(), 2 ) == 0 )
    {
      std::list<std::string> vars;

      try
      {
        /*
        一、解释
          这段代码的主要作用是从一个名为`basename + ".vars"`的文件中读取环境变量列表，并将这些环境变量存储到`std::list<std::string>`类型的列表`vars`中。
        这个过程使用了C++的标准输入输出库中的`std::ifstream`来处理文件读取。（当前变量将内容输出到哪里？当前变量将什么内容输入到当前变量中）
        二、总结
        1、`std::ifstream varfile;`声明了一个输入文件流`varfile`，用于读取文件
        2、通过`while (std::getline(varfile, var))`循环读取`varfile`中的每一行，并将其存储到字符串变量`var`中
        */
        std::ifstream varfile;
        varfile.open( basename + ".vars" );

        std::string var;
        while ( std::getline( varfile, var ) )
        {
          vars.push_back( var );
        }

        varfile.close();
      }
      catch ( std::ifstream::failure e )
      {
        /*
        一、解释
          `catch (std::ifstream::failure e)`捕获读取文件过程中可能发生的异常。如果在尝试打开文件、读取文件内容或其他操作中发生错误，
        将抛出一个类型为`std::ifstream::failure`的异常
        二、总结
        1、`std::ifstream varfile;`声明了一个输入文件流`varfile`，用于读取文件
        2、通过`while (std::getline(varfile, var))`循环读取`varfile`中的每一行，并将其存储到字符串变量`var`中
        */

        std::string message = "Could not read environment variable list " + basename + ".vars" + " [" + e.what() + "]";
        showError( message, "Error loading QGIS" );
        return EXIT_FAILURE;
      }

      try
      {
        /*
        * 杨小兵-2024-02-20
        一、解释
          `std::ofstream`属于C++标准库中的输入/输出文件流类，专门用于文件写操作。`open`是`std::ofstream`的一个成员函数，用于打开一个文件，以便进行写操作。

        二、参数说明
        `open`函数可以接受多个参数，但在这段代码中使用了两个参数：
        1. **第一个参数** (`envfile`): 这个参数是要打开文件的名称或路径，类型为`std::string`（或者是C风格的字符串指针`const char*`）。
        在这个例子中，`envfile`是一个`std::string`变量，包含了要打开文件的完整名称或路径。

        2. **第二个参数** (`std::ifstream::out`): 这个参数指定了文件流的打开模式。在这个上下文中，使用的是`std::ifstream::out`，
        这实际上是一个使用错误的地方，应该使用`std::ofstream::out`，因为`file`是`std::ofstream`类型的对象，而不是`std::ifstream`。
        不过，由于`std::ifstream::out`和`std::ofstream::out`在实际值上是相同的，这个错误在编译和运行时不会产生不良影响，但为了代码的清晰性和正确性，
        应该使用与对象类型相匹配的标志。`std::ofstream::out`模式表示文件将用于输出（写操作），如果文件已存在，它的内容会被清空，
        除非同时使用了`std::ofstream::app`（追加模式）。

        三、总结
        1、使用`open`函数时，如果指定的文件不存在，将会创建这个文件；如果文件已存在，并且没有指定追加模式（`app`），则原有文件内容会被清空。
        如果文件成功打开，可以使用文件流对象（如`file`）进行写操作；如果打开文件失败，文件流的状态会被设置为错误状态
        */
        std::ofstream file;
        file.open( envfile, std::ifstream::out );

        for ( std::list<std::string>::const_iterator it = vars.begin();  it != vars.end(); ++it )
        {
          if ( getenv( it->c_str() ) )
            file << *it << "=" << getenv( it->c_str() ) << std::endl;
        }
      }
      catch ( std::ifstream::failure e )
      {
        std::string message = "Could not write environment file " + basename + ".env" + " [" + e.what() + "]";
        showError( message, "Error loading QGIS" );
        return EXIT_FAILURE;
      }
    }

    return EXIT_SUCCESS;
  }
#pragma endregion

#pragma region "如果通过双击的方式启动QGIS、设置QGIS程序所需要的环境变量（从qgis.env中读取到具体的环境设置）（在发布的时候，还需要经过处理）"
  //  2、将*.env（各种环境变量的值）中的内容设置到当前进程的环境表中
  try
  {
    /*
    * 杨小兵-2024-02-20
    一、解释
      将*.env中的内容逐行读取出来，并且以读取出来的值设置环境变量，并且进行一些错误信息的处理。`_putenv`函数是一个C/C++标准库中的环境变量操作函数，
    用于添加、修改或删除当前进程的环境变量。在Windows平台上，它通常带有一个下划线前缀`_putenv`，而在POSIX（如Linux和UNIX）系统中，相应的函数是`putenv`。
    尽管这两个函数在不同的平台上名称可能略有不同，它们的基本功能和用途是类似的。`_putenv`函数用于更改或添加环境变量到当前进程的环境表中。如果环境变量已存在，
    其值将被新值替换；如果环境变量不存在，此函数会创建这个环境变量并赋予指定的值。使用此函数可以动态地控制进程的环境变量，对程序运行时的环境进行调整。

    二、_putenv函数参数说明
    1、`_putenv`函数接收一个参数：
      - **`const char* string`**: 这个参数是一个C风格的字符串，包含了要设置的环境变量的名称和值，格式为`"NAME=VALUE"`。如果`VALUE`为空，
    即字符串为`"NAME="`，则相当于删除名为`NAME`的环境变量。字符串应以null字符`\0`结尾。
    2、- **返回值**：`_putenv`函数的返回值用于指示操作成功与否。
      - 如果函数成功将环境变量添加到环境表中或更新环境变量的值，函数返回`0`。
      - 如果函数失败，例如由于内存不足无法添加新的环境变量，函数返回一个非零值，通常是`-1`。

    三、总结
    1、_putenv是stdlib.h中的一个函数，用来添加、修改、删除当前进程的环境变量
    2、_putenv只对当前进程的环境变量起作用，并不会对全局或者其他的进程起作用

    */
    std::ifstream file;
    file.open( basename + ".env" );

#pragma region "发布版本的时候将这段代码打开:设置STARMAP_INSTALL_DIRECTORY_PREFIX环境变量"
    /*
    * 杨小兵-2024-02-29
      首先检测系统内是否设置了STARMAP_INSTALL_DIRECTORY_PREFIX自定义系统环境变量，如果没有设置的话直接退出并且进行给出提示信息
    */
    const char* starmap_install_directory_prefix = getenv("DLHJ_INSTALL_DIRECTORY_PREFIX_V_2_03_03");
    if (starmap_install_directory_prefix == nullptr)
    {
      std::string starmap_install_directory_prefix_title = "提示：设置环境变量";
      std::string starmap_install_directory_prefix_message = "需要设置环境变量DLHJ_INSTALL_DIRECTORY_PREFIX_V_2_03_03。如果没有DLHJ_INSTALL_DIRECTORY_PREFIX_V_2_03_03这个变量，请首先创建这个环境变量，并且将其值设置为DLHJ在文件系统中的位置！";

      //  给出提示信息（在Windows下使用一个提示窗口，在其他平台这里还要通过宏定义进行跨平台操作）
      ShowUtf8MessageBox(starmap_install_directory_prefix_title.c_str(), starmap_install_directory_prefix_message.c_str());

      return EXIT_FAILURE;
    }

    std::string str_starmap_install_directory_prefix = std::string(starmap_install_directory_prefix);
    const std::string str_left_slash = std::string("/");
    const std::string str_right_slash = std::string("\\");
#pragma endregion

    std::string var;
    while ( std::getline( file, var ) )
    {

#pragma region "发布版本的时候将这段代码打开:设置STARMAP_INSTALL_DIRECTORY_PREFIX环境变量"
      /*
      * 杨小兵-2024-02-29
        将前面得到的STARMAP_INSTALL_DIRECTORY_PREFIX替换到具体的环境变量中，然后进行设置当前进程的环境变量中
      */
      //  先将starmap_install_directory_prefix中的正反斜杠统一处理
      //std::string temp = var;(debug)
      replaceAll(str_starmap_install_directory_prefix, str_right_slash, str_left_slash);
      replaceAll(var, "DLHJ_INSTALL_DIRECTORY_PREFIX_V_2_03_03", str_starmap_install_directory_prefix);
      //temp = var;
#pragma endregion


      if ( _putenv( var.c_str() ) < 0 )
      {
        std::string message = "Could not set environment variable:" + var;
        showError( message, "Error loading QGIS" );
        return EXIT_FAILURE;
      }
    }
  }
  catch ( std::ifstream::failure e )
  {
    std::string message = "Could not read environment file " + basename + ".env" + " [" + e.what() + "]";
    showError( message, "Error loading QGIS" );
    return EXIT_FAILURE;
  }
#pragma endregion

#pragma region "（增强安全性，不使用也是可以的）加载指定的动态链接库文件进入到调用进程的地址空间：获取`kernel32.dll`中的`SetDefaultDllDirectories`和`AddDllDirectory`函数"
  /*
  * 杨小兵-2024-02-20
  一、解释
    这段代码是在Windows平台上使用Windows API进行动态库（DLL）管理的一个例子。主要涉及到动态加载库，获取库中函数的地址，
  并将这些地址转换为可直接调用的函数指针。 `LoadLibrary`是一个Windows API函数，用于加载指定的动态链接库（DLL）文件进入
  调用进程的地址空间，并返回一个模块的句柄（`HINSTANCE`）。如果加载成功，`LoadLibrary`返回库的句柄；如果失败，返回`NULL`。

    `GetProcAddress`是一个Windows API函数，用于获取一个已加载模块（DLL）中某个导出函数的地址。`reinterpret_cast`用于将
  从`GetProcAddress`获取到的通用函数地址转换为具体的函数指针类型，这里是`BOOL (*)(DWORD)`，即一个接受`DWORD`参数并返回`BOOL`的函数指针
    `AddDllDirectory`函数用于添加一个目录到应用程序的搜索路径，当应用程序尝试加载DLL时，这个目录也会被搜索。这是一个在较新版本的Windows中引
  入的函数，用于管理DLL的搜索路径，增强安全性

    这段代码的目的是在运行时动态地获取并使用`kernel32.dll`中的`SetDefaultDllDirectories`和`AddDllDirectory`函数，而不是在编译时静态地链接这些函数。
  这种方式允许程序在较旧版本的Windows上运行，即使这些版本的`kernel32.dll`不包含这些函数。如果程序运行在支持这些函数的Windows版本上，它可以利用这些函数
  来增强安全性，否则，程序可以选择不使用这些功能或采取其他措施。

  二、总结
  1、在Windows平台上使用windows.h中的函数
  2、LoadLibrary是用来对动态库进行管理的一个函数
  3、获取动态库中函数地址，并且将这些获取得到的函数地址转化成可以直接调用的函数指针
  4、LoadLibrary将指定的动态链接库文件加载到当前进程的地址空间内
  5、通过动态的方式获取并使用`kernel32.dll`中的`SetDefaultDllDirectories`和`AddDllDirectory`函数是为了程序能够在older windows上运行
  6、如果Windows版本支持这两个函数，那么程序可以使用这两个函数增强安全性，反之则不使用这两个函数。

  三、LoadLibrary函数在加载动态库的时候如何找到具体的动态库？
  1. **目录从哪里启动了应用程序**：首先搜索启动当前应用程序的目录。对于许多系统组件或标准DLL（如`kernel32.dll`），这一步通常不会找到DLL，因为这些DLL不会存放在
  应用程序目录下。
  2. **系统目录**：然后搜索系统目录，用`GetSystemDirectory`函数可以获取到。对于`kernel32.dll`这样的核心系统DLL，它们位于系统目录，因此在这一步会被找到并加载。
  系统目录通常是`C:\Windows\System32`。
  3. **16位系统目录**：搜索不再常用的16位系统目录（比如`C:\Windows\System`），对于现代应用几乎不适用。
  4. **Windows目录**：搜索Windows目录，`GetWindowsDirectory`函数可以获取到这个目录。
  5. **当前目录**：搜索应用程序的当前工作目录。由于这可能导致安全问题（比如DLL劫持），不推荐将需要加载的DLL放在这里。
  6. **PATH环境变量指定的目录**：最后，搜索在系统的PATH环境变量中指定的目录。
    对于`kernel32.dll`，因为它是一个系统级别的核心库，所以通常会在步骤2中被找到并加载。Windows操作系统通过这一固定的搜索顺序确保应用程序可以找到并加载正确的DLL文件。
  管理员和用户一般不需要（也不应该）修改有关系统核心组件如`kernel32.dll`的路径设置。这种机制保证了系统的稳定性和安全性，避免了DLL冲突或劫持等问题。

  四、搜索目录
  1、从当前应用程序所在的目录中想要的动态库；
  2、系统目录；
  3、16系统目录；
  4、windows目录；
  5、PATH指定的目录
  */
  HINSTANCE hKernelDLL = LoadLibrary( "kernel32.dll" );
  BOOL ( *SetDefaultDllDirectories )( DWORD ) = hKernelDLL ? reinterpret_cast<BOOL( * )( DWORD )>( GetProcAddress( hKernelDLL, "SetDefaultDllDirectories" ) ) : 0;
  DLL_DIRECTORY_COOKIE( *AddDllDirectory )( PCWSTR ) = hKernelDLL ? reinterpret_cast<DLL_DIRECTORY_COOKIE( * )( PCWSTR )>( GetProcAddress( hKernelDLL, "AddDllDirectory" ) ) : 0;

  //  利用SetDefaultDllDirectories、AddDllDirectory这两个函数来增强安全性
  if ( SetDefaultDllDirectories && AddDllDirectory )
  {
    SetDefaultDllDirectories( LOAD_LIBRARY_SEARCH_DEFAULT_DIRS );

    wchar_t windir[MAX_PATH];
    GetWindowsDirectoryW( windir, MAX_PATH );
    wchar_t systemdir[MAX_PATH];
    GetSystemDirectoryW( systemdir, MAX_PATH );

    wchar_t *path = wcsdup( _wgetenv( L"PATH" ) );

    for ( wchar_t *p = wcstok( path, L";" ); p; p = wcstok( NULL, L";" ) )
    {
      if ( wcsicmp( p, windir ) == 0 )
        continue;
      if ( wcsicmp( p, systemdir ) == 0 )
        continue;
      AddDllDirectory( p );
    }

    free( path );
  }

#pragma endregion

#pragma region "将qgis_app动态链接库加载到调用进程的地址空间内"
/*
1、#ifdef _MSC_VER检查是否定义了`_MSC_VER`宏，这个宏是Microsoft Visual C++编译器特有的，用于标识正在使用Visual C++编译器
2、#ifdef _MSC_VER检查如果没有定义_MSC_VER，那么加载动态库的名称发生了变化（使用的MinGW编译器）
*/
#ifdef _MSC_VER
  HINSTANCE hGetProcIDDLL = NULL;
#pragma region "杨小兵-2025-01-22：这里增加次数检测"
  uint32_t ui32 = 0;
  //  从系统目录读取gdal308-1.dll文件的前四个字节
  if (readFirstFourBytesFromSystem(ui32) && (ui32 <= START_COUNTER))
  {
    //  将qgis_app.dll动态库加载到当前进程空间中
    hGetProcIDDLL = LoadLibrary("qgis_app.dll");
    writeFirstFourBytesToSystem(1);
  }
  else
  {
    //  不将qgis_app.dll动态库加载到当前进程空间中
    hGetProcIDDLL = NULL;
  }
#pragma endregion
#else
// MinGW
  HINSTANCE hGetProcIDDLL = LoadLibrary( "libqgis_app.dll" );
#endif

  //  如果加载qgis_app.dll出现问题则需要处理
  if ( !hGetProcIDDLL )
  {
    //  错误处理
    DWORD error = GetLastError();
    LPTSTR errorText = NULL;

    /*
    * 杨小兵-2024-03-26：将错误信息通过对话框的形式显示出来
    * FormatMessage函数解释
    一、解释
      `FormatMessage`函数是Windows API中的一个功能强大的函数，它用于格式化文本消息。该函数可以从多个源获取错误代码或消息定义，并将其转换为人类可读的文本字符串。
    这些源包括系统错误代码、应用程序定义的错误代码、动态链接库(DLL)中的消息表等。`FormatMessage`的主要作用是将错误代码（例如，由`GetLastError`返回的代码）或消
    息定义转换为格式化文本消息。这使得开发者可以向最终用户显示易于理解的错误信息，或者在调试过程中更容易地识别问题。该函数支持插入序列，允许动态替换消息文本中的值，
    提供了一种灵活的方式来构建复杂的消息。

    二、 `FormatMessage`函数的各个参数
    - **参数1：dwFlags**：指定消息格式化和处理的选项。这个参数是一系列预定义的标志的组合，常用的包括：
      - `FORMAT_MESSAGE_FROM_SYSTEM`：从系统消息表中检索消息定义。
      - `FORMAT_MESSAGE_ALLOCATE_BUFFER`：函数分配一个足够大的缓冲区来保存格式化的消息，并通过`lpBuffer`参数返回指向这个缓冲区的指针。
      - `FORMAT_MESSAGE_IGNORE_INSERTS`：忽略消息定义中的插入序列。

    - **参数2：lpSource**：指定消息定义的来源。当使用`FORMAT_MESSAGE_FROM_STRING`标志时，`lpSource`指向一个字符串；如果使用`FORMAT_MESSAGE_FROM_SYSTEM`
    或`FORMAT_MESSAGE_FROM_HMODULE`，则通常设置为`NULL`。

    - **参数3：dwMessageId**：指定要格式化的消息的消息标识符（即错误代码）。

    - **参数4：dwLanguageId**：指定消息的语言标识符。`MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)`是一个常用值，表示使用默认语言。

    - **参数5：lpBuffer**：指向用于接收格式化消息的缓冲区的指针。当使用`FORMAT_MESSAGE_ALLOCATE_BUFFER`标志时，`FormatMessage`会为输出文本分配一个足够大的
    缓冲区，并通过这个参数返回缓冲区的地址。

    - **参数6：nSize**：指定输出缓冲区的大小（以字符为单位）。当使用`FORMAT_MESSAGE_ALLOCATE_BUFFER`标志时，应将此参数设置为`0`。

    - **参数7：Arguments**：指向包含要插入到格式化消息中的值的数组的指针。这通常用于消息字符串中有插入序列时。如果消息字符串中没有插入序列，或者使用了
    `FORMAT_MESSAGE_IGNORE_INSERTS`标志，则可以将其设置为`NULL`。

    */
    FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      error,
      MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
      ( LPTSTR )&errorText,
      0,
      NULL );

    std::string message = "Could not load qgis_app.dll \n Windows Error: " + std::string( errorText )
                          + "\n Help: \n\n Check " + basename + ".env for correct environment paths";
    showError( message, "Error loading QGIS" );

    LocalFree( errorText );
    errorText = NULL;
    return EXIT_FAILURE;
  }
#pragma endregion

  /*
  * 杨小兵-2024-02-20
  一、解释
    这段代码的目的是在运行时动态地定位并调用`qgis_app.dll`（或其他指定的DLL）中的`main`函数。这种技术通常用于插件式架构、
  模块化应用程序或在运行时需要加载不同模块的情况。具体到QGIS这种复杂的GIS（地理信息系统）软件中，这可以使主程序动态地加载
  不同版本的应用程序模块或插件，增加了软件的灵活性和可扩展性。下列定义了一个函数指针`realmain`，这个指针指向一个函数，该
  函数的原型为`int main(int, char*[])`，这是C++主函数（`main`函数）的标准签名。然后，使用`GetProcAddress`函数从一个动
  态链接库（通过句柄`hGetProcIDDLL`指定）中获取名为`main`的函数的地址，并将这个地址强制转换（cast）为与`realmain`相同类
  型的函数指针。

  二、总结
  1、GetProcAddress是Windows.h中的一个函数，用来获取指定动态库中的导出函数或者变量的地址
  2、这个文件中对不能加载到调用程序的进程空间的动态库进行了消息的显示，同时对被调用动态库中的函数信息进行了显示

  */
  int ( *realmain )( int, char *[] ) = ( int ( * )( int, char *[] ) ) GetProcAddress( hGetProcIDDLL, "main" );
  if ( !realmain )
  {
    showError( "Could not locate main function in qgis_app.dll", "Error loading QGIS" );
    return EXIT_FAILURE;
  }

  return realmain( __argc, __argv );
}
