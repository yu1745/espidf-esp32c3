mv sdkconfig sdkconfig.bak
idf.py -B build_SHALI -D SDKCONFIG_DEFAULTS=.\sdkconfig.SHALI -D IDF_TOOLCHAIN=clang build
scripts/merge.ps1 build_SHALI
rm sdkconfig
mv sdkconfig.bak sdkconfig
