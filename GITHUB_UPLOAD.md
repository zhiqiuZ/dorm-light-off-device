# GitHub 上传指南（dorm-light-off-device）

目标：把 **源码 + 文档** 推到 GitHub 公开仓库；**工具链（427MB）和本地照片不进库**
（`.gitignore` 已配好，见根目录）。本机已装 Git（`C:\Program Files\Git`），无需 gh CLI。

---

## 第 0 步 · 上架前隐私检查（Public 仓库，想 30 秒）

- [ ] 仓库将公开：`GUANGDENG / 111222345` 这套演示密码会出现在文档里。**3 米内物理可达才是真实攻击面，可接受就保持；介意的话上线前把真装置 `dorm_light.ino` 里 `AP_PASS` 改成别的再烧**，仓库保留演示值即可。
- [ ] `P/`（开关面板/房间照片）已在 `.gitignore` 排除——里面有宿舍环境细节，别手滑 `-f` 强加。
- [ ] `1.docx` 原始需求文档默认也排除（含个人信息就让它留在本地）。

## 第 1 步 · 建空仓库（网页，2 分钟）

1. 登录 [github.com](https://github.com)（没有先注册）。
2. 右上角 `+` → **New repository**。
3. 填：
   - Repository name：`dorm-light-off-device`
   - Description：`ESP8266 dorm light-off robot: dual servo + web control + offline voice (SU-03T)`
   - 可见性：**Public**（已确认）
   - ⚠️ **不要勾** "Add a README file" / ".gitignore" / "license"（本地已有内容，勾了首推会冲突）。
4. Create repository → 页面会停在一个"…push an existing repository from the command line"的指引页，别关，正好照抄你的仓库地址。

## 第 2 步 · 本地提交（PowerShell，在项目根目录）

```powershell
cd "D:\DeepSeek Harness\my_projects\light_oc"

git config user.name  "你的GitHub用户名"     # 只首次需要
git config user.email "你注册GitHub的邮箱"    # 只首次需要

git init
git add -A
git status          # ★ 看一眼：绝不能出现 tools/、P/、*.docx；有几百MB文件说明 .gitignore 没生效，停下检查
git commit -m "v2.0.0 dual-channel + v3 voice design docs"
git branch -M main
git remote add origin https://github.com/你的用户名/dorm-light-off-device.git
git push -u origin main
```

**push 时会弹出浏览器登录 GitHub 授权**（Git 自带的 Credential Manager，
登录一次以后就不用了）——这是最省事的路径，不需要手动建 token。
如果弹的是"要密码"：GitHub 已禁用账号密码，去
`Settings → Developer settings → Personal access tokens → Tokens(classic)`
生成一个勾选 `repo` 的 token，push 时密码栏粘贴它。

## 第 3 步 · 验证

刷新仓库网页：应看到 `firmware/`、`tools 不存在`、`*.md` 设计文档、
`build_firmware.bat`、`.gitignore`。点开 README/PRD 确认渲染正常。

## 第 4 步 · 日常更新（以后每次改完）

```powershell
cd "D:\DeepSeek Harness\my_projects\light_oc"
git add -A
git commit -m "这次改了什么，一句话"
git push
```

## 第 5 步 · 发版本（可选，推荐做）

网页仓库 → `Releases → Draft a new release` → tag 填 `v2.0.0` →
把 `tools\arduino\build\dorm_light.ino.bin` 拖进附件 → Publish。
别人不用装工具链也能直接拿 `.bin` 走装置网页 `/ota` 升级——这正好复用你项目自己的 OTA 故事。

---

## 故障速查

| 现象 | 处理 |
|---|---|
| `git: 无法识别` | 用 Git Shell，或把 `C:\Program Files\Git\cmd` 加进 PATH；再不行重启一次 PowerShell |
| `remote origin already exists` | 换个仓库地址时先 `git remote set-url origin 新地址` |
| push 报 403/Permission denied | 用户名/token 不对，或仓库属于别人；重新登录凭据：`git credential-manager github logout` 后重推 |
| `file... is 100 MB or larger` 被拒 | `.gitignore` 漏了大文件 → `git rm --cached 路径` 移出后重新 commit |
| 想加回 1.docx/P 照片 | 从 `.gitignore` 删对应行 → `git add -A` 再提交（想清楚再公开） |
| 命令行实在不顺 | 图形替代：装 [GitHub Desktop](https://desktop.github.com) → File → Add local repository → 选本项目文件夹 → Publish repository（勾选 Public）→ 以后点 Push 按钮即可 |
