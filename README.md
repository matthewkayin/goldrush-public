# Gold Rush - Public Repo

This is the public-facing repo of my game Gold Rush. This repo contains all of the code for the game, but it does not contain any of the games assets. This repo is meant as a portfolio piece demonstrating my technical skills and what I have accomplished in making this game.

## What is Gold Rush?

Gold Rush is a Wild West real-time strategy game developed in C++ and OpenGL. As of the time of writing, the game is feature-complete and is planned to be released on Steam for Windows, Mac, and Linux in Fall of 2026. [Steam Page](https://store.steampowered.com/app/3774270/Gold_Rush/)

The rest of this README contains technical info. If you want to learn more about some of the interesting problems I solved while working on this, check out [this article](http://www.matthewkayin.com/goldrush.html) from my portfolio site.

## Project Structure

If you're interested in poking around the code, here is a brief overview of the project structure.

The `src` folder contains mostly my code, with the exception of a few files and functions here and there. The `vendor` folder contains third-party headers for the different libraries. The libraries used in this project are:

- SDL3 - Platform layer and windowing system.
- OpenGL - For 2D rendering. I am using the glad GL loader.
- Enet - A wrapper around UDP that allows for the reliable, in-order delivery of packets. Used for LAN multiplayer.
- Luajit - A fast Lua runtime. Used for campaign level scripts.
- Tracy - Used for profiling.
- Steam API - Used for Steam integration and online multiplayer.

## Code Overview

In this section, I will be discussing files under the `src` folder. 

`main.cpp` is the app entry point, however on Windows systems if you specify `int main()` it will consider the program a console application and a console will always be opened when you launch the program, so on Windows release builds `WinMain()` is used as the entry point instead. Both of these funnel into `gold_main()` which is the real main function.

`gold_main()` lives in `gold.cpp`. It's structure is as follows: 
  1. Initialize everything
  2. Run the game loop  
      1. Timekeep
      2. Poll input events
      3. Service the network and handle network events
      4. Update the game logic
      5. Render
  3. Deinitialize everything

The game loop itself operates as a state machine in which there are three states:
 - Menu - The state used when players are in the main menu.
 - Match - The state used when players are playing a match or watching a replay.
 - Editor - The state used when players are in the level editor. This state is only available in debug builds.

 Much of the code in `gold.cpp` is dedicated to orchestrating this state machine and managing the transitions between states. Each state also has a dedicated folder where the code for that state lives. One of the advantages of this structure is that it makes playtesting levels very fast, because the game can transition from Editor -> Match and then resume the Editor state from where it left off once playtesting is finished.

Now that I've covered the basic project structure, here is an overview of each `src` subfolder:
 - `menu` - Contains code for the Menu state.
 - `match` - Contains code for the Match state.
 - `editor` - Contains code for the Editor state.
 - `container` - Contains custom fixed-sized container classes.
 - `core` - Contains "engine" systems that are used by the different game states. Examples include the sound system, input system, and UI system.
 - `debug` - Handles a global debug env (which is loaded from an env json file). Used in debug mode to configure certain debug features without needing to recompile feature flags.
 - `network` - Contains the networking system.
 - `profile` - Handles profiling. Mostly this just contains the `TracyClient.cpp` file which is part of the Tracy library and must be compiled in to the source in order to instrument the code for profiling.
 - `render` - Contains the renderer.
 - `shared` - Contains code that is shared by both the Menu and Match state but which I felt were not quite "core" systems.
 - `util` - Contains utilities, including fixed-point math, a JSON parser, a checksum function, and an LCG random generator

## MatchState and MatchShell

One of the most important architectural concepts in this project is that the game state for the Match is divided into two structures, the `MatchShell` and the `MatchState`. 

This is done because the game must be deterministic in order for the multiplayer to work. The `MatchState`, therefore, is the simulation; it contains all data which must be in-sync across all players. The `MatchShell`, meanwhile, is the orchestrator of the `MatchState`. It updates the `MatchState` according to network inputs, player inputs, and its own internal timer. The `MatchShell` also behaves differently when players are viewing a replay whereas the `MatchState` behaves the same. 

In this sense, you can almost think of the `MatchState` as being like a tape, where the `MatchShell` is a tape player that is capable of recording, rewinding, and playing back a tape.

## Campaign Levels

Adding the campaign to the game introduced a number of challenges because there are things that the campaign does that are different from regular gameplay. For example, campaign levels have pre-made maps whereas regular gameplay uses procedurally generated maps, and campaign levels have objectives and scripted behaviors whereas regular gameplay does not. The game accounts for these differences in the following ways:
  - There is a `Scenario` object which lives in the `match/scenario` folder. 
    - This object contains all the data needed to load a level such as map data, unit spawn positions, player starting gold amounts, etc.
    - This object is also the "document" that the level editor edits. Scenarios can be saved and loaded either in a JSON format (for authoring) or in a custom binary `.scn` file format (for distribution).
  - In addition to its other initialization functions, the `MatchShell` can also be initialized by passing in a `Scenario`.
  - When loading from a `Scenario`, the `MatchShell` also accepts a Lua script path. The Lua script is then loaded and a Lua context is initialized, stored, and updated in the `MatchShell`.
    - Code related to the Lua scripts are saved in the `match/shell/script` folder.
    - The file `match/shell/script/module.cpp` contains definitions and a registry for all functions exposed by the game to the Lua script.
    - For performance-sensitive functions, the file `match/shell/script/ffi.cpp` contains definitions for functions accessed via LuaJIT's FFI feature. The FFI is created on the Lua-side under `scenario/modules/entities.lua` (`scenario` in this case is the project-root-level `scenario` folder).
    - `/match/shell/script/doc.cpp` contains code that generates a `.d.lua` file based on the contents of `module.cpp`. The docs can be generated by calling `make luadoc` from the project root, and the output is saved in `scenario/modules/scenario.d.lua`. This file helps with editing scripts because it makes the Lua language server aware of the functions that will be exposed by the host program.

## Network Interface

Networking is handled by two different libraries, enet for LAN multiplayer and Steam for online multiplayer. This presented a bit of a challenge, because the two libraries, while providing similar features, behave a little differently. I wanted to provide an interface that allowed the game to make network calls without worrying about which library it was using on the backend. Additionally, there were certain shared behaviors that I wanted to keep between both implementations. For example, when a player joins a lobby, there is a specific handshake that occurs to get everyone connected on a peer-to-peer basis (the joiner introduces themselves to the server, the server introduces the joiner to the other players, and the other players introduce themselves to the joiner), and I wanted to keep this handshake consistent across both backends.

The solution was to introduce the concept of a network backend and frontend. The frontend is contained in `network/network.cpp`, and it contains all of the functions which are used directly by the game. These frontend functions are also where shared behavior, such as the aforementioned handshake, is implemented. The frontend contains a pointer to two abstract classes which are instantiated differently depending on the selected backend. These classes are:
 - `INetworkHost` - Connects to other network "peers" and sends and receives messages between them.
 - `INetworkScanner` - Scans for lobbies available on the network.

I had some trouble writing these abstractions at first, because I was trying to write a singular `INetworkBackend`, and it wasn't working out. Splitting the backend into these two classes was a big leap in making the abstraction possible.
