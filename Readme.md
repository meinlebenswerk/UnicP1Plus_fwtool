# Unic P1+ Firmware tool

Short writeup of my experiences with the Unic P1+'s firmware.
(Unfortuntaly I broke the device, please read carefully if you want to try this on your own. Also if you have any Idea how to fix a botched nand, please get in touch) 

Okay, the story is simple, there is this cute, tiny Wifi projector, the Unic P1+. It *could* have been such a great device...

Unfortunately streaming via Wifi is fairly laggy and the promised support for directly streaming Video from and Android device (via otg) doesn't work. This would be a killer feature, since that should have delivered higher frame-rates as well as extended battery-life (at least on the projector side).

-> So i tried to break in!

### The web-interface

After watching way to many youtube-videos on ctfs, I started by nmapping the device which yielded:


```
# Nmap 7.80 scan initiated Fri May 29 12:27:22 2020 as: nmap -vv -sC -sV -sN -oN nmap/initial_wifi_on 192.168.168.1
Nmap scan report for _gateway (192.168.168.1)
Host is up, received arp-response (0.0050s latency).
Scanned at 2020-05-29 12:27:23 CEST for 204s
Not shown: 995 closed ports
Reason: 995 resets
PORT     STATE SERVICE          REASON       VERSION
53/tcp   open  domain           tcp-response dnsmasq 2.72
| dns-nsid: 
|_  bind.version: dnsmasq-2.72
7000/tcp open  afs3-fileserver? tcp-response
|_irc-info: Unable to open connection
7001/tcp open  afs3-callback?   tcp-response
7100/tcp open  font-service?    tcp-response
8080/tcp open  http             tcp-response thttpd 2.25b 29dec2003
| http-methods: 
|_  Supported Methods: GET HEAD
|_http-server-header: thttpd/2.25b 29dec2003
|_http-title: Index of /
MAC Address: D6:B7:61:56:D9:87 (Unknown)

Read data files from: /usr/bin/../share/nmap
Service detection performed. Please report any incorrect results at https://nmap.org/submit/ .
# Nmap done at Fri May 29 12:30:47 2020 -- 1 IP address (1 host up) scanned in 204.90 seconds
```
#### Thttpd
The version of thttpd should apparently be exploitable, see [CVE 2003-0899](https://www.google.com), but I could not get that to run, neither on my device nor on the projector itself.

But! I runs a file-explorer, and apparently it can also be used to serve cgi-scripts. But I didn't find anything great there, either...

#### dnsmasq
There's a whole lot of Exploits for dnsmasq < 2.78, see [exploit-db](https://www.exploit-db.com/search?q=dnsmasq) for more information. I also tried to get that to run, messed my dns-config up for good, but apart from that, nothing.


#### firmware
I obtained a Firmware update for it (I won't share it here, I don't want to get sued ;) )

I ran binwalk on the firmware update and I found it contains basically an header + an ext2 rootfs, bingo!


##### default root password
Poking around the rootfs and locating `/ect/shadow` reveals: `root:$1$EIYYGRBK$inOu3EFhrsNF0FzaGDozn.:14610:0:99999:7:::`, which decodes to "am2016".

Once extracted, the rootfs could easily be mounted and inspected and modified. But whenever I treid to change it and copied it back into the update-package, it wouldn't install.

After a bit of trial and error, I found a firmware-update ko that, who would have guessed, handles firmware updates. It includes a function FirmCheckSumFlow or something similar, which calculates image checksum checking.
I reversed the code and wrote a small c-implementation which is in `main.c`.

The script should be fairly self-explanatory, It takes in the update-binary (DOW_PX.bin) + a new rootfs and packs them into a new image and updates the checksum in the header.

And hooray, I can now update the firmware!

I modified the thttpd.conf to serve the root-fs and enable cgi-serving. Unfortunately the projector does not run netcat, ect... So I could not get a reverse-shell from that :/

But I could get it to run `cat /proc/version`:
```
Linux version 2.6.27.29 (root@ubuntu) (gcc version 4.3.3 (Sourcery G++ Lite 4.3-154) ) #15 PREEMPT Mon Jan 1 22:59:58 PST 2018
```

And `cat /proc/cpuinfo`:
```
system type		: MIPS AM7X
processor		: 0
cpu model		: MIPS 24K V4.12
BogoMIPS		: 321.53
wait instruction	: yes
microsecond timers	: yes
tlb_entries		: 32
extra interrupt vector	: yes
hardware watchpoint	: yes
ASEs implemented	: mips16 dsp
shadow register sets	: 2
core			: 0
VCED exceptions		: not available
VCEI exceptions		: not available
```



#### Other Notes

What's very interesting is: 

[Gianluca Pacchiella](https://github.com/gipi) took apart a [MiraScreen-device](https://github.com/gipi/teardown/tree/master/MiraScreen), whose cpu and OS seems to be fairly similar, but it has some small differences (root password, actual cgi-files).


## Big Issue:
- Well yes, the code, I apologize, I haven't properly coded C for a long time :/
- But also !!DON'T MAKE THE ROOTFS LARGER!!, there is no check in the firmware and I got too confident and broke my projector ://
  -> If anyone has an Idea how to fix this, please get in touch!

