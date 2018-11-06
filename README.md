## Overview
This is the source code of Multiness, a NES emulator supporting online multiplayer, which first created in 2016:
https://lehoangquyenblog.wordpress.com/published-games/74-2/

This project is a fork of the original Nestopia source code, plus Android (main focus), MacOS, WinRT & iOS  port.
The purpose of the project is to enhance the original, adding Online multiplayer features.

I created this software a long time ago to play online with my oversea friend. Some of the code are quite hackish,
because I had to quickly experiemnt the best possible solution for the network lags, so please don't mind them.
And if you can, clean them up if you want. Some of the multiplayer handling code should be in a dedicated file. 
But currently, I put them in `source/core/NstMachine.cpp` which might look messy.

## Notes: 
* Only Android is actively maintained currently. Restrictions Apple & Microsoft put on their respective platforms discourage me
from continuing support them. So expect iOS, WinRT projects to be broken in latest revision of the code.
* To build Android app:
    * Run `git submodule update --init --recursive` before building.
	* Open `projects/android` in Android Studio.
* Prerequisites for building OSX/iOS version:
    * automake, autoconf, pkgconfig, libtool.
	* run `init_thirdparty.sh` script.

## Multiplayer mechanisms:
* The core mechanism of multiplayer feature is actually very close to video streaming. The host player will render a frame and stream it to 2nd player.
* Pairing people online is done by Google Play Game Services or Facebook invite system.
* Note that Google Play Game Services can connect two 2 people p2p on its own but I didn't use it because the overhead is too high. It has to convert C packet to java byte array before sending out.
* So the p2p connection will be handled by RakNet, it will perform NAT punchthrough technique (this requires dediced server on the cloud) to find the public IP of 2 players and connect them.
* I didn't use any standard video codec in the industry. Instead I defined my own streaming method and compression which currently work fine for this emulator.
* The simple overview of this streaming method:
    * Compress and send out only the deltas between current frame and a key frame before it. 
    * Frame is sent out as key frame at the start of each defined interval.
    * The system will detect the network condition and attempt to reduce the framerate/resolution of streaming if the condition is poor
and try to increase framerate/resolution back to normal if the condition is good enough.

## License
* __Multiness__: This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version. Note that assets & resources files are not
covered by this license, see Assets section below.
* __Nestopia core__: See AUTHORS for list of authors. Can be redistributed and/or
modified under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License,
or (at your option) any later version.
* __RakNet__: (c) 2014, Oculus VR, Inc. BSD License.
* __miniupnp__: (c) 2005-2017, Thomas BERNARD. BSD License.

## Assets
### Multiness's icon and onscreen buttons
Images are distributed under the [Creative Commons Attribution-NonCommercial 4.0 International](https://creativecommons.org/licenses/by-nc/4.0/) license.  
Copyright held by Le Hoang Quyen.

#### The images are included in:
* `projects/android/feature_graphics2.xcf`
* `projects/android/app/src/main/assets/*.png`
* `projects/android/app/src/main/res/mipmap-*/ic_launcher.png`
* `projects/android/app/src/gpg/res/mipmap-*/ic_launcher.png`
* `raw_resources/icon.xcf`
* `raw_resources/icon_gpg.xcf`
* `raw_resources/buttons.xcf`
* `raw_resources/dpad.xcf`
* `raw_resources/dpad_highlight.xcf`
* `raw_resources/start_select.xcf`
* `raw_resources/start_select2.xcf`

### Other assets
* Original NES controller icon: Thanks to Ryan Dardis and Creative Stall from the Noun Project. [Creative Commons Attribution 3.0](https://creativecommons.org/licenses/by/3.0/) license.
* Donation, World and LAN icons: Designed by Freepik and distributed by Flaticon. [Flaticon basic license](https://file000.flaticon.com/downloads/license/license.pdf).
* Material Design Icons: from Google's https://material.io/tools/icons/?style=baseline. Licensed under Apache License 2.0.

##
[![donate](https://www.paypalobjects.com/webstatic/en_US/i/btn/png/btn_donate_92x26.png)](https://paypal.me/HQgame)  If you like this project. You can do it via Paypal.
