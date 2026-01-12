# Development

Distrobox Fedora:
```shell
$ distrobox enter clang-dev
```

Installation des dépendances du projet:
```shell
$ sudo dnf install -y     \
    clang               \
    clang-tools-extra   \
    llvm                \
    make                \
    libX11-devel        \
    mesa-libGL-devel
    
$ sudo dnf install -y   \
    libX11-devel        \
    libXi-devel         \
    mesa-libGL-devel    \
    glfw-devel
```

Compilation:
```shell
$ mkdir deps/cglm/build-static && cd deps/cglm/build-static
$ cmake ..   -DCGLM_SHARED=OFF   -DCGLM_STATIC=ON   -DCMAKE_BUILD_TYPE=Release   -DCMAKE_POSITION_INDEPENDENT_CODE=ON   -DCMAKE_INSTALL_PREFIX=$HOME/.local
$ make -j$(nproc) && make install
$ ls $HOME/.local/lib64/
 cmake   pkgconfig   libcglm.a
# pas super propre de linker à la main mais bon ... à voir pour rendre ça plus clean/dynamique
$ cat Makefile | grep cglm
# CGLM_CFLAGS := $(shell pkg-config --cflags cglm)
# CGLM_LIBS := $(shell pkg-config --libs cglm)
CGLM_LIBS := $(HOME)/.local/lib64/libcglm.a

$ /usr/bin/time /bin/sh -c 'make clean && make -j$(nproc)'
rm -rf build
rm -rf build/app
mkdir -p build
clang -std=c99 -Wall -Wextra -O2 -Isrc -Iinclude -Ideps/glad  -I/home/latty/.local/include src/main.c deps/glad/glad.c -o build/app -lglfw /home/latty/.local/lib64/libcglm.a -lm -ldl
3.78user 0.10system 0:03.93elapsed 99%CPU (0avgtext+0avgdata 135892maxresident)k
0inputs+648outputs (0major+36157minor)pagefaults 0swaps

$ file build/app
build/app: ELF 64-bit LSB executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=bc0d9823ee482c66edef683026ed1f8b04dcab69, for GNU/Linux 3.2.0, not stripped

# à noter pas de cglm qui est build en statique 
$ ldd build/app
	linux-vdso.so.1 (0x00007fe0d55e4000)
	libglfw.so.3 => /lib64/libglfw.so.3 (0x00007fe0d555c000)
	libm.so.6 => /lib64/libm.so.6 (0x00007fe0d5467000)
	libc.so.6 => /lib64/libc.so.6 (0x00007fe0d5274000)
	/lib64/ld-linux-x86-64.so.2 (0x00007fe0d55e6000)	
```

Lint & format:
```shell
$ make format
clang-format -i src/main.c deps/glad/glad.c
```

TODO: Glad et récupération des sources générées/targétées
https://github.com/Dav1dde/glad
https://gen.glad.sh/
