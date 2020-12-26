// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com
/*******************************************************************************
*                                                                              *
* Author    :  Damir Bakiev                                                    *
* Version   :  na                                                              *
* Date      :  01 February 2020                                                *
* Website   :  na                                                              *
* Copyright :  Damir Bakiev 2016-2020                                          *
*                                                                              *
* License:                                                                     *
* Use, modification & distribution is subject to Boost Software License Ver 1. *
* http://www.boost.org/LICENSE_1_0.txt                                         *
*                                                                              *
*******************************************************************************/

#include "mainwindow.h"
#include "settingsdialog.h"
#include "splashscreen.h"
#include "version.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QGLWidget>
#include <QSettings>
#include <QTranslator>

#include "leakdetector.h"

void initIcon(const QString& path);
void translation(QApplication* app);

int main(int argc, char* argv[])
{

#ifdef LEAK_DETECTOR
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
#endif

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    Q_INIT_RESOURCE(resources);
    QApplication app(argc, argv);

    app.setApplicationName("GGEasy");
    app.setOrganizationName(VER_COMPANYNAME_STR);
    app.setApplicationVersion(VER_PRODUCTVERSION_STR);

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, app.applicationDirPath());

    QGLFormat glf = QGLFormat::defaultFormat();
    glf.setSampleBuffers(true);
    glf.setSamples(16);
    QGLFormat::setDefaultFormat(glf);

    initIcon(qApp->applicationDirPath());

    //    QSystemSemaphore semaphore("GGEasy_Semaphore", 1); // создаём семафор
    //    semaphore.acquire(); // Поднимаем семафор, запрещая другим экземплярам работать с разделяемой памятью
    //#ifdef linux
    //    // в linux/unix разделяемая память не освобождается при аварийном завершении приложения,
    //    // поэтому необходимо избавиться от данного мусора
    //    QSharedMemory nix_fix_shared_memory("GGEasy_Memory");
    //    if (nix_fix_shared_memory.attach()) {
    //        nix_fix_shared_memory.detach();
    //    }
    //#endif
    //    MainWindow* mainWin = nullptr;
    //    QSharedMemory sharedMemory("GGEasy_Memory"); // Создаём экземпляр разделяемой памяти
    //    auto instance = [&sharedMemory]() -> MainWindow*& { return *static_cast<MainWindow**>(sharedMemory.data()); };
    //    bool is_running = false; // переменную для проверки ууже запущенного приложения
    //    if (sharedMemory.attach()) { // пытаемся присоединить экземпляр разделяемой памяти к уже существующему сегменту
    //        is_running = true; // Если успешно, то определяем, что уже есть запущенный экземпляр
    //    } else {
    //        sharedMemory.create(sizeof(mainWin)); // В противном случае выделяем 1 байт памяти
    //        is_running = false; // И определяем, что других экземпляров не запущено
    //    }
    //    semaphore.release(); // Опускаем семафор
    //    QCommandLineParser parser;
    //    parser.addPositionalArgument("url", "Url of file to open");
    //    parser.process(app);

    //    if (is_running) {

    //        system("pause");

    //        //        if (parser.positionalArguments().length()) {
    //        //            for (const QString& fileName : parser.positionalArguments()) {
    //        //                instance()->loadFile(fileName);
    //        //            }
    //        //        }
    //        return 1;
    //    } else {
    //    }
    //    if (parser.positionalArguments().length()) {
    //        QSplashScreen* splash = nullptr;
    //        splash = new QSplashScreen(QPixmap(QLatin1String(":/256.png")));
    //        splash->setAttribute(Qt::WA_DeleteOnClose);
    //        splash->show();
    //        mainWin = new MainWindow();

    //        instance() = mainWin;
    //        mainWin->setIconSize({ 24, 24 });
    //        mainWin->show();
    //        splash->finish(mainWin);
    //        for (const QString& fileName : parser.positionalArguments()) {
    //            //            if (fileName.endsWith("g2g")) {
    //            //                MainWindow* mainWin = new MainWindow();
    //            //                mainWin->setIconSize({ 24, 24 });
    //            //                mainWin->show();
    //            //                mainWin->loadFile(fileName);
    //            //            } else
    //            mainWin->loadFile(fileName);
    //        }
    //    } else {
    //        QSplashScreen* splash = nullptr;
    //        splash = new QSplashScreen(QPixmap(QLatin1String(":/256.png")));
    //        splash->setAttribute(Qt::WA_DeleteOnClose);
    //        splash->show();
    //        mainWin = new MainWindow();

    //        instance() = mainWin;
    //        mainWin->setIconSize({ 24, 24 });
    //        mainWin->show();
    //        splash->finish(mainWin);
    //    }

    SplashScreen* splash = new SplashScreen(QPixmap(QLatin1String(":/256.png")));
    splash->setAttribute(Qt::WA_DeleteOnClose);
    splash->show();

    {
        SettingsDialog().readSettings();
    }

    {
        QSettings settings;
        settings.beginGroup("MainWindow");
        QString locale(settings.value("locale").toString());
        if (locale.isEmpty())
            locale = QLocale().name().left(2);
        settings.setValue("locale", locale);
        settings.endGroup();
        MainWindow::translate(locale);
    }

    MainWindow mainWin;
    mainWin.setObjectName("MainWindow");
    mainWin.setIconSize({ 24, 24 });
    mainWin.show();
    splash->finish(&mainWin);

    QCommandLineParser parser;
    parser.addPositionalArgument("url", "Url of file to open");
    parser.process(app);
    for (const QString& fileName : parser.positionalArguments()) {
        mainWin.loadFile(fileName);
    }

    return app.exec();
}

void initIcon(const QString& path)
{
    QIcon::setThemeSearchPaths({
        path + "/../icons/",
        path + "/../icons/breeze/",
        path + "/icons/",
        path + "/icons/breeze/",
    });
    QIcon::setThemeName("Breeze");
}
