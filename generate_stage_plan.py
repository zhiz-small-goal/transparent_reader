import sys
import re
from pathlib import Path
from datetime import date

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
    # 需要先 pip install tkinterdnd2
    from tkinterdnd2 import TkinterDnD, DND_FILES
except ImportError:
    print("缺少依赖：tkinterdnd2\n请先执行: pip install tkinterdnd2")
    sys.exit(1)


def get_project_root() -> Path:
    """项目根目录（脚本所在目录）"""
    return Path(__file__).resolve().parent


def get_stage_file() -> Path:
    """返回 STAGE_PLAN.md 的路径（ai/STAGE_PLAN.md）"""
    project_root = get_project_root()
    ai_dir = project_root / "ai"
    return ai_dir / "STAGE_PLAN.md"


def detect_next_stage_no() -> str:
    """
    启动时自动检测 STAGE_PLAN.md 中最后一个“阶段 X”，
    返回“下一个编号”的字符串。
    例如最后是 阶段 30 -> 返回 "31"。
    如果文件不存在或没有匹配行，就从 "0" 开始。
    """
    stage_file = get_stage_file()
    if not stage_file.exists():
        return "0"

    pattern = re.compile(r"^##\s*阶段\s+(\d+)\s*：")
    last_no: int | None = None

    try:
        with stage_file.open("r", encoding="utf-8") as f:
            for line in f:
                m = pattern.match(line)
                if m:
                    try:
                        num = int(m.group(1))
                    except ValueError:
                        continue
                    last_no = num
    except Exception:
        # 读取失败就保守从 0 开始
        return "0"

    if last_no is None:
        return "0"
    return str(last_no + 1)


def append_stage_plan(stage_no: str,
                      stage_title: str,
                      status: str,
                      goals: list[str],
                      files: list[str]) -> tuple[Path, str]:
    """
    把一条阶段记录追加到 ai/STAGE_PLAN.md。
    返回 (文件路径, 本次追加的文本内容)。
    """
    project_root = get_project_root()
    ai_dir = project_root / "ai"
    ai_dir.mkdir(exist_ok=True)

    stage_file = ai_dir / "STAGE_PLAN.md"

    today_str = date.today().isoformat()

    lines: list[str] = []
    lines.append(f"## 阶段 {stage_no}：{stage_title}")
    lines.append("")
    lines.append(f"日期：{today_str}")
    lines.append(f"状态：{status}")
    lines.append("")

    lines.append("目标：")
    if goals:
        for g in goals:
            lines.append(f"- {g}")
    else:
        lines.append("- （暂未填写）")
    lines.append("")

    lines.append("涉及文件：")
    if files:
        for p in files:
            lines.append(f"- {p}")
    else:
        lines.append("- （暂未填写）")
    lines.append("")

    section_text = "\n".join(lines) + "\n"

    if not stage_file.exists():
        header = "# 阶段计划\n\n"
        stage_file.write_text(header + section_text, encoding="utf-8")
    else:
        with stage_file.open("a", encoding="utf-8") as f:
            f.write("\n---\n\n")
            f.write(section_text)

    return stage_file, section_text


def show_toast(root: Tk, text: str, duration_ms: int = 500) -> None:
    """右下角弹出一个小提示，duration_ms 毫秒后自动消失。"""
    toast = Toplevel(root)
    toast.overrideredirect(True)  # 去掉窗口边框
    toast.attributes("-topmost", True)

    label = Label(
        toast,
        text=text,
        bg="#333333",
        fg="white",
        padx=10,
        pady=5,
    )
    label.pack()

    # 先刷新一下尺寸信息
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


