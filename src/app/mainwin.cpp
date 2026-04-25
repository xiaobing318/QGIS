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
#include <windows.h>
#include <io.h>

#include <sstream>
#include <iostream>
#include <fstream>
#include <list>
#include <memory>
#include <ctime>
#include <cstdlib>
#include <cstdint>

#define START_COUNTER 100

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

std::string moduleExeBaseName( void )
{
  DWORD l = MAX_PATH;
  std::unique_ptr<char> filepath;
  for ( ;; )
  {
    filepath.reset( new char[l] );
    if ( GetModuleFileName( nullptr, filepath.get(), l ) < l )
      break;

    l += MAX_PATH;
  }

  std::string basename( filepath.get() );
  return basename;
}

void showUtf8MessageBox( const char *utf8Title, const char *utf8Message )
{
  int messageLen = MultiByteToWideChar( CP_UTF8, 0, utf8Message, -1, nullptr, 0 );
  int titleLen = MultiByteToWideChar( CP_UTF8, 0, utf8Title, -1, nullptr, 0 );
  if ( messageLen <= 0 || titleLen <= 0 )
  {
    MessageBoxA( nullptr, utf8Message, utf8Title, MB_ICONERROR | MB_OK );
    return;
  }

  std::wstring wideMessage( static_cast<size_t>( messageLen ), L'\0' );
  std::wstring wideTitle( static_cast<size_t>( titleLen ), L'\0' );
  MultiByteToWideChar( CP_UTF8, 0, utf8Message, -1, wideMessage.data(), messageLen );
  MultiByteToWideChar( CP_UTF8, 0, utf8Title, -1, wideTitle.data(), titleLen );
  MessageBoxW( nullptr, wideMessage.c_str(), wideTitle.c_str(), MB_ICONERROR | MB_OK );
}

void replaceAll( std::string &source, const std::string &from, const std::string &to )
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

std::string getSystemDirectory()
{
  char programDataDir[MAX_PATH] = {};
  if ( GetEnvironmentVariableA( "ProgramData", programDataDir, MAX_PATH ) == 0 )
  {
    if ( GetTempPathA( MAX_PATH, programDataDir ) == 0 )
      return std::string();
  }

  std::string qgis34407Dir = std::string( programDataDir ) + "\\QGIS34407";
  CreateDirectoryA( qgis34407Dir.c_str(), nullptr );
  return qgis34407Dir;
}

