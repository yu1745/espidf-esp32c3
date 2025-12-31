cd $args[0]
python -m esptool --chip esp32c3 merge_bin -o merged.bin 0x0 bootloader/bootloader.bin 0x10000 tcode.bin 0x8000 partition_table/partition-table.bin 0xe000 ota_data_initial.bin 0x310000 spiffs.bin
cd ..
