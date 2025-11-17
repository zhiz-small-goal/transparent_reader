// 文件：resources/web/main.js
// 作用：Markdown 渲染 + YAML 清理 + 智能段落修复 + 图片浮层 + 链接修复

(function () {

  // ===============================
  // markdown-it 初始化
  // ===============================
  if (!window.markdownit) {
    console.error("markdown-it 未加载，请确认 markdown-it.min.js 已在 index.html 中引入。");
    return;
  }

  let gMarkdownBaseUrl = ""; // 用于解析相对链接

  function isAbsoluteUrl(url) {
    if (!url) return false;
    return /^[a-zA-Z][a-zA-Z0-9+.-]*:/.test(url) || url.startsWith("/");
  }

  function resolveMarkdownHref(rawHref) {
    const href = (rawHref || "").trim();
    if (!href) return "";

    if (!gMarkdownBaseUrl || isAbsoluteUrl(href)) {
      return href;
    }
    try {
      const base = gMarkdownBaseUrl.endsWith("/") ? gMarkdownBaseUrl : gMarkdownBaseUrl + "/";
      return new URL(href, base).toString();
    } catch (e) {
      console.error("resolveMarkdownHref error:", e, href, gMarkdownBaseUrl);
      return href;
    }
  }

  // ===============================
  // 去除 YAML Front Matter
  // ===============================
  function stripFrontMatter(md) {
    if (md.startsWith("---")) {
      const end = md.indexOf("\n---", 3);
      if (end !== -1) {
        return md.slice(end + 4).trimStart();
      }
    }
    return md;
  }

  // ===============================
  // 智能段落修复（核心功能）不会破坏链接/代码块/列表
  // ===============================
  function smartNormalizeParagraphs(md) {
    const lines = md.split("\n");
    const out = [];

    for (let i = 0; i < lines.length; i++) {
      const cur = lines[i];
      out.push(cur);

      const next = lines[i + 1] !== undefined ? lines[i + 1] : "";

      const curTrim = cur.trim();
      const nextTrim = next.trim();

      // 当前行是否为“普通正文行”
      const curNormal =
        curTrim !== "" &&
        !curTrim.startsWith("#") &&
        !curTrim.startsWith(">") &&
        !curTrim.startsWith("- ") &&
        !curTrim.startsWith("* ") &&
        !curTrim.startsWith("```") &&
        !curTrim.match(/^\|.*\|$/) &&
        // 不拆断链接
        !curTrim.match(/^\[.*\]\(.*\)$/);

      // 下一行也是正文行
      const nextNormal =
        nextTrim !== "" &&
        !nextTrim.startsWith("#") &&
        !nextTrim.startsWith(">") &&
        !nextTrim.startsWith("- ") &&
        !nextTrim.startsWith("* ") &&
        !nextTrim.startsWith("```") &&
        !nextTrim.match(/^\|.*\|$/) &&
        !nextTrim.match(/^\[.*\]\(.*\)$/);

      // 在两段纯文本之间自动插入空行（=新段落）
      if (curNormal && nextNormal) {
        out.push("");
      }
    }
    return out.join("\n");
  }

  // ===============================
  // 初始化 markdown-it
  // ===============================
  const md = window.markdownit({
    html: false,
    linkify: true,
    breaks: true, // 单换行 → <br>
    typographer: false,
  });

  // 链接解析（相对 → 绝对）
  const defaultLinkOpen =
    md.renderer.rules.link_open ||
    function (tokens, idx, opts, env, self) {
      return self.renderToken(tokens, idx, opts);
    };

  md.renderer.rules.link_open = function (tokens, idx, opts, env, self) {
    const hrefIndex = tokens[idx].attrIndex("href");
    if (hrefIndex >= 0) {
      const orig = tokens[idx].attrs[hrefIndex][1] || "";
      const resolved = resolveMarkdownHref(orig);
      tokens[idx].attrs[hrefIndex][1] = resolved;
    }
    return defaultLinkOpen(tokens, idx, opts, env, self);
  };

  // 图片链接处理
  const defaultImage =
    md.renderer.rules.image ||
    function (tokens, idx, opts, env, self) {
      return self.renderToken(tokens, idx, opts);
    };

  md.renderer.rules.image = function (tokens, idx, opts, env, self) {
    const srcIndex = tokens[idx].attrIndex("src");
    if (srcIndex >= 0) {
      const orig = tokens[idx].attrs[srcIndex][1] || "";
      const resolved = resolveMarkdownHref(orig);
      tokens[idx].attrs[srcIndex][1] = resolved;
    }
    return defaultImage(tokens, idx, opts, env, self);
  };

  function renderMarkdownToHtml(markdown) {
    return md.render(markdown || "");
  }

  // ===============================
  // 图片浮层（lightbox）
  // ===============================
  function setupImageViewerOverlay() {
    if (document.getElementById("image-viewer-overlay")) return;

    const overlay = document.createElement("div");
    overlay.id = "image-viewer-overlay";
    overlay.className = "image-viewer-overlay";
    overlay.innerHTML =
      '<div class="image-viewer-backdrop"></div>' +
      '<div class="image-viewer-content">' +
      '  <button type="button" class="image-viewer-close">×</button>' +
      '  <img class="image-viewer-img" alt="" />' +
      "</div>";

    document.body.appendChild(overlay);

    const imgEl = overlay.querySelector(".image-viewer-img");
    const closeBtn = overlay.querySelector(".image-viewer-close");
    const backdrop = overlay.querySelector(".image-viewer-backdrop");

    function hideOverlay() {
      overlay.classList.remove("visible");
      imgEl.removeAttribute("src");
    }

    function showOverlay(src, alt) {
      imgEl.removeAttribute("src");
      imgEl.alt = alt || "";
      overlay.classList.add("visible");
      imgEl.src = src;
    }

    imgEl.addEventListener("error", () => hideOverlay());

    overlay.addEventListener("click", (ev) => {
      if (ev.target === overlay || ev.target === backdrop) hideOverlay();
    });

    closeBtn.addEventListener("click", hideOverlay);

    document.addEventListener("keydown", (ev) => {
      if (ev.key === "Escape") hideOverlay();
    });

    // 拦截图片链接
    document.addEventListener("click", (ev) => {
      const a = ev.target.closest && ev.target.closest("a");
      if (!a) return;
      const href = a.getAttribute("href");
      if (!href) return;

      const ext = href.toLowerCase();
      const isImg =
        ext.endsWith(".png") ||
        ext.endsWith(".jpg") ||
        ext.endsWith(".jpeg") ||
        ext.endsWith(".gif") ||
        ext.endsWith(".webp") ||
        ext.endsWith(".bmp") ||
        ext.endsWith(".svg");

      if (!isImg) return;

      ev.preventDefault();
      showOverlay(a.href, a.textContent || "");
    });
  }

  document.addEventListener("DOMContentLoaded", setupImageViewerOverlay);

  // ===============================
  // C++ 调用的入口
  // ===============================
  window.renderMarkdown = function (markdown, title, baseUrl) {
    const contentEl = document.getElementById("md-content");
    if (!contentEl) return;

    gMarkdownBaseUrl = baseUrl || "";

    // 去掉 YAML
    markdown = stripFrontMatter(markdown);

    // 智能段落修复（不会破坏链接）
    markdown = smartNormalizeParagraphs(markdown);

    // 渲染
    const html = renderMarkdownToHtml(markdown);
    contentEl.innerHTML = html;

    document.title = title;
    const loading = document.querySelector(".md-loading");
    if (loading) loading.hidden = true;
    contentEl.hidden = false;
  };

})();
