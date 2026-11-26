/*
    SPDX-FileCopyrightText: 2026 huzhiyu
    SPDX-License-Identifier: MIT
*/

#include <KPluginFactory>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <KTextEditor/Plugin>

#include <QHash>
#include <QRegularExpression>
#include <QTimer>

/**
 * 草稿自动高亮：面向"粘贴进未保存新标签页"的临时编辑场景。
 *
 * 边界与不变量：
 * - 只处理"无 URL（从未保存）且当前为普通文本"的文档；对已落盘文件绝不干预，
 *   文件名识别链优先级永远高于本插件的内容猜测。
 * - 只在 Plain Text 状态下猜测一次；一旦模式被设置（无论猜的还是用户手选），
 *   mode() 不再是 Plain Text，且 handled 集合兜底，不会反复改写用户的选择。
 * - 猜不中就保持原样，不打扰、不提示。
 */
class ScratchModePlugin final : public KTextEditor::Plugin
{
    Q_OBJECT

public:
    explicit ScratchModePlugin(QObject *parent = nullptr, const QVariantList & = QVariantList())
        : KTextEditor::Plugin(parent)
    {
        // 规则按特异性从强到弱排序，首个命中即生效
        const QList<QPair<QString, QString>> table = {
            // shebang：最强的内容信号，优先级最高
            {QStringLiteral("(?m)^#!.*\\bpython[23]?\\b"), QStringLiteral("Python")},
            {QStringLiteral("(?m)^#!.*\\b(ba|z|k|)sh\\b"), QStringLiteral("Bash")},
            // 标记/结构化格式，特征唯一
            {QStringLiteral("\\A\\s*<\\?php"), QStringLiteral("PHP")},
            {QStringLiteral("\\A\\s*<\\?xml\\s"), QStringLiteral("XML")},
            {QStringLiteral("(?im)\\A\\s*(<!DOCTYPE\\s+html|<html[>\\s])"), QStringLiteral("HTML")},
            {QStringLiteral("(?m)^(diff --git|Index:|---\\s+a/|\\+\\+\\+\\s+b/)"), QStringLiteral("Diff")},
            {QStringLiteral("(?m)^\\s*FROM\\s+\\S+[\\s\\S]*(?m)^\\s*(RUN|COPY|ENTRYPOINT|CMD|EXPOSE)\\b"), QStringLiteral("Dockerfile")},
            // JSON：以 { 或 [ 开头且带 "键": 结构
            {QStringLiteral("\\A\\s*[\\{\\[]\\s*[\\s\\S]*\"[^\"\\n]+\"\\s*:"), QStringLiteral("JSON")},
            // Go：package 行不带分号（区别于 Java 的 package x.y;）且出现 func/import
            {QStringLiteral("(?m)^package\\s+\\w+\\s*$[\\s\\S]*\\b(func|import)\\b"), QStringLiteral("Go")},
            // Rust
            {QStringLiteral("(?m)^\\s*(fn\\s+main\\s*\\(|use\\s+std::|let\\s+mut\\s|extern\\s+crate\\s)"), QStringLiteral("Rust")},
            // Python（无 shebang 时）
            {QStringLiteral("(?m)^\\s*(def\\s+\\w+\\s*\\(|class\\s+\\w+[^\\n]*:\\s*$|from\\s+[\\w.]+\\s+import\\b)"), QStringLiteral("Python")},
            // TypeScript：类型注解或 interface，先于 JavaScript 判定
            {QStringLiteral("(?m)^\\s*(export\\s+)?interface\\s+\\w+|(?m):\\s*(string|number|boolean|void)\\b"), QStringLiteral("TypeScript")},
            // Java
            {QStringLiteral("(?m)^\\s*public\\s+(final\\s+)?(abstract\\s+)?(class|interface|enum)\\s+\\w+"), QStringLiteral("Java")},
            // C++ / C：include 头 + std:: 判 C++，否则退回 C
            {QStringLiteral("(?m)^#\\s*include\\s*<[^>]+>[\\s\\S]*\\b(std::|namespace\\s+\\w+|template\\s*<)"), QStringLiteral("C++")},
            {QStringLiteral("(?m)^#\\s*include\\s*<[^>]+>"), QStringLiteral("C")},
            // SQL
            {QStringLiteral("(?im)^\\s*(SELECT\\s+[\\w*,\\s]+FROM|INSERT\\s+INTO\\b|CREATE\\s+(TABLE|DATABASE)\\b|ALTER\\s+TABLE\\b|UPDATE\\s+\\w+\\s+SET\\b|DELETE\\s+FROM\\b)"), QStringLiteral("SQL")},
            // Nginx 配置
            {QStringLiteral("(?m)^\\s*(server\\s*\\{|location\\s+[\\^~\\w/]|listen\\s+\\d)"), QStringLiteral("Nginx")},
            // JavaScript
            {QStringLiteral("(?m)^\\s*(const|let|var)\\s+\\w+\\s*=|(?m)^\\s*(async\\s+)?function\\s*\\*?\\s*\\w*\\s*\\(|(?m)^(import|export)\\s"), QStringLiteral("JavaScript")},
            // YAML：至少两行 "键:" 顶层结构（无花括号块）
            {QStringLiteral("(?m)^[A-Za-z_][\\w.-]*:\\s(\\S|$)[\\s\\S]*(?m)^[A-Za-z_][\\w.-]*:\\s(\\S|$)"), QStringLiteral("YAML")},
            // TOML 先于 INI（值类型/引号特征）
            {QStringLiteral("(?m)^\\[[\\w.-]+\\]\\s*$[\\s\\S]*(?m)^[a-zA-Z_-][\\w-]*\\s*=\\s*(true|false|\"|')"), QStringLiteral("TOML")},
            {QStringLiteral("(?m)^\\[[\\w.$ -]+\\]\\s*$[\\s\\S]*(?m)^[\\w-]+\\s*=\\s*\\S"), QStringLiteral("INI")},
            // Bash（无 shebang）：控制流/常用命令特征
            {QStringLiteral("(?m)^\\s*(if\\s+\\[\\s|fi\\s*$|echo\\s+|export\\s+\\w+=|for\\s+\\w+\\s+in\\s)"), QStringLiteral("Bash")},
            // Markdown：标题、围栏代码块、粗体
            {QStringLiteral("(?m)^#{1,6}\\s+\\S|^```|(?m)^\\*\\*[^*\\n]+\\*\\*"), QStringLiteral("Markdown")},
        };

        // 启动期校验模式名是否存在于 KSyntaxHighlighting，防止上游改名后悄悄失效
        const KSyntaxHighlighting::Repository repo;
        for (const auto &rule : table) {
            const auto def = repo.definitionForName(rule.second);
            if (!def.isValid()) {
                qWarning() << "[katescratchmode] 未知的高亮模式名，规则被忽略:" << rule.second;
                continue;
            }
            QRegularExpression re(rule.first);
            if (!re.isValid()) {
                qWarning() << "[katescratchmode] 无效正则，规则被忽略:" << rule.first;
                continue;
            }
            m_rules.append({re, rule.second});
        }

        auto editor = KTextEditor::Editor::instance();
        Q_ASSERT(editor);

        // 会话恢复时插件加载可能晚于文档创建，先接管存量文档
        const auto docs = editor->documents();
        for (auto *doc : docs) {
            watch(doc);
        }
        connect(editor, &KTextEditor::Editor::documentCreated, this, [this](KTextEditor::Editor *, KTextEditor::Document *doc) {
            watch(doc);
        });
    }

