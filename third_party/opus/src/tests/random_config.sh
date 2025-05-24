#!/bin/bash

dir="$1"
mkdir "$dir"
if [ $? -ne 0 ]
then
        exit 1
fi

cd "$dir"
if [ $? -ne 0 ]
then
        exit 1
fi


configure_path="$2"
config="random_config.txt"

case `seq 3 | shuf -n1` in
1)
approx=--enable-float-approx
math=-ffast-math
;;
2)
approx=--enable-float-approx
;;
*)
approx=
math=
;;
esac

CFLAGS='-g'

opt=`echo -e "-O1\n-O2\n-O3" | shuf -n1`

arch=`echo -e "\n-march=core2\n-march=sandybridge\n-march=broadwell\n-march=skylake" | shuf -n1`

footprint=`echo -e "\n-DSMALL_FOOTPRINT" | shuf -n1`
std=`echo -e "\n-std=c90\n-std=c99\n-std=c11\n-std=c17" | shuf -n1`
sanitize=`echo -e "\n-fsanitize=address -fno-sanitize-recover=all\n-fsanitize=undefined -fno-sanitize-recover=all -fsanitize-recover=signed-integer-overflow -DDISABLE_PTR_CHECK" | shuf -n1`


CFLAGS="$CFLAGS $std $opt $arch $footprint $math $sanitize"

echo "CFLAGS=$CFLAGS" > "$config"

lib=`echo -e "\n--disable-static\n--disable-shared" | shuf -n1`

arithmetic=`echo -e "\n--enable-deep-plc\n--enable-dred\n--enable-osce\n--enable-dred --enable-osce\n--enable-fixed-point\n--enable-fixed-point --enable-fixed-point-debug\n--enable-fixed-point --disable-float-api\n--enable-fixed-point --enable-fixed-point-debug --disable-float-api" | shuf -n1`

custom=`echo -e "\n--enable-custom-modes" | shuf -n1`

asm=`echo -e "\n--disable-asm\n--disable-rtcd\n--disable-intrinsics" | shuf -n1`
#asm=`echo -e "\n--disable-asm\n--disable-intrinsics" | shuf -n1`

assert=`echo -e "\n--enable-assertions" | shuf -n1`
harden=`echo -e "\n--enable-hardening" | shuf -n1`
fuzz=`echo -e "\n--enable-fuzzing" | shuf -n1`
checkasm=`echo -e "\n--enable-check-asm" | shuf -n1`
rfc8251=`echo -e "\n--disable-rfc8251" | shuf -n1`
lossgen=`echo -e "\n--enable-lossgen" | shuf -n1`

if [ "$rfc8251" = --disable-rfc8251 ]
then
        vectors="$3"
else
        vectors="$4"
fi
echo using testvectors at "$vectors" >> "$config"


config_opt="$lib $arithmetic $custom $asm $assert $harden $fuzz $checkasm $rfc8251 $approx $lossgen"

echo configure $config_opt >> "$config"

export CFLAGS
"$configure_path/configure" $config_opt > configure_output.txt 2>&1

if [ $? -ne 0 ]
then
        echo configure FAIL >> "$config"
        exit 1
fi

make > make_output.txt 2>&1

if [ $? -ne 0 ]
then
        echo make FAIL >> "$config"
        exit 1
fi

#Run valgrind 5% of the time (minus the asan cases)
if [ "`seq 20 | shuf -n1`" -ne 1 -o "$sanitize" = "-fsanitize=address -fno-sanitize-recover=all" ]
then
        make check > makecheck_output.txt 2>&1
else
        echo valgrind enabled >> "$config"
        valgrind --trace-children=yes --error-exitcode=128 make check > makecheck_output.txt 2>&1
fi

if [ $? -ne 0 ]
then
        echo check FAIL >> "$config"
        exit 1
fi


rate=`echo -e "8000\n12000\n16000\n24000\n48000" | shuf -n1`
echo testvectors for "$rate" Hz > testvectors_output.txt
../../../run_vectors.sh . "$vectors" "$rate" >> testvectors_output.txt 2>&1

if [ $? -ne 0 ]
then
        echo testvectors FAIL >> "$config"
        exit 1
fi

echo all tests PASS >> "$config"

#When everything's good, do some cleaning up to save space
make distclean > /dev/null 2>&1
rm -f tmp.out
gzip make_output.txt
