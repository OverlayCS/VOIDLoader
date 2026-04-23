# VØID Loader

**VØID Loader** is a universal Unity mod loader designed to unify and simplify mod loading across different Unity runtimes. It supports both **Mono** and **IL2CPP**, allowing you to run multiple types of mods through a single system.

---

## Features

* Universal compatibility with **Mono** and **IL2CPP** Unity games
* Load standard **C# DLL mods**
* Support for **MelonLoader** mods
* Support for **BepInEx** plugins
* Native support for **VØID Loader mods**
* Lightweight and efficient runtime integration

---

## Supported Mod Types

VØID Loader can load:

* Standard C# DLL mods
* MelonLoader mods
* BepInEx plugins
* Custom VØID Loader mods

---

## How It Works

VØID Loader hooks into the Unity runtime and provides a unified environment where different mod frameworks and standalone mods can run together. It abstracts the differences between Mono and IL2CPP, reducing compatibility issues and simplifying the modding process.

---

## Use Cases

* Run mods on games that do not support your preferred framework
* Combine mods from different ecosystems in a single setup
* Develop and test custom mod implementations using VØID Loader

---

## Notes

* Compatibility depends on the specific game and its protection mechanisms
* Some mods may require additional setup or configuration

---

## Planned Features

* Improved mod compatibility detection
* Built-in mod manager interface
* Automatic dependency resolution
* Expanded API for custom VØID mods
* External GUI System (you write the gui outside of the mod like in ImGUI or smth)

---

## Contributing

Contributions and feedback are welcome. Feel free to submit issues or pull requests to improve VØID Loader.

---

## Disclaimer

This project is intended for educational and modding purposes only. Ensure you comply with the terms of service of any software you modify.

---

VØID Loader — One loader. Every mod.
