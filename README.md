# vncsnapshot

VNC Snapshot is a command line utility for VNC (Virtual Network Computing) available from RealVNC, among others. The utility allows one to take a snapshot from a VNC server and save it as a JPEG file. Unix, Linux and Windows platforms are supported.

## Fork

This is a fork of the original vncsnapshot utility published on sourceforge [VNCSnapshot](https://sourceforge.net/projects/vncsnapshot/).

The initial import was using the codebase from sourceforge as of 2017-07-13.

## Features and Usage

VNC Snapshot includes all standard VNC viewer options, except those that apply to the viewer's window.

VNC Snapshot can only be used from the command line and always connects to the server in 'shared' mode.

It can be invoked in several ways:

    vncsnapshot options host:display JPEG-filename

    vncsnapshot options host:port JPEG-filename

    vncsnapshot options -listen local-display JPEG-filename

    vncsnapshot options -tunnel host:display JPEG-filename

    vncsnapshot options -via gateway host:display JPEG-filename

    The -listen, -tunnel and -via options have not been tested on Windows systems.

Options:

    -cursor 
    -nocursor	 	Attempt to include, or exclude, the mouse cursor from the snapshot. Currently, this works only when the remote server is TightVNC 1.2.7 or later. Other servers do not respond to these options and may or may not include the cursor in the snapshot.
    -passwd filename	Read encrypted password from filename instead of from the console. The filename can be made with the vncpasswd utility included in the vncsnapshot distribution.
    -encodings list	Use the given encodings. The default is 
			    "copyrect tight hextile zlib corre rre" 
			or 
			    "raw copyrect tight hextile zlib corre rre" 
			when VNC snapshot and the server are on the same machine.
  -compresslevel level	Compress network messages to level, if the server supports it. level is between 0 and 9, with 0 being no compression and 9 the maximum. The default is 4.
  -allowblank 
  -ignoreblank	 	Allow, or ignore, blank (all black) screens from the server. The default is to ignore blank screens, and to wait for the first non-blank screen instead. This is useful with some versions of RealVNC, which send an all-black screen initially before sending the actual screen image.
  -vncQuality quality	Use the specified image quality level (0-9) for tight encoding. The default is 9.
  -quality quality	Use the specified JPEG image quality (0-100) for the output file. The default is 100.
  -quiet		Do not print any messages. Opposite of -verbose.
  -verbose		Print messages; default.
  -rect wxh+x+y	 	Save a sub-rectangle of the screen, width w height h offset from the left by x and the top by y.
			A negative number for x or y makes it an offset from the opposite edge.
			A zero value for the width or height makes the snapshot extend to the right or bottom of the screen, respectively.
			The default is the entire screen.
  
  -count number	 	Take number snapshots; default 1. If greater than 1, vncsnapshot will insert a five-digit sequence number just before the output file's extension; i.e. if you specify out.jpeg as the output file, it will create out00001.jpeg, out00002.jpeg, and so forth.
  -fps rate	 	When taking multiple snapshots, take them every rate seconds; default 60.
  
## Our changes

2017-07-13	Accept a password from STDIN if input is not a TTY:

		echo -n "SECRET" | vnsnapshot ...
    
2017-07-13 	Accept a password from VNC_TICKET environemnt variable:

		VNC_TICKET="SECRET"
		vncsnapshot ...

2017-07-13 	Ignore server messages not implemented

## Build

On Windows systems, please read BUILD.win32.

On Unix and Linux systems, please read BUILD.unix.

## COPRIGHT / LICENSE

vncsnapshot was published under the GNU General Public License version 2.0 (GPLv2)

The original code was adapted from the TightVNC viewer by Grant McDorman, February 2002.

TightVNC is Copyright (C) 2001 Const Kaplinsky.  All Rights Reserved.
VNC is Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
This software is distributed under the GNU General Public Licence as published
by the Free Software Foundation.
