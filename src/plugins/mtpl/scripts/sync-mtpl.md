# MTPL 源码同步

使用 `sync-mtpl.py` 将本地 MTPL Git 仓库中已提交的源码同步到 QGIS MTPL 插件。同步前请确认 MTPL 仓库工作树干净，待同步内容均已提交。

## 同步当前 HEAD

```powershell
cd C:\Dev\github-repos\QGIS
& "C:\Softwares\OSGeo4W\OSGeo4W-40200\apps\Python312\python.exe" `
  .\src\plugins\mtpl\scripts\sync-mtpl.py `
  --source C:\Dev\github-repos\MTPL
```

## 同步指定提交

```powershell
cd C:\Dev\github-repos\QGIS
& "C:\Softwares\OSGeo4W\OSGeo4W-40200\apps\Python312\python.exe" `
  .\src\plugins\mtpl\scripts\sync-mtpl.py `
  --source C:\Dev\github-repos\MTPL `
  --commit 573e1665df2e46f894fd09e8a5ba03f51d90b3fe
```
