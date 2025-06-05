$user = $env:USERPROFILE
$proj = "$user\Documents\PlatformIO\Projects\nuki_bridge"
$gdb  = "$user\.platformio\packages\tool-xtensa-esp-elf-gdb\bin\xtensa-esp32-elf-gdb.exe"

python "$user\.platformio\packages\framework-espidf\components\espcoredump\espcoredump.py" `
  info_corefile -t raw `
  --gdb "$gdb" `
  -c "$proj\tmp\coredump.bin" `
  "$proj\.pio\build\esp32-poe-iso-wroom\firmware.elf"

