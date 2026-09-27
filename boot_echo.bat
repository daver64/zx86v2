@echo off
echo Starting QEMU with TCP echo server port forwarding...
echo This will forward host port 2007 to guest port 7
echo Test with: telnet localhost 2007

qemu-system-i386 ^
  -m 64M ^
  -cdrom dosdisc.img ^
  -kernel kernel.bin ^
  -serial stdio ^
  -netdev user,id=net0,hostfwd=tcp::2007-:7 ^
  -device rtl8139,netdev=net0 ^
  -boot d

echo.
echo Echo server test complete.
pause