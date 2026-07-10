#ifndef FM_DIALOGS_ISETTINGS_PAGE_H
#define FM_DIALOGS_ISETTINGS_PAGE_H

#include <QString>
#include <QWidget>

namespace fm {

// 设置页接口
// 子类需要实现 load/apply/reset 三个方法
class ISettingsPage
{
public:
    virtual ~ISettingsPage() = default;
    virtual QString id() const = 0;
    virtual QString title() const = 0;
    virtual QWidget *widget() = 0;
    virtual void load() = 0;  // 从 ConfigManager 加载
    virtual void apply() = 0; // 保存到 ConfigManager
    virtual void reset() = 0; // 恢复到上次 apply 的状态
};

} // namespace fm

#endif // FM_DIALOGS_ISETTINGS_PAGE_H
