(function () {
  function escapeHtml(str) {
    return str
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;");
  }

  function renderInline(text) {
    let out = escapeHtml(text);

    // 链接 [text](url)
    out = out.replace(
      /\[([^\]]+)\]\(([^)]+)\)/g,
      '<a href="$2">$1</a>'
    );

    // 行内代码 `code`
    out = out.replace(/`([^`]+)`/g, "<code>$1</code>");

    // 粗体 **text** 或 __text__
    out = out.replace(/(\*\*|__)(.+?)\1/g, "<strong>$2</strong>");

    // 斜体 *text* 或 _text_
    out = out.replace(/(\*|_)([^*_]+)\1/g, "<em>$2</em>");

    return out;
  }

  function flushParagraph(buffer, htmlParts) {
    if (buffer.length === 0) return;
    const text = buffer.join(" ").trim();
    if (!text) {
      buffer.length = 0;
      return;
    }
    htmlParts.push("<p>" + renderInline(text) + "</p>");
    buffer.length = 0;
  }

  function flushList(listItems, htmlParts, ordered) {
    if (listItems.length === 0) return;
    const tag = ordered ? "ol" : "ul";
    htmlParts.push("<" + tag + ">");
    for (const item of listItems) {
      htmlParts.push("<li>" + renderInline(item) + "</li>");
    }
    htmlParts.push("</" + tag + ">");
    listItems.length = 0;
  }

  function renderMarkdownToHtml(markdown) {
    const lines = markdown.split(/\r?\n/);
    const htmlParts = [];

    let inCodeBlock = false;
    let codeFenceLang = "";
    let codeLines = [];

    let paraBuffer = [];

    let inList = false;
    let listOrdered = false;
    let listItems = [];

    const startList = (ordered) => {
      if (inList && listOrdered === ordered) return;
      flushParagraph(paraBuffer, htmlParts);
      flushList(listItems, htmlParts, listOrdered);
      inList = true;
      listOrdered = ordered;
    };

    const endListIfAny = () => {
      if (!inList) return;
      flushList(listItems, htmlParts, listOrdered);
      inList = false;
      listItems = [];
    };

    for (let i = 0; i < lines.length; ++i) {
      const line = lines[i];

      // 代码块围栏 ```lang
      const fenceMatch = line.match(/^```(\w+)?\s*$/);
      if (fenceMatch) {
        if (inCodeBlock) {
          // 结束代码块
          const code = escapeHtml(codeLines.join("\n"));
          const langClass = codeFenceLang ? " class=\"lang-" + codeFenceLang + "\"" : "";
          htmlParts.push(
            "<pre><code" + langClass + ">" + code + "</code></pre>"
          );
          codeLines = [];
          inCodeBlock = false;
          codeFenceLang = "";
        } else {
          // 开始代码块
          flushParagraph(paraBuffer, htmlParts);
          endListIfAny();
          inCodeBlock = true;
          codeFenceLang = fenceMatch[1] || "";
        }
        continue;
      }

      if (inCodeBlock) {
        codeLines.push(line);
        continue;
      }

      // 空行：结束段落 / 列表
      if (/^\s*$/.test(line)) {
        flushParagraph(paraBuffer, htmlParts);
        endListIfAny();
        continue;
      }

      // 标题 # .. ######
      const headingMatch = line.match(/^(#{1,6})\s+(.+)$/);
      if (headingMatch) {
        flushParagraph(paraBuffer, htmlParts);
        endListIfAny();
        const level = headingMatch[1].length;
        const text = headingMatch[2].trim();
        const id = text
          .toLowerCase()
          .replace(/[^\w\u4e00-\u9fa5]+/g, "-")
          .replace(/-+/g, "-")
          .replace(/^-|-$/g, "");
        htmlParts.push(
          "<h" +
            level +
            (id ? ' id="' + id + '"' : "") +
            ">" +
            renderInline(text) +
            "</h" +
            level +
            ">"
        );
        continue;
      }

      // 引用 >
      const quoteMatch = line.match(/^\s*>\s?(.*)$/);
      if (quoteMatch) {
        flushParagraph(paraBuffer, htmlParts);
        endListIfAny();
        htmlParts.push(
          "<blockquote>" + renderInline(quoteMatch[1]) + "</blockquote>"
        );
        continue;
      }

      // 水平线 --- 或 *** 等
      if (/^(\s*[-*_]){3,}\s*$/.test(line)) {
        flushParagraph(paraBuffer, htmlParts);
        endListIfAny();
        htmlParts.push("<hr/>");
        continue;
      }

      // 无序列表 - * +
      const ulMatch = line.match(/^\s*[-*+]\s+(.+)$/);
      if (ulMatch) {
        startList(false);
        listItems.push(ulMatch[1].trim());
        continue;
      }

      // 有序列表 1. 2.
      const olMatch = line.match(/^\s*(\d+)\.\s+(.+)$/);
      if (olMatch) {
        startList(true);
        listItems.push(olMatch[2].trim());
        continue;
      }

      // 普通段落
      if (/^\s/.test(line)) {
        // 以空格开头，当成段落延续
        paraBuffer.push(line.trim());
      } else {
        // 新段落开始
        flushParagraph(paraBuffer, htmlParts);
        paraBuffer.push(line.trim());
      }
    }

    // 收尾
    flushParagraph(paraBuffer, htmlParts);
    flushList(listItems, htmlParts, listOrdered);

    if (inCodeBlock && codeLines.length > 0) {
      const code = escapeHtml(codeLines.join("\n"));
      htmlParts.push("<pre><code>" + code + "</code></pre>");
    }

    return htmlParts.join("\n");
  }

  function setDocumentTitle(title) {
    if (title && typeof title === "string") {
      document.title = title;
    } else {
      document.title = "TransparentMdReader";
    }
  }

  function showContent() {
    const loading = document.querySelector(".md-loading");
    const content = document.getElementById("md-content");
    if (loading) {
      loading.hidden = true;
    }
    if (content) {
      content.hidden = false;
    }
  }

  // 暴露给 C++ 调用的接口
  window.renderMarkdown = function (markdown, title) {
    const contentEl = document.getElementById("md-content");
    if (!contentEl) return;

    const html = renderMarkdownToHtml(markdown || "");
    contentEl.innerHTML = html || "<p><em>（空文档）</em></p>";

    setDocumentTitle(title);
    showContent();
  };
})();
