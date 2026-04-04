<p align="center">
  <img src="grandtide.png" alt="Grand Tide Logo">
</p>

# Grand Tide

**Status:** Actively in Development 🚧

## About the Game
**Grand Tide** is a thrilling 3D raft survival game infused with pirate and magical elements. Players find themselves surviving on a drifting raft, forced to fight off hostile enemy ships. Utilize magical and fire powers to defend your raft and conquer the seas! The game features an immersive atmosphere with dynamic lighting and continuous day-night cycles as you navigate through perilous waters.

## Engine Features
Built from the ground up using a custom C++ OpenGL graphics engine, the project showcases the following tech features:
* **Entity-Component-System (ECS) Framework**: For robust and scalable game object management.
* **Forward Renderer**: Efficient rendering pipeline.
* **Multiple Dynamic Lights**: Supporting complex lighting scenarios.
* **Material System**: Comprehensive Albedo, Specular, and Roughness mapping.
* **Sky Rendering**: Atmospheric skyboxes and horizon rendering.
* **Post-Processing Effects**: Enhancing visual quality with advanced screen-space effects.
* **3D Hit Detection**: Custom collision and intersection logic.

## How to Build & Run
To build the project locally, you will need:
* **CMake**
* A **C++17** compatible compiler

We have provided convenient scripts in the `scripts/` directory for both Windows and Linux to simplify the building, running, and testing process.

**For Linux (`.sh`):**
1. **Build:** Run `./scripts/build.sh` to configure and compile the project (uses `Release` configuration by default).
2. **Run:** Run `./scripts/run.sh` to launch the compiled executable.
3. **Test:** Run `./scripts/test.sh` to execute the CMake test framework (`ctest`).
4. **Integration Tests:** Run `./scripts/run-all.sh` to automatically run all the assignment configurations locally and output screenshots. Then run `./scripts/compare-all.sh` to verify your screenshots against the expected images!

**For Windows (`.bat` and `.ps1`):**
1. **Build:** Run `scripts\build.bat` to configure and compile the project.
2. **Run:** Run `scripts\run.bat` to launch the compiled executable.
3. **Test:** Run `scripts\test.bat` to execute the CMake test framework.
4. **Integration Tests:** Run `scripts\run-all.ps1` to execute all configurations. Then run `scripts\compare-all.ps1` to run image comparisons.

## ✨ Contributors  

<table>
<tr>
  <td align="center"> 
    <a href="https://github.com/ahmedfathy0-0">
      <img src="https://github.com/ahmedfathy0-0.png" width="100">
      <br />
      <sub> Ahmed Fathy </sub>
    </a>
  </td>
  <td align="center"> 
    <a href="https://github.com/YousefAdel23">
      <img src="https://github.com/YousefAdel23.png" width="100">
      <br />
      <sub> Yousef Adel </sub>
    </a>
  </td>
  <td align="center"> 
    <a href="https://github.com/ahmedGamalEllabban">
      <img src="https://github.com/ahmedGamalEllabban.png" width="100">
      <br />
      <sub> Ahmed Gamal </sub>
    </a>
  </td>
  <td align="center"> 
    <a href="https://github.com/mohamed-yasser23">
      <img src="https://github.com/mohamed-yasser23.png" width="100">
      <br />
      <sub> Mohamed Yasser </sub>
    </a>
  </td>
</tr>
</table>
