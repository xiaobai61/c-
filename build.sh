def_dir=/usr/local
build_part="all"
make_jobs=2

if [ -e "chaos/deps/jemalloc/lib/libjemalloc.a" ]; then
    build_part="chaos"
fi

if [ ! -z $1 ]; then
    build_part=$1
fi

if [ ! -z $2 ]; then
    def_dir=$2
fi

echo "install directory:"$def_dir

if [ "$build_part" = "all" ]; then
    cd chaos/deps/jemalloc
    sh autogen.sh --with-jemalloc-prefix=je_
    make -j${make_jobs}
    mret=$(echo $?)
    if [ $mret -ne 0 ]; then
        echo "COMPILE ERROR!!!!!!!!!!!!!!!!!!!!!"
        exit
    fi
    cd ../../../
fi

if [ "$build_part" = "all" ] || [ "$build_part" = "reconf" ]; then
    aclocal
    libtoolize
    autoconf
    autoheader
    automake --add-missing
    ./configure --prefix=$def_dir
fi

if [ "$build_part" = "all" ] || [ "$build_part" = "reconf" ] || [ "$build_part" = "chaos" ]; then
    make -C chaos -j${make_jobs}
    mret=$(echo $?)
    if [ $mret -ne 0 ]; then
        echo "COMPILE ERROR!!!!!!!!!!!!!!!!!!!!!"
        exit
    fi
#cp ./chaos/utility/libchaos_utility.a ./chaos/utility/libchaos_utility_je.a 
#cp ./chaos/libchaos.a ./chaos/libchaos_je.a 
    ar -q ./chaos/utility/libchaos_utility.a chaos/deps/jemalloc/src/*.o
    ar -q ./chaos/libchaos.a chaos/deps/jemalloc/src/*.o
fi


if [ "$build_part" = "all" ] || [ "$build_part" = "reconf" ] || [ "$build_part" = "chaos" ] || [ "$build_part" = "test" ]; then
    make -C test -j${make_jobs}
    mret=$(echo $?)
    if [ $mret -ne 0 ]; then
        echo "COMPILE ERROR!!!!!!!!!!!!!!!!!!!!!"
        exit
    fi
fi
