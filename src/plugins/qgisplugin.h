/***************************************************************************
     qgisplugin.h
     --------------------------------------
    Date                 : Sun Sep 16 12:12:31 AKDT 2007
    Copyright            : (C) 2007 by Gary E. Sherman
    Email                : sherman at mrcc dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#pragma region "QGIS - Plugin API介绍"
/*
* 杨小兵-2024-02-23
一、应用场景
  QGIS插件提供了一种扩展QGIS功能的方法。QGIS是一个开源的地理信息系统软件，用于地图制作、空间数据管理和分析等。通过开发插件，开发者可以添加新的功能，
如工具、算法、数据源支持等，以满足特定的用户需求和应用场景。所有QGIS插件必须继承自抽象基类`QgisPlugin`。这意味着插件需要实现`QgisPlugin`类中定义的
一些基本方法，这确保了插件能够与QGIS平台无缝集成，遵循一定的标准。

二、必需的方法
  插件必须实现几个必需的方法才能被QGIS注册和识别（如果没有实现这些必须需要override的函数，那么QGIS将不会注册和识别插件）。这些包括：
    - **version**：插件的版本号，用于管理和更新插件。
    - **description**：对插件功能的描述，帮助用户了解插件用途。

三、特殊函数
  - **classFactory**：这是一个工厂函数，用于创建插件实例。当QGIS加载插件时，会调用这个函数来实例化插件。（QGIS加载插件的时候将会通过这个函数实例化插件）
  - **unload**：当插件被卸载时，这个函数负责清理资源，如关闭打开的文件，删除临时数据等。

四、extern "C" 声明
  插件还必须以`extern "C"`声明`classFactory`和`unload`函数。这是因为QGIS是用C++编写的，而C++支持函数重载和名字改编（mangling），这可能导致函数名在
编译后变得无法识别。通过`extern "C"`声明，可以告诉编译器这些函数应该避免C++的名字改编，确保它们可以被QGIS正确地解析和调用。

五、总结
  通过以上解释，我们了解到QGIS插件是如何扩展QGIS功能的，它们需要继承自一个基类并实现特定的方法和函数。这些要求确保了插件能够被QGIS正确加载、执行和卸载，
同时`extern "C"`声明保证了函数在C++环境中的兼容性和正确解析。
*/

/**
 * QGIS - Plugin API
 *
 *  \section about  About QGIS Plugins
 * Plugins provide additional functionality to QGis. Plugins must
 * implement several required methods in order to be registered with
 * QGis. These methods include:
 * name:
 *
 * - version
 * - description
 *
 * All QGIS plugins must inherit from the abstract base class QgisPlugin.
 * This list will grow as the API is expanded.
 *
 * In addition, a plugin must implement the classFactory and unload
 * functions. Note that these functions must be declared as extern "C" in
 * order to be resolved properly and prevent C++ name mangling.
 */
#pragma endregion

#ifndef QGISPLUGIN_H
#define QGISPLUGIN_H

#define SIP_NO_FILE


#include <QString>

class QgisInterface;

//#include "qgisplugingui.h"

/**
 * \ingroup plugins
 * \class QgisPlugin
 * \brief Abstract base class from which all plugins must inherit
 * \note not available in Python bindings
 */
class QgisPlugin
{
  public:
#pragma region "注释解释"
    /*
    * 杨小兵-2024-02-23
    一、解释
    1. **接口定义** (`//! Interface to gui element collection object`):
        - 这句话定义了一个抽象接口，用于GUI元素集合对象的交互。在QGIS插件开发中，GUI（Graphical User Interface，图形用户界面）
        元素是与用户交互的视觉组件，例如按钮、菜单、对话框等。
        - `gui()`方法是一个纯虚函数（用`=0`标记），这意味着这个函数没有具体的实现，必须由继承这个接口的子类来实现。这是多态和接
        口编程的一部分，在设计模式中常见于工厂模式和策略模式等。
    
    2. **元素类型** (`//! Element types that can be added to the interface`):
        - 这句注释说明了可以添加到接口的元素类型。虽然具体的元素类型在这段注释中没有列出，但我们可以推测这包括了各种GUI元素，如前面提到的按钮、菜单项、
        对话框等。
        - 在QGIS插件开发中，了解可以添加哪些类型的GUI元素至插件界面是重要的，因为这直接影响了插件的功能性和用户体验。

    二、总结
      在QGIS插件开发中，`gui()`方法和可添加的元素类型定义了插件如何与用户进行交互。例如，如果你正在开发一个提供新的空间分析工具的插件，你可能需要向
    QGIS界面添加特定的按钮或菜单项，让用户可以触发你的分析功能。通过实现这个接口，你的插件可以灵活地添加所需的GUI元素，以提供直观且功能丰富的用户界面。
    */
    //! Interface to gui element collection object
    //virtual QgisPluginGui *gui()=0;
    //! Element types that can be added to the interface
#pragma endregion
#if 0
    enum Elements
    {
      MENU,
      MENU_ITEM,
      TOOLBAR,
      TOOLBAR_BUTTON,
    };