def main() -> None:
    # 用 TkinterDnD.Tk 支持拖拽文件
    root = TkinterDnD.Tk()
    root.title("STAGE_PLAN 生成器")
    root.geometry("700x520")

    # ===== 上半部分：基本信息 =====
    top_frame = Frame(root)
    top_frame.pack(fill=X, padx=10, pady=10)

    # 启动时自动检测下一个阶段编号
    auto_stage_no = detect_next_stage_no()

    # 阶段编号
    Label(top_frame, text="阶段编号：").grid(row=0, column=0, sticky="w")
    stage_no_var = StringVar(value=auto_stage_no)
    stage_no_entry = Entry(top_frame, textvariable=stage_no_var, width=10)
    stage_no_entry.grid(row=0, column=1, sticky="w", padx=(0, 20))

    # 阶段标题
    Label(top_frame, text="阶段标题：").grid(row=0, column=2, sticky="w")
    stage_title_var = StringVar(value="工程骨架")
    stage_title_entry = Entry(top_frame, textvariable=stage_title_var, width=30)
    stage_title_entry.grid(row=0, column=3, sticky="w")

    # 状态
    Label(top_frame, text="状态：").grid(row=1, column=0, sticky="w", pady=(8, 0))
    status_var = StringVar(value="进行中")
    status_entry = Entry(top_frame, textvariable=status_var, width=10)
    status_entry.grid(row=1, column=1, sticky="w", pady=(8, 0))

    # ===== 中间：目标（多行文本） =====
    goals_frame = Frame(root)
    goals_frame.pack(fill=BOTH, expand=True, padx=10, pady=(0, 10))

    Label(goals_frame, text="本阶段目标（每行一个）：").pack(anchor="w")

    goals_text = Text(goals_frame, height=8)
    goals_text.pack(fill=BOTH, expand=True)

    goals_text.insert(
        "1.0",
        "例如：\n"
        "让 MainWindow 显示一个 QWebEngineView 并加载本地 index.html\n"
        "确保工程能编译并正常运行\n",
    )

    # ===== 下半部分：涉及文件（支持拖拽） =====
    files_frame = Frame(root)
    files_frame.pack(fill=BOTH, expand=True, padx=10, pady=(0, 10))

    Label(files_frame, text="涉及文件（从资源管理器拖拽到下面列表）：").pack(anchor="w")

    list_frame = Frame(files_frame)
    list_frame.pack(fill=BOTH, expand=True)

    files_listbox = Listbox(list_frame, selectmode="extended")
    files_listbox.pack(side=LEFT, fill=BOTH, expand=True)

    scrollbar = Scrollbar(list_frame, command=files_listbox.yview)
    scrollbar.pack(side=RIGHT, fill=Y)
    files_listbox.config(yscrollcommand=scrollbar.set)

    # 注册拖拽目标
    files_listbox.drop_target_register(DND_FILES)

    def on_drop(event):
        paths = event.widget.tk.splitlist(event.data)
        for p in paths:
            p = p.strip()
            if p and p not in files_listbox.get(0, END):
                files_listbox.insert(END, p)

    files_listbox.dnd_bind("<<Drop>>", on_drop)

    # 删除/清空按钮
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

    # ===== 底部：生成按钮 =====
    bottom_frame = Frame(root)
    bottom_frame.pack(fill=X, padx=10, pady=10)

    def on_generate():
        stage_no = stage_no_var.get().strip() or "?"
        stage_title = stage_title_var.get().strip() or "未命名阶段"
        status = status_var.get().strip() or "进行中"

        raw_goals = goals_text.get("1.0", END).splitlines()
        goals = [g.strip() for g in raw_goals if g.strip()]

        files = list(files_listbox.get(0, END))

        try:
            stage_path, section_text = append_stage_plan(
                stage_no=stage_no,
                stage_title=stage_title,
                status=status,
                goals=goals,
                files=files,
            )
        except Exception as e:
            messagebox.showerror("写入失败", f"写入 STAGE_PLAN.md 失败：\n{e}")
            return

        # 复制到剪贴板
        try:
            root.clipboard_clear()
            root.clipboard_append(section_text)
        except Exception:
            pass

        # 自动消失的小提示
        show_toast(
            root,
            f"已写入：{stage_path.name}\n内容已复制到剪贴板",
            duration_ms=500,
        )

        # 生成后，自动把阶段编号 +1，方便继续下一阶段
        try:
            current_no = int(stage_no)
            stage_no_var.set(str(current_no + 1))
        except ValueError:
            # 如果当前不是纯数字，就先不自动加
            pass

    Button(bottom_frame, text="生成 / 追加到 STAGE_PLAN.md", command=on_generate).pack(
        side=RIGHT
    )

    root.mainloop()


if __name__ == "__main__":
    main()
