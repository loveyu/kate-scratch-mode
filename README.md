# kate-scratch-mode

Kate (KF6) 插件：**未保存的粘贴草稿按内容自动猜测语言并切换高亮模式**。
场景：把 Kate 当临时编辑器用，粘贴即有语法高亮和折叠，永不落盘。

## 行为边界

- 只处理"从未保存（URL 为空）且当前为普通文本"的文档；已落盘文件一律不碰，文件名识别优先。
- 防抖 400ms，猜中一次后该文档不再重复猜测；猜不中保持纯文本，无提示。
- 规则表按特异性排序，首个命中生效；启动时会用 KSyntaxHighlighting 校验模式名，无效规则自动忽略。

## 构建（Debian trixie）

```bash
sudo apt install libkf6texteditor-dev extra-cmake-modules cmake g++   # 编译期依赖
cd /data/code/github/kate-scratch-mode
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DKDE_INSTALL_USE_QT_SYS_PATHS=ON
cmake --build build
sudo cmake --install build     # 装到 /usr/lib/x86_64-linux-gnu/qt6/plugins/kf6/ktexteditor/
```

也可以直接从 GitHub Actions 的 [build 工作流](actions/workflows/ci.yml) 下载现成产物
`katescratchmode.so`（Debian trixie 容器构建），放到上述插件目录即可。

## 启用

1. 重启 Kate
2. 设置 → 配置 Kate → 插件 → 勾选 **Scratch Mode（草稿自动高亮）** → 确定
3. 新建标签页（Ctrl+N）→ 粘贴代码 → 约半秒后高亮与折叠出现

## 加语言 / 调规则

编辑 `katescratchmode.cpp` 里的 `table`（正则 → KSyntaxHighlighting 定义名，
模式名以 `工具 → 模式` 菜单里的英文名为准），然后重新构建安装，重启 Kate。

## 排障

```bash
# 验证插件元数据与加载（输出版本信息则正常）
QT_QPA_PLATFORM=offscreen kate --anonymous 2>&1 | grep -i scratch
```

- 插件没生效：确认勾选后是否重启了 Kate；确认粘贴的是未保存新标签页。
- KTextEditor 大版本升级后插件可能因 ABI 变化加载失败（Kate 启动时报 warning），
  重新执行上面的构建安装即可。

## 卸载

1. Kate 设置里取消勾选
2. `sudo rm /usr/lib/x86_64-linux-gnu/qt6/plugins/kf6/ktexteditor/katescratchmode.so`
3. 可选：`sudo apt autoremove` 清掉编译依赖
