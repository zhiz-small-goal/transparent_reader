import sys
from pathlib import Path
from datetime import date
import textwrap

from tkinter import (
    BOTH,
    END,
    LEFT,
    RIGHT,
    X,
    Y,
    StringVar,
    Tk,
    Frame,
    Label,
    Entry,
    Text,
    Listbox,
    Button,
    Scrollbar,
    messagebox,
    Toplevel,
)

try:
    # 需要先: pip install tkinterdnd2
    from tkinterdnd2 import TkinterDnD, DND_FILES
except ImportError:
    print("缺少依赖：tkinterdnd2\n请先执行: pip install tkinterdnd2")
    sys.exit(1)


# ===================== 工具函数 =====================

def get_project_root() -> Path:
    """项目根目录 = 本脚本所在目录"""
    return Path(__file__).resolve().parent


def get_bug_file() -> Path:
    """BUG_FIX_PROMPTS.md 路径: ai/BUG_FIX_PROMPTS.md"""
    project_root = get_project_root()
    ai_dir = project_root / "ai"
    return ai_dir / "BUG_FIX_PROMPTS.md"


def build_prompt_text(
    project_background: str,
    actual_behavior: str,
    expected_behavior: str,
    log_text: str,
    file_paths: list[str],
) -> str:
    """
    根据各个输入字段和文件路径，拼出完整的“模板 B” Prompt 文本。
    会自动读取文件内容，放进 ```cpp 代码块。
    """
    # 统一去掉前后空白
    project_background = project_background.strip()
    actual_behavior = actual_behavior.strip()
    expected_behavior = expected_behavior.strip()
    log_text = log_text.strip()

    # ========== 顶部固定部分 ==========
    parts: list[str] = []

    parts.append("你现在是一个熟悉 C++17 和 Qt 6 的资深桌面开发助手。")
    parts.append("")

    # ========== 项目背景 ==========
    parts.append("【项目背景（可选）】")
    if project_background:
        parts.append(project_background)
    else:
        parts.append("（略）")
    parts.append("")

    # ========== 错误现象 ==========
    parts.append("【错误现象】")
    parts.append("请根据下面的现象和日志，找出问题原因并修复：")
    parts.append("")

    # 实际现象
    parts.append("- 实际现象：")
    if actual_behavior:
        for line in actual_behavior.splitlines():
            line = line.strip()
            if line:
                parts.append(f"  - {line}")
    else:
        parts.append("  - （未填写，只有预期行为）")
    parts.append("")

    # 预期行为
    parts.append("- 预期行为：")
    if expected_behavior:
        for line in expected_behavior.splitlines():
            line = line.strip()
            if line:
                parts.append(f"  - {line}")
    else:
        parts.append("  - （未填写）")
    parts.append("")

    # ========== 日志 ==========
    parts.append("【错误日志 / 编译错误】")
    if log_text:
        parts.append(log_text)
    else:
        parts.append("（无日志，仅有行为异常）")
    parts.append("")

    # ========== 当前代码 ==========
    parts.append("【当前代码】")
    parts.append("问题应该出在下面这些文件中，请阅读完整代码：")
    parts.append("")

    project_root = get_project_root()

    if not file_paths:
        parts.append("（当前没有附带任何代码文件，如有需要请先告诉我需要哪些信息）")
        parts.append("")
    else:
        for raw_path in file_paths:
            p = Path(raw_path.strip())
            if not p.exists():
                parts.append("```cpp")
                parts.append(f"// {raw_path}  （文件不存在或路径错误）")
                parts.append("// TODO: 这里原本应该包含该文件的完整内容，但当前路径无效。")
                parts.append("```")
                parts.append("")
                continue

            try:
                code_text = p.read_text(encoding="utf-8")
            except Exception as e:  # noqa: BLE001
                parts.append("```cpp")
                parts.append(f"// {raw_path}")
                parts.append(f"// TODO: 读取文件失败：{e!r}")
                parts.append("```")
                parts.append("")
                continue

            # 尽量显示相对路径，避免太长
            try:
                rel_path = p.relative_to(project_root)
                path_label = str(rel_path)
            except ValueError:
                path_label = str(p)

            parts.append("```cpp")
            parts.append(f"// {path_label}")
            parts.append(code_text.rstrip("\n"))
            parts.append("```")
            parts.append("")

    # ========== 修改要求 ==========
    parts.append("【修改要求】")
    mod_req = textwrap.dedent(
        """\
        - 优先找出真正的原因，只改必要的地方，不要大范围重构。
        - 保持 public 接口（对外可见的类名 / 函数名 / 参数）不变，除非确实错误。
        - 尽量只修改我明确提到 / 贴出的这些文件，除非确实需要改动其它文件。
        - 你直接需要用简短文字说明关键改动点。
        - 如果有地方需要我补充信息，请用注释 `// TODO: 这里需要确认 X` 标出，不要胡乱猜测。"""
    )
    parts.append(mod_req)

    return "\n".join(parts).strip() + "\n"


