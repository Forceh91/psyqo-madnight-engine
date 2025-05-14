# PS1 Mesh Loader Example (using psyqo)

This is a practical example of how to load a 3D model from the CD-ROM on the original PlayStation using [psyqo](https://github.com/pcsx-redux/nugget/tree/main/psyqo). It demonstrates:

- Loading a custom `.meshbin` model format from the CD-ROM
- Basic camera control and analog stick input
- Real-time rendering using fixed-point math and the GTE

This project was assembled from various `psyqo` examples, and camera logic was inspired by the [`psxsplash`](https://github.com/psxsplash/psxsplash) project. It aims to provide a real-world example of doing something meaningful with CD-ROM data streaming on the PS1.

---

## 🔧 Requirements

- **psyqo** (included as a submodule)  
  https://github.com/pcsx-redux/nugget/tree/main/psyqo

- **mkpsxiso** (for building the PS1 CD image)  
  https://github.com/Lameguy64/mkpsxiso

Clone the repo with submodules:

```bash
git clone --recursive <this-repo>
```

## 🧱 Mesh Format: `.meshbin`

This example uses a custom binary mesh format (`.meshbin`) designed to be simple to parse on the PS1.

This is generated from a `.obj` exported from Blender at a **scale of 0.125**. For the sake of making it actually visible in the GTE, **each vertex position is multiplied by 128**. This presumes that **1m in blender is 128px on the PS1**.

### 📦 Format Structure

The file is written in the following order (little-endian), using Python-style `struct.pack()` layout:

---

### 🗂️ Header

| Type     | Description   |
|----------|---------------|
| `uint32` | Vertex count  |
| `uint32` | Index count   |
| `uint32` | Face count    |

---

## 🧍 Vertex Positions

```c
int32 x, y, z; // 3x int32_t per vertex
```

## Vertex Colors
#### Note: This isn't implemented in this example

For each vertex paint colour make sure that 0=0, and 1.0 = 128. The PS1 does support up to 255, but 128 is the "neutral" colour, anything higher will make your vertex colour super bright.

For each vertex:

```c
int16 r, g, b; // 3x int16_t per vertex
```

If no color data is provided, RGB values are set to -1.

## Faces (Vertex Indices)

For each face:

```c
int16 v0, v1, v2, v3; // 4x int16_t per face
```

## Normals
#### Note: This isn't implemented in this example

| Type     | Description   |
|----------|---------------|
| `uint32` | Normal count  |

Then for each normal:

```c
int16 x, y, z; // 3x int16_t per normal
```

Then for each face (normal indices):

```c
int16 n0, n1, n2, n3; // 4x int16_t per face
```

## UVs
#### Note: This isn't implemented in this example

| Type     | Description   |
|----------|---------------|
| `uint32` | UV count  |

Make sure that your UVs are multiplied by your texture size. So for example if your texture is 128x128px, then 0=0, 0.5=64, 1.0=128.

Then for each UV:

```c
int16 u, v; // 2x int16_t per UV
```

Then for each face (UV indices):

```c
int16 t0, t1, t2, t3; // 4x int16_t per face
```

## ISO Image

Use [`mkpsxiso`](https://github.com/Lameguy64/mkpsxiso) to pack your disc image, following standard PS1 CD-ROM structure. See the `cdrom` directory for more information on this.

---

## 🎮 Controls

- **Left Analog Stick** — Move forward/backward/left/right  
- **Right Analog Stick** — Rotate camera  
- **Triangle / Cross** — Additional forward/back movement  
- **Square / Circle** — Additional left/right movement  
- **L1 / R1** — Move camera up/down  

Deadzone handling and fixed-point scaling are used to make input smoother and less sensitive.

---

## 📝 License

This project is licensed under the **MIT License**. See the `LICENSE` file for details.