    // KTextEditor::Plugin 纯虚接口；本插件无界面无菜单，给一个空对象即可
    QObject *createView(KTextEditor::MainWindow *mainWindow) override
    {
        return new QObject(mainWindow);
    }

private:
    void watch(KTextEditor::Document *doc)
    {
        if (m_timers.contains(doc)) {
            return;
        }
        // 文档销毁时同步清理定时器与已处理标记，防止悬空指针
        connect(doc, &QObject::destroyed, this, [this, doc]() {
            m_timers.remove(doc);
            m_handled.remove(doc);
        });

        auto *timer = new QTimer(this);
        timer->setSingleShot(true);
        // 防抖 400ms：逐字输入不频繁触发，整段粘贴只触发一次
        timer->setInterval(400);
        m_timers.insert(doc, timer);
        connect(timer, &QTimer::timeout, this, [this, doc]() {
            tryDetect(doc);
        });

        connect(doc, &KTextEditor::Document::textChanged, timer, [timer]() {
            timer->start();
        });

        // 接管时已有内容的文档（会话恢复）立即排一次检测
        if (!doc->text().isEmpty()) {
            timer->start();
        }
    }

    void tryDetect(KTextEditor::Document *doc)
    {
        // 边界：只碰"从未保存 + 仍是普通文本 + 未处理过"的文档
        if (!doc->url().isEmpty() || m_handled.contains(doc)) {
            return;
        }
        const QString current = doc->mode();
        if (!current.isEmpty() && current != QLatin1String("Normal")) {
            return;
        }

        const QString text = doc->text();
        if (text.size() < 8) {
            return;
        }

        for (const auto &rule : std::as_const(m_rules)) {
            if (rule.pattern.match(text).hasMatch()) {
                m_handled.insert(doc, true);
                if (!doc->setMode(rule.mode)) {
                    // setMode 失败则回退旧接口，仍失败则放并标记，避免反复尝试
                    doc->setHighlightingMode(rule.mode);
                }
                qDebug() << "[katescratchmode] 内容猜测 ->" << rule.mode;
                return;
            }
        }
    }

    struct Rule {
        QRegularExpression pattern;
        QString mode;
    };
    QList<Rule> m_rules;
    QHash<KTextEditor::Document *, QTimer *> m_timers;
    QHash<KTextEditor::Document *, bool> m_handled;
};

K_PLUGIN_FACTORY_WITH_JSON(ScratchModePluginFactory, "katescratchmode.json", registerPlugin<ScratchModePlugin>();)

#include "katescratchmode.moc"
