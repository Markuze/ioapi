sudo grep -P "\Wstartup_64" /boot/System.map-4.14.0-kaslr
sudo grep -P "\Wargv_split" /boot/System.map-4.14.0-kaslr
sudo grep -P "\Wcall_usermodehelper$" /boot/System.map-4.14.0-kaslr

sudo cat /proc/kallsyms | sudo grep -P "\Wstartup_64"
sudo cat /proc/kallsyms |sudo grep -P "\Wargv_split"
sudo cat /proc/kallsyms |sudo grep -P "\Wcall_usermodehelper$"
