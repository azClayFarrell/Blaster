Author: Clay Farrell

This project is part of a video lecture series on Udemy by Stephen Ulibarri.

The purpose of the project is to make a Multiplayer Third Person Shooter game using Unreal Engine 5.

Visual assets are not included, only the code that utilizes them, so the project will therefore not
function as intended by simply downloading the source code from this repository and running it.

CURRENT STATE:
In the projects current state, players are able to start a game session by hosting a server locally. Other players
are then able to connect to a game session that is in progress using peer to peer connection. Movement, projectiles,
and other interactive aspects of the game are replicated across the network for a consistent experience between the
host and clients.

IMPROVEMENTS:
Some of the files in the project are many lines too long and do multiple things.
If I were remaking this project on my own, I would make more use of Unreal Engines Component type. Components allow
functionality to be abstracted out and placed in their own files, then reused by different classes that require the
exact same functionality. Managing a characters health would be an example of a good use case for something like this
since health would be handled the same or nearly the same for player characters, destructible objects, and non-player
characters.
There are a few functions and files in the project where the documentation is sparse. I felt comfortable doing this
since I have the video lectures as a reference if I needed to see why something was coded in a particular way. If
I was working on this on my own without the videos as a resource I would have been more diligent with the documentation.
