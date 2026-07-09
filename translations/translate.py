#!/usr/bin/env python3
"""批量翻译 fm_zh.ts 文件"""
import xml.etree.ElementTree as ET

# 英文 → 中文翻译字典
translations = {
    # 菜单
    '&File': '文件(&F)',
    'F&avorites': '收藏(&A)',
    '&Settings': '设置(&S)',
    '&Help': '帮助(&H)',
    '&About': '关于(&A)',
    '&Quit': '退出(&Q)',
    '&Volumes': '卷(&V)',
    '(Loading...)': '(加载中...)',
    '(No volumes)': '(无卷)',
    '(No favorites)': '(无收藏)',
    '&Add to Favorites...': '添加到收藏(&A)...',
    'Remove Favorite': '删除收藏',
    '&Language': '语言(&L)',
    '&Theme': '主题(&T)',
    '&English': '英文(&E)',
    '&Chinese': '中文(&C)',
    '&Default': '默认(&D)',
    '&Settings...': '设置(&S)...',

    # 文件菜单
    'New &Tab': '新建标签页(&T)',
    '&Close Tab': '关闭标签页(&C)',
    '&Clone Tab': '克隆标签页(&C)',
    'New &File': '新建文件(&F)',
    'New &Folder': '新建文件夹(&F)',

    # 设置菜单
    '&Switch Active Panel': '切换活动面板(&S)',
    'Switch to &Vertical Layout': '切换到垂直布局(&V)',
    'Switch to &Horizontal Layout': '切换到水平布局(&H)',
    '&Reset Splitter': '重置分隔(&R)',
    'Hide &Panel 1': '隐藏面板 1(&P)',
    'Show &Panel 1': '显示面板 1(&P)',
    'Hide &Panel 2': '隐藏面板 2(&P)',
    'Show &Panel 2': '显示面板 2(&P)',
    'Show &Hidden Files': '显示隐藏文件(&H)',

    # 文件列表右键菜单
    '&Open': '打开(&O)',
    'Open &With...': '打开方式(&W)...',
    '&Rename': '重命名(&R)',
    'Cu&t': '剪切(&T)',
    '&Copy': '复制(&C)',
    '&Paste': '粘贴(&P)',
    'Cut to &Opposite': '剪切到对面(&O)',
    'Copy to O&pposite': '复制到对面(&P)',
    'Copy &Path': '复制路径(&P)',
    'Copy File &Name': '复制文件名(&N)',
    'Move to &Trash': '移到回收站(&T)',
    '&Delete Permanently': '彻底删除(&D)',
    'P&roperties': '属性(&R)',
    '&Back': '后退(&B)',
    '&Forward': '前进(&F)',
    '&Up': '上一级(&U)',
    '&Home': '主目录(&H)',
    '&Refresh': '刷新(&R)',

    # 选项卡上下文菜单
    'Close Tab (Context)': '关闭标签页',
    'Close Other Tabs': '关闭其他标签页',
    'Clone Tab (Context)': '克隆标签页',
    # 选项卡右键菜单实际显示文本
    'Close': '关闭',
    'Close Others': '关闭其他',
    'Clone': '克隆',

    # 键盘导航
    'Focus Tab Bar': '聚焦标签栏',
    'Focus Panel': '聚焦面板',
    'Next Tab': '下一个标签页',

    # 文件操作错误
    'Cannot copy folder "%1" into itself.': '无法将文件夹“%1”复制到自身。',

    # 对话框
    'Open With': '打开方式',
    'Select an application to open "%1":': '选择用于打开 "%1" 的应用：',
    'Custom command:': '自定义命令：',
    'Browse...': '浏览...',
    'Remember this choice for this file type': '记住此文件类型的打开方式',
    'Add Favorite': '添加收藏',
    'Favorite name:': '收藏名称：',
    'New Favorite': '新收藏',
    'A favorite with this name already exists.': '同名的收藏已存在。',
    'New File': '新建文件',
    'New Folder': '新建文件夹',
    'File name:': '文件名：',
    'Folder name:': '文件夹名：',
    'Language': '语言',
    'Language will be applied after restart.': '语言将在重启后生效。',
    'Theme': '主题',
    'Configuration Error': '配置错误',
    'The configuration file is corrupted or cannot be read.': '配置文件已损坏或无法读取。',
    'Rebuild': '重建',
    'Exit': '退出',
    'About fm': '关于 fm',
    'About': '关于',
    'Settings': '设置',
    'Interface': '界面',
    'Panels': '面板',
    'File Browser': '文件浏览',
    'Shortcuts': '快捷键',
    'Active Panel': '活动面板',
    'Panel 1': '面板 1',
    'Panel 2': '面板 2',
    'Layout': '布局',
    'Left / Right': '左右',
    'Top / Bottom': '上下',
    'Show Panels': '显示面板',
    'Show Panel 1': '显示面板 1',
    'Show Panel 2': '显示面板 2',
    'Visible Columns': '可见列',
    'Show hidden files (. prefix)': '显示隐藏文件（以 . 开头）',
    'Action': '动作',
    'Shortcut': '快捷键',
    'Conflict': '冲突',
    'Yes': '是',
    'Double-click a shortcut cell to edit. Conflicting items shown in red.': '双击快捷键单元格进行编辑。冲突项以红色显示。',
    'Edit Shortcut': '编辑快捷键',
    'Press a new key sequence:': '按下新的快捷键：',

    # 冲突对话框
    'File Conflict': '文件冲突',
    'A file named "%1" already exists in "%2".\n\nSource: %3\nWould you like to overwrite it?':
        '在 "%2" 中已存在名为 "%1" 的文件。\n\n源：%3\n是否覆盖？',
    'Overwrite': '覆盖',
    'Skip': '跳过',
    'Rename': '重命名',
    'Overwrite All': '全部覆盖',
    'Skip All': '全部跳过',
    'Rename All': '全部重命名',
    'Cancel': '取消',

    # 错误对话框
    'Error': '错误',

    # 文件操作错误
    'Cannot copy: %1 -> %2': '无法复制：%1 → %2',

    # 彻底删除确认
    'Delete Permanently': '彻底删除',
    'Are you sure you want to permanently delete "%1"?\nThis action cannot be undone.':
        '确定要彻底删除"%1"吗？\n此操作无法撤销。',
    'Are you sure you want to permanently delete %1 items?\nThis action cannot be undone.':
        '确定要彻底删除 %1 项吗？\n此操作无法撤销。',
    'Cannot remove: %1': '无法删除：%1',
    'Cannot rmdir: %1': '无法删除目录：%1',
    'Cannot create trash directory: %1': '无法创建回收站目录：%1',
    'Cannot create trash/files: %1': '无法创建 trash/files：%1',
    'Cannot create trash/info: %1': '无法创建 trash/info：%1',
    'Cannot move to trash: %1': '无法移到回收站：%1',
    'Cannot write trash info: %1': '无法写入 trashinfo：%1',
    'Cannot create file: %1': '无法创建文件：%1',
    'Cannot create folder: %1': '无法创建文件夹：%1',
    'Cannot open: %1': '无法打开：%1',
    'Cannot remove source after copy: %1': '复制后无法删除源：%1',
    'Cannot rename: %1': '无法重命名：%1',
    'Invalid .desktop file: %1': '无效的 .desktop 文件：%1',
    'Clipboard is empty.': '剪贴板为空。',
    'Name already exists: %1': '名称已存在：%1',
    'File does not exist: %1': '文件不存在：%1',
    'Unmount Failed': '卸载失败',
    'Mount Failed': '挂载失败',
    'Eject Failed': '弹出失败',
    'Invalid device path: %1': '无效的设备路径：%1',
    'No drive for device: %1': '设备没有对应的驱动器：%1',

    # 文件操作进度
    'Copying...': '复制中...',
    'Moving...': '移动中...',
    'Canceling...': '取消中...',
    'Cancel': '取消',

    # 属性对话框
    'Properties': '属性',
    'Basic': '基本信息',
    'Name:': '名称：',
    'Location:': '位置：',
    'Size:': '大小：',
    'Type:': '类型：',
    'MIME Type:': 'MIME 类型：',
    'Target:': '目标：',
    '(folder)': '(文件夹)',
    'User & Permissions': '用户与权限',
    'Owner:': '所有者：',
    'Group:': '组：',
    'Permissions:': '权限：',
    'Timestamps': '时间戳',
    'Created': '创建时间',
    'Modified': '修改时间',
    'System': '系统信息',
    'Device:': '设备：',
    'Inode:': 'Inode：',
    'Links:': '链接数：',

    # 列名
    'Icon': '图标',
    'Name': '名称',
    'Size': '大小',
    'Type': '类型',
    'Group': '组',
    'Owner': '所有者',
    'Permissions': '权限',

    # About
    'About fm': '关于 fm',

    # 其他
    'Current: %1': '当前：%1',
    'Open': '打开',
    'Copy': '复制',
    'Cut': '剪切',
    'Copy Path': '复制路径',
    'Copy File Name': '复制文件名',
    'Copy to Opposite': '复制到对面',
    'Cut to Opposite': '剪切到对面',
    'Close': '关闭',
    'Back': '后退',
    'Forward': '前进',
    'Default': '默认',
    'Chinese': '中文',
    'English': '英文',
    'Eject': '弹出',
    'Unmount': '卸载',
    'Safely Unmount': '安全卸载',
    'Mount': '挂载',
    '(No external devices)': '(无外部设备)',
    'Volume Operation Failed': '卷操作失败',
    'Clone Tab': '克隆标签页',
    'Close Tab': '关闭标签页',
    'Delete Permanently': '彻底删除',
    'Trash': '回收站',

    # 快捷键管理器（无 & 助记符版本）
    'New Tab': '新建标签页',
    'Quit': '退出',
    'Open With...': '打开方式...',
    'Paste': '粘贴',
    'Move to Trash': '移到回收站',
    'Up': '上一级',
    'Refresh': '刷新',
    'Switch Active Panel': '切换活动面板',
    'Toggle Orientation': '切换布局方向',
    'Toggle Panel 1 Visible': '切换面板 1 可见性',
    'Toggle Panel 2 Visible': '切换面板 2 可见性',
    'Show Hidden Files': '显示隐藏文件',

    # 错误信息
    'Only local files are supported': '仅支持本地文件',
    'Path does not exist: %1': '路径不存在：%1',
    'Not a directory: %1': '不是目录：%1',
    'Permission denied: %1': '权限不足：%1',

    # 输入对话框
    'Name cannot be empty.': '名称不能为空。',
    "Name cannot contain '/' or '\\'.": "名称不能包含 / 或 \\。",
    'Name already exists. Please choose another.': '名称已存在，请选择其他名称。',
    'New name:': '新名称：',

    # 打开方式对话框
    'Select Application': '选择应用程序',

    # 文件列表模型列名
    'MIME': 'MIME',

    # 属性对话框补充
    'Path': '路径',
    'System Info': '系统信息',
    'Inode': 'Inode',
    'SymLink target': '符号链接目标',

    # 设置页补充
    'Language:': '语言：',
    'Theme:': '主题：',
    'Icon Theme': '图标主题',
    'Icon theme:': '图标主题：',
    'Automatic': '自动',

    # 进度对话框
    'Working...': '处理中...',

    # 关于对话框 HTML
    '<h2>fm</h2><p>Linux dual-panel file manager</p><p><b>Version:</b> 1.0.0</p><p><b>Author:</b> liudng</p><p>Copyright © 2026 liudng</p><p>Licensed under GPL-3.0-or-later</p><p><a href="https://github.com/liudng/fm-qt">https://github.com/liudng/fm-qt</a></p>':
        '<h2>fm</h2><p>Linux 双面板文件管理器</p><p><b>版本：</b> 1.0.0</p><p><b>作者：</b> liudng</p><p>Copyright © 2026 liudng</p><p>采用 GPL-3.0-or-later 许可证</p><p><a href="https://github.com/liudng/fm-qt">https://github.com/liudng/fm-qt</a></p>',
}

# 读取 .ts 文件
tree = ET.parse('fm_zh.ts')
root = tree.getroot()

count = 0
for msg in root.iter('message'):
    src = msg.find('source')
    tr = msg.find('translation')
    if src is not None and tr is not None and src.text:
        if src.text in translations:
            tr.text = translations[src.text]
            if tr.get('type') == 'unfinished':
                del tr.attrib['type']
            count += 1

# 写回
tree.write('fm_zh.ts', encoding='utf-8', xml_declaration=True)
print(f'Translated {count} messages')
