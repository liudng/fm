#include "app/fm_application.h"
#include "app/single_instance.h"
#include "ui/main_window.h"

#include <QCommandLineOption>
#include <QCommandLineParser>

int main(int argc, char **argv)
{
    fm::FmApplication app(argc, argv);

    // 命令行参数
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("fm - Linux dual-panel file manager"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("paths"),
                                 QStringLiteral("Optional paths to open in panel1 [panel2]"),
                                 QStringLiteral("[path1] [path2]"));
    parser.process(app);

    if (!app.initialize()) {
        // 已有实例运行：把命令行路径发送给它后退出
        const QStringList args = parser.positionalArguments();
        if (!args.isEmpty()) {
            // 通过 SingleInstance 发送路径
            // 注意：FmApplication::initialize 已经创建 singleInstance_
            // 这里复用 sendPaths（简化：直接新建一个临时实例发送）
            fm::SingleInstance sender;
            sender.sendPaths(args);
        }
        return 0;
    }

    // 处理命令行路径（首个实例直接调用 MainWindow 追加选项卡）
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        app.mainWindow()->addPathsToPanels(args);
    }

    return app.exec();
}
