# pi-touchscreen-controller
Raspberry PI Touchscreen Controller App (like a screensaver, but it is more a photoframe during idle time)


## Installation

* Compile pi-touchscreen-controller.c
* Copy pi-touchscreen-controller to /usr/local/bin
* Change parameter within start-pi-touchscreen-controller to your own definition
* Copy start-pi-touchscreen-controller to /usr/local/bin
* Install pi-touchscreen-controller.service
* * copy file to /etc/systemd/system/
* * sudo systemctl daemon-reload
* Start pi-touchscreen-controller.service