bool createQGIS34407DllFile( const std::string &filepath )
{
  std::ofstream file( filepath, std::ios::out | std::ios::binary );
  if ( !file )
    return false;

  std::uint32_t initialCounter = 0;
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

bool readFirstFourBytesFromSystem( std::uint32_t &value )
{
  std::string systemDir = getSystemDirectory();
  if ( systemDir.empty() )
    return false;

  std::string filepath = systemDir + "\\QGIS34407.dll";
  std::ifstream checkFile( filepath, std::ios::in | std::ios::binary );
  if ( !checkFile )
  {
    if ( !createQGIS34407DllFile( filepath ) )
      return false;
  }

  std::ifstream file( filepath, std::ios::in | std::ios::binary );
  if ( !file )
    return false;

  std::uint32_t number = 0;
  file.read( reinterpret_cast<char *>( &number ), sizeof( number ) );
  if ( !file )
    return false;

  value = number;
  return true;
}

bool writeFirstFourBytesToSystem( std::uint32_t increment )
{
  std::string systemDir = getSystemDirectory();
  if ( systemDir.empty() )
    return false;

  std::string filepath = systemDir + "\\QGIS34407.dll";
  std::fstream file( filepath, std::ios::in | std::ios::out | std::ios::binary );
  if ( !file )
    return false;

  std::uint32_t number = 0;
  file.read( reinterpret_cast<char *>( &number ), sizeof( number ) );
  if ( !file )
    return false;

  std::uint32_t sum = number + increment;
  file.seekp( 0, std::ios::beg );
  if ( !file )
    return false;

  file.write( reinterpret_cast<const char *>( &sum ), sizeof( sum ) );
  return !!file;
}

int CALLBACK WinMain( HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/ )
{
  std::string exename( moduleExeBaseName() );
  std::string basename( exename.substr( 0, exename.size() - 4 ) );

  if ( getenv( "OSGEO4W_ROOT" ) && __argc == 2 && strcmp( __argv[1], "--postinstall" ) == 0 )
  {
    std::string envfile( basename + ".env" );

    // write or update environment file
    if ( _access( envfile.c_str(), 0 ) < 0 || _access( envfile.c_str(), 2 ) == 0 )
    {
      std::list<std::string> vars;

      try
      {
        std::ifstream varfile;
        varfile.open( basename + ".vars" );

        std::string var;
        while ( std::getline( varfile, var ) )
        {
          vars.push_back( var );
        }

        varfile.close();
      }
      catch ( std::ifstream::failure &e )
      {
        std::string message = "Could not read environment variable list " + basename + ".vars" + " [" + e.what() + "]";
        showError( message, "Error loading QGIS" );
        return EXIT_FAILURE;
      }

      try
      {
        std::ofstream file;
        file.open( envfile, std::ifstream::out );

        for ( std::list<std::string>::const_iterator it = vars.begin(); it != vars.end(); ++it )
        {
          if ( getenv( it->c_str() ) )
            file << *it << "=" << getenv( it->c_str() ) << std::endl;
        }
      }
      catch ( std::ifstream::failure &e )
      {
        std::string message = "Could not write environment file " + basename + ".env" + " [" + e.what() + "]";
        showError( message, "Error loading QGIS" );
        return EXIT_FAILURE;
      }
    }

    return EXIT_SUCCESS;
  }

  try
  {
    std::ifstream file;
    file.open( basename + ".env" );

    const char *installDirectoryPrefix = getenv( "QGIS34407_INSTALL_DIRECTORY_PREFIX_V_0_01_00" );
    if ( !installDirectoryPrefix )
    {
      std::string title = "Missing environment variable";
      std::string message = "QGIS34407_INSTALL_DIRECTORY_PREFIX_V_0_01_00 must be set before starting QGIS.";
      showUtf8MessageBox( title.c_str(), message.c_str() );
      return EXIT_FAILURE;
    }

    std::string normalizedInstallDirectoryPrefix = std::string( installDirectoryPrefix );
    replaceAll( normalizedInstallDirectoryPrefix, "\\", "/" );

    std::string var;
    while ( std::getline( file, var ) )
    {
      replaceAll( var, "QGIS34407_INSTALL_DIRECTORY_PREFIX_V_0_01_00", normalizedInstallDirectoryPrefix );
      if ( _putenv( var.c_str() ) < 0 )
      {
        std::string message = "Could not set environment variable:" + var;
        showError( message, "Error loading QGIS" );
        return EXIT_FAILURE;
      }
    }
  }
  catch ( std::ifstream::failure &e )
  {
    std::string message = "Could not read environment file " + basename + ".env" + " [" + e.what() + "]";
    showError( message, "Error loading QGIS" );
    return EXIT_FAILURE;
  }

#ifndef _MSC_VER // MinGW
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
  HINSTANCE hKernelDLL = LoadLibrary( "kernel32.dll" );
  BOOL ( *SetDefaultDllDirectories )( DWORD ) = hKernelDLL ? reinterpret_cast<BOOL ( * )( DWORD )>( GetProcAddress( hKernelDLL, "SetDefaultDllDirectories" ) ) : 0;
  DLL_DIRECTORY_COOKIE ( *AddDllDirectory )( PCWSTR ) = hKernelDLL ? reinterpret_cast<DLL_DIRECTORY_COOKIE ( * )( PCWSTR )>( GetProcAddress( hKernelDLL, "AddDllDirectory" ) ) : 0;
#ifndef _MSC_VER // MinGW
#pragma GCC diagnostic pop
#endif

  if ( SetDefaultDllDirectories && AddDllDirectory )
  {
    SetDefaultDllDirectories( LOAD_LIBRARY_SEARCH_DEFAULT_DIRS );

    wchar_t windir[MAX_PATH];
    GetWindowsDirectoryW( windir, MAX_PATH );
    wchar_t systemdir[MAX_PATH];
    GetSystemDirectoryW( systemdir, MAX_PATH );

    wchar_t *path = wcsdup( _wgetenv( L"PATH" ) );

#ifdef _UCRT
    for ( wchar_t *p = wcstok( path, L";", nullptr ); p; p = wcstok( NULL, L";", nullptr ) )
#else
    for ( wchar_t *p = wcstok( path, L";" ); p; p = wcstok( NULL, L";" ) )
#endif
    {
      if ( wcsicmp( p, windir ) == 0 )
        continue;
      if ( wcsicmp( p, systemdir ) == 0 )
        continue;
      AddDllDirectory( p );
    }

    free( path );
  }


#ifdef _MSC_VER
  HINSTANCE hGetProcIDDLL = nullptr;
  std::uint32_t launchCount = 0;
  if ( !readFirstFourBytesFromSystem( launchCount ) )
  {
    showError( "Could not read launch counter from system storage.", "Error loading QGIS" );
    return EXIT_FAILURE;
  }

  if ( launchCount > START_COUNTER )
  {
    showError( "Launch count verification failed.", "Error loading QGIS" );
    return EXIT_FAILURE;
  }

  hGetProcIDDLL = LoadLibrary( "qgis_app.dll" );
  if ( hGetProcIDDLL && !writeFirstFourBytesToSystem( 1 ) )
  {
    showError( "Could not update launch counter in system storage.", "Error loading QGIS" );
    return EXIT_FAILURE;
  }
#else
  // MinGW
  HINSTANCE hGetProcIDDLL = LoadLibrary( "libqgis_app.dll" );
#endif

  if ( !hGetProcIDDLL )
  {
    DWORD error = GetLastError();
    LPTSTR errorText = NULL;

    FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      error,
      MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
      ( LPTSTR ) &errorText,
      0,
      NULL
    );

    std::string message = "Could not load qgis_app.dll \n Windows Error: " + std::string( errorText )
                          + "\n Help: \n\n Check " + basename + ".env for correct environment paths";
    showError( message, "Error loading QGIS" );

    LocalFree( errorText );
    errorText = NULL;
    return EXIT_FAILURE;
  }

#ifndef _MSC_VER // MinGW
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
  int ( *realmain )( int, char *[] ) = ( int ( * )( int, char *[] ) ) GetProcAddress( hGetProcIDDLL, "main" );
#ifndef _MSC_VER // MinGW
#pragma GCC diagnostic pop
#endif

  if ( !realmain )
  {
    showError( "Could not locate main function in qgis_app.dll", "Error loading QGIS" );
    return EXIT_FAILURE;
  }

  return realmain( __argc, __argv );
}