def append_bug_prompt_to_file(
    title: str,
    severity: str,
    prompt_text: str,
) -> Path:
    """
    把本次构造好的 Prompt 记录到 ai/BUG_FIX_PROMPTS.md 中，方便以后回顾。
    返回该文件路径。
    """
    project_root = get_project_root()
    ai_dir = project_root / "ai"
    ai_dir.mkdir(exist_ok=True)

    bug_file = get_bug_file()
    today_str = date.today().isoformat()

    lines: list[str] = []

    if not bug_file.exists():
        lines.append("# Bug 修复请求记录")
        lines.append("")

    else:
        # 分隔前一条记录
        lines.append("\n---\n")

    title = title.strip() or "未命名问题"
    severity = severity.strip() or "未填写"

    lines.append(f"## {today_str} - {title}")
    lines.append("")
    lines.append(f"严重程度：{severity}")
    lines.append("")
    lines.append("```text")
    lines.append(prompt_text.rstrip("\n"))
    lines.append("```")
    lines.append("")

    with bug_file.open("a", encoding="utf-8") as f:
        f.write("\n".join(lines))

    return bug_file


def show_toast(root: Tk, text: str, duration_ms: int = 500) -> None:
    """右下角弹出一个小提示，duration_ms 毫秒后自动消失。"""
    toast = Toplevel(root)
    toast.overrideredirect(True)
    toast.attributes("-topmost", True)

    label = Label(
        toast,
        text=text,
        bg="#333333",
        fg="white",
        padx=10,
        pady=5,
        justify=LEFT,
    )
    label.pack()

    root.update_idletasks()
    toast.update_idletasks()

    root_x = root.winfo_rootx()
    root_y = root.winfo_rooty()
    root_w = root.winfo_width()
    root_h = root.winfo_height()

    toast_w = toast.winfo_reqwidth()
    toast_h = toast.winfo_reqheight()

    x = root_x + root_w - toast_w - 20
    y = root_y + root_h - toast_h - 20
    toast.geometry(f"+{x}+{y}")

    toast.after(duration_ms, toast.destroy)


# ===================== 主界面 =====================

