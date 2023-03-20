# dxva-gles-interop

## install angle

https://github.com/google/angle

```
cd workdir
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat

# 32-bit version
.\vcpkg\vcpkg install angle

# 64-bit version
.\vcpkg\vcpkg install angle:x64-windows
```

note: if get fetch error in git, set git proxy with cmd "git config --global http.proxy=http://xxx.xxx.xxx:yyy" 

## set angle path

create environment variable "ANGLE_PATH" and set it to the path where ANGLE is installed by vcpkg, such as
ANGLE_PATH=D:\code\angle_code\vcpkg\installed\x64-windows

