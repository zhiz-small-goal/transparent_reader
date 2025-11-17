TransparentMdReader – 图片无法渲染问题最终修复笔记
一、问题现象

在渲染 Markdown 时，文档中的图片链接（例如 media/xxx.png）无法显示，甚至前端图片查看浮层也不会触发。

二、问题根因（100% 确认）
根本原因：Markdown 内容从未进入前端渲染管线（markdown-it）

之前的 openMarkdownFile() 使用的是：

basicMarkdownToHtml()
m_view->setHtml(html, QUrl::fromLocalFile(path));


这是旧式渲染流程，会导致：

前端 index.html / main.js 完全没有执行

window.renderMarkdown() 从未被调用

gMarkdownBaseUrl 永远为空（图片相对路径失效）

markdown-it 渲染系统完全被绕过

图片解析、图片浮层、内部链接解析统统无效

总结一句话：你加载的是纯 HTML，而不是前端渲染页。

三、最终修复方案（标准做法）
1）不再使用 basicMarkdownToHtml() + setHtml()

必须改用两段式渲染：

阶段 1：加载前端页面 index.html（一次）
m_view->setUrl(locateIndexPage());

阶段 2：加载完成后调用 renderMarkdownInPage()
m_view->page()->runJavaScript(
    "window.renderMarkdown(markdown, title, baseUrl);"
);

四、关键代码改动点
（1）openMarkdownFile() 必须改为：

读取 markdown 文本

生成 baseUrl = md 所在目录

如果前端未就绪 → 记录待渲染内容，跳转 index.html

如果前端已就绪 → 调用 renderMarkdownInPage()

这一条确保 markdown-it 正常执行。

（2）renderMarkdownInPage() 改为执行 JS：
window.renderMarkdown(markdown, title, baseUrl);


确保把 baseUrl 传给 JavaScript → markdown-it 的 URL 解析才能生效。

（3）MainWindow 构造函数必须监听 loadFinished：

否则 index.html 第一次加载不会得到渲染。

connect(m_view->page(), &QWebEnginePage::loadFinished,
        this, [this](bool ok) {
    m_pageLoaded = ok;
    if (ok && !m_pendingMarkdown.isEmpty()) {
        renderMarkdownInPage(m_pendingMarkdown,
                             m_pendingTitle,
                             QUrl(m_pendingBaseUrl));
        m_pendingMarkdown.clear();
        m_pendingTitle.clear();
        m_pendingBaseUrl.clear();
    }
});

五、修复生效后的行为验证

打开 Markdown → main.js 和 markdown-it 确实执行

gMarkdownBaseUrl 由 JS 正确显示为：

file:///…/你的md目录/


图片链接会自动解析成绝对路径：

file:///…/media/xxxx.png


图片正确显示

点击图片 → 前端图片查看浮层正常弹出

相对 markdown 链接内部跳转正常运行

六、技术总结

这次问题不是 markdown-it 或图片路径错误，而是：

渲染管线混用了旧方案与新方案，导致前端根本没参与渲染。

新版架构应该：

C++ 负责：读取 markdown、确定 baseUrl、通知前端渲染

前端负责：markdown-it 渲染 + URL 解析 + 事件处理 + 图片浮层等

C++ 不应再生成 HTML，也不应再 setHtml()

七、建议同步清理的旧逻辑

可删除：

basicMarkdownToHtml() 函数

所有 setHtml() 路径

无用 CSS/JS 资源

保持渲染路径简单：

Markdown → C++ 读文件 → JS(renderMarkdown) → markdown-it 渲染 → 显示