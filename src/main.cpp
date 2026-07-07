#include "app/fm_application.h"
#include "ui/main_window.h"

#include <QCommandLineOption>
#include <QCommandLineParser>

int main(int argc, char **argv) {
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
        return 1;
    }

    // 处理命令行路径（Phase 1：直接调用 MainWindow 追加选项卡）
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        app.mainWindow()->addPathsToPanels(args);
    }

    return app.exec();
}
