# Lauss

Proof-of-concept blocking ad banners in LINE clients on Windows

## Disclaimers

In addition to the lack of warranty as the way this software is distributed, the use of this software may lead to violations of the LY Corporation Common Terms of Use and/or any other agreement that you have agreed with LY Corporation. Please be aware of that you use this software at your own risk.

## Introduction

LINE is an instant messaging service particularly popular in East Asia. It is a considerably decent product except for this one gigantic pain in the ass: ill-willed ad banners. This project aims to demonstrate that blocking those vicious ad banners in the Windows version is achievable.

### Rationale

Linking a phone number to your LINE account is not a requirement for using the service, while highly encouraged by its developer and provider, LY Corporation. Unfortunately, if you do share this valuable personal data with the company, you will begin to see ad banners getting embedded in different corners on your LINE clients, regardless of the platform. The reason seems to be that they now know which country you live in and thus they are confident enough to send you targeted ads based on that geolocation data.

Asking and gathering users' personal data to generate revenue are generally fine as the company provides their services free-of-charge in return. However, when a user complies to the company's greed for ever more data, instead of giving back some rewards, LY Corporation decided to dump them all sorts of rubbish out of gratitude. In plain terms, the company slaps users in the face when the users *voluntarily* give in more personal data.

## How to use

1. A LINE client must already be active in the system. Login is not required though.
2. Execute `Launcher.exe`.
3. You might need to close and re-open LINE client's main window in order to see any effect.

## How to build

Source code and solution/project files in this project are maintained with Visual Studio 2022. Everything needed to build the software is already in the repository. Just hit the build button and you should be good to go. Output executables can be found in the `bin/` folder.

### Additional notes

The Windows version of LINE's client is still built as a 32-bit application. Lauss uses a variant of DLL injection techniques to load `Payload.dll` into LINE client's memory space, so it also has to be 32-bit.

## Copyright

Copyright (C) 2023 Mifan Bang <https://debug.tw>.

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
