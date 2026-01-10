Import-Module .\scripts\port.psm1
$port = Find-SerialPort
mv sdkconfig sdkconfig.bak
$port = "COM49"
idf.py -p $port -B build_SHALI -D SDKCONFIG_DEFAULTS=.\sdkconfig.SHALI -D IDF_TOOLCHAIN=clang build
#  flash monitor
scripts/merge.ps1 build_SHALI
rm sdkconfig
mv sdkconfig.bak sdkconfig
