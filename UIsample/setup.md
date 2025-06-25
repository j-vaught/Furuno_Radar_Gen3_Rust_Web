# Setup Procedure

## Supported versions

The operation of this sample program was confirmed in wxWidgets3.1.5. <br>
Operation in other versions is not guaranteed. <br>

## Setup Procedure

### Windows(64bit)

For more information, please see [How to use wxMSW binaries](https://docs.wxwidgets.org/3.1.5/plat_msw_binaries.html). 
1. Downloading wxWidgets <br>
In [wxWidgets Download Page](https://www.wxwidgets.org/downloads/)
From Binaries/Download Windows Binaries, download and deploy Header Files and Development Files that match your Visual Studio version. <br>
You can get older versions from [GitHub Release Archive](https://github.com/wxWidgets/wxWidgets/releases). <br>
After Header Files and Development Files for Visual Studio 2013 are expanded, the directories are structured as follows. 
```
wxMSW-3.1.5_vc120_x64_Dev
    ├─wxwidgets.props
    ├─build
    │  └─msw
    ├─include
    │  ├─msvc
    │  │  └─wx
    │  └─wx
    │      ├─android
    │      ├─aui
    │      ├─dfb
    │      │  └─private
    │      ├─generic
    │      │  └─private
    │      ├─gtk
    │      │  ├─gnome
    │      │  └─private
    │      ├─gtk1
    │      │  └─private
    │      ├─html
    │      ├─meta
    │      ├─motif
    │      │  └─private
    │      ├─msw
    │      │  ├─ole
    │      │  ├─private
    │      │  ├─rt
    │      │  │  └─private
    │      │  └─wrl
    │      ├─osx
    │      │  ├─carbon
    │      │  │  └─private
    │      │  ├─cocoa
    │      │  │  └─private
    │      │  ├─core
    │      │  │  └─private
    │      │  ├─iphone
    │      │  │  └─private
    │      │  └─private
    │      ├─persist
    │      ├─private
    │      ├─propgrid
    │      ├─protocol
    │      ├─qt
    │      │  └─private
    │      ├─ribbon
    │      ├─richtext
    │      │  └─bitmaps
    │      ├─stc
    │      ├─univ
    │      ├─unix
    │      │  └─private
    │      ├─x11
    │      │  ├─nanox
    │      │  │  └─X11
    │      │  └─private
    │      ├─xml
    │      └─xrc
    └─lib
        └─vc120_x64_dll
            ├─mswu
            │  └─wx
            │      └─msw
            └─mswud
                └─wx
                    └─msw
```

2. Importing a props file<br>
Add the provided wxwidgets.props file as a property sheet to your project. 

3. Copying of dll <br>
Copy the following three dlls in WxMSW-3.1.5_vc120_x64_Dev\lib\vc120_x64_dll to the same folder as an executable. <br>
・wxbase315ud_vc120_x64.dll<br>
・wxbase315ud_xml_vc120_x64.dll<br>
・wxmsw315ud_core_vc120_x64.dll<br>

4. Defining preprocessors <br>
**trial license**<br>
You don't need to define any additional preprocessors.<br>
If FLM_COMPILE_NEW_LICENSE is defined at a C++ project property page, please delete it.<br>
**official license**<br>
Please define FLM_COMPILE_NEW_LICENSE at a C++ project property page.<br>


### Linux

For more information, please see [Compiling and getting started](https://wiki.wxwidgets.org/Compiling_and_getting_started). 

1. Downloading wxWidgets <br>
Download and extract the source code (tar. bz2 format) from [wxWidgets Download Page](https://www.wxwidgets.org/downloads/). 
You can get older versions from [GitHub Release Archive](https://github.com/wxWidgets/wxWidgets/releases). <br>

2. Building a Library <br>
Execute following commands in order. 

```sh
cd ~/wx/wxWidgets-3.1.5
mkdir gtk-build # the name is not really relevant
cd gtk-build
../configure # Add options if required
make -j3 # use 3 cores. Set to the number of cores your have. 'make' uses 1 core
sudo make install # some platforms require to use 'su' instead of 'sudo'
make clean
```

3. Defining macros <br>
**trial license**<br>
Please open ALLBUILD.Makefile and edit as follows.<br>
`LICENSE_PATTERN=-DFLM_COMPILE_NEW_LICENSE=0`<br>
**official license**<br>
Please open ALLBUILD.Makefile and edit as follows.<br>
`LICENSE_PATTERN=-DFLM_COMPILE_NEW_LICENSE=1`<br>

