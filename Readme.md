# 直播流信息提取

> 给感兴趣的国内开发者使用的，就用中文写了，希望能有所帮助。

该工程是我自己开发的一款小工具，一直喜欢看马里奥制造的直播，于是想取直播流，结合一些图像处理技术实时的获取一些游戏信息。这个东西其实早年在SMM1（超级马里奥制造1），桀哥（斗鱼74751）使用过这样的技术（积分竞猜），想着自己也实现一个。

如果你也想为喜欢的主播做一些数据统计之类的事情，希望我的开发可以减轻你的痛苦时间，我开发的时候考虑到了拓展性，所以大部分结构都是模块化的。目前可以做到以下工作，希望以后有机会可以拓展：

- 斗鱼
  - 保存所有弹幕
  - 保存最清晰的直播流
  - 处理SMM2的直播画面
    - 自动截图多人对战开始、结束、关卡名称、参与ID、分数变化、比赛结果。
    - 录制完成后自动OCR识别上述截图中需要提取的文字信息
  - 免值守自动检测直播状态
  - 自动切片
  - 自动上传B站

## 环境配置

1. 安装 [vcpkg](https://github.com/microsoft/vcpkg)，建议安装在某磁盘的根目录。

2. 设置系统环境变量

   - `VCPKG_ROOT`: 你的vcpkg安装路径，如`C:/vcpkg`
   - `VCPKG_DEFAULT_TRIPLET`: `x64-windows`
   - `VCPKG_FEATURE_FLAGS`: `manifests,registries,versions,binarycaching`

3. 安装[Windows 10 SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)，并安装`Debugging Tools for Windows`，如图

   ![image-20210406203841321](https://i.loli.net/2021/04/06/IrScw4tBReknqfj.png)

4. 安装依赖，使用vcpkg manifests功能

   建议你先升级一下vcpkg，`git pull`然后`.\bootstrap-vcpkg.bat`，防止版本过老

   启用`Use Vcpkg Manifest`，就会使用我提供的`vcpkg.json`以及`vcpkg-configuration.json`进行安装依赖

   ![image-20210312152208606](https://typora-schwarzer.oss-cn-hangzhou.aliyuncs.com/image-20210312152208606.png)

   手动运行一次编译，vcpkg会自动下载安装依赖。**（真的会花很久时间）**

   有可能提示你的VS2019没有安装英语语言包，请打开Visual Studio Installer，修改，添加英语语言包。

5. 尽管由于你直接使用了我的工程，我认为有必要把我所做修改告诉你，避免你以后踩坑

   1. 对所有配置，平台x64，点左侧`vcpkg`，修改Triplet为x64-windows
   2. 左侧`C++ -> 预处理器`，添加`_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS`以及`V8_COMPRESS_POINTERS`，因为有一个依赖以`C++14`开发，但本工程使用了一些`C++17`的特性。后面的V8是因为对于VS传统C++控制台应用（非CMake应用），vcpkg不支持自动添加，会导致运行失败。
   3. 原因同上，需要自己去vcpkg目录（但我已经做了这一步）复制`$(VCPKG_ROOT)\installed\x64-windows\bin\snapshot_blob.bin`（Release）或者`$(VCPKG_ROOT)\installed\x64-windows\debug\bin\snapshot_blob.bin`（Debug）到程序生成目录旁边（就是sln文件旁边的x64），原因参考该[Issue](https://github.com/microsoft/vcpkg/issues/15461)。
   4. 调整`调试->工作目录`为`$(SolutionDir)$(Platform)\$(Configuration)\`，指向exe生成文件夹
   5. 左侧`C/C++ -> 命令行`添加`/utf-8 `

6. （可选）下载tesseract模型，并放在final文件夹内，开发时用的是best模型，[点这儿](https://github.com/tesseract-ocr/tessdata_best)下载，本文只用到了eng，已经放在里面了，你也可以下完整的去做其他事，挺大的（1.3G），这里的final可以在`common.h`修改

   - 只下eng

     ![image-20210405214949106](https://i.loli.net/2021/04/05/BxgPNz7qZehc524.png)

   - 所有都下

     ![image-20210107213642876](https://typora-schwarzer.oss-cn-hangzhou.aliyuncs.com/image-20210107213642876.png)

7. 修改程序输入输出目录，在common.h里，一般只用修改final的路径

   ![image-20210405215106290](https://i.loli.net/2021/04/05/96dJfLQtKbh41Pr.png)

8. 用Proxifier和Fiddler抓B站投稿工具的access_key，放在workingFolder的`bili-accesstoken.txt`，你的UID放在`bili-uid.txt`，登录斗鱼抓包拿Cookie放在`douyu-cookie.txt`

   ![image-20210405215158987](https://i.loli.net/2021/04/05/9lDNsaXMy5vRz6r.png)

## 测试样例

程序保留了一定的测试输入，你可以把`main.cpp`中的`roomId`改为74751，并在中午12点（周一除外）访问该斗鱼直播间，进行测试。

> `match_74751`保留了对该直播间SMM2环节的匹配图像，但并不适用所有主播的SMM2，因为推流设置有所不同，既然没能力改善算法，只能对每个主播画面进行截取了。