    \todo XXX this may be a hint that there should be subclasses
#endif
    /*
    * 杨小兵-2024-02-23
      在这个例子中，定义了一个名为`PluginType`的枚举类型，它包含了三个枚举值：`UI`、`MapLayer`、和`Renderer`。
    这三个值代表不同类型的插件，每个枚举值后面都有一条注释，说明了该枚举值的含义。

    - `UI = 1`：这个枚举值表示用户界面插件。通过赋值操作（`= 1`），这个枚举值显式地被设置为整数值1。在C++中，如果不显式指定枚举值的整数值，
    第一个枚举值的默认整数值为0，后续枚举值依次递增。
    - `MapLayer`：这个枚举值表示地图层插件。由于它没有显式指定整数值，并且它紧跟在`UI`之后，所以根据C++枚举的规则，它的整数值为`UI`所指定的值加1，即2。
    - `Renderer`：这个枚举值表示一个新的渲染器类插件。同样，它没有显式指定整数值，因此它的整数值为`MapLayer`的整数值加1，即3。

    - 用户界面插件（`UI`）可以用来添加或修改用户界面元素。
    - 地图层插件（`MapLayer`）可以用来添加新的地图层，如卫星图层或交通信息图层。
    - 渲染器插件（`Renderer`）可以用来实现不同的渲染效果，比如3D渲染或特殊效果渲染。

      代码中每行枚举值后面的注释用`//!<`引导，这是一种特殊的注释方式，通常用于文档生成工具（如Doxygen）中，以便自动生成代码文档。这些注释为每个枚举值提供
    了简短的描述，有助于理解每个枚举值的用途和含义。
    */
    enum PluginType
    {
      UI = 1,   //!< User interface plug-in
      MapLayer, //!< Map layer plug-in
      Renderer, //!< A plugin for a new renderer class
    };


    /**
     * Constructor for QgisPlugin
     */
    QgisPlugin( QString const &name = "",
                QString const &description = "",
                QString const &category = "",
                QString const &version = "",
                PluginType type = MapLayer )
      : mName( name )
      , mDescription( description )
      , mCategory( category )
      , mVersion( version )
      , mType( type )
    {}

    /*
    * 杨小兵-2024-02-23
    一、解释
      在C++中，`= default`是一种特殊的语法，用于明确告诉编译器为某个函数生成默认的实现。这个语法通常用于构造函数、析构函数和赋值运算符，
    可以用来显式地声明使用编译器生成的默认版本，而不是手动实现它们。使用`= default`可以提高代码的清晰性和简洁性，同时保持类的特殊成员函
    数的默认行为。

    - **默认构造函数**：当你希望一个类拥有默认构造函数，但又不想手动编写构造函数体时，可以使用`= default`。
    - **拷贝构造函数和拷贝赋值运算符**：当你需要类的拷贝行为符合默认语义（逐成员拷贝），但类中包含其他规则（如虚函数或特殊成员函数）时，
    使用`= default`可以显式声明这一意图。
    - **移动构造函数和移动赋值运算符**：为了支持C++11引入的移动语义，使用`= default`可以让编译器为你生成移动构造函数和移动赋值运算符的
    默认实现。
    - **析构函数**：在类中显式声明虚析构函数时，使用`= default`可以避免手动编写空的析构函数体，尤其在基类中这样做可以保证派生类的对象通
    过基类指针正确地被删除。

    它们都确保了返回的`QString`对象引用是常量，并且这两个成员函数都不会修改类的状态，这在需要保证对象状态不被改变的上下文中非常有用，比
    如在多线程环境下访问共享数据时。

      QString const &name() const和const QString &name() const：C++中`const`的位置相对于类型名来说是灵活的，只要它出现在类型名和`*`
    或`&`符号之间，就能正确地修饰类型。在这种情况下，无论`const`出现在类型名之前还是之后，结果都是相同的，都表示引用或指针指向的对象是常量，
    不能被修改。

    二、总结
    1、= default明确告诉编译器为当前函数生成默认的实现
    2、用于构造函数、析构函数、赋值运算符
    */
    virtual ~QgisPlugin() = default;

    //! Gets the name of the plugin
    QString const &name() const
    {
      return mName;
    }

    QString        &name()
    {
      return mName;
    }

    //! Version of the plugin
    QString const &version() const
    {
      return mVersion;
    }

    //! Version of the plugin
    QString &version()
    {
      return mVersion;
    }

    //! A brief description of the plugin
    QString const &description() const
    {
      return mDescription;
    }

    //! A brief description of the plugin
    QString        &description()
    {
      return mDescription;
    }

    //! Plugin category
    QString const &category() const
    {
      return mCategory;
    }

    //! Plugin category
    QString        &category()
    {
      return mCategory;
    }

    //! Plugin type, either UI or map layer
    QgisPlugin::PluginType const &type() const
    {
      return mType;
    }


