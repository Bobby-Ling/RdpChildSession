# RDP 子会话

看到了 [在一个窗口里原生打开本机第二个会话? 鲜为人知的环回会话]( https://www.bilibili.com/video/BV1LPLRzPEMF) 后决定折腾一下.

## 用途

- 可以将某些软件/游戏的全屏变成窗口化(以一种高层级的方式实现Powertoys的Crop And Lock功能)
- 后台输入

## 说明

[Devolutions/MsRdpEx](https://github.com/Devolutions/MsRdpEx.git)扩展了Windows的RDP客户端mstscex.exe, 功能非常完善, 支持在.rdp文件中以如下的方式配置扩展的属性.

```rdp
ConnectToChildSession:i:1
EnableHardwareMode:i:1
EnableFrameBufferRedirection:i:1
```

推荐配合[Frame rate is limited to 30 FPS in Windows-based remote sessions](https://learn.microsoft.com/en-us/troubleshoot/windows-server/remote/frame-rate-limited-to-30-fps)使用.

## 限制和已知问题

- 浏览器、Steam等不能在一个主机和子会话中同时运行
- 在BUG10系统上已经关闭的"咨询和兴趣"会突然出现并且无法通过右键任务栏选项关闭...

## 参考

> - [Child Sessions](https://learn.microsoft.com/en-us/windows/win32/termserv/child-sessions)
> - [Frame rate is limited to 30 FPS in Windows-based remote sessions](https://learn.microsoft.com/en-us/troubleshoot/windows-server/remote/frame-rate-limited-to-30-fps)
> - [Devolutions/MsRdpEx](https://github.com/Devolutions/MsRdpEx.git)
> - [Yamatohimemiya/RDCMan](https://github.com/Yamatohimemiya/RDCMan.git)
> - [CN116016480B - 基于虚拟桌面的流程自动化控制方法及系统](https://patents.google.com/patent/CN116016480B/zh) (居然有人拿这个申请专利...)