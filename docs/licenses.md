# Open Source Licenses
This file lists all the opensource licenses that are being used by this project,
and how this project complies with them.  

All the content in this document is copyrighted by the following authors:  
* Copyright (c) 2024 by Lior Shalmay <liorshalmay1@gmail.com>  

## LGPL3
### Used libraries
#### [ansi_colours](https://github.com/mina86/ansi_colours)
##### Files that are being used from the project
* `ansi256.c`
* `ansi_colours.h`
##### copyright notice
Copyright 2018 by Michał Nazarewicz <mina86@mina86.com>

### License Complience explained
First of all, by default all LGPL3 libraries are opted out of the build process, thus the produced code does not contain 
any LGPL3 code, and no LGPL3 source code is downloaded to the computer by the build process of this project.  

Secondly, we the maintainers and contributors obligate to not distribute any binaries that contain LGPL3 protected code, 
which were produced by this code base. Thus, this code can remain under MIT license, since the LGPL3 license covers the 
code regarding distributed binaries, and the distributed binaries does not contain LGPL3 code, therefore the code base does
not need to be licensed under LGPL3.  
In addition, precaution measures where taken to notify the user about the LGPL3 license, and the consequences of building this 
code base with LGPL3 components. Therefore, it's the end user's responsibility to distribute this code base and the binaries 
that he compiled under the LGPL3 license, we do not take any responsibility, nor accountability on the end user's choices and actions, 
we just give him the freedom to choose if he is going to use LGPL3 components.

Thirdly, the original libraries code is left unmodified, therefore the code modification related sections are irrelevant.

In conclusion, all the copyright notices and LGPL3 notices were provided to the users, and the OPT-IN mechanism makes sure
that it's the end users choice to use LGPL3 components and not the maintainers choice. Therefore, this code base is distributed
under MIT license while being compatible with the restrictions of the LGPL3 license.

### Personal Notes on the LGPL3 and GPL3 licenses
There is some hypocrisy among the GNU community, while they fight for user's "freedom" and "protect" it, the end result
is the complete opposite freedom is taken both from the end users and from the opensource code developers.  

For the end user the freedom is taken by the form of less features available on the distriputed software, because there are 
certain features that come from libraries/source code that are incompatible with GNU license so when recieving a GNU protected
binary (and vice versa), you have less features, and if you want the lost features back, you need to compile the software from 
source, without the GNU protected parts which can sometimes be very tidious and praticly imposiable for novice users.  
In example, the git openssl backend is not available when git is linked with GNU, because openSSL is incompatible with GNU.
Forcing the end user to compile git himself (not an easy task) and this will occur in any GNU based linux distribution which is 
insane. In contrast using Apple's propriety software gives the user better experience because there are no GNU shenanigans.  

For the developer the rapist conditions that the GNU license imposes leaves the developer with 3 unattractive options:
The first option is to ignore any GPL protected code, thus limiting his available options to incorporate from the open source 
community. So his freedom is taken by limiting the available options which are in the firstplace shared so the community/humanity
can enjoy the code/solution that have already been written.  

The second option that the devloper has is to surrender to GPL's license thus making his entire code protected by GPL's license
and now he is raping his users to use GPL's license.  

The third option that the developer has is to find ways around the licnse (which is getting harder and harder with each generation),
experience which is similar to navigating in a mine field, especially when developers are not usualy lawyers and independent
developers usually do not have the resources nor time to deal with open source licenses, leaving them wihtout freedom.  

In conclusion, the end result is that the GPL licenses "eat" other less restrictive licenses and forcing any project that 
"touches" a GPL software to also become a GPL software so over time more and more projects are licensed under GPL making it
harder and harder to not use GPL code each passing day.  
When will this madness stop?  
When will developer start complain against the rapist conditions that GPL3 license imposes?  


In this porject, I needed a certain logic that converts RGB to ANSI-256 color in a very time and space efficient way. There aren't
many available solutions (this is the only reasonable one that I have found). For me it was either using this solution or to drop 
color support for terminals that do not support ansi-full-colour yet. 
Because the code is protected under GPL I needed to invest a lot of time and effort in order to find a way around, plus writing all 
these texts to show that I am complying with GNU's restrictions.  
So I have wasted all this time on the licnese instread of developing code, and the end result is that the user will not even enjoy 
the added feature out of the box, he will instead need to go to the code base himself and compile the binary by himself so for GNU 
to be happy. Both me and the end user got screwed. No body is benefiting from this beside GNU who get their name and conditions 
posted on everything.  

GNU please change you conditions to more senseable conditions that will benefit both the developers and the end users.

Everyone is premitted to redistribute the above text under one condition, to keep the copyright notice.