    //! Plugin type, either UI or map layer
    QgisPlugin::PluginType        &type()
    {
      return mType;
    }

    /// function to initialize connection to GUI
    virtual void initGui() = 0;

    //! Unload the plugin and cleanup the GUI
    virtual void unload() = 0;

  private:

    /// plug-in name
    QString mName;

    /// description
    QString mDescription;

    /// category
    QString mCategory;

    /// version
    QString mVersion;

    /// UI or MAPLAYER plug-in

    /**
     * \todo Really, might be indicative that this needs to split into
     * maplayer vs. ui plug-in vs. other kind of plug-in
     */
    PluginType mType;

}; // class QgisPlugin

#pragma region "qgis 主应用程序使用的 Typedef"
/*
* 杨小兵-2024-02-23
一、解释
  在C++中，`typedef`是一种关键字，用于为现有的类型提供一个新的名称（别名）。这使得代码更易于理解和维护，尤其是在处理复杂的类型或函数指针时。
下列代码段定义了一系列的`typedef`，这些`typedef`为QGIS（一个开源地理信息系统软件）插件框架中的特定功能提供了别名，主要涉及插件的创建、卸载、
以及获取插件的信息（如名称、描述、类型等）不需要实例化插件对象。

二、每一个`typedef`的解释及其注释内容
1. **`create_t`**
   - **作用**：定义了一个函数指针类型，该函数接受一个`QgisInterface*`参数，并返回一个指向`QgisPlugin`对象的指针。这个函数用于创建插件对象。
   - **注释**：为qgis主应用使用的函数，返回指向插件对象的通用指针。

2. **`unload_t`**
   - **作用**：定义了一个函数指针类型，该函数接受一个指向`QgisPlugin`对象的指针，并不返回任何值。这个函数用于卸载插件并释放其资源。
   - **注释**：用于卸载插件并释放其资源的函数。

3. **`name_t`**
   - **作用**：定义了一个函数指针类型，该函数不接受参数，并返回一个指向`QString`对象的常量指针。这个函数用于获取插件的名称，不需要实例化插件对象。
   - **注释**：用于获取插件名称的函数，无需实例化插件。

4. **`description_t`**
   - **作用**：定义了一个函数指针类型，用于获取插件的描述。它不接受参数，并返回一个指向`QString`对象的常量指针。
   - **注释**：用于获取插件描述的函数，无需实例化插件。

5. **`category_t`**
   - **作用**：定义了一个函数指针类型，用于获取插件的分类。它不接受参数，并返回一个指向`QString`对象的常量指针。
   - **注释**：用于获取插件分类的函数，无需实例化插件。

6. **`type_t`**
   - **作用**：定义了一个函数指针类型，用于获取插件的类型。它不接受参数，并返回一个整数值。
   - **注释**：用于获取插件类型的函数，无需实例化插件。

7. **`version_t`**
   - **作用**：定义了一个函数指针类型，用于获取插件的版本号。它不接受参数，并返回一个指向`QString`对象的常量指针。
   - **注释**：用于获取插件版本号的函数，无需实例化插件。

8. **`icon_t`**
   - **作用**：定义了一个函数指针类型，用于获取插件的图标文件名。它不接受参数，并返回一个指向`QString`对象的常量指针。
   - **注释**：用于获取插件图标文件名的函数，无需实例化插件。

9. **`experimental_t`**
   - **作用**：定义了一个函数指针类型，用于获取插件的实验状态。它不接受参数，并返回一个指向`QString`对象的常量指针。
   - **注释**：用于获取插件的实验状态的函数，无需实例化插件。

10. **`create_date_t`**
    - **作用**：定义了一个函数指针类型，用于获取插件的创建日期。它不接受参数，并返回一个指向`QString`对象的常量指针。
    - **注释**：用于获取插件的创建
二、总结
*/
// Typedefs used by qgis main app

//! Typedef for the function that returns a generic pointer to a plugin object
typedef QgisPlugin *create_t( QgisInterface * );

//! Typedef for the function to unload a plugin and free its resources
typedef void unload_t( QgisPlugin * );

//! Typedef for getting the name of the plugin without instantiating it
typedef const QString *name_t();

//! Typedef for getting the description without instantiating the plugin
typedef const QString *description_t();

//! Typedef for getting the category without instantiating the plugin
typedef const QString *category_t();

//! Typedef for getting the plugin type without instantiating the plugin
typedef int type_t();

//! Typedef for getting the plugin version without instantiating the plugin
typedef const QString *version_t();

//! Typedef for getting the plugin icon file name without instantiating the plugin
typedef const QString *icon_t();

//! Typedef for getting the experimental status without instantiating the plugin
typedef const QString *experimental_t();

//! Typedef for getting the create date without instantiating the plugin
typedef const QString *create_date_t();

//! Typedef for getting the update date status without instantiating the plugin
typedef const QString *update_date_t();
#pragma endregion

#endif // QGISPLUGIN_H
