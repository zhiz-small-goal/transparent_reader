// 文件：resources/web/main.js
// 作用：使用 markdown-it 渲染 Markdown，并处理链接解析 + 图片浮层

(function () {
  // ===============================
  // Markdown-it 初始化
  // ===============================
  if (!window.markdownit) {
    console.error(
      "markdown-it 未加载，请确认 markdown-it.min.js 已在 index.html 中引入。"
    );
    return;
  }

  // 当前文档的 baseUrl（例如 file:///F:/.../md_convert/）
  let gMarkdownBaseUrl = "";

  function isAbsoluteUrl(url) {
    if (!url) return false;
    return /^[a-zA-Z][a-zA-Z0-9+.-]*:/.test(url) || url.startsWith("/");
  }

  function resolveMarkdownHref(rawHref) {
    const href = (rawHref || "").trim();
    if (!href) return "";

    // 已经是绝对地址（http/https/file/mailto 等）或缺少 baseUrl 时，原样返回
    if (!gMarkdownBaseUrl || isAbsoluteUrl(href)) {
      return href;
    }

    try {
      const base = gMarkdownBaseUrl.endsWith("/")
        ? gMarkdownBaseUrl
        : gMarkdownBaseUrl + "/";
      return new URL(href, base).toString();
    } catch (e) {
      console.error("resolveMarkdownHref error:", e, href, gMarkdownBaseUrl);
      return href;
    }
  }

  // 创建 markdown-it 实例（行为大致和 VS Code 预览一致）
  const md = window.markdownit({
    html: false, // 不允许原始 HTML，避免样式被破坏
    linkify: true, // 识别裸露链接
    breaks: false,
    typographer: false,
  });

  // 自定义链接渲染：把相对链接改成基于 baseUrl 的绝对地址
  const defaultLinkOpen =
    md.renderer.rules.link_open ||
    function (tokens, idx, options, env, self) {
      return self.renderToken(tokens, idx, options);
    };

  md.renderer.rules.link_open = function (tokens, idx, options, env, self) {
    const hrefIndex = tokens[idx].attrIndex("href");
    if (hrefIndex >= 0) {
      const orig = tokens[idx].attrs[hrefIndex][1] || "";
      const resolved = resolveMarkdownHref(orig);
      tokens[idx].attrs[hrefIndex][1] = resolved;
    }
    return defaultLinkOpen(tokens, idx, options, env, self);
  };

  // 自定义图片渲染：同样修正 src（支持 ![](media/xxx.png) 这类）
  const defaultImage =
    md.renderer.rules.image ||
    function (tokens, idx, options, env, self) {
      return self.renderToken(tokens, idx, options);
    };

  md.renderer.rules.image = function (tokens, idx, options, env, self) {
    const srcIndex = tokens[idx].attrIndex("src");
    if (srcIndex >= 0) {
      const orig = tokens[idx].attrs[srcIndex][1] || "";
      const resolved = resolveMarkdownHref(orig);
      tokens[idx].attrs[srcIndex][1] = resolved;
    }
    return defaultImage(tokens, idx, options, env, self);
  };

  function renderMarkdownToHtml(markdown) {
    return md.render(markdown || "");
  }

  // ===============================
  // 标题 & 内容显示控制
  // ===============================
  function setDocumentTitle(title) {
    if (!title) return;
    document.title = title;
  }

  function showContent() {
    const root = document.getElementById("md-root");
    const loading = root && root.querySelector(".md-loading");
    const content = document.getElementById("md-content");
    if (loading) loading.hidden = true;
    if (content) content.hidden = false;
  }

  // ===============================
  // 图片查看浮层（Lightbox）
  // ===============================
  function setupImageViewerOverlay() {
    if (document.getElementById("image-viewer-overlay")) {
      return;
    }

    const overlay = document.createElement("div");
    overlay.id = "image-viewer-overlay";
    overlay.className = "image-viewer-overlay";
    overlay.innerHTML =
      '<div class="image-viewer-backdrop"></div>' +
      '<div class="image-viewer-content">' +
      '  <button type="button" class="image-viewer-close" aria-label="关闭图片">×</button>' +
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

    function showOverlay(src, altText) {
      imgEl.removeAttribute("src");
      imgEl.alt = altText || "";
      overlay.classList.add("visible");
      imgEl.src = src;
    }

    // 图片加载失败：自动关闭浮层并提示（后续可以改成 toast）
    imgEl.addEventListener("error", function () {
      const failedUrl = imgEl.src;
      hideOverlay();
      console.warn("图片加载失败或文件不存在:", failedUrl);
      alert("图片加载失败或文件不存在：\n" + failedUrl);
    });

    // 背景点击关闭
    overlay.addEventListener("click", function (ev) {
      if (ev.target === overlay || ev.target === backdrop) {
        hideOverlay();
      }
    });

    // 关闭按钮
    closeBtn.addEventListener("click", function () {
      hideOverlay();
    });

    // Esc 关闭
    document.addEventListener("keydown", function (ev) {
      if (ev.key === "Escape") {
        hideOverlay();
      }
    });

    // 拦截图片链接点击（以图片扩展名结尾的 <a href="xxx">）
    document.addEventListener("click", function (ev) {
      const a = ev.target.closest && ev.target.closest("a");
      if (!a) return;

      const href = a.getAttribute("href");
      if (!href) return;

      const lowerHref = href.toLowerCase();
      const isImage =
        lowerHref.endsWith(".png") ||
        lowerHref.endsWith(".jpg") ||
        lowerHref.endsWith(".jpeg") ||
        lowerHref.endsWith(".gif") ||
        lowerHref.endsWith(".webp") ||
        lowerHref.endsWith(".bmp") ||
        lowerHref.endsWith(".svg");

      if (!isImage) return;

      ev.preventDefault();

      const resolvedUrl = a.href; // 此时已经是绝对路径
      const altText =
        a.textContent ||
        a.getAttribute("title") ||
        a.getAttribute("alt") ||
        "";
      showOverlay(resolvedUrl, altText);
    });
  }

  document.addEventListener("DOMContentLoaded", function () {
    try {
      setupImageViewerOverlay();
    } catch (e) {
      console.error("setupImageViewerOverlay error:", e);
    }
  });

  // ===============================
  // 暴露给 C++ 的渲染入口
  // ===============================
  // C++ 调用：window.renderMarkdown(markdown, title, baseUrl)
  window.renderMarkdown = function (markdown, title, baseUrl) {
    const contentEl = document.getElementById("md-content");
    console.log("BASE=", baseUrl);
    if (!contentEl) return;

    if (typeof baseUrl === "string" && baseUrl.length > 0) {
      gMarkdownBaseUrl = baseUrl;
    } else {
      gMarkdownBaseUrl = "";
    }

    const html = renderMarkdownToHtml(markdown);
    contentEl.innerHTML = html || "<p><em>（空文档）</em></p>";

    setDocumentTitle(title);
    showContent();
  };

  
})();
