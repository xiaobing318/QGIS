## Description
```c
/*
Notes:杨小兵-2025-03-19

1、这部分内容描述的内容是提交一个 pull request 需要遵守的规范模板，目前提供了一个模板，不知道是否有利用价值。
2、使用一些描述文字来取代下列内容，这些文字是用来解释 PR 的合理性、细节。
3、下列注释中的内容是介绍了一些关于提交 PR 需要注意的内容。
4、什么是 "backported"？
  4.1 "Backported"（中文可以翻译为“回迁”）是一个软件开发中的术语。它指的是将一个在新版本中开发的功能、修复或改进，移植到较旧的版本中。简单来说，就是把新版本里的好东西“搬”到旧版本里，让用旧版本的人也能用上这些改进，而不需要升级到最新版。
5 为什么需要 backporting？
  5.1 在软件开发中，通常会有多个版本同时存在，比如：
    5.1.1 稳定版：经过充分测试、很可靠的版本，用户用起来比较放心。
    5.1.2 开发版：包含最新功能和修复，但可能不够稳定，有点像“试验田”。
    5.1.3 长期支持版（LTS）：专门为需要长期稳定性的用户设计的版本，会持续维护一段时间。
  5.2 有时候，开发者会在开发版里修了一个很重要的 bug，或者加了一个很实用的新功能。但稳定版的用户因为各种原因（比如系统兼容性、公司政策等）不能升级到开发版，他们也希望用上这个修复或功能。这时候，开发者就会把这些改动“backport”到稳定版，让旧版本的用户也能受益，而不会被迫升级。

*/
```

[Replace this with some text explaining the rationale and details about this pull request]

<!--
  BEFORE HITTING SUBMIT -- Please BUILD AND TEST your changes thoroughly. This is YOUR responsibility! Do NOT rely on the QGIS code maintainers to do this for you!!

  IMPORTANT NOTES FOR FIRST TIME CONTRIBUTORS
  ===========================================

  Congratulations, you are about to make a pull request to QGIS! To make this as easy and pleasurable for everyone, please take the time to read these lines before opening the pull request.

  Include a few sentences describing the overall goals for this pull request (PR). If applicable also add screenshots or - even better - screencasts.
  Include both: *what* you changed and *why* you changed it.

  If this is a pull request that adds new functionality which needs documentation, give an especially detailed explanation.
  In this case, start with a short abstract and then write some text that can be copied 1:1 to the documentation in the best case.

  Also mention if you think this PR needs to be backported. And list relevant or fixed issues.

------------------------

  Reviewing is a process done by project maintainers, mostly on a volunteer basis. We try to keep the overhead as small as possible and appreciate if you help us to do so by checking the following list.
  Feel free to ask in a comment if you have troubles with any of them.

  - Commit messages are descriptive and explain the rationale for changes.

  - Commits which fix bugs include `Fixes #11111` at the bottom of the commit message. If this is your first pull request and you forgot to do this, write the same statement into this text field with the pull request description.

  - New unit tests have been added for relevant changes

  - you have read the QGIS Developer Guide (https://docs.qgis.org/testing/en/docs/developers_guide/index.html) and your PR complies with its QGIS Coding Standards
-->
