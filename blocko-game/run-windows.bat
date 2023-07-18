setlocal EnableDelayedExpansion
set cmd="which gcc || pacman -S --noconfirm mingw-w64-ucrt-x86_64-gcc ; which make || pacman -S --noconfirm make ; make windows"
c:\msys64\msys2_shell.cmd -defterm -no-start -ucrt64 -here -c %cmd% && bin || pause
