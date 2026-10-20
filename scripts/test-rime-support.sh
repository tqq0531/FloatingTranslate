# Sourced by test scripts after changing to the project directory.
RIME_PREFIX="$(brew --prefix librime)"
clang -O2 -I"$RIME_PREFIX/include" -IBridge -c Bridge/FuyiRime.c -o build/FuyiRime-tests.o
RIME_ARGS=(-import-objc-header Bridge/FuyiRime.h build/FuyiRime-tests.o "$PWD/build/浮译.app/Contents/Frameworks/librime.1.dylib" -Xlinker -rpath -Xlinker "$PWD/build/浮译.app/Contents/Frameworks")
