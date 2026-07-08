/***************************************************************************
    startuphelper.h
    ---------------------
    begin                : June 2026
    copyright            : (C) 2026 by Ethan Yang
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGISSTARTUPHELPER_H
#define QGISSTARTUPHELPER_H

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#elif defined( __linux__ ) && !defined( ANDROID )
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace QgsStartupHelper
{
  inline constexpr const char *INSTALL_DIRECTORY_PREFIX_PLACEHOLDER = "QGIS40200_INSTALL_DIRECTORY_PREFIX_V_0_00_01";
  inline constexpr const char *INSTALL_DIRECTORY_NAME = "QGIS40200";
  inline constexpr std::uint32_t START_COUNTER = 100;
  inline constexpr const char *SHOW_MECHANISM_ERRORS_ENVIRONMENT_VARIABLE = "QGIS_STARTUPHELPER_SHOW_MECHANISM_ERRORS";
  inline constexpr const char *GENERIC_STARTUP_ENVIRONMENT_ERROR = "Could not verify the QGIS startup environment.";

#ifdef _WIN32
  inline constexpr const char *WINDOWS_COUNTER_FILE_NAME = "QGIS40200.dll";
  inline constexpr const char *WINDOWS_LOCK_FILE_NAME = "QGIS40200.lock";
#elif defined( __linux__ ) && !defined( ANDROID )
  inline constexpr const char *LINUX_COUNTER_FILE_NAME = ".QGIS40200.dat";
  inline constexpr const char *LINUX_LOCK_FILE_NAME = ".QGIS40200.lock";
#endif

  inline bool asciiEqualsIgnoreCase( const char *left, const char *right )
  {
    if ( !left || !right )
      return false;

    while ( *left && *right )
    {
      char leftChar = *left;
      char rightChar = *right;
      if ( leftChar >= 'a' && leftChar <= 'z' )
        leftChar = static_cast<char>( leftChar - 'a' + 'A' );
      if ( rightChar >= 'a' && rightChar <= 'z' )
        rightChar = static_cast<char>( rightChar - 'a' + 'A' );

      if ( leftChar != rightChar )
        return false;

      ++left;
      ++right;
    }

    return *left == '\0' && *right == '\0';
  }

  inline bool showMechanismErrors()
  {
    return asciiEqualsIgnoreCase( std::getenv( SHOW_MECHANISM_ERRORS_ENVIRONMENT_VARIABLE ), "ON" );
  }

  inline void setMechanismError( std::string &error, const char *message )
  {
    error = showMechanismErrors() ? message : GENERIC_STARTUP_ENVIRONMENT_ERROR;
  }

#define QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, message ) \
  do                                                            \
  {                                                             \
    setMechanismError( ( error ), ( message ) );                 \
  }                                                             \
  while ( false )

#ifdef _WIN32

  inline bool utf8ToWideString( const char *utf8, std::wstring &wide )
  {
    const int requiredLength = MultiByteToWideChar( CP_UTF8, 0, utf8, -1, nullptr, 0 );
    if ( requiredLength <= 0 )
      return false;

    wide.assign( static_cast<size_t>( requiredLength ), L'\0' );
    if ( MultiByteToWideChar( CP_UTF8, 0, utf8, -1, &wide[0], requiredLength ) <= 0 )
      return false;

    if ( !wide.empty() && wide.back() == L'\0' )
      wide.pop_back();

    return true;
  }

  inline void showStartupMessageBox( const char *utf8Title, const char *utf8Message )
  {
    std::wstring title;
    std::wstring message;
    if ( !utf8ToWideString( utf8Title, title ) || !utf8ToWideString( utf8Message, message ) )
    {
      MessageBoxA( nullptr, utf8Message, utf8Title, MB_ICONERROR | MB_OK );
      return;
    }

    MessageBoxW( nullptr, message.c_str(), title.c_str(), MB_ICONERROR | MB_OK );
  }
#endif

  inline void replaceAll( std::string &source, const std::string &from, const std::string &to )
  {
    if ( from.empty() )
      return;

    size_t startPos = 0;
    while ( ( startPos = source.find( from, startPos ) ) != std::string::npos )
    {
      source.replace( startPos, from.length(), to );
      startPos += to.length();
    }
  }

  inline bool normalizeInstallDirectoryPrefix( std::string &normalizedPrefix, std::string &error )
  {
    const char *installDirectoryPrefix = std::getenv( INSTALL_DIRECTORY_PREFIX_PLACEHOLDER );
    if ( !installDirectoryPrefix )
    {
      error = "Environment variable \"" + std::string( INSTALL_DIRECTORY_PREFIX_PLACEHOLDER ) + "\" must be set before starting QGIS.";
      return false;
    }

    normalizedPrefix = std::string( installDirectoryPrefix );
    replaceAll( normalizedPrefix, "\\", "/" );
    return true;
  }

  inline void replaceInstallDirectoryPrefix( std::string &line, const std::string &normalizedPrefix )
  {
    replaceAll( line, INSTALL_DIRECTORY_PREFIX_PLACEHOLDER, normalizedPrefix );
  }

  inline bool replaceInstallDirectoryPrefix( std::string &line, std::string &error )
  {
    if ( line.find( INSTALL_DIRECTORY_PREFIX_PLACEHOLDER ) == std::string::npos )
      return true;

    std::string normalizedPrefix;
    if ( !normalizeInstallDirectoryPrefix( normalizedPrefix, error ) )
      return false;

    replaceAll( line, INSTALL_DIRECTORY_PREFIX_PLACEHOLDER, normalizedPrefix );
    return true;
  }

  inline bool verifyStartupCounterValue( const std::uint32_t counterValue, std::string &error )
  {
    if ( counterValue > START_COUNTER )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Launch count verification failed." );
      return false;
    }

    return true;
  }

#ifdef _WIN32
  inline bool getSystemDirectory( std::string &systemDir, std::string &error )
  {
    std::string programDataDir;
    DWORD programDataDirLength = GetEnvironmentVariableA( "ProgramData", nullptr, 0 );
    if ( programDataDirLength > 0 )
    {
      programDataDir.assign( static_cast<size_t>( programDataDirLength ), '\0' );
      const DWORD copiedLength = GetEnvironmentVariableA( "ProgramData", &programDataDir[0], programDataDirLength );
      if ( copiedLength > 0 && copiedLength < programDataDirLength )
        programDataDir.resize( static_cast<size_t>( copiedLength ) );
      else
        programDataDir.clear();
    }

    if ( programDataDir.empty() )
    {
      const DWORD tempPathLength = GetTempPathA( 0, nullptr );
      if ( tempPathLength == 0 )
      {
        QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not locate system storage for launch counter." );
        return false;
      }

      programDataDir.assign( static_cast<size_t>( tempPathLength ), '\0' );
      const DWORD copiedLength = GetTempPathA( tempPathLength, &programDataDir[0] );
      if ( copiedLength == 0 || copiedLength >= tempPathLength )
      {
        QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not locate system storage for launch counter." );
        return false;
      }

      programDataDir.resize( static_cast<size_t>( copiedLength ) );
    }

    std::string qgis34407Dir = programDataDir + "\\" + INSTALL_DIRECTORY_NAME;
    if ( !CreateDirectoryA( qgis34407Dir.c_str(), nullptr ) )
    {
      const DWORD lastError = GetLastError();
      if ( lastError != ERROR_ALREADY_EXISTS )
      {
        QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not prepare system storage for launch counter." );
        return false;
      }
    }

    const DWORD attributes = GetFileAttributesA( qgis34407Dir.c_str() );
    if ( attributes == INVALID_FILE_ATTRIBUTES || !( attributes & FILE_ATTRIBUTE_DIRECTORY ) )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Launch counter system storage path is not a directory." );
      return false;
    }

    systemDir = qgis34407Dir;
    return true;
  }

  struct ScopedWindowsFileLock
  {
    HANDLE file = INVALID_HANDLE_VALUE;
    OVERLAPPED overlapped = {};
    bool locked = false;

    ~ScopedWindowsFileLock()
    {
      release();
    }

    ScopedWindowsFileLock() = default;
    ScopedWindowsFileLock( const ScopedWindowsFileLock & ) = delete;
    ScopedWindowsFileLock &operator=( const ScopedWindowsFileLock & ) = delete;

    void release()
    {
      if ( file == INVALID_HANDLE_VALUE )
        return;

      if ( locked )
        UnlockFileEx( file, 0, MAXDWORD, MAXDWORD, &overlapped );

      CloseHandle( file );
      file = INVALID_HANDLE_VALUE;
      locked = false;
      overlapped = {};
    }
  };

  inline bool acquireWindowsStartupLock( const std::string &lockPath, ScopedWindowsFileLock &lock, std::string &error )
  {
    lock.file = CreateFileA(
      lockPath.c_str(),
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr,
      OPEN_ALWAYS,
      FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
      nullptr
    );
    if ( lock.file == INVALID_HANDLE_VALUE )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not acquire launch counter lock." );
      return false;
    }

    if ( !LockFileEx( lock.file, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &lock.overlapped ) )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not acquire launch counter lock." );
      return false;
    }

    lock.locked = true;
    return true;
  }

  inline bool createLaunchCounterFile( const std::string &filepath )
  {
    std::ofstream file( filepath, std::ios::out | std::ios::binary );
    if ( !file )
      return false;

    const std::uint32_t initialCounter = 0;
    file.write( reinterpret_cast<const char *>( &initialCounter ), sizeof( initialCounter ) );
    if ( !file )
      return false;

    std::srand( static_cast<unsigned int>( std::time( nullptr ) ) );
    for ( int i = 4; i < 2048; ++i )
    {
      char randomByte = static_cast<char>( std::rand() % 256 );
      file.write( &randomByte, 1 );
      if ( !file )
        return false;
    }

    return true;
  }

  inline bool launchCounterFileExists( const std::string &filepath, bool &exists, std::string &error )
  {
    const DWORD attributes = GetFileAttributesA( filepath.c_str() );
    if ( attributes != INVALID_FILE_ATTRIBUTES )
    {
      if ( attributes & FILE_ATTRIBUTE_DIRECTORY )
      {
        QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Launch counter path points to a directory." );
        return false;
      }

      exists = true;
      return true;
    }

    const DWORD lastError = GetLastError();
    if ( lastError == ERROR_FILE_NOT_FOUND || lastError == ERROR_PATH_NOT_FOUND )
    {
      exists = false;
      return true;
    }

    QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not inspect launch counter file in system storage." );
    return false;
  }

  inline bool readLaunchCounterFromSystem( std::uint32_t &value, std::string &error )
  {
    std::string systemDir;
    if ( !getSystemDirectory( systemDir, error ) )
      return false;

    const std::string filepath = systemDir + "\\" + WINDOWS_COUNTER_FILE_NAME;
    bool counterFileExists = false;
    if ( !launchCounterFileExists( filepath, counterFileExists, error ) )
      return false;

    if ( !counterFileExists )
    {
      if ( !createLaunchCounterFile( filepath ) )
      {
        QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not initialize launch counter in system storage." );
        return false;
      }
    }

    std::ifstream file( filepath, std::ios::in | std::ios::binary );
    if ( !file )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not read launch counter from system storage." );
      return false;
    }

    std::uint32_t number = 0;
    file.read( reinterpret_cast<char *>( &number ), sizeof( number ) );
    if ( !file )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not read launch counter from system storage." );
      return false;
    }

    value = number;
    return true;
  }

  inline bool incrementLaunchCounterInSystem( std::string &error )
  {
    std::string systemDir;
    if ( !getSystemDirectory( systemDir, error ) )
      return false;

    const std::string filepath = systemDir + "\\" + WINDOWS_COUNTER_FILE_NAME;
    std::fstream file( filepath, std::ios::in | std::ios::out | std::ios::binary );
    if ( !file )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not update launch counter in system storage." );
      return false;
    }

    std::uint32_t number = 0;
    file.read( reinterpret_cast<char *>( &number ), sizeof( number ) );
    if ( !file )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not update launch counter in system storage." );
      return false;
    }

    const std::uint32_t nextLaunchCount = number + 1;
    file.seekp( 0, std::ios::beg );
    if ( !file )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not update launch counter in system storage." );
      return false;
    }

    file.write( reinterpret_cast<const char *>( &nextLaunchCount ), sizeof( nextLaunchCount ) );
    if ( !file )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not update launch counter in system storage." );
      return false;
    }

    return true;
  }

  inline bool verifyLaunchCounter( const std::uint32_t launchCount, std::string &error )
  {
    return verifyStartupCounterValue( launchCount, error );
  }

  inline bool verifyLaunchCounterInSystem( std::string &error )
  {
    std::uint32_t launchCount = 0;
    return readLaunchCounterFromSystem( launchCount, error ) && verifyLaunchCounter( launchCount, error );
  }

  inline bool loadLibraryWithLaunchCounter( const char *libraryName, HINSTANCE &library, std::string &error )
  {
    library = nullptr;

    std::string systemDir;
    if ( !getSystemDirectory( systemDir, error ) )
      return false;

    ScopedWindowsFileLock startupLock;
    if ( !acquireWindowsStartupLock( systemDir + "\\" + WINDOWS_LOCK_FILE_NAME, startupLock, error ) )
      return false;

    bool success = true;
    DWORD loadLibraryError = ERROR_SUCCESS;
    if ( !verifyLaunchCounterInSystem( error ) )
    {
      success = false;
    }
    else
    {
      library = LoadLibraryA( libraryName );
      loadLibraryError = library ? ERROR_SUCCESS : GetLastError();

      if ( library && !incrementLaunchCounterInSystem( error ) )
      {
        FreeLibrary( library );
        library = nullptr;
        success = false;
      }
    }

    startupLock.release();
    if ( !library && loadLibraryError != ERROR_SUCCESS )
      SetLastError( loadLibraryError );

    return success;
  }
#elif defined( __linux__ ) && !defined( ANDROID )
  struct ScopedFileDescriptor
  {
    int fd = -1;

    ~ScopedFileDescriptor()
    {
      close();
    }

    ScopedFileDescriptor() = default;
    ScopedFileDescriptor( const ScopedFileDescriptor & ) = delete;
    ScopedFileDescriptor &operator=( const ScopedFileDescriptor & ) = delete;

    void close()
    {
      if ( fd >= 0 )
      {
        ::close( fd );
        fd = -1;
      }
    }
  };

  inline bool linuxPathIsAbsolute( const std::string &path )
  {
    return !path.empty() && path[0] == '/';
  }

  inline std::string linuxTrimTrailingSlashes( std::string path )
  {
    while ( path.size() > 1 && path.back() == '/' )
      path.pop_back();

    return path;
  }

  inline std::string linuxJoinPath( const std::string &basePath, const char *filename )
  {
    return linuxTrimTrailingSlashes( basePath ) + "/" + filename;
  }

  inline bool linuxEnsureDirectoryTree( const std::string &path, std::string &error )
  {
    if ( !linuxPathIsAbsolute( path ) )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Launch counter user state path is not absolute." );
      return false;
    }

    const std::string normalizedPath = linuxTrimTrailingSlashes( path );
    size_t end = 1;
    while ( true )
    {
      end = normalizedPath.find( '/', end );
      const std::string currentPath = end == std::string::npos ? normalizedPath : normalizedPath.substr( 0, end );
      if ( currentPath.size() > 1 )
      {
        if ( ::mkdir( currentPath.c_str(), S_IRWXU ) != 0 && errno != EEXIST )
        {
          QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not prepare user state storage for launch counter." );
          return false;
        }

        struct stat pathStatus = {};
        if ( ::stat( currentPath.c_str(), &pathStatus ) != 0 || !S_ISDIR( pathStatus.st_mode ) )
        {
          QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Launch counter user state path is not a directory." );
          return false;
        }
      }

      if ( end == std::string::npos )
        break;

      ++end;
    }

    if ( ::chmod( normalizedPath.c_str(), S_IRWXU ) != 0 )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not secure user state storage for launch counter." );
      return false;
    }

    return true;
  }

  inline bool getUserStateDirectory( std::string &stateDir, std::string &error )
  {
    const char *xdgStateHome = std::getenv( "XDG_STATE_HOME" );
    std::string basePath;
    if ( xdgStateHome && linuxPathIsAbsolute( xdgStateHome ) )
    {
      basePath = xdgStateHome;
    }
    else
    {
      const char *homePath = std::getenv( "HOME" );
      if ( !homePath || !linuxPathIsAbsolute( homePath ) )
      {
        QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not locate user state storage for launch counter." );
        return false;
      }

      basePath = linuxJoinPath( homePath, ".local/state" );
    }

    stateDir = linuxJoinPath( basePath, INSTALL_DIRECTORY_NAME );
    return linuxEnsureDirectoryTree( stateDir, error );
  }

  inline bool linuxWriteAll( const int fd, const char *buffer, const size_t bytes )
  {
    size_t written = 0;
    while ( written < bytes )
    {
      const ssize_t result = ::write( fd, buffer + written, bytes - written );
      if ( result < 0 )
      {
        if ( errno == EINTR )
          continue;

        return false;
      }

      if ( result == 0 )
        return false;

      written += static_cast<size_t>( result );
    }

    return true;
  }

  inline bool linuxReadAll( const int fd, char *buffer, const size_t bytes )
  {
    size_t readBytes = 0;
    while ( readBytes < bytes )
    {
      const ssize_t result = ::read( fd, buffer + readBytes, bytes - readBytes );
      if ( result < 0 )
      {
        if ( errno == EINTR )
          continue;

        return false;
      }

      if ( result == 0 )
        return false;

      readBytes += static_cast<size_t>( result );
    }

    return true;
  }

  inline bool linuxValidatePrivateRegularFile( const int fd, const char *errorMessage, std::string &error )
  {
    struct stat fileStatus = {};
    if ( ::fstat( fd, &fileStatus ) != 0 || !S_ISREG( fileStatus.st_mode ) )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, errorMessage );
      return false;
    }

    if ( fileStatus.st_uid != ::geteuid() || fileStatus.st_nlink != 1 )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, errorMessage );
      return false;
    }

    return true;
  }

  inline bool acquireLinuxStartupLock( const std::string &lockPath, ScopedFileDescriptor &lockFile, std::string &error )
  {
    lockFile.fd = ::open( lockPath.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR );
    if ( lockFile.fd < 0 )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not acquire launch counter lock." );
      return false;
    }

    if ( !linuxValidatePrivateRegularFile( lockFile.fd, "Launch counter lock is not a private regular file.", error ) )
      return false;

    if ( ::fchmod( lockFile.fd, S_IRUSR | S_IWUSR ) != 0 )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not secure launch counter lock." );
      return false;
    }

    struct flock lock = {};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    while ( ::fcntl( lockFile.fd, F_SETLKW, &lock ) != 0 )
    {
      if ( errno == EINTR )
        continue;

      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not acquire launch counter lock." );
      return false;
    }

    return true;
  }

  inline bool initializeLinuxCounterFile( const int fd, std::string &error )
  {
    if ( ::ftruncate( fd, 0 ) != 0 || ::lseek( fd, 0, SEEK_SET ) < 0 )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not initialize launch counter in user state storage." );
      return false;
    }

    const std::uint32_t initialCounter = 0;
    if ( !linuxWriteAll( fd, reinterpret_cast<const char *>( &initialCounter ), sizeof( initialCounter ) ) )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not initialize launch counter in user state storage." );
      return false;
    }

    std::srand( static_cast<unsigned int>( std::time( nullptr ) ) );
    for ( int i = 4; i < 2048; ++i )
    {
      const char randomByte = static_cast<char>( std::rand() % 256 );
      if ( !linuxWriteAll( fd, &randomByte, 1 ) )
      {
        QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not initialize launch counter in user state storage." );
        return false;
      }
    }

    if ( ::lseek( fd, 0, SEEK_SET ) < 0 )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not initialize launch counter in user state storage." );
      return false;
    }

    return true;
  }

  inline bool openLinuxCounterFile( const std::string &counterPath, ScopedFileDescriptor &counterFile, std::string &error )
  {
    bool counterFileCreated = true;
    counterFile.fd = ::open( counterPath.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR );
    if ( counterFile.fd < 0 && errno == EEXIST )
    {
      counterFileCreated = false;
      counterFile.fd = ::open( counterPath.c_str(), O_RDWR | O_CLOEXEC | O_NOFOLLOW );
    }

    if ( counterFile.fd < 0 )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not open launch counter in user state storage." );
      return false;
    }

    if ( !linuxValidatePrivateRegularFile( counterFile.fd, "Launch counter is not a private regular file.", error ) )
      return false;

    if ( ::fchmod( counterFile.fd, S_IRUSR | S_IWUSR ) != 0 )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not secure launch counter in user state storage." );
      return false;
    }

    if ( counterFileCreated && !initializeLinuxCounterFile( counterFile.fd, error ) )
      return false;

    return true;
  }

  inline bool readLinuxCounter( const int fd, std::uint32_t &counterValue, std::string &error )
  {
    if ( ::lseek( fd, 0, SEEK_SET ) < 0 || !linuxReadAll( fd, reinterpret_cast<char *>( &counterValue ), sizeof( counterValue ) ) )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not read launch counter from user state storage." );
      return false;
    }

    return true;
  }

  inline bool writeLinuxCounter( const int fd, const std::uint32_t counterValue, std::string &error )
  {
    if ( ::lseek( fd, 0, SEEK_SET ) < 0 || !linuxWriteAll( fd, reinterpret_cast<const char *>( &counterValue ), sizeof( counterValue ) ) )
    {
      QGIS_STARTUPHELPER_SET_MECHANISM_ERROR( error, "Could not update launch counter in user state storage." );
      return false;
    }

    return true;
  }

  inline bool verifyAndIncrementLaunchCounterInUserState( std::string &error )
  {
    std::string stateDir;
    if ( !getUserStateDirectory( stateDir, error ) )
      return false;

    ScopedFileDescriptor lockFile;
    if ( !acquireLinuxStartupLock( linuxJoinPath( stateDir, LINUX_LOCK_FILE_NAME ), lockFile, error ) )
      return false;

    ScopedFileDescriptor counterFile;
    if ( !openLinuxCounterFile( linuxJoinPath( stateDir, LINUX_COUNTER_FILE_NAME ), counterFile, error ) )
      return false;

    std::uint32_t counterValue = 0;
    if ( !readLinuxCounter( counterFile.fd, counterValue, error ) || !verifyStartupCounterValue( counterValue, error ) )
      return false;

    return writeLinuxCounter( counterFile.fd, counterValue + 1, error );
  }
#endif
} // namespace QgsStartupHelper

#undef QGIS_STARTUPHELPER_SET_MECHANISM_ERROR

#endif // QGISSTARTUPHELPER_H
