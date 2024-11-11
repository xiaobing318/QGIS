Building QGIS from source - step by step（从源码一步步构建QGIS）

<!-- TOC start (generated with https://bitdowntoc.derlin.ch/) -->

- [1. Introduction（简介）](#1-introduction简介)
- [2. Overview（概览）](#2-overview概览)
- [3. Building on GNU/Linux](#3-building-on-gnulinux)
  - [3.1. Building QGIS with Qt 5.x（使用QT5来构建QGIS）](#31-building-qgis-with-qt-5x使用qt5来构建qgis)
  - [3.2. Prepare apt（准备apt）](#32-prepare-apt准备apt)
  - [3.3. Install build dependencies（安装需要的构建库）](#33-install-build-dependencies安装需要的构建库)
  - [3.4. Setup ccache (Optional, but recommended)（配置ccache，虽然是可选的但是推荐进行配置）](#34-setup-ccache-optional-but-recommended配置ccache虽然是可选的但是推荐进行配置)
  - [3.5. Prepare your development environment（准备你的开发环境）](#35-prepare-your-development-environment准备你的开发环境)
  - [3.6. Check out the QGIS Source Code（check out QGIS的源码）](#36-check-out-the-qgis-source-codecheck-out-qgis的源码)
  - [3.7. Starting the compile（开始进行编译）](#37-starting-the-compile开始进行编译)
    - [3.7.1 Available compilation flags（可用的编译flags）](#371-available-compilation-flags可用的编译flags)
  - [3.8. Compiling with 3D（编译3D）](#38-compiling-with-3d编译3d)
    - [3.8.1. Compiling with 3D on old Debian based distributions（在旧的基于 Debian 的发行版上使用 3D 进行编译）](#381-compiling-with-3d-on-old-debian-based-distributions在旧的基于-debian-的发行版上使用-3d-进行编译)
  - [3.9. Building different branches](#39-building-different-branches)
  - [3.10. Building Debian packages（构建 Debian 软件包）](#310-building-debian-packages构建-debian-软件包)
    - [3.10.1. Building packages with Oracle support](#3101-building-packages-with-oracle-support)
  - [3.11. On Fedora Linux](#311-on-fedora-linux)
    - [3.11.1. Install build dependencies](#3111-install-build-dependencies)
    - [3.11.2. Suggested system tweaks](#3112-suggested-system-tweaks)
    - [3.11.3. Additional tools for QGIS development](#3113-additional-tools-for-qgis-development)
    - [3.11.4. QT6 experimental builds with Fedora Rawhide](#3114-qt6-experimental-builds-with-fedora-rawhide)
- [4. Building on Windows](#4-building-on-windows)
  - [4.1. Building with Microsoft Visual Studio](#41-building-with-microsoft-visual-studio)
    - [4.1.1. Visual Studio 2022 Community Edition](#411-visual-studio-2022-community-edition)
    - [4.1.2. Other tools and dependencies](#412-other-tools-and-dependencies)
    - [4.1.3. Clone the QGIS Source Code](#413-clone-the-qgis-source-code)
    - [4.1.4. OSGeo4W](#414-osgeo4w)
  - [4.2. Building on Linux with mingw64](#42-building-on-linux-with-mingw64)
    - [4.2.1. Building with Docker](#421-building-with-docker)
      - [4.2.1.1. Initial setup](#4211-initial-setup)
      - [4.2.1.2. Building the dependencies](#4212-building-the-dependencies)
      - [4.2.1.3. Cross-Building QGIS](#4213-cross-building-qgis)
    - [4.2.2. Testing QGIS](#422-testing-qgis)
  - [4.3 Building on Windows with vcpkg](#43-building-on-windows-with-vcpkg)
    - [4.3.1 Install Build Tools](#431-install-build-tools)
    - [4.3.2 Build QGIS](#432-build-qgis)
      - [4.3.2.1 Build with an SDK](#4321-build-with-an-sdk)
      - [4.3.2.1 Build all the dependencies locally](#4321-build-all-the-dependencies-locally)
- [5. Building on MacOS X](#5-building-on-macos-x)
  - [5.1. Install Developer Tools](#51-install-developer-tools)
  - [5.2. Install CMake and other build tools](#52-install-cmake-and-other-build-tools)
  - [5.3. Install Qt5 and QGIS-Deps](#53-install-qt5-and-qgis-deps)
  - [5.4. QGIS source](#54-qgis-source)
  - [5.5. Configure the build](#55-configure-the-build)
  - [5.6. Building](#56-building)
- [6. Setting up the WCS test server on GNU/Linux](#6-setting-up-the-wcs-test-server-on-gnulinux)
  - [6.1. Preparation](#61-preparation)
  - [6.2. Setup mapserver](#62-setup-mapserver)
  - [6.3. Create a home page](#63-create-a-home-page)
  - [6.4. Now deploy it](#64-now-deploy-it)
  - [6.5. Debugging](#65-debugging)
- [7. Setting up a Jenkins Build Server](#7-setting-up-a-jenkins-build-server)
- [8. Debug output and running tests](#8-debug-output-and-running-tests)
- [9. Authors and Acknowledgments](#9-authors-and-acknowledgments)

<!-- TOC end -->

# 1. Introduction（简介）

This document is the original installation guide of the described software
QGIS. The software and hardware descriptions named in this
document are in most cases registered trademarks and are therefore subject
to the legal requirements. QGIS is subject to the GNU General Public
License. Find more information on the QGIS Homepage:
https://qgis.org

 - 本文档是所述软件 QGIS 的原始安装指南。本文档中提及的软件和硬件描述在大多数情况下均为注册商标，因此受法律要求约束。QGIS 受 GNU 通用公共许可证约束。有关更多信息，请访问 QGIS 主页：https://qgis.org

The details, that are given in this document have been written and verified
to the best of knowledge and responsibility of the editors. Nevertheless,
mistakes concerning the content are possible. Therefore, all data are not
liable to any duties or guarantees. The editors and publishers do not take
any responsibility or liability for failures and their consequences. You are
always welcome for indicating possible mistakes.

 - 本文档中提供的详细信息均由编辑尽其所能编写和验证。然而，内容方面可能存在错误。因此，所有数据均不承担任何责任或保证。编辑和出版商对失败及其后果不承担任何责任。欢迎您随时指出可能的错误。

Because the code of QGIS evolves from release to release, These instructions are
regularly updated to match the corresponding release. Instructions for the current
master branch are available at https://github.com/qgis/QGIS/blob/master/INSTALL.md.
If you wish to build another version of QGIS, ensure to checkout the appropriate
release branch. The QGIS source code can be found [in the repository](https://github.com/qgis/QGIS).

 - 由于 QGIS 的代码在各个版本之间不断演变，因此这些说明会定期更新以匹配相应的版本。当前主分支的说明可在 https://github.com/qgis/QGIS/blob/master/INSTALL.md 上找到。如果您希望构建另一个版本的 QGIS，请确保签出相应的发布分支。QGIS 源代码可在 [存储库](https://github.com/qgis/QGIS) 中找到。

Please visit https://qgis.org for information on joining our mailing lists
and getting involved in the project further.

 - 请访问 https://qgis.org 了解加入我们的邮件列表以及进一步参与该项目的信息。

**Note to document writers:** Please use this document as the central
place for describing build procedures. Please do not remove this notice.

 - **文档编写者请注意：**请将此文档作为描述构建过程的中心位置。请不要删除此通知。

# 2. Overview（概览）

QGIS, like a number of major projects (e.g., KDE 4.0),
uses [CMake](https://www.cmake.org) for building from source.

 - 类似一些关键主要的项目（例如KDE 4.0），QGIS使用的是CMake来对源代码进行构建。

Following a summary of the required dependencies for building（以下是构建所需的依赖项的摘要）:

Required build tools（所需的构建工具）:

* CMake >= 3.12.0
* Flex >= 2.5.6
* Bison >= 2.4
* Python >= 3.7

Required build dependencies（所需的构建依赖）:

* Qt >= 5.15.2
* Proj >= 7.2.0
* GEOS >= 3.9
* Sqlite3 >= 3.0.0
* SpatiaLite >= 4.2.0
* libspatialindex
* GDAL/OGR >= 3.2.0
* Qwt >= 5.0 & (< 6.1 with internal QwtPolar)
* expat >= 1.95
* QScintilla2
* QCA
* qtkeychain (>= 0.5)
* libzip

Optional dependencies（可选的一些依赖项）:

* for GRASS providers and plugin - GRASS >= 7.0.0.
* for georeferencer - GSL >= 1.8
* for PostGIS support - PostgreSQL >= 8.0.x
* for gps plugin - gpsbabel
* for mapserver export and PyQGIS - Python >= 3.6
* for python support - SIP >= 4.12, PyQt >= 5.3 must match Qt version, Qscintilla2
* for qgis mapserver - FastCGI
* for oracle provider - Oracle OCI library

Indirect dependencies（所需要的间接依赖项）:

Some proprietary formats (e.g., ECW and MrSid) supported by GDAL require
proprietary third party libraries.  QGIS doesn't need any of those itself to
build, but will only support those formats if GDAL is built accordingly.  Refer
to [format list](https://gdal.org/index.html) for instructions how to include
those formats in GDAL.

 - GDAL 支持的一些专有格式（例如 ECW 和 MrSid）需要专有的第三方库。QGIS 本身不需要任何这些库来构建，但只有在 GDAL 相应构建时才会支持这些格式。有关如何在 GDAL 中包含这些格式的说明，请参阅[格式列表](https://gdal.org/index.html)。

# 3. Building on GNU/Linux

## 3.1. Building QGIS with Qt 5.x（使用QT5来构建QGIS）

**Requires:** Ubuntu / Debian derived distro（Ubuntu / Debian 衍生发行版）

**Note:** Refer to the section [Building Debian packages](#310-building-debian-packages)
for building debian packages. Unless you plan to develop on QGIS, that is
probably the easiest option to compile and install QGIS.

 - **注意：**请参阅 [构建 Debian 软件包](#310-building-debian-packages) 部分以了解如何构建 Debian 软件包。除非您计划在 QGIS 上进行开发，否则这可能是编译和安装 QGIS 最简单的方法。

These notes are for Ubuntu - other versions and Debian derived distros may
require slight variations in package names.

 - 这些说明适用于 Ubuntu - 其他版本和 Debian 衍生发行版可能需要在包名称上略有不同。

These notes are for if you want to build QGIS from source. One of the major
aims here is to show how this can be done using binary packages for ***all***
dependencies - building only the core QGIS stuff from source. I prefer this
approach because it means we can leave the business of managing system packages
to apt and only concern ourselves with coding QGIS!

 - 如果您想要从源代码构建 QGIS，这些说明是为您准备的。这里的主要目的之一是展示如何使用二进制包来完成***所有***依赖项 - 仅从源代码构建核心 QGIS 内容。我更喜欢这种方法，因为这意味着我们可以将管理系统包的工作留给 apt，而只关心编写 QGIS 代码！（QGIS 依赖于多个第三方库和工具，如 GDAL、PROJ、Qt 等。通过使用二进制包来管理这些依赖项，用户可以简化安装过程，避免手动编译和配置每个依赖项。总体而言，作者提倡通过利用现有的包管理工具来处理 QGIS 项目的依赖项，仅从源代码构建核心组件。这种方法能够显著简化构建过程，减少维护负担，使开发者能够更专注于 QGIS 的功能开发和改进，从而提高项目的开发效率和代码质量。）

This document assumes you have made a fresh install and have a 'clean' system.
These instructions should work fine if this is a system that has already been
in use for a while, you may need to just skip those steps which are irrelevant
to you.

 - 本文档假设您已完成全新安装并拥有“干净”的系统。如果这是一个已使用一段时间的系统，这些说明应该可以正常工作，您可能需要跳过与您无关的步骤。

## 3.2. Prepare apt（准备apt）

You will need a recent enough distribution in order for all dependencies
to be met. The supported distributions are listed in the following section.

 - 您需要一个足够新的发行版才能满足所有依赖项。以下部分列出了支持的发行版。

Now update your local sources database（现在更新您的本地源数据库）:

```bash
//  更新本地源数据库
sudo apt-get update
```

## 3.3. Install build dependencies（安装需要的构建库）

|Distribution|Install command for packages|
|------------|----------------------------|
| bullseye | ``apt-get install bison build-essential ca-certificates ccache cmake cmake-curses-gui dh-python doxygen expect flex flip gdal-bin git graphviz grass-dev libdraco-dev libexiv2-dev libexpat1-dev libfcgi-dev libgdal-dev libgeos-dev libgsl-dev libpdal-dev libpq-dev libproj-dev libprotobuf-dev libqca-qt5-2-dev libqca-qt5-2-plugins libqscintilla2-qt5-dev libqt5opengl5-dev libqt5serialport5-dev libqt5sql5-sqlite libqt5svg5-dev libqt5webkit5-dev libqt5xmlpatterns5-dev libqwt-qt5-dev libspatialindex-dev libspatialite-dev libsqlite3-dev libsqlite3-mod-spatialite libyaml-tiny-perl libzip-dev libzstd-dev lighttpd locales ninja-build ocl-icd-opencl-dev opencl-headers pandoc pdal pkg-config poppler-utils protobuf-compiler pyqt5-dev pyqt5-dev-tools pyqt5.qsci-dev python3-all-dev python3-autopep8 python3-dev python3-gdal python3-jinja2 python3-lxml python3-mock python3-nose2 python3-owslib python3-plotly python3-psycopg2 python3-pygments python3-pyproj python3-pyqt5 python3-pyqt5.qsci python3-pyqt5.qtmultimedia python3-pyqt5.qtpositioning python3-pyqt5.qtserialport python3-pyqt5.qtsql python3-pyqt5.qtsvg python3-pyqt5.qtwebkit python3-sip python3-sip-dev python3-termcolor python3-yaml qt3d-assimpsceneimport-plugin qt3d-defaultgeometryloader-plugin qt3d-gltfsceneio-plugin qt3d-scene2d-plugin qt3d5-dev qt5keychain-dev qtbase5-dev qtbase5-private-dev qtmultimedia5-dev qtpositioning5-dev qttools5-dev qttools5-dev-tools spawn-fcgi xauth xfonts-100dpi xfonts-75dpi xfonts-base xfonts-scalable xvfb`` |
| bookworm | ``apt-get install bison build-essential ca-certificates ccache cmake cmake-curses-gui dh-python doxygen expect flex flip gdal-bin git graphviz grass-dev libdraco-dev libexiv2-dev libexpat1-dev libfcgi-dev libgdal-dev libgeos-dev libgsl-dev libpq-dev libproj-dev libprotobuf-dev libqca-qt5-2-dev libqca-qt5-2-plugins libqscintilla2-qt5-dev libqt5opengl5-dev libqt5serialport5-dev libqt5sql5-sqlite libqt5svg5-dev libqt5webkit5-dev libqt5xmlpatterns5-dev libqwt-qt5-dev libspatialindex-dev libspatialite-dev libsqlite3-dev libsqlite3-mod-spatialite libyaml-tiny-perl libzip-dev libzstd-dev lighttpd locales ninja-build ocl-icd-opencl-dev opencl-headers pandoc pkg-config poppler-utils protobuf-compiler pyqt5-dev pyqt5-dev-tools pyqt5.qsci-dev python3-all-dev python3-autopep8 python3-dev python3-gdal python3-jinja2 python3-lxml python3-mock python3-nose2 python3-owslib python3-plotly python3-psycopg2 python3-pygments python3-pyproj python3-pyqt5 python3-pyqt5.qsci python3-pyqt5.qtmultimedia python3-pyqt5.qtpositioning python3-pyqt5.qtserialport python3-pyqt5.qtsql python3-pyqt5.qtsvg python3-pyqt5.qtwebkit python3-pyqtbuild python3-sip python3-termcolor python3-yaml qt3d-assimpsceneimport-plugin qt3d-defaultgeometryloader-plugin qt3d-gltfsceneio-plugin qt3d-scene2d-plugin qt3d5-dev qtbase5-dev qtbase5-private-dev qtkeychain-qt5-dev qtmultimedia5-dev qtpositioning5-dev qttools5-dev qttools5-dev-tools sip-tools spawn-fcgi xauth xfonts-100dpi xfonts-75dpi xfonts-base xfonts-scalable xvfb`` |
| jammy | ``apt-get install bison build-essential ca-certificates ccache cmake cmake-curses-gui dh-python doxygen expect flex flip gdal-bin git graphviz grass-dev libdraco-dev libexiv2-dev libexpat1-dev libfcgi-dev libgdal-dev libgeos-dev libgsl-dev libpdal-dev libpq-dev libproj-dev libprotobuf-dev libqca-qt5-2-dev libqca-qt5-2-plugins libqscintilla2-qt5-dev libqt5opengl5-dev libqt5serialport5-dev libqt5sql5-sqlite libqt5svg5-dev libqt5webkit5-dev libqt5xmlpatterns5-dev libqwt-qt5-dev libspatialindex-dev libspatialite-dev libsqlite3-dev libsqlite3-mod-spatialite libyaml-tiny-perl libzip-dev libzstd-dev lighttpd locales ninja-build ocl-icd-opencl-dev opencl-headers pandoc pdal pkg-config poppler-utils protobuf-compiler pyqt5-dev pyqt5-dev-tools pyqt5.qsci-dev python3-all-dev python3-autopep8 python3-dev python3-gdal python3-jinja2 python3-lxml python3-mock python3-nose2 python3-owslib python3-plotly python3-psycopg2 python3-pygments python3-pyproj python3-pyqt5 python3-pyqt5.qsci python3-pyqt5.qtmultimedia python3-pyqt5.qtpositioning python3-pyqt5.qtserialport python3-pyqt5.qtsql python3-pyqt5.qtsvg python3-pyqt5.qtwebkit python3-pyqtbuild python3-sip python3-termcolor python3-yaml qt3d-assimpsceneimport-plugin qt3d-defaultgeometryloader-plugin qt3d-gltfsceneio-plugin qt3d-scene2d-plugin qt3d5-dev qtbase5-dev qtbase5-private-dev qtkeychain-qt5-dev qtmultimedia5-dev qtpositioning5-dev qttools5-dev qttools5-dev-tools sip-tools spawn-fcgi xauth xfonts-100dpi xfonts-75dpi xfonts-base xfonts-scalable xvfb`` |
| kinetic | ``apt-get install bison build-essential ca-certificates ccache cmake cmake-curses-gui dh-python doxygen expect flex flip gdal-bin git graphviz grass-dev libdraco-dev libexiv2-dev libexpat1-dev libfcgi-dev libgdal-dev libgeos-dev libgsl-dev libpq-dev libproj-dev libprotobuf-dev libqca-qt5-2-dev libqca-qt5-2-plugins libqscintilla2-qt5-dev libqt5opengl5-dev libqt5serialport5-dev libqt5sql5-sqlite libqt5svg5-dev libqt5webkit5-dev libqt5xmlpatterns5-dev libqwt-qt5-dev libspatialindex-dev libspatialite-dev libsqlite3-dev libsqlite3-mod-spatialite libyaml-tiny-perl libzip-dev libzstd-dev lighttpd locales ninja-build ocl-icd-opencl-dev opencl-headers pandoc pkg-config poppler-utils protobuf-compiler pyqt5-dev pyqt5-dev-tools pyqt5.qsci-dev python3-all-dev python3-autopep8 python3-dev python3-gdal python3-jinja2 python3-lxml python3-mock python3-nose2 python3-owslib python3-plotly python3-psycopg2 python3-pygments python3-pyproj python3-pyqt5 python3-pyqt5.qsci python3-pyqt5.qtmultimedia python3-pyqt5.qtpositioning python3-pyqt5.qtserialport python3-pyqt5.qtsql python3-pyqt5.qtsvg python3-pyqt5.qtwebkit python3-pyqtbuild python3-sip python3-termcolor python3-yaml qt3d-assimpsceneimport-plugin qt3d-defaultgeometryloader-plugin qt3d-gltfsceneio-plugin qt3d-scene2d-plugin qt3d5-dev qtbase5-dev qtbase5-private-dev qtkeychain-qt5-dev qtmultimedia5-dev qtpositioning5-dev qttools5-dev qttools5-dev-tools sip-tools spawn-fcgi xauth xfonts-100dpi xfonts-75dpi xfonts-base xfonts-scalable xvfb`` |
| lunar | ``apt-get install bison build-essential ca-certificates ccache cmake cmake-curses-gui dh-python doxygen expect flex flip gdal-bin git graphviz grass-dev libdraco-dev libexiv2-dev libexpat1-dev libfcgi-dev libgdal-dev libgeos-dev libgsl-dev libpq-dev libproj-dev libprotobuf-dev libqca-qt5-2-dev libqca-qt5-2-plugins libqscintilla2-qt5-dev libqt5opengl5-dev libqt5serialport5-dev libqt5sql5-sqlite libqt5svg5-dev libqt5webkit5-dev libqt5xmlpatterns5-dev libqwt-qt5-dev libspatialindex-dev libspatialite-dev libsqlite3-dev libsqlite3-mod-spatialite libyaml-tiny-perl libzip-dev libzstd-dev lighttpd locales ninja-build ocl-icd-opencl-dev opencl-headers pandoc pkg-config poppler-utils protobuf-compiler pyqt5-dev pyqt5-dev-tools pyqt5.qsci-dev python3-all-dev python3-autopep8 python3-dev python3-gdal python3-jinja2 python3-lxml python3-mock python3-nose2 python3-owslib python3-plotly python3-psycopg2 python3-pygments python3-pyproj python3-pyqt5 python3-pyqt5.qsci python3-pyqt5.qtmultimedia python3-pyqt5.qtpositioning python3-pyqt5.qtserialport python3-pyqt5.qtsql python3-pyqt5.qtsvg python3-pyqt5.qtwebkit python3-pyqtbuild python3-sip python3-termcolor python3-yaml qt3d-assimpsceneimport-plugin qt3d-defaultgeometryloader-plugin qt3d-gltfsceneio-plugin qt3d-scene2d-plugin qt3d5-dev qtbase5-dev qtbase5-private-dev qtkeychain-qt5-dev qtmultimedia5-dev qtpositioning5-dev qttools5-dev qttools5-dev-tools sip-tools spawn-fcgi xauth xfonts-100dpi xfonts-75dpi xfonts-base xfonts-scalable xvfb`` |
| mantic | ``apt-get install bison build-essential ca-certificates ccache cmake cmake-curses-gui dh-python doxygen expect flex flip gdal-bin git graphviz grass-dev libdraco-dev libexiv2-dev libexpat1-dev libfcgi-dev libgdal-dev libgeos-dev libgsl-dev libpq-dev libproj-dev libprotobuf-dev libqca-qt5-2-dev libqca-qt5-2-plugins libqscintilla2-qt5-dev libqt5opengl5-dev libqt5serialport5-dev libqt5sql5-sqlite libqt5svg5-dev libqt5webkit5-dev libqt5xmlpatterns5-dev libqwt-qt5-dev libspatialindex-dev libspatialite-dev libsqlite3-dev libsqlite3-mod-spatialite libyaml-tiny-perl libzip-dev libzstd-dev lighttpd locales ninja-build ocl-icd-opencl-dev opencl-headers pandoc pkg-config poppler-utils protobuf-compiler pyqt5-dev pyqt5-dev-tools pyqt5.qsci-dev python3-all-dev python3-autopep8 python3-dev python3-gdal python3-jinja2 python3-lxml python3-mock python3-nose2 python3-owslib python3-plotly python3-psycopg2 python3-pygments python3-pyproj python3-pyqt5 python3-pyqt5.qsci python3-pyqt5.qtmultimedia python3-pyqt5.qtpositioning python3-pyqt5.qtserialport python3-pyqt5.qtsql python3-pyqt5.qtsvg python3-pyqt5.qtwebkit python3-pyqtbuild python3-sip python3-termcolor python3-yaml qt3d-assimpsceneimport-plugin qt3d-defaultgeometryloader-plugin qt3d-gltfsceneio-plugin qt3d-scene2d-plugin qt3d5-dev qtbase5-dev qtbase5-private-dev qtkeychain-qt5-dev qtmultimedia5-dev qtpositioning5-dev qttools5-dev qttools5-dev-tools sip-tools spawn-fcgi xauth xfonts-100dpi xfonts-75dpi xfonts-base xfonts-scalable xvfb`` |
| noble | ``apt-get install bison build-essential ca-certificates ccache cmake cmake-curses-gui dh-python doxygen expect flex flip gdal-bin git graphviz grass-dev libdraco-dev libexiv2-dev libexpat1-dev libfcgi-dev libgdal-dev libgeos-dev libgsl-dev libpq-dev libproj-dev libprotobuf-dev libqca-qt5-2-dev libqca-qt5-2-plugins libqscintilla2-qt5-dev libqt5opengl5-dev libqt5serialport5-dev libqt5sql5-sqlite libqt5svg5-dev libqt5webkit5-dev libqt5xmlpatterns5-dev libqwt-qt5-dev libspatialindex-dev libspatialite-dev libsqlite3-dev libsqlite3-mod-spatialite libyaml-tiny-perl libzip-dev libzstd-dev lighttpd locales ninja-build ocl-icd-opencl-dev opencl-headers pandoc pkg-config poppler-utils protobuf-compiler pyqt5-dev pyqt5-dev-tools pyqt5.qsci-dev python3-all-dev python3-autopep8 python3-dev python3-gdal python3-jinja2 python3-lxml python3-mock python3-nose2 python3-owslib python3-plotly python3-psycopg2 python3-pygments python3-pyproj python3-pyqt5 python3-pyqt5.qsci python3-pyqt5.qtmultimedia python3-pyqt5.qtpositioning python3-pyqt5.qtserialport python3-pyqt5.qtsql python3-pyqt5.qtsvg python3-pyqt5.qtwebkit python3-pyqtbuild python3-sip python3-termcolor python3-yaml qt3d-assimpsceneimport-plugin qt3d-defaultgeometryloader-plugin qt3d-gltfsceneio-plugin qt3d-scene2d-plugin qt3d5-dev qtbase5-dev qtbase5-private-dev qtkeychain-qt5-dev qtmultimedia5-dev qtpositioning5-dev qttools5-dev qttools5-dev-tools sip-tools spawn-fcgi xauth xfonts-100dpi xfonts-75dpi xfonts-base xfonts-scalable xvfb`` |
| sid | ``apt-get install bison build-essential ca-certificates ccache cmake cmake-curses-gui dh-python doxygen expect flex flip gdal-bin git graphviz grass-dev libdraco-dev libexiv2-dev libexpat1-dev libfcgi-dev libgdal-dev libgeos-dev libgsl-dev libpq-dev libproj-dev libprotobuf-dev libqca-qt5-2-dev libqca-qt5-2-plugins libqscintilla2-qt5-dev libqt5opengl5-dev libqt5serialport5-dev libqt5sql5-sqlite libqt5svg5-dev libqt5webkit5-dev libqt5xmlpatterns5-dev libqwt-qt5-dev libspatialindex-dev libspatialite-dev libsqlite3-dev libsqlite3-mod-spatialite libyaml-tiny-perl libzip-dev libzstd-dev lighttpd locales ninja-build ocl-icd-opencl-dev opencl-headers pandoc pkg-config poppler-utils protobuf-compiler pyqt5-dev pyqt5-dev-tools pyqt5.qsci-dev python3-all-dev python3-autopep8 python3-dev python3-gdal python3-jinja2 python3-lxml python3-mock python3-nose2 python3-owslib python3-plotly python3-psycopg2 python3-pygments python3-pyproj python3-pyqt5 python3-pyqt5.qsci python3-pyqt5.qtmultimedia python3-pyqt5.qtpositioning python3-pyqt5.qtserialport python3-pyqt5.qtsql python3-pyqt5.qtsvg python3-pyqt5.qtwebkit python3-pyqtbuild python3-sip python3-termcolor python3-yaml qt3d-assimpsceneimport-plugin qt3d-defaultgeometryloader-plugin qt3d-gltfsceneio-plugin qt3d-scene2d-plugin qt3d5-dev qtbase5-dev qtbase5-private-dev qtkeychain-qt5-dev qtmultimedia5-dev qtpositioning5-dev qttools5-dev qttools5-dev-tools sip-tools spawn-fcgi xauth xfonts-100dpi xfonts-75dpi xfonts-base xfonts-scalable xvfb`` |

(extracted from the control.in file in `debian/`)

 - （从 `debian/` 中的 control.in 文件中提取，需要注意的是上述提到的不同基于debian发行的操作系统构建QGIS所需要的依赖都是在debian/control.in文件中提取得到的。）

See [debian-ubuntu](https://qgis.org/en/site/forusers/alldownloads.html#debian-ubuntu) for
currently supported distributions (plain xenial's GDAL for instance is too old
and we build with GDAL2 from ubuntugis).

 - 请参阅 [debian-ubuntu](https://qgis.org/en/site/forusers/alldownloads.html#debian-ubuntu) 了解当前支持的发行版（例如，普通的 xenial 的 GDAL 太旧了，我们使用 ubuntugis 的 GDAL2 进行构建）。

To build [QGIS server landing page/catalog webapp](https://docs.qgis.org/latest/en/docs/server_manual/catalog.html) additional dependencies are required（要构建 [QGIS 服务器登陆页面/目录 Web 应用程序](https://docs.qgis.org/latest/en/docs/server_manual/catalog.html)，需要其他依赖项）:

* Node.js (current LTS recommended): https://nodejs.org/en/download/
* Yarn Package Manager: https://yarnpkg.com/getting-started/install

Additionally, the cmake flag `WITH_SERVER_LANDINGPAGE_WEBAPP` needs to be turned on.（此外，需要打开 cmake 标志“WITH_SERVER_LANDINGPAGE_WEBAPP”。）

## 3.4. Setup ccache (Optional, but recommended)（配置ccache，虽然是可选的但是推荐进行配置）

You should also setup ccache to speed up compile times（您还应该设置 ccache 来加快编译时间：）:

```bash
//  将当前目录切换到/usr/local/bin中，/usr/local/bin是一个常用于存放用户安装的可执行文件的目录。这些文件通常不由系统的包管理器管理，而是由用户或第三方软件安装。
cd /usr/local/bin
sudo ln -s /usr/bin/ccache gcc
sudo ln -s /usr/bin/ccache g++
```

or simply add `/usr/lib/ccache` to your `PATH`.（或者简单的将/usr/lib/ccache添加到PATH环境变量中）

## 3.5. Prepare your development environment（准备你的开发环境）

As a convention I do all my development work in $HOME/dev/<language>, so in this case we will create a work environment for C++ development work like this（按照惯例，我所有的开发工作都在 $HOME/dev/<language> 中进行，因此在这种情况下，我们将为 C++ 开发工作创建一个工作环境，如下所示：）:

```bash
mkdir -p ${HOME}/dev/cpp
cd ${HOME}/dev/cpp
```

This directory path will be assumed for all instructions that follow.（所有后续指令均假定采用此目录路径。）

## 3.6. Check out the QGIS Source Code（check out QGIS的源码）

There are two ways the source can be checked out. Use the anonymous method
if you do not have edit privileges for the QGIS source repository, or use
the developer checkout if you have permissions to commit source code
changes.

 - 有两种方法可以检出源代码。如果您没有 QGIS 源存储库的编辑权限，请使用匿名方法；如果您有权限提交源代码更改，请使用开发人员检出。

1. Anonymous Checkout（匿名的方式检出）

```bash
//  将当前的目录切换到${HOME}/dev/cpp（在自己的电脑上需要修改）
cd ${HOME}/dev/cpp
//  使用git拷贝QGIS的源代码
git clone https://github.com/qgis/QGIS.git
```

2. Developer Checkout（开发者检出）

```bash
//  将当前的目录切换到${HOME}/dev/cpp（在自己的电脑上需要修改）
cd ${HOME}/dev/cpp
//  使用git拷贝QGIS的源代码
git clone git@github.com:qgis/QGIS.git
```

## 3.7. Starting the compile（开始进行编译）

I compile my development version of QGIS into my ~/apps directory to avoid
conflicts with Ubuntu packages that may be under /usr. This way for example
you can use the binary packages of QGIS on your system along side with your
development version. I suggest you do something similar:

 - 我将 QGIS 的开发版本编译到 ~/apps 目录中，以避免与可能位于 /usr 下的 Ubuntu 软件包发生冲突。这样，例如，您可以在系统上同时使用 QGIS 的二进制软件包和开发版本。我建议您做类似的事情：

```bash
//  创建一个新的目录
mkdir -p ${HOME}/apps
```

Now we create a build directory and run ccmake（现在我们创建一个构建目录并运行 ccmake：）:

```bash
//  进入到QGIS目录中
cd QGIS
//  创建一个名为build-master的目录
mkdir build-master
//  进入到build-master文件中
cd build-master
//  使用ccmake对上一级目录中的CMakeLists.txt文件进行构建
ccmake ..
```

When you run ccmake (note the .. is required!), a menu will appear where you can configure various aspects of the build（当您运行 ccmake 时（请注意 .. 是必需的！），将出现一个菜单，您可以在其中配置构建的各个方面：）:

* If you want QGIS to have debugging capabilities then set `CMAKE_BUILD_TYPE` to `Debug`.（* 如果您希望 QGIS 具有调试功能，则将“CMAKE_BUILD_TYPE”设置为“Debug”。）
* If you do not have root access or do not want to overwrite existing QGIS
  installs (by your package manager for example), set the `CMAKE_INSTALL_PREFIX`
  to somewhere you have write access to (For example `${HOME}/apps`).（如果您没有 root 权限或不想覆盖现有的 QGIS 安装（例如通过您的包管理器），请将“CMAKE_INSTALL_PREFIX”设置为您有写权限的地方（例如“${HOME}/apps”）。）

Now press 'c' to configure, 'e' to dismiss any error messages that may appear. and 'g' to generate the make files. Note that sometimes 'c' needs to be pressed several times before the 'g' option becomes available. After the 'g' generation is complete, press 'q' to exit the ccmake interactive dialog.（现在按“c”进行配置，按“e”忽略可能出现的任何错误消息。按“g”生成 make 文件。请注意，有时需要按“c”几次才能使用“g”选项。“g”生成完成后，按“q”退出 ccmake 交互式对话框。）

**Warning:** Make sure that your build directory is completely empty when you enter the command. Do never try to "re-use" an existing **Qt5** build directory. If you want to use `ccmake` or other interactive tools, run the command in the empty build directory once before starting to use the interactive tools.

 - **警告：**输入命令时，请确保构建目录完全为空。切勿尝试“重复使用”现有的 **Qt5** 构建目录。如果要使用“ccmake”或其他交互式工具，请在开始使用交互式工具之前，在空的构建目录中运行一次命令。

Now on with the build（现在进行构建）:
```bash
//  使用make对产生的makefile进行构建
make -jX
```

where X is the number of available cores. Depending on your platform, this can speed up the build time considerably.（其中 X 是可用核心的数量。根据您的平台，这可以大大加快构建时间。）

Then you can directly run from the build directory（然后你可以直接从构建目录运行）:
```bash
//  运行qgis
./output/bin/qgis
```
Another option is to install to your system:
```bash
//  使用make对qgis进行安装
make install
```

After that you can try to run QGIS:
```bash
//  在对应的目录中运行qgis
$HOME/apps/bin/qgis
```
If all has worked properly the QGIS application should start up and appear on your screen. If you get the error message "error while loading shared libraries", execute this command in your shell.（如果一切正常，QGIS 应用程序应该会启动并出现在您的屏幕上。如果您收到错误消息“加载共享库时出错”，请在您的 shell 中执行此命令。）

```bash
//  更新动态链接器的配置文件
sudo ldconfig
```
If that doesn't help add the install path to LD_LIBRARY_PATH:（如果这没有帮助，请将安装路径添加到 LD_LIBRARY_PATH）
```bash
//  使用export将qgis安装目录写入到LD_LIBRARY_PATH变量中
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${HOME}/apps/lib/
```

Optionally, if you already know what aspects you want in your custom build then you can skip the interactive ccmake .. part by using the cmake -D option for each aspect, e.g.:（或者，如果您已经知道您想要在自定义构建中包含哪些方面，那么您可以跳过交互式 ccmake .. 部分，而是对每个方面使用 cmake -D 选项，例如：）

```bash
cmake -D CMAKE_BUILD_TYPE=Debug -D CMAKE_INSTALL_PREFIX=${HOME}/apps ..
```
Also, if you want to speed your build times, you can easily do it with ninja, an alternative to make with similar build options.（此外，如果您想加快构建时间，您可以轻松地使用 ninja 来完成，它是具有类似构建选项的替代方案。）

For example, to configure your build you can do either one of:
```bash
ccmake -G Ninja ..
cmake -G Ninja -D CMAKE_BUILD_TYPE=Debug -D CMAKE_INSTALL_PREFIX=${HOME}/apps ..
```
Build and install with ninja:
```bash
ninja   # (uses all cores by default; also supports the above described -jX option)
ninja install
```
To build even faster, you can build just the targets you need using, for example:
```bash
ninja qgis
ninja pycore
# if it's on desktop related code only:
ninja qgis_desktop
```

### 3.7.1 Available compilation flags（可用的编译flags）

QGIS build is tunable according to your needs. Many flags are available to activate or deactivate some functionalities, here is a subset:（QGIS 构建可根据您的需要进行调整。有许多标志可用于激活或停用某些功能，以下是其中的一部分：）

* `BUILD_WITH_QT6`: Enable (experimental) Qt6 support
* `WITH_3D`: Determines whether QGIS 3D library should be built
* `WITH_ANALYSIS`: Determines whether QGIS analysis library should be built
* `WITH_AUTH`: Determines whether QGIS authentication methods should be built
* `WITH_BINDINGS`: Determines whether Python bindings should be built
* `WITH_COPC`: Determines whether Cloud Optimized Point Cloud (COPC) support should be built
* `WITH_DESKTOP`: Determines whether QGIS desktop should be built
* `WITH_EPT`: Determines whether Entwine Point Cloud (EPT) support should be built
* `WITH_GRASS${GRASS_CACHE_VERSION}`: Determines whether GRASS ${GRASS_SEARCH_VERSION} plugin should be built
* `WITH_GUI`: Determines whether QGIS GUI library (and everything built on top of it) should be built
* `WITH_HANA`: Determines whether SAP HANA Spatial support should be built
* `WITH_ORACLE`: Determines whether Oracle support should be built
* `WITH_PDAL`: Determines whether PDAL support should be built
* `WITH_POSTGRESQL`: Determines whether POSTGRESQL support should be built
* `WITH_QGIS_PROCESS`: Determines whether the standalone \"qgis_process\" tool should be built
* `WITH_QSPATIALITE`: Determines whether QSPATIALITE sql driver should be built
* `WITH_SERVER`: Determines whether QGIS server should be built
* `WITH_SPATIALITE`: Determines whether Spatialite support should be built (required for spatialite, virtual, wfs providers)

A complete list can been extracted from the source code with the following command line:（可以使用以下命令行从源代码中提取完整列表：）

```bash
cmake .. -N -LH | grep -B1 WITH_
```

## 3.8. Compiling with 3D（编译3D）

In the cmake, you need to enable（在 cmake 中，需要启用）:
```bash
WITH_3D=True
```

### 3.8.1. Compiling with 3D on old Debian based distributions（在旧的基于 Debian 的发行版上使用 3D 进行编译）

QGIS 3D requires Qt53DExtras. These headers have been removed
from Qt upstream on Debian Buster and Ubuntu focal (20.04) based distributions.
A copy has been made in the QGIS repository in `external/qt3dextra-headers`.
To compile with 3D enabled on these distributions, you need to add some cmake options:

 - QGIS 3D 需要 Qt53DExtras。这些标头已从基于 Debian Buster 和 Ubuntu focal (20.04) 的发行版上的 Qt 上游中删除。已在 QGIS 存储库中的“external/qt3dextra-headers”中创建副本。要在这些发行版上启用 3D 进行编译，您需要添加一些 cmake 选项：

```bash
CMAKE_PREFIX_PATH={path to QGIS Git repo}/external/qt3dextra-headers/cmake
QT5_3DEXTRA_INCLUDE_DIR={path to QGIS Git repo}/external/qt3dextra-headers
QT5_3DEXTRA_LIBRARY=/usr/lib/x86_64-linux-gnu/libQt53DExtras.so
Qt53DExtras_DIR={path to QGIS Git repo}/external/qt3dextra-headers/cmake/Qt53DExtras
```

Above instructions do not apply to newer versions of Debian and Ubuntu.（上述说明不适用于较新版本的 Debian 和 Ubuntu。）

## 3.9. Building different branches

By using `git worktree`, you can switch between different branches to use several sources in parallel, based on the same Git configuration. We recommend you to read the documentation about this Git command:（通过使用“git worktree”，您可以在不同的分支之间切换，以基于相同的 Git 配置并行使用多个源。我们建议您阅读有关此 Git 命令的文档：）

```bash
git commit
git worktree add ../my_new_functionality
cd ../my_new_functionality
git fetch qgis/master
git rebase -i qgis/master
# only keep the commits to be pushed
git push -u my_own_repo my_new_functionality
```

## 3.10. Building Debian packages（构建 Debian 软件包）

Instead of creating a personal installation as in the previous step you can also create debian package. This is done from the QGIS root directory, where you'll find a debian directory.（除了像上一步一样创建个人安装外，您还可以创建 debian 包。这可以从 QGIS 根目录完成，您将在其中找到一个 debian 目录。）

First setup a changelog entry for your distribution. For example for Debian Bookworm:（首先为你的发行版设置一个更新日志条目。例如对于 Debian Bookworm：）

```bash
dch -l ~bookworm --force-distribution --distribution bookworm "bookworm build"
```

You also need to install the [build dependencies](#33-install-build-dependencies).
Alternatively use:

```bash
debian/rules templates
sudo mk-build-deps -i
```

The QGIS packages will be created with:

```bash
dpkg-buildpackage -us -uc -b
```

**Note:** Install `devscripts` to get `dch` and `mk-build-deps`.

**Note:** If you have `libqgis1-dev` installed, you need to remove it first
using `dpkg -r libqgis1-dev`.  Otherwise `dpkg-buildpackage` will complain about a
build conflict.

**Note:** By default tests are run in the process of building and their
results are uploaded to https://cdash.orfeo-toolbox.org/index.php?project=QGIS.
You can turn the tests off using `DEB_BUILD_OPTIONS=nocheck` in front of the
build command. The upload of results can be avoided with `DEB_TEST_TARGET=test`.

The packages are created in the parent directory (ie. one level up).
Install them using `dpkg`.  E.g.:

```bash
sudo debi
```

### 3.10.1. Building packages with Oracle support

To build packages with Oracle support you need the Oracle libraries (currently
21.11) as additional build dependencies:

```bash
curl -JLO https://download.oracle.com/otn_software/linux/instantclient/2111000/oracle-instantclient-devel-21.11.0.0.0-1.el8.x86_64.rpm
curl -JLO https://download.oracle.com/otn_software/linux/instantclient/2111000/oracle-instantclient-basiclite-21.11.0.0.0-1.el8.x86_64.rpm
sudo apt install alien
fakeroot alien oracle-instantclient-devel-21.11.0.0.0-1.el8.x86_64.rpm oracle-instantclient-basiclite-21.11.0.0.0-1.el8.x86_64.rpm
sudo dpkg -i oracle-instantclient-devel_21.11.0.0.0-2_amd64.deb oracle-instantclient-basiclite_21.11.0.0.0-2_amd64.deb
```

(if the client version changes it's necessary to adapt `ORACLE_INCLUDEDIR` and `ORACLE_LIBDIR` in `debian/rules` accordingly)

The packaging files enable Oracle support if the distribution contains `-oracle`:

```bash
dch -l ~sid~oracle --force-distribution --distribution sid-oracle "sid build with oracle"                                                                                                                      │
dpkg-buildpackage -us -uc -b
```

## 3.11. On Fedora Linux

We assume that you have the source code of QGIS ready and created a new subdirectory called `build` or `build-qt5` in it.（我们假设您已经准备好 QGIS 的源代码并在其中创建了一个名为“build”或“build-qt5”的新子目录。）

### 3.11.1. Install build dependencies

|Distribution|Install command for packages|
|------------|----------------------------|
| Fedora 38 Workstation | ``dnf install qt5-qtbase-private-devel qt5-qtwebkit-devel qt5-qtlocation-devel qt5-qtmultimedia-devel qt5-qttools-static qca-qt5-devel qca-qt5-ossl qt5-qt3d-devel python3-qt5-devel python3-qscintilla-qt5-devel qscintilla-qt5-devel python3-qscintilla-qt5 clang flex bison geos-devel gdal  gdal-devel hdf5-devel sqlite-devel libspatialite-devel qt5-qtsvg-devel spatialindex-devel expat-devel netcdf-devel proj-devel qwt-qt5-devel gsl-devel PDAL PDAL-devel postgresql-devel cmake python3-gdal gdal-python-tools python3-psycopg2 python3-PyYAML python3-pygments python3-jinja2 python3-OWSLib qca-qt5-ossl qwt-qt5-devel qtkeychain-qt5-devel libzip-devel exiv2-devel  PyQt-builder protobuf-lite protobuf-lite-devel libzstd-devel qt5-qtserialport-devel draco-devel python3-devel`` |
| Fedora 37 Workstation | ``dnf install qt5-qtbase-private-devel qt5-qtwebkit-devel qt5-qtlocation-devel qt5-qtmultimedia-devel qt5-qttools-static qca-qt5-devel qca-qt5-ossl qt5-qt3d-devel python3-qt5-devel python3-qscintilla-qt5-devel qscintilla-qt5-devel python3-qscintilla-qt5 clang flex bison geos-devel gdal  gdal-devel hdf5-devel sqlite-devel libspatialite-devel qt5-qtsvg-devel spatialindex-devel expat-devel netcdf-devel proj-devel qwt-qt5-devel gsl-devel PDAL PDAL-devel postgresql-devel cmake python3-gdal gdal-python-tools python3-psycopg2 python3-PyYAML python3-pygments python3-jinja2 python3-OWSLib qca-qt5-ossl qwt-qt5-devel qtkeychain-qt5-devel qwt-devel libzip-devel exiv2-devel python3-sip-devel protobuf-lite protobuf-lite-devel libzstd-devel qt5-qtserialport-devel draco-devel`` |
| Fedora 35/36 Workstation | ``dnf install qt5-qtbase-private-devel qt5-qtwebkit-devel qt5-qtlocation-devel qt5-qtmultimedia-devel qt5-qttools-static qca-qt5-devel qca-qt5-ossl qt5-qt3d-devel python3-qt5-devel python3-qscintilla-qt5-devel qscintilla-qt5-devel python3-qscintilla-qt5 clang flex bison geos-devel gdal gdal-devel hdf5-devel sqlite-devel libspatialite-devel qt5-qtsvg-devel spatialindex-devel expat-devel netcdf-devel proj-devel qwt-qt5-devel gsl-devel PDAL PDAL-devel postgresql-devel cmake gdal-python3 gdal-python-tools python3-psycopg2 python3-PyYAML python3-pygments python3-jinja2 python3-OWSLib qca-qt5-ossl qwt-qt5-devel qtkeychain-qt5-devel qwt-devel libzip-devel exiv2-devel python3-sip-devel protobuf-lite protobuf-lite-devel libzstd-devel qt5-qtserialport-devel draco-devel`` |
| older versions | ``dnf install qt5-qtbase-private-devel qt5-qtwebkit-devel qt5-qtlocation-devel qt5-qtmultimedia-devel qt5-qttools-static qca-qt5-devel qca-qt5-ossl qt5-qt3d-devel python3-qt5-devel python3-qscintilla-qt5-devel qscintilla-qt5-devel python3-qscintilla-devel python3-qscintilla-qt5 clang flex bison geos-devel gdal gdal-devel sqlite-devel libspatialite-devel qt5-qtsvg-devel qt5-qtbase-tds qt5-qtbase-odbc spatialindex-devel expat-devel proj-devel qwt-qt5-devel gsl-devel postgresql-devel cmake gdal-python3 python3-psycopg2 python3-PyYAML python3-pygments python3-jinja2 python3-OWSLib qca-qt5-ossl qwt-qt5-devel qtkeychain-qt5-devel qwt-devel sip-devel libzip-devel exiv2-devel draco-devel`` |

To build QGIS server additional dependencies are required:

```bash
dnf install fcgi-devel
```

And for building [QGIS server landing page/catalog webapp](https://docs.qgis.org/latest/en/docs/server_manual/services.html#qgis-server-catalog):

```bash
dnf install nodejs yarnpkg
```

Additionally, the cmake flag `WITH_SERVER_LANDINGPAGE_WEBAPP` needs to be turned on.

Make sure that your build directory is completely empty when you enter the
following command. Do never try to "re-use" an existing Qt5 build directory.
If you want to use `ccmake` or other interactive tools, run the following
command in the empty build directory once before starting to use the interactive
tools.

```bash
cmake ..
```

If everything went OK you can finally start to compile. (As usual append a `-jX`
where X is the number of available cores option to make to speed up your build
process)

```bash
make
```

Run from the build directory

```bash
./output/bin/qgis
```

Or install to your system

```bash
make install
```

### 3.11.2. Suggested system tweaks

By default Fedora disables debugging calls from Qt applications. This prevents
the useful debug output which is normally printed when running the unit tests.

To enable debug prints for the current user, execute:

```bash
cat > ~/.config/QtProject/qtlogging.ini << EOL
[Rules]
default.debug=true
EOL
```

### 3.11.3. Additional tools for QGIS development

If you're going to be developing QGIS on a Fedora system, the following extra packages
are required for the various QGIS source formatting and preparation scripts.

```bash
dnf install ag ccache expect ninja-build astyle python3-autopep8 python3-mock python3-nose2 perl-YAML-Tiny
```

### 3.11.4. QT6 experimental builds with Fedora Rawhide

This requires latest QGIS master (>= January 25th 2024).

To save ~ 700 MB, if you want to install PROJ without its optional grids, first do:

```bash
dnf5 install -y --setopt=install_weak_deps=False proj-devel
```

Installation of all needed build dependencies:

|Distribution|Install command for packages|
|------------|----------------------------|
| Fedora 40 Workstation | ``dnf install qt6-qtbase-private-devel qt6-qtlocation-devel qt6-qtmultimedia-devel qt6-qttools-static qca-qt6-devel qca-qt6-ossl qt6-qt3d-devel qt6-qtwebengine-devel python3-pyqt6-devel python3-qscintilla-qt6-devel qscintilla-qt6-devel python3-qscintilla-qt6 clang flex bison geos-devel gdal gdal-devel hdf5-devel sqlite-devel libspatialite-devel qt6-qtsvg-devel spatialindex-devel expat-devel netcdf-devel proj-devel qwt-qt6-devel gsl-devel PDAL PDAL-devel postgresql-devel cmake python3-gdal gdal-python-tools python3-psycopg2 python3-PyYAML python3-pygments python3-jinja2 python3-OWSLib qca-qt6-ossl qwt-qt6-devel qtkeychain-qt6-devel libzip-devel exiv2-devel PyQt-builder protobuf-lite protobuf-lite-devel libzstd-devel qt6-qtserialport-devel draco-devel python3-devel qt6-qt5compat-devel python3-pyqt6-webengine`` |
| Fedora 39 Workstation | ``dnf install qt6-qtbase-private-devel qt6-qtwebkit-devel qt6-qtlocation-devel qt6-qtmultimedia-devel qt6-qttools-static qca-qt6-devel qt6-qtwebengine-devel qca-qt6-ossl qt6-qt3d-devel python3-qt6-devel python3-qscintilla-qt6-devel qscintilla-qt6-devel python3-qscintilla-qt6 clang flex bison geos-devel gdal gdal-devel hdf5-devel sqlite-devel libspatialite-devel qt6-qtsvg-devel spatialindex-devel expat-devel netcdf-devel proj-devel qwt-qt6-devel gsl-devel PDAL PDAL-devel postgresql-devel cmake python3-gdal gdal-python-tools python3-psycopg2 python3-PyYAML python3-pygments python3-jinja2 python3-OWSLib qca-qt6-ossl qwt-qt6-devel qtkeychain-qt6-devel libzip-devel exiv2-devel PyQt-builder protobuf-lite protobuf-lite-devel libzstd-devel qt6-qtserialport-devel draco-devel python3-devel qt6-qt5compat-devel`` |


To build,

```bash
cmake .. -DBUILD_WITH_QT6=ON -DWITH_QTWEBKIT=OFF -DWITH_QTWEBENGINE=ON
```

# 4. Building on Windows

## 4.1. Building with Microsoft Visual Studio

This section describes how to build QGIS using Visual Studio (MSVC) 2019 on Windows.
The official Windows packages are built using OSGeo4W.

This section describes the setup required to allow Visual Studio to be used to
build QGIS.

### 4.1.1. Visual Studio 2022 Community Edition

Download and install the [free (as in free beer) Community installer](https://download.visualstudio.microsoft.com/download/pr/68d6b204-9df0-4fcc-abcc-08ee0eff9cb2/b029547488a9383b0c8d8a9c813e246feb3ec19e0fe55020d4878fde5f0983fe/vs_Community.exe)

Select "Desktop Development with C++"

### 4.1.2. Other tools and dependencies

Download and install following packages:

* [CMake](https://cmake.org/files/v3.12/cmake-3.12.3-win64-x64.msi)
* GNU flex, GNU bison and GIT with [cygwin 64bit](https://cygwin.com/setup-x86_64.exe)
* [OSGeo4W](https://download.osgeo.org/osgeo4w/v2/osgeo4w-setup.exe)
* [ninja](https://github.com/ninja-build/ninja/releases/download/v1.7.2/ninja-win.zip): Copy the `ninja.exe` to `C:\OSGeo4W\bin\`

For the QGIS build you need to install following packages from cygwin:

* bison
* flex
* git (even if you already have Git for Windows installed)

and from OSGeo4W (select *Advanced Install*):

* qgis-dev-deps

  * This will also select packages the above packages depend on.

  * Note: If you install other packages, this might cause issues. Particularly, make sure
    **not** to install the msinttypes package. It installs a stdint.h file in
    `OSGeo4W\include`, that conflicts with Visual Studio own stdint.h, which for
    example breaks the build of the virtual layer provider.

If you intend to also build all the dependencies, you can refer to [the OSGeo4W repository](https://github.com/jef-n/OSGeo4W).

### 4.1.3. Clone the QGIS Source Code

Choose a directory to store the QGIS source code.
For example, to put it in the OSGeo4W install, navigate there:

```cmd
cd C:\OSGeo4W
```

This directory will be assumed for all instructions
that follow.

On the command prompt clone the QGIS source from
git to the source directory `QGIS`:

```cmd
git clone git://github.com/qgis/QGIS.git
```

This requires Git. If you have Git for Windows on your PATH already,
you can do this from a normal command prompt. If you do not, you can
use the Git package that was installed as part of Cygwin by opening
a Cygwin[64] Terminal

And, to avoid Git in Windows reporting changes to files not actually modified:

```cmd
cd QGIS
git config core.filemode false
```

### 4.1.4. OSGeo4W

The official Windows packages are made in OSGeo4W.  Also standalone MSI
installers are also produced using the packages from OSGeo4W.

The nightly build of master (package qgis-dev) is made with the [OSGeo4W build
recipe](https://github.com/jef-n/OSGeo4W/blob/master/src/qgis-dev/osgeo4w/package.sh).

There are others for the long-term release build (qgis-ltr), the nightly of the
next long-term point release (qgis-ltr-dev), the latest release (qgis), the
nightly build of the next point release (qgis-rel-dev).  All of those are
currently based on Qt5.  A build recipe for master with Qt6 is available as
qgis-qt6-dev.

To setup a build environment (including Visual C++ if not already installed) and to
build the nightly run in a command line (cmd):

```cmd
mkdir osgeo4w-build
cd osgeo4w-build
curl -JLO https://raw.githubusercontent.com/jef-n/OSGeo4W/refs/heads/master/bootstrap.cmd
curl -JLO https://raw.githubusercontent.com/jef-n/OSGeo4W/refs/heads/master/bootstrap.sh
bootstrap.cmd qgis-dev
```

To build other packages you can simply replace qgis-dev with other packages.
Calling `bootstrap.cmd` without any arguments will build all packages
(including all dependencies).

After the build is succeeded the OSGeo4W packages will be in `x86_64/`.

To install the package you can add the `osgeo4w-build` directory as `User URL`
in the OSGeo4W installer and select your `qgis-dev` package.

The MSIs are created with `scripts\msis.sh` (actually `scripts/creatmsi.pl`).


## 4.2. Building on Linux with mingw64

With this approach you can cross build a Windows binary on Linux using mingw64
in a Docker container.

To build on Linux from your QGIS sources directory, launch:

```cmd
ms-windows/mingw/build.sh
```

After a successful build, you will find two packages in the QGIS sources
directory:

- qgis-portable-win64.zip (QGIS for Windows 64bit)
- qgis-portable-win64-debugsym.zip (debug symbols)

This method is also used in the continuous integrations process. After each pull
request the two packages mentioned above are stored as GitHub actions artifacts
and are available for download making it possible to quickly test changes on Windows.

### 4.2.1. Building with Docker

This is the simplest way, but you need to have Docker installed
on your system.

You can use a Docker image to cross build QGIS by calling
the script ms-windows/mxe/build.sh from the root directory of QGIS repository.

=== Building without Docker ====

This requires to install mxe toolchain on your system and build
all dependencies by yourself.

#### 4.2.1.1. Initial setup

Please follow the instructions on mxe website to setup your building toolchain http://mxe.cc/,
take note of the path where you have installed mxe.

#### 4.2.1.2. Building the dependencies

Please see README.md under ms-windows/mxe for detailed instructions and for the
list of dependencies that need to be built in mxe before attempting to build QGIS.

#### 4.2.1.3. Cross-Building QGIS

Edit the build-mxe.sh script and optionally adjust the path where your mxe installation is located, you
can also change the build and release directories.

### 4.2.2. Testing QGIS

Copy and unzip on the Windows machine package produced by the build and launch the qgis binary: no installation
is required.

## 4.3 Building on Windows with vcpkg

Vcpkg is a free and open source cross platform ecosystem for libraries.
It provides precise control over the versions of dependencies that are used.

### 4.3.1 Install Build Tools

1. Download and install the free (as in free beer) [Microsoft Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&channel=Release&version=VS2022).
2. Obtain [winflexbison](https://github.com/lexxmark/winflexbison/releases).

### 4.3.2 Build QGIS

There are two ways, to build with vcpkg. Either you download an sdk, which allows you
to reuse dependencies that are already built. This is generally easier and recommended.
It is also possible to build all dependencies yourself, which gives you more control
over the dependencies.

#### 4.3.2.1 Build with an SDK

1. Download and unzip SDK. Get the latest [QGIS master SDK](https://nightly.link/qgis/QGIS/workflows/windows-qt6/master/qgis-sdk-x64-windows.zip).
2. Build

We will now configure QGIS.

Open a _Developer PowerShell for VS 2022_

```ps
# We assume you have a copy of the QGIS source code available
# and have changed the working directory into it

# Configure
cmake -S . `
      -B build `
      -DSDK_PATH="path/to/vcpkg-export-[date]" `
      -DBUILD_WITH_QT6=ON `
      -DWITH_QTWEBKIT=OFF `
      -DVCPKG_TARGET_TRIPLET=x64-windows-release `
      -DFLEX_EXECUTABLE="path/to/flex-executable" `
      -DBISON_EXECUTABLE="path/to/bison-executable"
```

This will provide you with a configured project. You can either build it directly
from the command line.

```ps
# Build
# Note: you can also use RelWithDebInfo to have debug symbols in your build
cmake --build build --config Release
```

Or you can open the generated `.sln` file in the `build` folder with Visual Studio.
This will allow you to use all the tools that this IDE has to offer.

#### 4.3.2.1 Build all the dependencies locally

It is also possible to build all the dependencies locally.
This will require some time, cpu and disk space.

```ps
# We assume you have a copy of the QGIS source code available
# and have changed the working directory into it

# Configure
cmake -S . `
      -B build `
      -D WITH_VCPKG=ON `
      -D BUILD_WITH_QT6=ON `
      -D WITH_QTWEBKIT=OFF `
      -D VCPKG_TARGET_TRIPLET=x64-windows-release `
      -D VCPKG_HOST_TRIPLET=x64-windows-release
```

**Manage dependency versions**

The dependencies are defined in the file `vcpkg/vcpkg.json`.
This file defines which versions of registries are used.
To update to the most recent version, you can use the command `vcpkg x-update-baseline --x-manifest-root=vcpkg`.
If you want to patch or update a specific dependency, you can copy the port for this
dependency into the folder `vcpkg/ports`. Whenever the build is reconfigured, it will check for dependencies that need to be rebuilt.


# 5. Building on MacOS X

If you want to test QGIS, easiest option is to download and install all-in-one self-containing bundle directly from

https://qgis.org/downloads/macos

On the other hand, if you want to build or develop QGIS on your own, you need a set of dependencies and tools.
These instructions will use the same set of dependencies that are used for all-in-one QGIS bundle,
but you can build QGIS with Homebrew, MacPorts or Conda dependencies too.

https://github.com/qgis/QGIS-Mac-Packager

Included are notes for building on latest Mac OS X with latest updates installed.
The build uses clang compiler.

Parallel Compilation: On multiprocessor/multicore Macs, it's possible to
speed up compilation, but it's not automatic.  Whenever you type "make" (but
NOT "make install"), instead type:

```bash
make -j [#cpus]
```

Replace [#cpus] with the number of cores and/or processors your Mac has.
To find out how many CPUs you have available, run the following in Terminal:

```bash
/usr/sbin/sysctl -n hw.ncpu
```

## 5.1. Install Developer Tools

Developer tools are not a part of a standard OS X installation.
As minimum you require command line tools

```bash
sudo xcode-select --install
```

but installation of Xcode from the App Store is recommended too.

## 5.2. Install CMake and other build tools

For example install Homebrew

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
```

and these development/build tools

```bash
brew install git cmake ninja pkg-config wget bash-completion curl gnu-sed coreutils ccache libtool astyle help2man autoconf automake pandoc
```

if you have these tools installed from MacPorts or Conda, it is the same, but we will need to be able to
run `cmake` and others from Terminal in the following steps

## 5.3. Install Qt5 and QGIS-Deps

To build QGIS, we need Qt5 and FOSS dependencies on hand. The Qt5 version ideally should match the version that was
used to build dependency package.

Download the latest QGIS-Deps install script, qt package and QGIS-Deps packages from

https://qgis.org/downloads/macos/deps

You should have one bash script and two tar archive in your download folder.
Run the install script to install Qt and QGIS-Deps to `/opt/` area. You need
root privileges or have write access to `/opt/Qt` and `/opt/QGIS`.

Alternatively you can download and install Qt Open Source for MacOS from the

https://www.qt.io/

in the same version as referenced in the install script. It must be installed in `/opt/Qt`

Note that the QGIS-Deps package is not yet signed, so you may need to add Terminal
to System Preferences -> Security & Privacy -> Privacy -> Developer Tools or manually accept usage
of the libraries when asked by system.

## 5.4. QGIS source

Unzip the QGIS source to a working folder of your choice.
If you are reading this from the source, you've already done this.

If you want to experiment with the latest development sources, go to the github
QGIS project page:

http://github.com/qgis/QGIS

It should default to the master branch.  Click the Downloads button and
select Download .tar.gz. Double-click the tarball to unzip it.

*Alternatively*, use git and clone the repository by
```bash
git clone git://github.com/qgis/QGIS.git
```

## 5.5. Configure the build

CMake supports out of source build so we will create a 'build' dir for the
build process. OS X uses `${HOME}/Applications` as a standard user app folder (it
gives it the system app folder icon).  If you have the correct permissions you
may want to build straight into your `/Applications` folder. The instructions
below assume you are building into a `${HOME}/Applications` directory.

In a Terminal cd to the qgis source folder previously downloaded, then:

```bash
cd ..
mkdir build
cd build

QGIS_DEPS_VERSION=0.9;\
QT_VERSION=5.15.2;\
PATH=/opt/QGIS/qgis-deps-${QGIS_DEPS_VERSION}/stage/bin:$PATH;\
cmake \
  -D CMAKE_INSTALL_PREFIX=~/Applications \
  -D CMAKE_BUILD_TYPE=Release \
  -D QGIS_MAC_DEPS_DIR=/opt/QGIS/qgis-deps-${QGIS_DEPS_VERSION}/stage \
  -D CMAKE_PREFIX_PATH=/opt/Qt/${QT_VERSION}/clang_64 \
  ../QGIS
```

Note: Don't forget the `../QGIS` on the last line, which tells CMake to look for the source files.

Note: Double check on the screen output that all libraries are picked from QGIS-Deps `/opt/QGIS`
and not from system `/usr/lib` or Homebrew's `/usr/local/` or system Frameworks `/Library/Frameworks/`.
Especially check Proj, GDAL, sqlite3 and Python paths.

After the initial Terminal configure, you can use ccmake to make further changes:

```bash
cd build
ccmake ../QGIS
```

## 5.6. Building

Now we can start the build process (remember the parallel compilation note at
the beginning, this is a good place to use it, if you can):

```bash
make -j [#cpus]
```

Now you can run the QGIS from build directory by `./output/bin/QGIS.app/Contents/MacOS/QGIS`

If all built without errors you can then install it:

```bash
make install
```

or, for an /Applications build:

```bash
sudo make install
```

For running the installed QGIS, you need to keep the dependencies in `/opt/` folder in place.
If you want to create bundle that runs without these dependencies, please read the documentation in project

https://github.com/qgis/QGIS-Mac-Packager

# 6. Setting up the WCS test server on GNU/Linux

**Requires:** Ubuntu / Debian derived distro

These notes are for Ubuntu - other versions and Debian derived distros may
require slight variations in package names.

## 6.1. Preparation

Note the git repo below will change to the default QGIS repo once this work
is integrated into master.

```bash
git remote add blazek git://github.com/blazek/Quantum-GIS.git
git fetch blazek
git branch --track wcs2 blazek/wcs2
git checkout wcs2
cd /var/www/
sudo mkdir wcs
sudo chown timlinux wcs
cd wcs/
mkdir cgi-bin
cd cgi-bin/
```

## 6.2. Setup mapserver

```bash
sudo apt-get install cgi-mapserver
```

Set the contents of `/var/www/wcs/cgi-bin/wcstest-1.9.0` to:

```bash
#! /bin/sh
MS_MAPFILE=/var/www/wcs/testdata/qgis-1.9.0/raster/wcs.map
export MS_MAPFILE
/usr/lib/cgi-bin/mapserv
```

Then do:

```bash
chmod +x var/www/wcs/cgi-bin/wcstest-1.9.0
mkdir -p /var/www/wcs/testdata/qgis-1.9.0/raster/
cd /var/www/wcs/testdata/qgis-1.9.0/raster/
cp -r /home/timlinux/QGIS/tests/testdata/raster/* .
```

Edit `/var/www/wcs/testdata/qgis-1.9.0/raster/wcs.map` and set the shapepath to this:

```bash
SHAPEPATH "/var/www/wcs/testdata/qgis-1.9.0/raster"
```

Then create `/var/www/wcs/7-wcs.example.com.conf` setting the contents to this:

```bash
<VirtualHost*:80>
ServerName wcs.example.com
ServerAdmin wcs-admin@example.com

LogLevel warn
LogFormat "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\" \"%{forensic-id}n\"" combined
CustomLog /var/log/apache2/wcs_example.com/access.log combined
ErrorLog /var/log/apache2/wcs_example.com/error.log

DocumentRoot /var/www/wcs/html

ScriptAlias /cgi-bin/ /var/www/wcs/cgi-bin/
<Directory "/var/www/wcs/cgi-bin">
	AllowOverride None
	Options +ExecCGI -MultiViews +SymLinksIfOwnerMatch
	Order allow,deny
	Allow from all
</Directory>

RewriteEngine on
RewriteRule /1.9.0/wcs /cgi-bin/wcstest-1.9.0 [PT]

</VirtualHost>
```


## 6.3. Create a home page

```bash
mkdir html
vim html/index.html
```

Set the contents to:
```
This is the test platform for QGIS' wcs client. You can use these services
from QGIS directly (to try out WCS for example) by pointing your QGIS to:
http://wcs.example.com/1.9.0/wcs
```

## 6.4. Now deploy it

```bash
sudo mkdir /var/log/apache2/wcs_example.com
sudo chown www-data /var/log/apache2/wcs_example.com
cd /etc/apache2/sites-available/
sudo ln -s /var/www/wcs/7-wcs.example.com.conf .
cd /var/www/wcs/
sudo a2ensite 7-wcs.example.com.conf
sudo /etc/init.d/apache2 reload
```

## 6.5. Debugging

```bash
sudo tail -f /var/log/apache2/wcs_example.com/error.log
```

# 7. Setting up a Jenkins Build Server

**Assumption:** You know how to make a working build environment and want to
deploy it under Jenkins for continuous integration testing now.

These notes are terse, I will expand on them later as the need arises. The
procedure is:

* Install Jenkins and get it configured according to your own preferences
* Make sure you have the git, github, junit etc plugins installed. A complete
list of the plugins I have installed follows (note that you almost certainly
don't need evey plugin listed here):
  * External Monitor Job Type Plugin
  * LDAP Plugin
  * pam-auth
  * javadoc
  * ant
  * Jenkins Subversion Plug-in
  * Git Plugin
  * Maven 2 Project Plugin
  * Jenkins SLOCCount Plug-in
  * Jenkins Sounds plugin
  * Jenkins Translation Assistance plugin
  * ruby-runtime
  * Jenkins CVS Plug-in
  * Coverage/Complexity Scatter Plot PlugIn
  * Status Monitor Plugin
  * Git Parameter Plug-In
  * github-api
  * GitHub plugin
  * Jenkins Violations plugin
  * git-notes Plugin
  * Twitter plugin
  * Jenkins Cobertura Plugin
  * Jenkins Gravatar plugin
  * Jenkins SSH Slaves plugin
* Create a Job called 'QGIS'
* Use the following options for your job:
  * Job Name: QGIS
  * Job Type: Build a free-style software project
  * Tick enable project based security (you need to elsewhere configure your
Jenkins security to per project settings)
  * Allow Anonymous user Read and Discover access
  * Set the github project to https://github.com/qgis/QGIS/
  * Set source code management to Git
  * Set repository url to git://github.com/qgis/QGIS.git
  * In advanced repository url settings set refspec to `+refs/heads/master:refs/remotes/origin/master`
  * Set branch to build to master
  * Repository Browser: Auto
  * Build triggers: set to Poll SCM and set schedule to `*****` (polls every minute)
  * Build - Execute shell and set shell script to:

```bash
cd build
cmake ..
xvfb-run --auto-servernum --server-num=1 \
  --server-args="-screen 0 1024x768x24" \
  make Experimental || true
if [ -f Testing/TAG ] ; then
  xsltproc ../tests/ctest2junix.xsl \
    Testing/`head -n 1 < Testing/TAG`/Test.xml > \
    CTestResults.xml
fi
```

  * Add Junit post build action and set 'Publish Junit test result report' to:
`build/CTestResults.xml`
  * Email notification: Send separate e-mails to individuals who broke the build
  * Jenkins sounds - set up sounds for Failure, Success and Unstable.
  * Save

Now open the Job dash board and push something to QGIS and wait a minute to
validate automated builds work.

**Note:** You will need to log in to the Jenkins user account and go to
/var/lib/jenkins/jobs/QGIS/workspace, then make a `build` directory and run
the initial cmake setup and then do test build. This process is the same as
described elsewhere in this doc.

I based some of the set up from this nice blog article here:

http://alexott.blogspot.com/2012/03/jenkins-cmakectest.html

# 8. Debug output and running tests

If you are interested in seeing embedded debug output, change the following
CMake option:

```bash
-D CMAKE_BUILD_TYPE=DEBUG  # (or RELWITHDEBINFO)
```

This will flood your terminal or system log with lots of useful output from
QgsDebugMsg() calls in source code.

If you would like to run the test suite, you will need to do so from the build
directory, as it will not work with the installed/bundled app. First set the
CMake option to enable tests:

```bash
-D ENABLE_TESTS=TRUE
```

Then run all tests from build directory:

```bash
cd build
make test
```

To run all tests and report to http://cdash.orfeo-toolbox.org/index.php?project=QGIS

```bash
cd build
make Experimental
```

You can define the host name reported via 'make Experimental' by setting a CMake
option:

```bash
-D SITE="my.domain.org"
```

To run specific test(s) (see 'man ctest'):

```bash
cd build
# show listing of tests, without running them
ctest --show-only

# run specific C++ or Python test(s) matching a regular expression
ctest --verbose --tests-regex SomeTestName
```

# 9. Authors and Acknowledgments

The following people have contributed to this document:

* Windows MINGW Section
  * Tim Sutton, Godofredo Contreras 2006
  * CMake additions Magnus Homann 2007
  * Python additions Martin Dobias 2007
  * With thanks to Tisham Dhar for preparing the initial msys environment

* Windows MSVC Section (Detailed install)
  * David Willis 2007
  * MSVC install additions Tim Sutton 2007
  * PostgreSQL, Qt compile, SIP, Python, AutoExp additions Juergen Fischer 2007

* Windows MSVC Section (Simplified install)
  * Tim Sutton 2007
  * Juergen Fischer 2007
  * Florian Hillen 2010

* OSX Section
  * Tim Sutton 2007
  * With special thanks to Tom Elwertowski and William Kyngesburye
  * Larry Shaffer 2012
  * Peter Petrik 2020

* GNU/Linux Section
  * Tim Sutton 2006
  * Debian package section: Juergen Fischer 2008

* WCS Test Server Section
  * Tim Sutton, Radim Blazek 2012

* Jenkins CI Configuration
  * Tim Sutton 2012

* Latex Generator
  * Tim Sutton 2011

* Debug Output/Tests Section
  * Larry Shaffer 2012, by way of 'Test Friday' Tim Sutton

* MXE/Mingw64 section
  * Alessandro Pasotti (2018-2021)