def main() -> None:
    root = TkinterDnD.Tk()
    root.title("Bug 修复 Prompt 生成器（模板 B）")
    root.geometry("900x650")

    # ---------- 顶部：标题 + 严重程度 ----------
    top_frame = Frame(root)
    top_frame.pack(fill=X, padx=10, pady=10)

    Label(top_frame, text="问题标题：").grid(row=0, column=0, sticky="w")
    title_var = StringVar(value="小逻辑 / Bug 简述")
    title_entry = Entry(top_frame, textvariable=title_var, width=40)
    title_entry.grid(row=0, column=1, sticky="w", padx=(0, 20))

    Label(top_frame, text="严重程度：").grid(row=0, column=2, sticky="w")
    severity_var = StringVar(value="普通")
    severity_entry = Entry(top_frame, textvariable=severity_var, width=10)
    severity_entry.grid(row=0, column=3, sticky="w")

    # ---------- 中部：项目背景 ----------
    bg_frame = Frame(root)
    bg_frame.pack(fill=BOTH, expand=True, padx=10, pady=(0, 5))

    Label(bg_frame, text="项目背景（可选，可以粘 AI_README 里的相关段落）：").pack(anchor="w")
    bg_text = Text(bg_frame, height=4)
    bg_text.pack(fill=BOTH, expand=True)

    # ---------- 中部：实际现象 / 预期行为 ----------
    middle_frame = Frame(root)
    middle_frame.pack(fill=BOTH, expand=True, padx=10, pady=(0, 5))

    # 实际现象
    actual_frame = Frame(middle_frame)
    actual_frame.pack(side=LEFT, fill=BOTH, expand=True, padx=(0, 5))

    Label(actual_frame, text="实际现象（每行一个要点）：").pack(anchor="w")
    actual_text = Text(actual_frame, height=6)
    actual_text.pack(fill=BOTH, expand=True)
    actual_text.insert(
        "1.0",
        "例如：\n"
        "拖拽 .md 文件到窗口上没有任何反应\n"
        "点击菜单“打开”时程序直接崩溃\n",
    )

    # 预期行为
    expected_frame = Frame(middle_frame)
    expected_frame.pack(side=RIGHT, fill=BOTH, expand=True, padx=(5, 0))

    Label(expected_frame, text="预期行为（每行一个要点）：").pack(anchor="w")
    expected_text = Text(expected_frame, height=6)
    expected_text.pack(fill=BOTH, expand=True)
    expected_text.insert(
        "1.0",
        "例如：\n"
        "拖拽 .md 文件到窗口上应自动打开并显示\n"
        "菜单“打开”仅弹出文件选择对话框，不会崩溃\n",
    )

    # ---------- 中部：错误日志 ----------
    log_frame = Frame(root)
    log_frame.pack(fill=BOTH, expand=True, padx=10, pady=(0, 5))

    Label(log_frame, text="错误日志 / 编译错误（可直接粘贴终端输出）：").pack(anchor="w")
    log_text = Text(log_frame, height=6)
    log_text.pack(fill=BOTH, expand=True)

    # ---------- 下部：涉及文件（拖拽） ----------
    files_frame = Frame(root)
    files_frame.pack(fill=BOTH, expand=True, padx=10, pady=(0, 5))

    Label(files_frame, text="涉及文件（从资源管理器拖拽 .cpp/.h/.hpp 等到下面列表）：").pack(anchor="w")

    list_frame = Frame(files_frame)
    list_frame.pack(fill=BOTH, expand=True)

    files_listbox = Listbox(list_frame, selectmode="extended")
    files_listbox.pack(side=LEFT, fill=BOTH, expand=True)

    scrollbar = Scrollbar(list_frame, command=files_listbox.yview)
    scrollbar.pack(side=RIGHT, fill=Y)
    files_listbox.config(yscrollcommand=scrollbar.set)

    # 注册拖拽
    files_listbox.drop_target_register(DND_FILES)

    def on_drop(event):
        paths = event.widget.tk.splitlist(event.data)
        for p in paths:
            p = p.strip()
            if p and p not in files_listbox.get(0, END):
                files_listbox.insert(END, p)

    files_listbox.dnd_bind("<<Drop>>", on_drop)

    # 删除 / 清空按钮
    btn_frame = Frame(files_frame)
    btn_frame.pack(fill=X, pady=(5, 0))

    def remove_selected():
        selection = list(files_listbox.curselection())
        for idx in reversed(selection):
            files_listbox.delete(idx)

    def clear_all():
        files_listbox.delete(0, END)

    Button(btn_frame, text="删除选中", command=remove_selected).pack(side=LEFT)
    Button(btn_frame, text="清空列表", command=clear_all).pack(side=LEFT, padx=5)

    # ---------- 底部：生成按钮 ----------
    bottom_frame = Frame(root)
    bottom_frame.pack(fill=X, padx=10, pady=10)

    def on_generate():
        title = title_var.get().strip()
        severity = severity_var.get().strip()

        project_background = bg_text.get("1.0", END)
        actual = actual_text.get("1.0", END)
        expected = expected_text.get("1.0", END)
        logs = log_text.get("1.0", END)
        files = list(files_listbox.get(0, END))

        try:
            prompt = build_prompt_text(
                project_background=project_background,
                actual_behavior=actual,
                expected_behavior=expected,
                log_text=logs,
                file_paths=files,
            )
        except Exception as e:  # noqa: BLE001
            messagebox.showerror("生成失败", f"构造 Prompt 时出错：\n{e}")
            return

        # 写入到 ai/BUG_FIX_PROMPTS.md
        try:
            bug_file = append_bug_prompt_to_file(
                title=title,
                severity=severity,
                prompt_text=prompt,
            )
        except Exception as e:  # noqa: BLE001
            messagebox.showerror("写入失败", f"写入 BUG_FIX_PROMPTS.md 失败：\n{e}")
            return

        # 复制到剪贴板
        try:
            root.clipboard_clear()
            root.clipboard_append(prompt)
        except Exception:
            pass

        show_toast(
            root,
            f"已写入：{bug_file.name}\nPrompt 已复制到剪贴板",
            duration_ms=500,
        )

    Button(
        bottom_frame,
        text="生成 Prompt / 复制到剪贴板",
        command=on_generate,
    ).pack(side=RIGHT)

    root.mainloop()


if __name__ == "__main__":
    main()
