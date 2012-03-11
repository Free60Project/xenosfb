rm /usr/lib/xorg/modules/drivers/xenosfb_drv.so
cp src/.libs/xenosfb_drv.so /usr/lib/xorg/modules/drivers/xenosfb_drv.so
killall Xorg
