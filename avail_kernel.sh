sudo grep -P "^menuentry" /boot/grub/grub.cfg |cut -d\' -f2| \
perl -e 'my $i = 0; while(<>) {$i++; next if /systemd/; chomp; printf "%d: $_\n",$i-1; }' 2>/dev/null
